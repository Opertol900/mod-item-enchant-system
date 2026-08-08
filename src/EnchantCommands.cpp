#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "EnchantConditions.h"
#include "EnchantConfig.h"
#include "EnchantDatabase.h"
#include "EnchantManager.h"
#include "EnchantRates.h"
#include "Item.h"
#include "Log.h"
#include "Player.h"
#include "WorldSessionMgr.h"

#include <algorithm>
#include <sstream>
#include <string>

using namespace Acore::ChatCommands;

namespace ItemEnchant
{
namespace
{
constexpr uint32 RBAC_PERM_ENCHANT_USE = 2000;
constexpr uint32 RBAC_PERM_ENCHANT_ADMIN = 2001;

bool ParsePosition(std::string_view args, uint32& bag, uint32& slot, uint32* value = nullptr)
{
    std::istringstream stream{ std::string(args) };
    if (!(stream >> bag >> slot) || bag > 255 || slot > 255)
        return false;
    if (value && !(stream >> *value))
        return false;
    return true;
}

Item* ItemAt(Player* player, uint32 bag, uint32 slot)
{
    return player ? player->GetItemByPos(uint8(bag), uint8(slot)) : nullptr;
}

Player* SelectedOrSelf(ChatHandler* handler)
{
    return handler ? handler->getSelectedPlayerOrSelf() : nullptr;
}

void LogAdminChange(ChatHandler* handler, char const* action, Player const* target, Item const* item,
    uint8 oldLevel, uint8 newLevel)
{
    if (!sEnchantConfig.logsEnabled || !target || !item)
        return;
    std::string const executor = handler && handler->GetPlayer() ? handler->GetPlayer()->GetName() : "console";
    LOG_INFO("module.itemenchant",
        "GM command {}: executor={}, target={}({}), item={}({}), level {} -> {}",
        action, executor, target->GetName(), target->GetGUID().GetCounter(), item->GetEntry(),
        item->GetGUID().GetCounter(), uint32(oldLevel), uint32(newLevel));
}
}

class item_enchant_commandscript : public CommandScript
{
public:
    item_enchant_commandscript() : CommandScript("item_enchant_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable enchantCommands =
        {
            { "use",    HandleUse,    RBAC_PERM_ENCHANT_USE,   Console::No },
            { "info",   HandleInfo,   RBAC_PERM_ENCHANT_USE,   Console::No },
            { "add",    HandleAdd,    RBAC_PERM_ENCHANT_ADMIN, Console::No },
            { "set",    HandleSet,    RBAC_PERM_ENCHANT_ADMIN, Console::No },
            { "remove", HandleRemove, RBAC_PERM_ENCHANT_ADMIN, Console::No },
            { "max",    HandleMax,    RBAC_PERM_ENCHANT_ADMIN, Console::No },
            { "reload", HandleReload, RBAC_PERM_ENCHANT_ADMIN, Console::Yes },
            { "stats",  HandleStats,  RBAC_PERM_ENCHANT_ADMIN, Console::Yes },
        };
        static ChatCommandTable commands = { { "enchant", enchantCommands } };
        return commands;
    }

    static bool HandleUse(ChatHandler* handler, std::string_view args)
    {
        Player* player = handler->GetPlayer();
        uint32 scrollBag, scrollSlot, targetBag, targetSlot;
        std::istringstream stream{ std::string(args) };
        if (!player || !(stream >> scrollBag >> scrollSlot >> targetBag >> targetSlot) ||
            scrollBag > 255 || scrollSlot > 255 || targetBag > 255 || targetSlot > 255)
            return false;
        Item* scroll = ItemAt(player, scrollBag, scrollSlot);
        Item* target = ItemAt(player, targetBag, targetSlot);
        return sEnchantManager.TryEnchant(player, scroll, target);
    }

    static bool HandleInfo(ChatHandler* handler, std::string_view args)
    {
        uint32 bag, slot;
        if (!ParsePosition(args, bag, slot))
            return false;
        Item* item = ItemAt(handler->GetPlayer(), bag, slot);
        if (!item)
            return false;
        handler->PSendSysMessage("Item {} (GUID {}) enchant level: +{}.", item->GetEntry(),
            item->GetGUID().GetCounter(), uint32(sEnchantDatabase.GetLevel(item)));
        return true;
    }

    static bool HandleAdd(ChatHandler* handler, std::string_view args)
    {
        uint32 bag, slot, amount;
        if (!sEnchantConfig.gmCommandsEnabled || !ParsePosition(args, bag, slot, &amount))
            return false;
        Player* targetPlayer = SelectedOrSelf(handler);
        Item* item = ItemAt(targetPlayer, bag, slot);
        if (!item)
            return false;
        uint8 const oldLevel = sEnchantDatabase.GetLevel(item);
        uint32 const level = std::min<uint32>(oldLevel + amount, sEnchantConfig.gmMaxEnchant);
        if (!sEnchantManager.SetLevelByCommand(targetPlayer, item, uint8(level)))
            return false;
        LogAdminChange(handler, "add", targetPlayer, item, oldLevel, uint8(level));
        handler->PSendSysMessage("Enchant level set to +{}.", level);
        return true;
    }

    static bool HandleSet(ChatHandler* handler, std::string_view args)
    {
        uint32 bag, slot, level;
        if (!sEnchantConfig.gmCommandsEnabled || !ParsePosition(args, bag, slot, &level) || level > sEnchantConfig.gmMaxEnchant)
            return false;
        Player* targetPlayer = SelectedOrSelf(handler);
        Item* item = ItemAt(targetPlayer, bag, slot);
        if (!item)
            return false;
        uint8 const oldLevel = sEnchantDatabase.GetLevel(item);
        if (!sEnchantManager.SetLevelByCommand(targetPlayer, item, uint8(level)))
            return false;
        LogAdminChange(handler, "set", targetPlayer, item, oldLevel, uint8(level));
        handler->PSendSysMessage("Enchant level set to +{}.", level);
        return true;
    }

    static bool HandleRemove(ChatHandler* handler, std::string_view args)
    {
        uint32 bag, slot;
        if (!sEnchantConfig.gmCommandsEnabled || !ParsePosition(args, bag, slot))
            return false;
        Player* targetPlayer = SelectedOrSelf(handler);
        Item* item = ItemAt(targetPlayer, bag, slot);
        if (!item)
            return false;
        uint8 const oldLevel = sEnchantDatabase.GetLevel(item);
        if (!sEnchantManager.SetLevelByCommand(targetPlayer, item, 0))
            return false;
        LogAdminChange(handler, "remove", targetPlayer, item, oldLevel, 0);
        handler->SendSysMessage("Enchant level removed.");
        return true;
    }

    static bool HandleMax(ChatHandler* handler, std::string_view args)
    {
        uint32 bag, slot;
        if (!sEnchantConfig.gmCommandsEnabled || !ParsePosition(args, bag, slot))
            return false;
        Player* targetPlayer = SelectedOrSelf(handler);
        Item* item = ItemAt(targetPlayer, bag, slot);
        if (!item)
            return false;
        uint8 const oldLevel = sEnchantDatabase.GetLevel(item);
        uint8 const maximum = sEnchantConditions.GetMaxEnchant(item->GetTemplate(), ScrollType::Normal);
        if (!sEnchantManager.SetLevelByCommand(targetPlayer, item, maximum))
            return false;
        LogAdminChange(handler, "max", targetPlayer, item, oldLevel, maximum);
        handler->PSendSysMessage("Enchant level set to configured maximum +{}.", uint32(maximum));
        return true;
    }

    static bool HandleReload(ChatHandler* handler)
    {
        sWorldSessionMgr->DoForAllOnlinePlayers([](Player* player)
        {
            sEnchantManager.ReapplyEquipmentModifiers(player, false);
        });

        if (!sConfigMgr->Reload())
        {
            sWorldSessionMgr->DoForAllOnlinePlayers([](Player* player)
            {
                sEnchantManager.ReapplyEquipmentModifiers(player, true);
            });
            handler->SendSysMessage("Unable to reload configuration.");
            return false;
        }
        sEnchantConfig.Load();
        sEnchantRates.Load();
        sWorldSessionMgr->DoForAllOnlinePlayers([](Player* player)
        {
            sEnchantManager.ReapplyEquipmentModifiers(player, true);
        });
        handler->SendSysMessage("Item enchant configuration reloaded.");
        return true;
    }

    static bool HandleStats(ChatHandler* handler)
    {
        EnchantStatistics const stats = sEnchantDatabase.GetStatistics();
        handler->PSendSysMessage("Enchant attempts: {}, success: {}, failed: {}, destroyed: {}, pending: {}.",
            stats.total, stats.successful, stats.failed, stats.destroyed, stats.pending);
        return true;
    }
};

void AddSC_item_enchant_commands()
{
    new item_enchant_commandscript();
}
}
