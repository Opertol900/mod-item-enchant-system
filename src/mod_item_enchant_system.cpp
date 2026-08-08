#include "AllItemScript.h"
#include "EnchantConfig.h"
#include "EnchantDatabase.h"
#include "EnchantManager.h"
#include "EnchantRates.h"
#include "EnchantStatCalculator.h"
#include "Item.h"
#include "Player.h"
#include "PlayerScript.h"
#include "WorldScript.h"
#include "WorldSessionMgr.h"

#include <unordered_map>

void AddSC_item_mod_enchant_scroll();

namespace ItemEnchant
{
void AddSC_item_enchant_commands();

class item_enchant_world_script : public WorldScript
{
public:
    item_enchant_world_script() : WorldScript("item_enchant_world_script") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        if (reload)
            sWorldSessionMgr->DoForAllOnlinePlayers([](Player* player)
            {
                sEnchantManager.ReapplyEquipmentModifiers(player, false);
            });
    }

    void OnAfterConfigLoad(bool reload) override
    {
        sEnchantConfig.Load();
        sEnchantRates.Load();
        if (reload)
            sWorldSessionMgr->DoForAllOnlinePlayers([](Player* player)
            {
                sEnchantManager.ReapplyEquipmentModifiers(player, true);
            });
    }

    void OnStartup() override
    {
        sEnchantDatabase.CleanupOrphans();
    }
};

class item_enchant_player_script : public PlayerScript
{
public:
    item_enchant_player_script() : PlayerScript("item_enchant_player_script") { }

    void OnPlayerLoadFromDB(Player* player) override
    {
        sEnchantDatabase.LoadForPlayer(player);
    }

    void OnPlayerLogin(Player* player) override
    {
        if (sEnchantConfig.enabled)
            for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
                if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                    sEnchantStatCalculator.ApplyStaticBonuses(player, item, sEnchantDatabase.GetLevel(item), true);
        sEnchantManager.RecoverPending(player);
        if (sEnchantConfig.enabled)
            sEnchantManager.RefreshAuras(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        uint32 const playerGuid = player->GetGUID().GetCounter();
        _staticReconcileTimers.erase(playerGuid);
        sEnchantStatCalculator.ForgetPlayer(playerGuid);
        sEnchantManager.ClearPlayer(playerGuid);
        sEnchantDatabase.UnloadPlayer(playerGuid);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!sEnchantConfig.enabled)
            return;
        uint32& elapsed = _staticReconcileTimers[player->GetGUID().GetCounter()];
        elapsed += diff;
        if (elapsed < 1000)
            return;
        elapsed = 0;
        sEnchantStatCalculator.ReconcilePlayer(player);
        sEnchantManager.RefreshAuras(player);
    }

    void OnPlayerEquip(Player* player, Item* item, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        if (sEnchantConfig.enabled && player->IsInWorld())
        {
            sEnchantStatCalculator.ApplyStaticBonuses(player, item, sEnchantDatabase.GetLevel(item), true);
            sEnchantManager.RefreshAuras(player);
        }
    }

    void OnPlayerUnequip(Player* player, Item* item) override
    {
        if (sEnchantConfig.enabled && player->IsInWorld())
        {
            sEnchantStatCalculator.ApplyStaticBonuses(player, item, sEnchantDatabase.GetLevel(item), false);
            sEnchantManager.RefreshAuras(player);
        }
    }

    void OnPlayerApplyItemModsBefore(Player* player, uint8 slot, bool /*apply*/, uint8 /*statIndex*/,
        uint32 statType, int32& value) override
    {
        if (!sEnchantConfig.enabled || !sEnchantStatCalculator.IsStatAllowed(statType))
            return;
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            value = sEnchantStatCalculator.Scale(value, sEnchantDatabase.GetLevel(item));
    }

    void OnPlayerApplyWeaponDamage(Player* player, uint8 slot, ItemTemplate const* /*itemTemplate*/,
        float& minDamage, float& maxDamage, uint8 /*damageIndex*/) override
    {
        if (!sEnchantConfig.enabled || !sEnchantStatCalculator.IsWeaponDamageAllowed())
            return;
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            uint8 const level = sEnchantDatabase.GetLevel(item);
            minDamage = sEnchantStatCalculator.Scale(minDamage, level);
            maxDamage = sEnchantStatCalculator.Scale(maxDamage, level);
        }
    }

private:
    std::unordered_map<uint32, uint32> _staticReconcileTimers;
};

class item_enchant_all_item_script : public AllItemScript
{
public:
    item_enchant_all_item_script() : AllItemScript("item_enchant_all_item_script") { }

    bool CanItemRemove(Player* player, Item* item) override
    {
        if (sEnchantConfig.enabled && player && item && item->IsEquipped())
        {
            sEnchantStatCalculator.ApplyStaticBonuses(player, item, sEnchantDatabase.GetLevel(item), false);
            sEnchantManager.RefreshAuras(player, item);
        }
        return true;
    }
};
}

void Addmod_item_enchant_systemScripts()
{
    new ItemEnchant::item_enchant_world_script();
    new ItemEnchant::item_enchant_player_script();
    new ItemEnchant::item_enchant_all_item_script();
    AddSC_item_mod_enchant_scroll();
    ItemEnchant::AddSC_item_enchant_commands();
}
