#include "EnchantConditions.h"

#include "EnchantConfig.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "SharedDefines.h"
#include "WorldSession.h"

#include <algorithm>
#include <mutex>
#include <utility>

namespace ItemEnchant
{
EnchantConditions& EnchantConditions::Instance()
{
    static EnchantConditions instance;
    return instance;
}

void EnchantConditions::SetTransmogDetector(TransmogDetector detector)
{
    std::unique_lock lock(_transmogDetectorMutex);
    _transmogDetector = std::move(detector);
}

bool EnchantConditions::IsTransmogrified(Item const* item) const
{
    TransmogDetector detector;
    {
        std::shared_lock lock(_transmogDetectorMutex);
        detector = _transmogDetector;
    }
    return detector && detector(item);
}

ItemCategory EnchantConditions::GetCategory(ItemTemplate const* itemTemplate) const
{
    if (!itemTemplate)
        return ItemCategory::Other;
    if (itemTemplate->Class == ITEM_CLASS_WEAPON)
        return ItemCategory::Weapon;
    if (itemTemplate->Class != ITEM_CLASS_ARMOR)
        return ItemCategory::Other;

    if (itemTemplate->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
        return ItemCategory::Shield;
    if (itemTemplate->InventoryType == INVTYPE_NECK || itemTemplate->InventoryType == INVTYPE_FINGER ||
        itemTemplate->InventoryType == INVTYPE_TRINKET)
        return ItemCategory::Jewelry;
    if (itemTemplate->InventoryType == INVTYPE_CLOAK)
        return ItemCategory::Cloak;
    if (itemTemplate->InventoryType == INVTYPE_RELIC)
        return ItemCategory::Relic;
    return ItemCategory::Armor;
}

bool EnchantConditions::IsScrollEntry(uint32 entry, ScrollType* type) const
{
    for (auto const& [scrollType, scrollEntry] : sEnchantConfig.scrollEntries)
    {
        if (scrollEntry == entry)
        {
            if (type)
                *type = scrollType;
            return true;
        }
    }
    return false;
}

uint8 EnchantConditions::GetMaxEnchant(ItemTemplate const* itemTemplate, ScrollType scrollType) const
{
    if (!itemTemplate)
        return 0;
    if (scrollType == ScrollType::GameMaster)
        return sEnchantConfig.gmMaxEnchant;

    uint8 maximum = 0;
    if (auto const itemItr = sEnchantConfig.maxByItem.find(itemTemplate->ItemId);
        itemItr != sEnchantConfig.maxByItem.end() && itemItr->second)
        maximum = itemItr->second;
    else if (itemTemplate->Quality < sEnchantConfig.maxByQuality.size() &&
        sEnchantConfig.maxByQuality[itemTemplate->Quality])
        maximum = sEnchantConfig.maxByQuality[itemTemplate->Quality];
    else
    {
        ItemCategory const category = GetCategory(itemTemplate);
        auto const categoryItr = sEnchantConfig.maxByCategory.find(category);
        maximum = categoryItr != sEnchantConfig.maxByCategory.end() && categoryItr->second
            ? categoryItr->second : sEnchantConfig.globalMaxEnchant;
    }

    if (scrollType == ScrollType::Event)
        maximum = std::min(maximum, sEnchantConfig.eventMaxLevel);
    else if (scrollType == ScrollType::Crystal)
        maximum = std::min(maximum, sEnchantConfig.crystalMaxLevel);
    return maximum;
}

ValidationResult EnchantConditions::Validate(Player const* player, Item const* item, ScrollType scrollType) const
{
    if (!sEnchantConfig.enabled)
        return { false, "system_disabled" };
    if (!player || !item || item->GetOwnerGUID() != player->GetGUID())
        return { false, "not_owner" };
    if (item->IsInTrade() || item->IsWrapped())
        return { false, "item_busy" };
    if (scrollType == ScrollType::GameMaster && player->GetSession()->GetSecurity() < sEnchantConfig.gmSecurityLevel)
        return { false, "gm_only" };
    if (scrollType != ScrollType::GameMaster)
    {
        if (sEnchantConfig.disallowInCombat && player->IsInCombat())
            return { false, "in_combat" };
        if (sEnchantConfig.disallowInBattleground && player->InBattleground())
            return { false, "in_battleground" };
        if (sEnchantConfig.disallowInArena && player->InArena())
            return { false, "in_arena" };
    }

    ItemTemplate const* itemTemplate = item->GetTemplate();
    if (!itemTemplate || itemTemplate->InventoryType == INVTYPE_NON_EQUIP || IsScrollEntry(itemTemplate->ItemId))
        return { false, "not_equipment" };
    if (scrollType == ScrollType::GameMaster)
        return { true, {} };
    if (!sEnchantConfig.allowRefundable && (item->IsRefundable() || item->IsBOPTradable()))
        return { false, "refundable" };
    if (!sEnchantConfig.allowTransmogItems && IsTransmogrified(item))
        return { false, "transmog_forbidden" };

    ItemCategory const category = GetCategory(itemTemplate);
    if ((category == ItemCategory::Weapon && !sEnchantConfig.enableWeaponEnchant) ||
        (category == ItemCategory::Armor && !sEnchantConfig.enableArmorEnchant) ||
        (category == ItemCategory::Shield && !sEnchantConfig.enableShieldEnchant) ||
        (category == ItemCategory::Jewelry && !sEnchantConfig.enableJewelryEnchant) ||
        (category == ItemCategory::Cloak && !sEnchantConfig.enableCloakEnchant) ||
        (category == ItemCategory::Relic && !sEnchantConfig.enableRelicEnchant) ||
        (category == ItemCategory::Other && !sEnchantConfig.enableOtherEnchant))
        return { false, "category_disabled" };

    if (sEnchantConfig.whitelistMode && !sEnchantConfig.whitelistItems.contains(itemTemplate->ItemId))
        return { false, "not_whitelisted" };
    if (sEnchantConfig.blacklistItems.contains(itemTemplate->ItemId) ||
        sEnchantConfig.blacklistDisplays.contains(itemTemplate->DisplayInfoID))
        return { false, "blacklisted" };
    for (auto const& [first, last] : sEnchantConfig.blacklistRanges)
        if (itemTemplate->ItemId >= first && itemTemplate->ItemId <= last)
            return { false, "blacklisted" };

    if (!sEnchantConfig.allowedItemClasses.empty() && !sEnchantConfig.allowedItemClasses.contains(itemTemplate->Class))
        return { false, "class_forbidden" };
    if (!sEnchantConfig.allowedSubclasses.empty() && !sEnchantConfig.allowedSubclasses.contains(itemTemplate->SubClass))
        return { false, "subclass_forbidden" };
    if (!sEnchantConfig.allowedInventoryTypes.empty() &&
        !sEnchantConfig.allowedInventoryTypes.contains(itemTemplate->InventoryType))
        return { false, "inventory_type_forbidden" };
    if (!sEnchantConfig.allowedBondingTypes.empty() &&
        !sEnchantConfig.allowedBondingTypes.contains(itemTemplate->Bonding))
        return { false, "bonding_forbidden" };
    if (!sEnchantConfig.allowedExpansions.empty())
    {
        uint32 expansion = 255;
        if (auto const itr = sEnchantConfig.expansionByItem.find(itemTemplate->ItemId);
            itr != sEnchantConfig.expansionByItem.end())
            expansion = itr->second;
        else
            for (EnchantExpansionRange const& range : sEnchantConfig.expansionRanges)
                if (itemTemplate->ItemId >= range.firstEntry && itemTemplate->ItemId <= range.lastEntry)
                {
                    expansion = range.expansion;
                    break;
                }

        if (expansion == 255)
        {
            if (!sEnchantConfig.allowUnknownExpansion)
                return { false, "unknown_expansion" };
        }
        else if (!sEnchantConfig.allowedExpansions.contains(expansion))
            return { false, "expansion_forbidden" };
    }
    if (sEnchantConfig.blacklistQualities.contains(itemTemplate->Quality) ||
        itemTemplate->Quality >= sEnchantConfig.allowedQualities.size() ||
        !sEnchantConfig.allowedQualities[itemTemplate->Quality])
        return { false, "quality_forbidden" };
    if (!sEnchantConfig.allowHeirloom && itemTemplate->Quality == ITEM_QUALITY_HEIRLOOM)
        return { false, "heirloom_forbidden" };
    if (itemTemplate->ItemLevel < sEnchantConfig.minItemLevel || itemTemplate->ItemLevel > sEnchantConfig.maxItemLevel)
        return { false, "item_level_forbidden" };
    if (itemTemplate->RequiredLevel < sEnchantConfig.minRequiredLevel ||
        itemTemplate->RequiredLevel > sEnchantConfig.maxRequiredLevel)
        return { false, "required_level_forbidden" };
    if (!sEnchantConfig.allowQuestItems &&
        (itemTemplate->Class == ITEM_CLASS_QUEST || itemTemplate->Bonding == BIND_QUEST_ITEM))
        return { false, "quest_item" };
    if (sEnchantConfig.soulboundOnly && !item->IsSoulBound())
        return { false, "soulbound_only" };
    if (!sEnchantConfig.allowAccountBound && item->IsBoundAccountWide())
        return { false, "account_bound" };

    return { true, {} };
}
}
