#include "EnchantStatCalculator.h"

#include "EnchantConfig.h"
#include "EnchantDatabase.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "SharedDefines.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace ItemEnchant
{
EnchantStatCalculator& EnchantStatCalculator::Instance()
{
    static EnchantStatCalculator instance;
    return instance;
}

bool EnchantStatCalculator::StaticBonus::Empty() const
{
    return !armor && !block && !holy && !fire && !nature && !frost && !shadow && !arcane;
}

bool EnchantStatCalculator::IsStatAllowed(uint32 statType) const
{
    if (sEnchantConfig.forbiddenStats.contains(statType))
        return false;
    if (sEnchantConfig.statMode == StatMode::AllStats)
        return true;
    if (sEnchantConfig.statMode == StatMode::Custom)
        return sEnchantConfig.customStats.contains(statType);
    return statType == ITEM_MOD_STRENGTH || statType == ITEM_MOD_AGILITY || statType == ITEM_MOD_STAMINA ||
        statType == ITEM_MOD_INTELLECT || statType == ITEM_MOD_SPIRIT;
}

bool EnchantStatCalculator::IsSyntheticStatAllowed(uint32 statType) const
{
    if (sEnchantConfig.forbiddenStats.contains(statType))
        return false;
    return sEnchantConfig.statMode == StatMode::AllStats ||
        (sEnchantConfig.statMode == StatMode::Custom && sEnchantConfig.customStats.contains(statType));
}

bool EnchantStatCalculator::IsWeaponDamageAllowed() const
{
    return sEnchantConfig.scaleWeaponDamage && IsSyntheticStatAllowed(ENCHANT_STAT_WEAPON_DAMAGE);
}

float EnchantStatCalculator::GetMultiplier(uint8 enchantLevel) const
{
    if (!enchantLevel)
        return 1.0f;

    switch (sEnchantConfig.formulaMode)
    {
        case FormulaMode::LinearPercent:
            return 1.0f + (sEnchantConfig.linearPercentPerLevel * float(enchantLevel) / 100.0f);
        case FormulaMode::TablePercent:
        {
            uint32 bestLevel = 0;
            float percent = 0.0f;
            for (auto const& [level, configuredPercent] : sEnchantConfig.statTablePercent)
            {
                if (level <= enchantLevel && level >= bestLevel)
                {
                    bestLevel = level;
                    percent = configuredPercent;
                }
            }
            return 1.0f + percent / 100.0f;
        }
        case FormulaMode::Exponential:
            return std::pow(sEnchantConfig.exponentialMultiplier, float(enchantLevel));
        case FormulaMode::Fixed:
            return 1.0f;
    }
    return 1.0f;
}

int32 EnchantStatCalculator::Scale(int32 baseValue, uint8 enchantLevel) const
{
    if (!enchantLevel || !baseValue)
        return baseValue;
    if (sEnchantConfig.formulaMode == FormulaMode::Fixed)
    {
        int32 const amount = int32(std::lround(sEnchantConfig.fixedAmountPerLevel * enchantLevel));
        return baseValue + (baseValue < 0 ? -amount : amount);
    }
    return int32(std::lround(float(baseValue) * GetMultiplier(enchantLevel)));
}

float EnchantStatCalculator::Scale(float baseValue, uint8 enchantLevel) const
{
    if (!enchantLevel || baseValue == 0.0f)
        return baseValue;
    if (sEnchantConfig.formulaMode == FormulaMode::Fixed)
    {
        float const amount = sEnchantConfig.fixedAmountPerLevel * enchantLevel;
        return baseValue + (baseValue < 0.0f ? -amount : amount);
    }
    return baseValue * GetMultiplier(enchantLevel);
}

void EnchantStatCalculator::ApplyStaticBonuses(Player* player, Item const* item, uint8 enchantLevel, bool apply) const
{
    if (!player || !item)
        return;

    uint32 const itemGuid = item->GetGUID().GetCounter();
    StaticBonus desired;
    if (apply && sEnchantConfig.enabled && enchantLevel && !item->IsBroken())
        desired = CalculateStaticBonus(player, item, enchantLevel);

    std::lock_guard lock(_staticBonusMutex);
    auto current = _appliedStaticBonuses.find(itemGuid);
    if (current != _appliedStaticBonuses.end())
    {
        if (apply && current->second == desired)
            return;
        if (current->second.ownerGuid == player->GetGUID().GetCounter())
            ApplyStaticBonusValues(player, current->second, false);
        _appliedStaticBonuses.erase(current);
    }
    if (apply && !desired.Empty())
    {
        ApplyStaticBonusValues(player, desired, true);
        _appliedStaticBonuses[itemGuid] = desired;
    }
}

EnchantStatCalculator::StaticBonus EnchantStatCalculator::CalculateStaticBonus(
    Player const* player, Item const* item, uint8 enchantLevel) const
{
    StaticBonus result;
    if (!player || !item || !item->GetTemplate())
        return result;

    ItemTemplate const* itemTemplate = item->GetTemplate();
    result.ownerGuid = player->GetGUID().GetCounter();

    auto bonus = [this, enchantLevel](int32 value) { return Scale(value, enchantLevel) - value; };
    if (itemTemplate->Armor && IsSyntheticStatAllowed(ENCHANT_STAT_ARMOR))
        result.armor = bonus(itemTemplate->Armor);
    if (itemTemplate->Block && IsSyntheticStatAllowed(ENCHANT_STAT_BLOCK))
        result.block = bonus(itemTemplate->Block);
    if (itemTemplate->HolyRes && IsSyntheticStatAllowed(ENCHANT_STAT_HOLY_RESISTANCE))
        result.holy = bonus(itemTemplate->HolyRes);
    if (itemTemplate->FireRes && IsSyntheticStatAllowed(ENCHANT_STAT_FIRE_RESISTANCE))
        result.fire = bonus(itemTemplate->FireRes);
    if (itemTemplate->NatureRes && IsSyntheticStatAllowed(ENCHANT_STAT_NATURE_RESISTANCE))
        result.nature = bonus(itemTemplate->NatureRes);
    if (itemTemplate->FrostRes && IsSyntheticStatAllowed(ENCHANT_STAT_FROST_RESISTANCE))
        result.frost = bonus(itemTemplate->FrostRes);
    if (itemTemplate->ShadowRes && IsSyntheticStatAllowed(ENCHANT_STAT_SHADOW_RESISTANCE))
        result.shadow = bonus(itemTemplate->ShadowRes);
    if (itemTemplate->ArcaneRes && IsSyntheticStatAllowed(ENCHANT_STAT_ARCANE_RESISTANCE))
        result.arcane = bonus(itemTemplate->ArcaneRes);
    return result;
}

void EnchantStatCalculator::ApplyStaticBonusValues(Player* player, StaticBonus const& bonus, bool apply) const
{
    if (bonus.armor)
        player->HandleStatFlatModifier(UNIT_MOD_ARMOR, BASE_VALUE, float(bonus.armor), apply);
    if (bonus.block)
        player->HandleBaseModFlatValue(SHIELD_BLOCK_VALUE, float(bonus.block), apply);
    if (bonus.holy)
        player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_HOLY, BASE_VALUE, float(bonus.holy), apply);
    if (bonus.fire)
        player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_FIRE, BASE_VALUE, float(bonus.fire), apply);
    if (bonus.nature)
        player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_NATURE, BASE_VALUE, float(bonus.nature), apply);
    if (bonus.frost)
        player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_FROST, BASE_VALUE, float(bonus.frost), apply);
    if (bonus.shadow)
        player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_SHADOW, BASE_VALUE, float(bonus.shadow), apply);
    if (bonus.arcane)
        player->HandleStatFlatModifier(UNIT_MOD_RESISTANCE_ARCANE, BASE_VALUE, float(bonus.arcane), apply);
}

void EnchantStatCalculator::ReconcilePlayer(Player* player) const
{
    if (!player)
        return;

    std::unordered_set<uint32> equippedItems;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            equippedItems.insert(item->GetGUID().GetCounter());
            ApplyStaticBonuses(player, item, sEnchantDatabase.GetLevel(item), true);
        }
    }

    uint32 const ownerGuid = player->GetGUID().GetCounter();
    std::lock_guard lock(_staticBonusMutex);
    for (auto itr = _appliedStaticBonuses.begin(); itr != _appliedStaticBonuses.end();)
    {
        if (itr->second.ownerGuid == ownerGuid && !equippedItems.contains(itr->first))
        {
            ApplyStaticBonusValues(player, itr->second, false);
            itr = _appliedStaticBonuses.erase(itr);
        }
        else
            ++itr;
    }
}

void EnchantStatCalculator::ForgetPlayer(uint32 playerGuid) const
{
    std::lock_guard lock(_staticBonusMutex);
    for (auto itr = _appliedStaticBonuses.begin(); itr != _appliedStaticBonuses.end();)
    {
        if (itr->second.ownerGuid == playerGuid)
            itr = _appliedStaticBonuses.erase(itr);
        else
            ++itr;
    }
}
}
