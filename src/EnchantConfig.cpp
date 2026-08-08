#include "EnchantConfig.h"

#include "Config.h"
#include "Log.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <type_traits>

namespace ItemEnchant
{
namespace
{
std::string Trim(std::string value)
{
    auto const notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::vector<std::string> Split(std::string const& value, char delimiter = ',')
{
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string token;
    while (std::getline(stream, token, delimiter))
    {
        token = Trim(token);
        if (!token.empty())
            result.push_back(token);
    }
    return result;
}

uint32 ToUInt(std::string const& value, uint32 fallback = 0)
{
    try
    {
        std::size_t consumed = 0;
        unsigned long parsed = std::stoul(value, &consumed);
        return consumed == value.size() ? uint32(parsed) : fallback;
    }
    catch (...)
    {
        return fallback;
    }
}

uint8 ToByte(uint32 value)
{
    return uint8(std::min(value, 255u));
}

float ToFloat(std::string const& value, float fallback = 0.0f)
{
    try
    {
        std::size_t consumed = 0;
        float parsed = std::stof(value, &consumed);
        return consumed == value.size() ? parsed : fallback;
    }
    catch (...)
    {
        return fallback;
    }
}

std::unordered_set<uint32> ParseSet(std::string const& value)
{
    std::unordered_set<uint32> result;
    for (std::string const& token : Split(value))
        if (uint32 parsed = ToUInt(token); parsed || token == "0")
            result.insert(parsed);
    return result;
}

template <typename T>
std::unordered_map<uint32, T> ParseMap(std::string const& value, T fallback)
{
    std::unordered_map<uint32, T> result;
    for (std::string const& token : Split(value))
    {
        std::size_t const separator = token.find(':');
        if (separator == std::string::npos)
            continue;
        uint32 const key = ToUInt(Trim(token.substr(0, separator)));
        std::string const rawValue = Trim(token.substr(separator + 1));
        if (!key)
            continue;
        if constexpr (std::is_same_v<T, float>)
            result[key] = ToFloat(rawValue, fallback);
        else if constexpr (std::is_same_v<T, uint8>)
            result[key] = ToByte(ToUInt(rawValue, uint32(fallback)));
        else
            result[key] = T(ToUInt(rawValue, uint32(fallback)));
    }
    return result;
}

FailureMode ParseFailureMode(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    if (value == "destroy")
        return FailureMode::Destroy;
    if (value == "reset")
        return FailureMode::Reset;
    return FailureMode::Keep;
}
}

EnchantConfig& EnchantConfig::Instance()
{
    static EnchantConfig instance;
    return instance;
}

void EnchantConfig::Load()
{
    enabled = sConfigMgr->GetOption<bool>("ItemEnchant.Enable", true);
    debug = sConfigMgr->GetOption<bool>("ItemEnchant.Debug", false);
    logsEnabled = sConfigMgr->GetOption<bool>("ItemEnchant.EnableLogs", true);
    gmCommandsEnabled = sConfigMgr->GetOption<bool>("ItemEnchant.EnableGMCommands", true);
    disallowInCombat = sConfigMgr->GetOption<bool>("ItemEnchant.DisallowInCombat", true);
    disallowInBattleground = sConfigMgr->GetOption<bool>("ItemEnchant.DisallowInBattleground", true);
    disallowInArena = sConfigMgr->GetOption<bool>("ItemEnchant.DisallowInArena", true);
    bindOnEnchant = sConfigMgr->GetOption<bool>("ItemEnchant.BindOnEnchant", true);
    soulboundOnly = sConfigMgr->GetOption<bool>("ItemEnchant.AllowSoulboundOnly", false);
    allowRefundable = sConfigMgr->GetOption<bool>("ItemEnchant.AllowRefundable", false);
    allowAccountBound = sConfigMgr->GetOption<bool>("ItemEnchant.AllowAccountBound", true);
    allowQuestItems = sConfigMgr->GetOption<bool>("ItemEnchant.AllowQuestItems", false);
    allowHeirloom = sConfigMgr->GetOption<bool>("ItemEnchant.AllowHeirloom", false);
    allowTransmogItems = sConfigMgr->GetOption<bool>("ItemEnchant.AllowTransmogItems", true);
    allowUnknownExpansion = sConfigMgr->GetOption<bool>("ItemEnchant.AllowUnknownExpansion", true);
    useDatabaseCache = sConfigMgr->GetOption<bool>("ItemEnchant.EnableDatabaseCache", true);
    broadcastHighEnchant = sConfigMgr->GetOption<bool>("ItemEnchant.BroadcastHighEnchant", false);
    enableAura = sConfigMgr->GetOption<bool>("ItemEnchant.EnableAura", false);
    scaleWeaponDamage = sConfigMgr->GetOption<bool>("ItemEnchant.ScaleWeaponDamage", true);
    enableWeaponEnchant = sConfigMgr->GetOption<bool>("ItemEnchant.EnableWeaponEnchant", true);
    enableArmorEnchant = sConfigMgr->GetOption<bool>("ItemEnchant.EnableArmorEnchant", true);
    enableShieldEnchant = sConfigMgr->GetOption<bool>("ItemEnchant.EnableShieldEnchant", true);
    enableJewelryEnchant = sConfigMgr->GetOption<bool>("ItemEnchant.EnableJewelryEnchant", true);
    enableCloakEnchant = sConfigMgr->GetOption<bool>("ItemEnchant.EnableCloakEnchant", true);
    enableRelicEnchant = sConfigMgr->GetOption<bool>("ItemEnchant.EnableRelicEnchant", true);
    enableOtherEnchant = sConfigMgr->GetOption<bool>("ItemEnchant.EnableOtherEnchant", false);

    globalMaxEnchant = uint8(std::clamp(sConfigMgr->GetOption<uint32>("ItemEnchant.GlobalMaxEnchant", 25), 1u, 255u));
    gmMaxEnchant = uint8(std::clamp(sConfigMgr->GetOption<uint32>("ItemEnchant.GMMaxEnchant", 100), 1u, 255u));
    safeWeaponLevel = uint8(std::min(sConfigMgr->GetOption<uint32>("ItemEnchant.SafeWeaponLevel", 3), 255u));
    safeArmorLevel = uint8(std::min(sConfigMgr->GetOption<uint32>("ItemEnchant.SafeArmorLevel", 4), 255u));
    crystalMaxLevel = uint8(std::min(sConfigMgr->GetOption<uint32>("ItemEnchant.CrystalMaxLevel", 6), 255u));
    eventMaxLevel = uint8(std::min(sConfigMgr->GetOption<uint32>("ItemEnchant.EventMaxLevel", 25), 255u));
    broadcastLevel = uint8(std::min(sConfigMgr->GetOption<uint32>("ItemEnchant.BroadcastLevel", 16), 255u));
    minItemLevel = sConfigMgr->GetOption<uint32>("ItemEnchant.MinItemLevel", 1);
    maxItemLevel = sConfigMgr->GetOption<uint32>("ItemEnchant.MaxItemLevel", 1000);
    minRequiredLevel = sConfigMgr->GetOption<uint32>("ItemEnchant.MinRequiredLevel", 0);
    maxRequiredLevel = sConfigMgr->GetOption<uint32>("ItemEnchant.MaxRequiredLevel", 255);
    attemptCooldownMs = std::max(sConfigMgr->GetOption<uint32>("ItemEnchant.AttemptCooldownMs", 750), 250u);
    gossipPageSize = std::clamp(sConfigMgr->GetOption<uint32>("ItemEnchant.GossipPageSize", 20), 5u, 28u);
    gmSecurityLevel = sConfigMgr->GetOption<uint32>("ItemEnchant.GMSecurityLevel", 2);

    normalRateMultiplier = std::max(0.0f, sConfigMgr->GetOption<float>("ItemEnchant.ScrollRate.Normal", 1.0f));
    blessedRateMultiplier = std::max(0.0f, sConfigMgr->GetOption<float>("ItemEnchant.ScrollRate.Blessed", 1.0f));
    safeRateMultiplier = std::max(0.0f, sConfigMgr->GetOption<float>("ItemEnchant.ScrollRate.Safe", 1.0f));
    eventRateMultiplier = std::max(0.0f, sConfigMgr->GetOption<float>("ItemEnchant.ScrollRate.Event", 1.0f));

    statMode = StatMode(std::clamp(sConfigMgr->GetOption<uint32>("ItemEnchant.StatMode", 1), 1u, 3u));
    formulaMode = FormulaMode(std::clamp(sConfigMgr->GetOption<uint32>("ItemEnchant.FormulaMode", 1), 1u, 4u));
    eventFailureMode = ParseFailureMode(sConfigMgr->GetOption<std::string>("ItemEnchant.EventFailureMode", "keep"));
    linearPercentPerLevel = std::max(0.0f, sConfigMgr->GetOption<float>("ItemEnchant.LinearPercentPerLevel", 5.0f));
    fixedAmountPerLevel = std::max(0.0f, sConfigMgr->GetOption<float>("ItemEnchant.FixedAmountPerLevel", 3.0f));
    exponentialMultiplier = std::max(1.0f, sConfigMgr->GetOption<float>("ItemEnchant.ExponentialMultiplier", 1.03f));

    allowedQualities.fill(false);
    allowedQualities[0] = sConfigMgr->GetOption<bool>("ItemEnchant.AllowPoor", false);
    allowedQualities[1] = sConfigMgr->GetOption<bool>("ItemEnchant.AllowCommon", false);
    allowedQualities[2] = sConfigMgr->GetOption<bool>("ItemEnchant.AllowUncommon", true);
    allowedQualities[3] = sConfigMgr->GetOption<bool>("ItemEnchant.AllowRare", true);
    allowedQualities[4] = sConfigMgr->GetOption<bool>("ItemEnchant.AllowEpic", true);
    allowedQualities[5] = sConfigMgr->GetOption<bool>("ItemEnchant.AllowLegendary", false);
    allowedQualities[6] = sConfigMgr->GetOption<bool>("ItemEnchant.AllowArtifact", false);
    allowedQualities[7] = allowHeirloom;

    maxByQuality.fill(0);
    maxByQuality[0] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Poor", 0));
    maxByQuality[1] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Common", 0));
    maxByQuality[2] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Uncommon", 0));
    maxByQuality[3] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Rare", 0));
    maxByQuality[4] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Epic", 0));
    maxByQuality[5] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Legendary", 0));
    maxByQuality[6] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Artifact", 0));
    maxByQuality[7] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Heirloom", 0));

    maxByCategory.clear();
    maxByCategory[ItemCategory::Weapon] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Weapon", 20));
    maxByCategory[ItemCategory::Armor] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Armor", 16));
    maxByCategory[ItemCategory::Shield] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Shield", 16));
    maxByCategory[ItemCategory::Jewelry] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Jewelry", 16));
    maxByCategory[ItemCategory::Cloak] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Cloak", 16));
    maxByCategory[ItemCategory::Relic] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Relic", 16));
    maxByCategory[ItemCategory::Other] = ToByte(sConfigMgr->GetOption<uint32>("ItemEnchant.Max.Other", 0));

    maxByItem = ParseMap<uint8>(sConfigMgr->GetOption<std::string>("ItemEnchant.Max.ByItem", ""), 0);
    auraByLevel = ParseMap<uint32>(sConfigMgr->GetOption<std::string>("ItemEnchant.AuraByLevel", ""), 0);
    statTablePercent = ParseMap<float>(sConfigMgr->GetOption<std::string>("ItemEnchant.StatTablePercent", ""), 0.0f);
    for (auto& [level, percent] : statTablePercent)
        percent = std::max(0.0f, percent);
    customStats = ParseSet(sConfigMgr->GetOption<std::string>("ItemEnchant.CustomStats", "3,4,5,6,7"));
    forbiddenStats = ParseSet(sConfigMgr->GetOption<std::string>("ItemEnchant.ForbiddenStats", ""));
    blacklistQualities = ParseSet(sConfigMgr->GetOption<std::string>("ItemEnchant.BlacklistQualities", ""));
    blacklistItems = ParseSet(sConfigMgr->GetOption<std::string>("ItemEnchant.BlacklistItems", ""));
    whitelistItems = ParseSet(sConfigMgr->GetOption<std::string>("ItemEnchant.WhitelistItems", ""));
    blacklistDisplays = ParseSet(sConfigMgr->GetOption<std::string>("ItemEnchant.BlacklistDisplays", ""));
    allowedItemClasses = ParseSet(sConfigMgr->GetOption<std::string>("ItemEnchant.AllowedClasses", "2,4"));
    allowedSubclasses = ParseSet(sConfigMgr->GetOption<std::string>("ItemEnchant.AllowedSubclasses", ""));
    allowedInventoryTypes = ParseSet(sConfigMgr->GetOption<std::string>("ItemEnchant.AllowedInventoryTypes", ""));
    allowedBondingTypes = ParseSet(sConfigMgr->GetOption<std::string>("ItemEnchant.AllowedBondingTypes", ""));
    allowedExpansions = ParseSet(sConfigMgr->GetOption<std::string>("ItemEnchant.AllowedExpansions", ""));
    expansionByItem = ParseMap<uint8>(sConfigMgr->GetOption<std::string>("ItemEnchant.Expansion.ByItem", ""), 255);
    whitelistMode = sConfigMgr->GetOption<bool>("ItemEnchant.WhitelistMode", false);

    blacklistRanges.clear();
    for (std::string const& token : Split(sConfigMgr->GetOption<std::string>("ItemEnchant.BlacklistRanges", "")))
    {
        std::size_t const separator = token.find('-');
        if (separator == std::string::npos)
            continue;
        uint32 first = ToUInt(Trim(token.substr(0, separator)));
        uint32 second = ToUInt(Trim(token.substr(separator + 1)));
        if (first && second)
            blacklistRanges.emplace_back(std::min(first, second), std::max(first, second));
    }

    expansionRanges.clear();
    for (std::string const& token : Split(sConfigMgr->GetOption<std::string>("ItemEnchant.Expansion.Ranges", "")))
    {
        std::size_t const separator = token.find(':');
        std::size_t const rangeSeparator = token.find('-');
        if (separator == std::string::npos || rangeSeparator == std::string::npos || rangeSeparator > separator)
            continue;
        uint32 first = ToUInt(Trim(token.substr(0, rangeSeparator)));
        uint32 second = ToUInt(Trim(token.substr(rangeSeparator + 1, separator - rangeSeparator - 1)));
        uint32 expansion = ToUInt(Trim(token.substr(separator + 1)), 255);
        if (first && second && expansion <= 2)
            expansionRanges.push_back({ std::min(first, second), std::max(first, second), uint8(expansion) });
    }

    scrollEntries.clear();
    scrollEntries[ScrollType::Normal] = sConfigMgr->GetOption<uint32>("ItemEnchant.Scroll.Normal", 900100);
    scrollEntries[ScrollType::Blessed] = sConfigMgr->GetOption<uint32>("ItemEnchant.Scroll.Blessed", 900101);
    scrollEntries[ScrollType::Safe] = sConfigMgr->GetOption<uint32>("ItemEnchant.Scroll.Safe", 900102);
    scrollEntries[ScrollType::Crystal] = sConfigMgr->GetOption<uint32>("ItemEnchant.Scroll.Crystal", 900103);
    scrollEntries[ScrollType::Event] = sConfigMgr->GetOption<uint32>("ItemEnchant.Scroll.Event", 900104);
    scrollEntries[ScrollType::GameMaster] = sConfigMgr->GetOption<uint32>("ItemEnchant.Scroll.GM", 900105);

    if (minItemLevel > maxItemLevel)
    {
        LOG_WARN("module.itemenchant", "MinItemLevel exceeds MaxItemLevel; the values were swapped.");
        std::swap(minItemLevel, maxItemLevel);
    }
    if (minRequiredLevel > maxRequiredLevel)
    {
        LOG_WARN("module.itemenchant", "MinRequiredLevel exceeds MaxRequiredLevel; the values were swapped.");
        std::swap(minRequiredLevel, maxRequiredLevel);
    }

    std::unordered_set<uint32> uniqueScrollEntries;
    for (auto const& [type, entry] : scrollEntries)
    {
        if (!entry || !uniqueScrollEntries.insert(entry).second)
        {
            LOG_ERROR("module.itemenchant", "Scroll entries must be non-zero and unique. Invalid entry {} for type {}.",
                entry, ToString(type));
            enabled = false;
        }
    }

    LOG_INFO("module.itemenchant", "Item enchant configuration loaded: enabled={}, max={}, statMode={}, formula={}",
        enabled, uint32(globalMaxEnchant), uint32(statMode), uint32(formulaMode));
}
}
