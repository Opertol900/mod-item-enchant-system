#include "EnchantManager.h"

#include "Bag.h"
#include "Chat.h"
#include "EnchantConditions.h"
#include "EnchantConfig.h"
#include "EnchantDatabase.h"
#include "EnchantItem.h"
#include "EnchantRates.h"
#include "EnchantStatCalculator.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Random.h"
#include "ScriptedGossip.h"
#include "Timer.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>

namespace ItemEnchant
{
namespace
{
constexpr uint32 GOSSIP_SENDER_ENCHANT = 7310;
constexpr uint32 ACTION_PREVIOUS_PAGE = 1;
constexpr uint32 ACTION_NEXT_PAGE = 2;
constexpr uint32 ACTION_CLOSE = 3;
constexpr uint32 ACTION_ITEM_BASE = 100;

bool IsRussian(Player const* player)
{
    return player && player->GetSession() && player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_ruRU;
}

FailureMode GetFailureMode(ScrollType type)
{
    switch (type)
    {
        case ScrollType::Normal: return FailureMode::Destroy;
        case ScrollType::Blessed: return FailureMode::Reset;
        case ScrollType::Safe: return FailureMode::Keep;
        case ScrollType::Event: return sEnchantConfig.eventFailureMode;
        case ScrollType::Crystal:
        case ScrollType::GameMaster: return FailureMode::Keep;
    }
    return FailureMode::Keep;
}
}

EnchantManager& EnchantManager::Instance()
{
    static EnchantManager instance;
    return instance;
}

uint64 EnchantManager::NextOperationId() const
{
    static std::atomic<uint64> sequence{ 0 };
    uint64 const milliseconds = uint64(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    return (milliseconds << 16) | (++sequence & 0xFFFF);
}

Item* EnchantManager::FindItem(Player* player, uint32 itemGuid) const
{
    return player && itemGuid ? player->GetItemByGuid(ObjectGuid::Create<HighGuid::Item>(itemGuid)) : nullptr;
}

std::vector<Item*> EnchantManager::GetEligibleItems(Player* player, ScrollType scrollType) const
{
    std::vector<Item*> items;
    if (!player)
        return items;

    auto addIfEligible = [&](Item* item)
    {
        if (!item || !sEnchantConditions.Validate(player, item, scrollType).allowed)
            return;
        if (sEnchantDatabase.GetLevel(item) >= sEnchantConditions.GetMaxEnchant(item->GetTemplate(), scrollType))
            return;
        items.push_back(item);
    };

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        addIfEligible(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        addIfEligible(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        if (Bag* bag = player->GetBagByPos(bagSlot))
            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
                addIfEligible(bag->GetItemByPos(slot));
    }

    std::sort(items.begin(), items.end(), [](Item const* left, Item const* right)
    {
        uint8 const leftLevel = sEnchantDatabase.GetLevel(left);
        uint8 const rightLevel = sEnchantDatabase.GetLevel(right);
        if (leftLevel != rightLevel)
            return leftLevel > rightLevel;
        return left->GetEntry() < right->GetEntry();
    });
    return items;
}

bool EnchantManager::ShowItemSelection(Player* player, Item* scroll)
{
    if (!player || !scroll || scroll->GetOwnerGUID() != player->GetGUID())
        return false;
    if (!sEnchantConfig.enabled)
    {
        SendValidationError(player, "system_disabled");
        return false;
    }

    ScrollType scrollType;
    if (!sEnchantConditions.IsScrollEntry(scroll->GetEntry(), &scrollType))
        return false;
    if (scrollType == ScrollType::GameMaster && player->GetSession()->GetSecurity() < sEnchantConfig.gmSecurityLevel)
    {
        SendValidationError(player, "gm_only");
        return false;
    }

    SelectionSession session;
    session.scrollGuid = scroll->GetGUID().GetCounter();
    session.scrollType = scrollType;
    session.openedAt = std::chrono::steady_clock::now();
    for (Item* item : GetEligibleItems(player, scrollType))
        session.itemGuids.push_back(item->GetGUID().GetCounter());

    {
        std::lock_guard lock(_mutex);
        _sessions[player->GetGUID().GetCounter()] = std::move(session);
    }
    return RenderSelection(player, scroll);
}

bool EnchantManager::RenderSelection(Player* player, Item* scroll)
{
    if (!player || !scroll)
        return false;

    SelectionSession session;
    {
        std::lock_guard lock(_mutex);
        auto const itr = _sessions.find(player->GetGUID().GetCounter());
        if (itr == _sessions.end() || itr->second.scrollGuid != scroll->GetGUID().GetCounter())
            return false;
        session = itr->second;
    }

    ClearGossipMenuFor(player);
    uint32 const pageSize = sEnchantConfig.gossipPageSize;
    uint32 const first = session.page * pageSize;
    uint32 const last = std::min(first + pageSize, uint32(session.itemGuids.size()));
    for (uint32 index = first; index < last; ++index)
    {
        if (Item* item = FindItem(player, session.itemGuids[index]))
        {
            uint8 const level = sEnchantDatabase.GetLevel(item);
            std::string label = sEnchantItem.GetDisplayName(player, item);
            label += "  ->  +" + std::to_string(uint32(level) + 1);
            std::string confirmation = IsRussian(player) ? "Подтвердить попытку заточки? Будет израсходован один свиток."
                : "Confirm this enchant attempt? One scroll will be consumed.";
            if (session.scrollType == ScrollType::Normal)
                confirmation += IsRussian(player) ? " При неудаче предмет будет уничтожен."
                    : " Failure will destroy the target item.";
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, label, GOSSIP_SENDER_ENCHANT, ACTION_ITEM_BASE + index,
                confirmation, 0, false);
        }
    }

    if (session.itemGuids.empty())
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            IsRussian(player) ? "Нет подходящих предметов." : "No eligible items.",
            GOSSIP_SENDER_ENCHANT, ACTION_CLOSE);
    if (session.page > 0)
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, IsRussian(player) ? "< Назад" : "< Previous",
            GOSSIP_SENDER_ENCHANT, ACTION_PREVIOUS_PAGE);
    if (last < session.itemGuids.size())
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, IsRussian(player) ? "Далее >" : "Next >",
            GOSSIP_SENDER_ENCHANT, ACTION_NEXT_PAGE);
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, IsRussian(player) ? "Закрыть" : "Close",
        GOSSIP_SENDER_ENCHANT, ACTION_CLOSE);
    SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, scroll->GetGUID());
    return true;
}

void EnchantManager::HandleGossipSelection(Player* player, Item* scroll, uint32 sender, uint32 action)
{
    if (!player || !scroll || sender != GOSSIP_SENDER_ENCHANT)
        return;

    uint32 const playerGuid = player->GetGUID().GetCounter();
    SelectionSession session;
    {
        std::lock_guard lock(_mutex);
        auto itr = _sessions.find(playerGuid);
        if (itr == _sessions.end() || itr->second.scrollGuid != scroll->GetGUID().GetCounter() ||
            std::chrono::steady_clock::now() - itr->second.openedAt > std::chrono::seconds(60))
        {
            _sessions.erase(playerGuid);
            CloseGossipMenuFor(player);
            return;
        }
        if (action == ACTION_PREVIOUS_PAGE && itr->second.page)
            --itr->second.page;
        else if (action == ACTION_NEXT_PAGE && (itr->second.page + 1) * sEnchantConfig.gossipPageSize < itr->second.itemGuids.size())
            ++itr->second.page;
        session = itr->second;
    }

    if (action == ACTION_PREVIOUS_PAGE || action == ACTION_NEXT_PAGE)
    {
        RenderSelection(player, scroll);
        return;
    }
    if (action == ACTION_CLOSE)
    {
        ClearSelection(playerGuid);
        CloseGossipMenuFor(player);
        return;
    }
    if (action < ACTION_ITEM_BASE || action - ACTION_ITEM_BASE >= session.itemGuids.size())
        return;

    Item* target = FindItem(player, session.itemGuids[action - ACTION_ITEM_BASE]);
    ClearSelection(playerGuid);
    CloseGossipMenuFor(player);
    if (!target)
    {
        SendValidationError(player, "item_missing");
        return;
    }
    TryEnchant(player, scroll, target);
}

bool EnchantManager::AcquireAttempt(uint32 playerGuid)
{
    std::lock_guard lock(_mutex);
    if (_busyPlayers.contains(playerGuid))
        return false;
    uint32 const now = getMSTime();
    auto const last = _lastAttemptMs.find(playerGuid);
    if (last != _lastAttemptMs.end() && getMSTimeDiff(last->second, now) < sEnchantConfig.attemptCooldownMs)
        return false;
    _lastAttemptMs[playerGuid] = now;
    _busyPlayers.insert(playerGuid);
    return true;
}

void EnchantManager::ReleaseAttempt(uint32 playerGuid)
{
    std::lock_guard lock(_mutex);
    _busyPlayers.erase(playerGuid);
}

void EnchantManager::ClearSelection(uint32 playerGuid)
{
    std::lock_guard lock(_mutex);
    _sessions.erase(playerGuid);
}

bool EnchantManager::TryEnchant(Player* player, Item* scroll, Item* target, bool recovery)
{
    if (!player || !scroll || !target || scroll == target || scroll->GetOwnerGUID() != player->GetGUID())
        return false;

    ScrollType scrollType;
    if (!sEnchantConditions.IsScrollEntry(scroll->GetEntry(), &scrollType))
        return false;
    ValidationResult const validation = sEnchantConditions.Validate(player, target, scrollType);
    if (!validation.allowed)
    {
        if (sEnchantConfig.debug)
            LOG_DEBUG("module.itemenchant", "Rejected player={}, scroll={}, item={}: {}",
                player->GetGUID().GetCounter(), scroll->GetEntry(), target->GetEntry(), validation.reason);
        SendValidationError(player, validation.reason);
        return false;
    }

    uint8 const oldLevel = sEnchantDatabase.GetLevel(target);
    uint8 const maximum = sEnchantConditions.GetMaxEnchant(target->GetTemplate(), scrollType);
    if (oldLevel >= maximum)
    {
        SendValidationError(player, "max_level");
        return false;
    }

    uint32 const playerGuid = player->GetGUID().GetCounter();
    if (!recovery && !AcquireAttempt(playerGuid))
    {
        if (sEnchantConfig.logsEnabled)
            LOG_WARN("module.itemenchant", "Blocked concurrent or throttled enchant attempt: player={}, scroll={}, item={}",
                playerGuid, scroll->GetEntry(), target->GetEntry());
        SendValidationError(player, "too_fast");
        return false;
    }

    PendingOperation operation;
    operation.operationId = NextOperationId();
    operation.ownerGuid = playerGuid;
    operation.itemGuid = target->GetGUID().GetCounter();
    operation.itemEntry = target->GetEntry();
    operation.scrollGuid = scroll->GetGUID().GetCounter();
    operation.scrollEntry = scroll->GetEntry();
    operation.scrollType = scrollType;
    operation.oldLevel = oldLevel;
    operation.chanceBasisPoints = sEnchantRates.GetChanceBasisPoints(oldLevel + 1,
        sEnchantConditions.GetCategory(target->GetTemplate()), scrollType);
    operation.roll = urand(1, 10000);
    operation.success = operation.roll <= operation.chanceBasisPoints;

    AttemptResult result = AttemptResult::Success;
    if (operation.success)
        operation.newLevel = oldLevel + 1;
    else
    {
        switch (GetFailureMode(scrollType))
        {
            case FailureMode::Destroy:
                operation.destroyItem = true;
                operation.newLevel = 0;
                result = AttemptResult::FailedDestroyed;
                break;
            case FailureMode::Reset:
                operation.newLevel = 0;
                result = AttemptResult::FailedReset;
                break;
            case FailureMode::Keep:
                operation.newLevel = oldLevel;
                result = AttemptResult::FailedKept;
                break;
        }
    }

    if (!sEnchantDatabase.BeginPending(operation))
    {
        if (!recovery)
            ReleaseAttempt(playerGuid);
        SendValidationError(player, "database_error");
        return false;
    }

    bool const applied = ApplyOperation(player, scroll, target, operation, result, recovery);
    if (!recovery)
        ReleaseAttempt(playerGuid);
    return applied;
}

bool EnchantManager::ApplyOperation(Player* player, Item* scroll, Item* target, PendingOperation const& operation,
    AttemptResult result, bool recovery)
{
    std::string const itemName = sEnchantItem.GetLocalizedName(player, target);
    uint8 const finalLevel = operation.newLevel;
    bool const levelChanged = !operation.destroyItem && finalLevel != operation.oldLevel;
    bool const equipped = target->IsEquipped();
    uint8 const slot = target->GetSlot();

    if (levelChanged && equipped)
    {
        if (sEnchantConfig.enabled)
            sEnchantStatCalculator.ApplyStaticBonuses(player, target, operation.oldLevel, false);
        player->_ApplyItemMods(target, slot, false);
    }

    if (operation.destroyItem)
    {
        player->DestroyItem(target->GetBagSlot(), target->GetSlot(), true);
        sEnchantDatabase.EraseCachedLevel(operation.itemGuid);
    }
    else
    {
        sEnchantDatabase.SetCachedLevel(operation.itemGuid, operation.ownerGuid, finalLevel);
        if (operation.success && sEnchantConfig.bindOnEnchant && !target->IsSoulBound())
        {
            target->SetBinding(true);
            target->SetState(ITEM_CHANGED, player);
        }
        if (levelChanged && equipped)
        {
            player->_ApplyItemMods(target, slot, true);
            if (sEnchantConfig.enabled)
                sEnchantStatCalculator.ApplyStaticBonuses(player, target, finalLevel, true);
        }
    }

    uint32 consumeCount = 1;
    player->DestroyItemCount(scroll, consumeCount, true);

    if (!sEnchantDatabase.CommitOperation(player, operation, result, finalLevel, recovery ? "recovery" : ""))
    {
        LOG_ERROR("module.itemenchant", "Failed to commit operation {} for player {}", operation.operationId, operation.ownerGuid);
        SendValidationError(player, "database_error");
        player->GetSession()->KickPlayer("Item enchant transaction could not be verified");
        return false;
    }

    RefreshAuras(player);
    SendResult(player, recovery ? AttemptResult::Recovered : result, itemName, finalLevel);
    if (operation.success && sEnchantConfig.broadcastHighEnchant && finalLevel >= sEnchantConfig.broadcastLevel)
    {
        std::string text = player->GetName() + " enchanted " + itemName + " to +" + std::to_string(finalLevel) + ".";
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, text);
    }
    LOG_INFO("module.itemenchant", "Operation {}: player={}, item={}({}), {} -> {}, result={}, chance={}, roll={}",
        operation.operationId, operation.ownerGuid, operation.itemEntry, operation.itemGuid,
        uint32(operation.oldLevel), uint32(finalLevel), ToString(result), operation.chanceBasisPoints, operation.roll);
    return true;
}

bool EnchantManager::SetLevelByCommand(Player* player, Item* target, uint8 newLevel)
{
    if (!player || !target || target->GetOwnerGUID() != player->GetGUID())
        return false;
    ItemTemplate const* itemTemplate = target->GetTemplate();
    if (!itemTemplate || itemTemplate->InventoryType == INVTYPE_NON_EQUIP ||
        sEnchantConditions.IsScrollEntry(target->GetEntry()))
        return false;

    uint8 const oldLevel = sEnchantDatabase.GetLevel(target);
    if (oldLevel == newLevel)
        return true;
    bool const equipped = target->IsEquipped();
    uint8 const slot = target->GetSlot();
    if (equipped)
    {
        if (sEnchantConfig.enabled)
            sEnchantStatCalculator.ApplyStaticBonuses(player, target, oldLevel, false);
        player->_ApplyItemMods(target, slot, false);
    }

    sEnchantDatabase.SetCachedLevel(target->GetGUID().GetCounter(), player->GetGUID().GetCounter(), newLevel);
    if (!sEnchantDatabase.SetLevel(target->GetGUID().GetCounter(), player->GetGUID().GetCounter(), target->GetEntry(), newLevel))
    {
        sEnchantDatabase.SetCachedLevel(target->GetGUID().GetCounter(), player->GetGUID().GetCounter(), oldLevel);
        if (equipped)
        {
            player->_ApplyItemMods(target, slot, true);
            if (sEnchantConfig.enabled)
                sEnchantStatCalculator.ApplyStaticBonuses(player, target, oldLevel, true);
        }
        return false;
    }
    if (equipped)
    {
        player->_ApplyItemMods(target, slot, true);
        if (sEnchantConfig.enabled)
            sEnchantStatCalculator.ApplyStaticBonuses(player, target, newLevel, true);
    }
    target->SetState(ITEM_CHANGED, player);
    player->SaveToDB(false, false);
    RefreshAuras(player);
    return true;
}

void EnchantManager::RecoverPending(Player* player)
{
    if (!player)
        return;
    for (PendingOperation const& operation : sEnchantDatabase.LoadPending(player->GetGUID().GetCounter()))
    {
        Item* target = FindItem(player, operation.itemGuid);
        Item* scroll = FindItem(player, operation.scrollGuid);
        if (!scroll || scroll->GetEntry() != operation.scrollEntry)
        {
            sEnchantDatabase.CompletePending(operation.operationId, "aborted");
            sEnchantDatabase.LogAttempt(operation, AttemptResult::Aborted, "scroll_missing");
            continue;
        }
        if (!target)
        {
            if (operation.destroyItem)
            {
                uint32 count = 1;
                player->DestroyItemCount(scroll, count, true);
                if (!sEnchantDatabase.CommitOperation(player, operation, AttemptResult::FailedDestroyed, 0,
                    "recovered_destroy"))
                {
                    LOG_ERROR("module.itemenchant", "Failed to commit recovered operation {} for player {}",
                        operation.operationId, operation.ownerGuid);
                    player->GetSession()->KickPlayer("Recovered item enchant transaction could not be verified");
                    return;
                }
                SendResult(player, AttemptResult::Recovered, std::string{}, 0);
            }
            else
            {
                sEnchantDatabase.CompletePending(operation.operationId, "aborted");
                sEnchantDatabase.LogAttempt(operation, AttemptResult::Aborted, "target_missing");
            }
            continue;
        }
        if (target->GetEntry() != operation.itemEntry || sEnchantDatabase.GetLevel(target) != operation.oldLevel)
        {
            sEnchantDatabase.CompletePending(operation.operationId, "aborted");
            sEnchantDatabase.LogAttempt(operation, AttemptResult::Aborted, "target_state_mismatch");
            continue;
        }

        AttemptResult result = AttemptResult::Success;
        if (!operation.success)
        {
            FailureMode const failureMode = GetFailureMode(operation.scrollType);
            result = operation.destroyItem ? AttemptResult::FailedDestroyed :
                (failureMode == FailureMode::Reset ? AttemptResult::FailedReset : AttemptResult::FailedKept);
        }
        ApplyOperation(player, scroll, target, operation, result, true);
    }
}

void EnchantManager::RefreshAuras(Player* player, Item const* excludedItem) const
{
    if (!player)
        return;

    uint8 highestLevel = 0;
    if (sEnchantConfig.enabled && sEnchantConfig.enableAura)
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
            if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                if (item != excludedItem && !item->IsBroken())
                    highestLevel = std::max(highestLevel, sEnchantDatabase.GetLevel(item));

    uint32 selectedLevel = 0;
    uint32 selectedSpell = 0;
    if (sEnchantConfig.enabled && sEnchantConfig.enableAura)
        for (auto const& [level, spellId] : sEnchantConfig.auraByLevel)
        {
            if (level <= highestLevel && level >= selectedLevel)
            {
                selectedLevel = level;
                selectedSpell = spellId;
            }
        }

    uint32 const playerGuid = player->GetGUID().GetCounter();
    uint32 currentSpell = 0;
    bool wasTracked = false;
    {
        std::lock_guard lock(_mutex);
        auto const itr = _activeAuraByPlayer.find(playerGuid);
        if (itr != _activeAuraByPlayer.end())
        {
            wasTracked = true;
            currentSpell = itr->second;
        }
        _activeAuraByPlayer[playerGuid] = selectedSpell;
    }

    if (!wasTracked && sEnchantConfig.enableAura)
        for (auto const& [level, spellId] : sEnchantConfig.auraByLevel)
            if (spellId && spellId != selectedSpell)
                player->RemoveAurasDueToSpell(spellId);
    if (currentSpell && currentSpell != selectedSpell)
        player->RemoveAurasDueToSpell(currentSpell);
    if (selectedSpell && !player->HasAura(selectedSpell))
        player->CastSpell(player, selectedSpell, true);
}

void EnchantManager::ReapplyEquipmentModifiers(Player* player, bool apply) const
{
    if (!player)
        return;

    if (!apply && sEnchantConfig.enableAura)
    {
        for (auto const& [level, spellId] : sEnchantConfig.auraByLevel)
            if (spellId)
                player->RemoveAurasDueToSpell(spellId);
        std::lock_guard lock(_mutex);
        _activeAuraByPlayer.erase(player->GetGUID().GetCounter());
    }

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        uint8 const level = sEnchantDatabase.GetLevel(item);
        if (!apply && sEnchantConfig.enabled)
            sEnchantStatCalculator.ApplyStaticBonuses(player, item, level, false);
        player->_ApplyItemMods(item, slot, apply);
        if (apply && sEnchantConfig.enabled)
            sEnchantStatCalculator.ApplyStaticBonuses(player, item, level, true);
    }

    if (apply)
        RefreshAuras(player);
}

void EnchantManager::ClearPlayer(uint32 playerGuid)
{
    std::lock_guard lock(_mutex);
    _sessions.erase(playerGuid);
    _busyPlayers.erase(playerGuid);
    _lastAttemptMs.erase(playerGuid);
    _activeAuraByPlayer.erase(playerGuid);
}

void EnchantManager::SendResult(Player* player, AttemptResult result, std::string const& itemName, uint8 level) const
{
    ChatHandler handler(player->GetSession());
    bool const russian = IsRussian(player);
    switch (result)
    {
        case AttemptResult::Success:
            handler.PSendSysMessage(russian ? "Успешная заточка: {} теперь +{}." : "Enchant succeeded: {} is now +{}.", itemName, uint32(level));
            break;
        case AttemptResult::FailedDestroyed:
            handler.PSendSysMessage(russian ? "Заточка не удалась. Предмет {} уничтожен." : "Enchant failed. {} was destroyed.", itemName);
            break;
        case AttemptResult::FailedReset:
            handler.PSendSysMessage(russian ? "Заточка не удалась. {} сохранён, уровень сброшен." : "Enchant failed. {} survived, but its level was reset.", itemName);
            break;
        case AttemptResult::FailedKept:
            handler.PSendSysMessage(russian ? "Заточка не удалась. {} сохранён на уровне +{}." : "Enchant failed. {} remains at +{}.", itemName, uint32(level));
            break;
        case AttemptResult::Recovered:
            handler.PSendSysMessage(russian ? "Незавершённая операция заточки безопасно восстановлена." : "An interrupted enchant operation was safely recovered.");
            break;
        case AttemptResult::Aborted:
            break;
    }
}

void EnchantManager::SendValidationError(Player* player, std::string const& reason) const
{
    bool const russian = IsRussian(player);
    std::string message;
    if (reason == "system_disabled") message = russian ? "Система заточки отключена." : "The enchant system is disabled.";
    else if (reason == "in_combat") message = russian ? "Нельзя точить предметы в бою." : "You cannot enchant items in combat.";
    else if (reason == "in_battleground" || reason == "in_arena") message = russian ? "Заточка здесь запрещена." : "Enchanting is not allowed here.";
    else if (reason == "max_level") message = russian ? "Достигнут максимальный уровень заточки." : "The maximum enchant level has been reached.";
    else if (reason == "gm_only") message = russian ? "Этот свиток доступен только администраторам." : "This scroll is restricted to administrators.";
    else if (reason == "too_fast") message = russian ? "Подождите перед следующей попыткой." : "Please wait before trying again.";
    else if (reason == "database_error") message = russian ? "Операция остановлена: ошибка базы данных." : "The operation was stopped by a database error.";
    else if (reason == "item_missing") message = russian ? "Предмет больше не найден." : "The item can no longer be found.";
    else if (reason == "blacklisted" || reason == "not_whitelisted") message = russian ? "Этот предмет запрещено точить." : "This item cannot be enchanted.";
    else if (reason == "not_owner") message = russian ? "Вы не владеете этим предметом." : "You do not own this item.";
    else message = russian ? "Этот предмет не соответствует условиям заточки." : "This item does not meet the enchant requirements.";
    ChatHandler(player->GetSession()).SendSysMessage(message);
}
}
