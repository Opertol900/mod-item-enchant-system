#ifndef MOD_ITEM_ENCHANT_SYSTEM_CONFIG_H
#define MOD_ITEM_ENCHANT_SYSTEM_CONFIG_H

#include "EnchantTypes.h"

#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ItemEnchant
{
struct EnchantExpansionRange
{
    uint32 firstEntry = 0;
    uint32 lastEntry = 0;
    uint8 expansion = 0;
};

class EnchantConfig
{
public:
    static EnchantConfig& Instance();

    void Load();

    bool enabled = true;
    bool debug = false;
    bool logsEnabled = true;
    bool gmCommandsEnabled = true;
    bool disallowInCombat = true;
    bool disallowInBattleground = true;
    bool disallowInArena = true;
    bool bindOnEnchant = true;
    bool soulboundOnly = false;
    bool allowRefundable = false;
    bool allowAccountBound = true;
    bool allowQuestItems = false;
    bool allowHeirloom = false;
    bool allowTransmogItems = true;
    bool allowUnknownExpansion = true;
    bool useDatabaseCache = true;
    bool broadcastHighEnchant = false;
    bool enableAura = false;
    bool scaleWeaponDamage = true;
    bool enableWeaponEnchant = true;
    bool enableArmorEnchant = true;
    bool enableShieldEnchant = true;
    bool enableJewelryEnchant = true;
    bool enableCloakEnchant = true;
    bool enableRelicEnchant = true;
    bool enableOtherEnchant = false;

    uint8 globalMaxEnchant = 25;
    uint8 gmMaxEnchant = 100;
    uint8 safeWeaponLevel = 3;
    uint8 safeArmorLevel = 4;
    uint8 crystalMaxLevel = 6;
    uint8 eventMaxLevel = 25;
    uint8 broadcastLevel = 16;
    uint32 minItemLevel = 1;
    uint32 maxItemLevel = 1000;
    uint32 minRequiredLevel = 0;
    uint32 maxRequiredLevel = 255;
    uint32 attemptCooldownMs = 750;
    uint32 gossipPageSize = 20;
    uint32 gmSecurityLevel = 2;

    float normalRateMultiplier = 1.0f;
    float blessedRateMultiplier = 1.0f;
    float safeRateMultiplier = 1.0f;
    float eventRateMultiplier = 1.0f;

    StatMode statMode = StatMode::MainStatsOnly;
    FormulaMode formulaMode = FormulaMode::LinearPercent;
    FailureMode eventFailureMode = FailureMode::Keep;
    float linearPercentPerLevel = 5.0f;
    float fixedAmountPerLevel = 3.0f;
    float exponentialMultiplier = 1.03f;

    std::array<bool, 8> allowedQualities{};
    std::array<uint8, 8> maxByQuality{};
    std::unordered_map<ItemCategory, uint8> maxByCategory;
    std::unordered_map<uint32, uint8> maxByItem;
    std::unordered_map<uint32, uint32> auraByLevel;
    std::unordered_map<uint32, float> statTablePercent;
    std::unordered_set<uint32> customStats;
    std::unordered_set<uint32> forbiddenStats;
    std::unordered_set<uint32> blacklistQualities;
    std::unordered_set<uint32> blacklistItems;
    std::unordered_set<uint32> whitelistItems;
    std::vector<std::pair<uint32, uint32>> blacklistRanges;
    std::unordered_set<uint32> blacklistDisplays;
    std::unordered_set<uint32> allowedItemClasses;
    std::unordered_set<uint32> allowedSubclasses;
    std::unordered_set<uint32> allowedInventoryTypes;
    std::unordered_set<uint32> allowedBondingTypes;
    std::unordered_set<uint32> allowedExpansions;
    std::unordered_map<uint32, uint8> expansionByItem;
    std::vector<EnchantExpansionRange> expansionRanges;
    bool whitelistMode = false;

    std::unordered_map<ScrollType, uint32> scrollEntries;

private:
    EnchantConfig() = default;
};
}

#define sEnchantConfig ItemEnchant::EnchantConfig::Instance()

#endif
