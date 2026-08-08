#include "EnchantRates.h"

#include "Config.h"
#include "EnchantConfig.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>

namespace ItemEnchant
{
namespace
{
std::unordered_map<uint8, uint32> ParseRates(std::string const& value)
{
    std::unordered_map<uint8, uint32> rates;
    std::stringstream stream(value);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        std::size_t const separator = token.find(':');
        if (separator == std::string::npos)
            continue;
        try
        {
            uint32 const level = std::stoul(token.substr(0, separator));
            float const percent = std::stof(token.substr(separator + 1));
            if (level > 0 && level <= 255)
                rates[uint8(level)] = uint32(std::lround(std::clamp(percent, 0.0f, 100.0f) * 100.0f));
        }
        catch (...)
        {
        }
    }
    return rates;
}

std::optional<uint32> FindRate(std::unordered_map<uint8, uint32> const& rates, uint8 level)
{
    auto const exact = rates.find(level);
    if (exact != rates.end())
        return exact->second;

    uint8 bestLevel = 0;
    uint32 bestRate = 0;
    for (auto const& [configuredLevel, rate] : rates)
    {
        if (configuredLevel <= level && configuredLevel >= bestLevel)
        {
            bestLevel = configuredLevel;
            bestRate = rate;
        }
    }
    if (bestLevel)
        return bestRate;
    return std::nullopt;
}
}

EnchantRates& EnchantRates::Instance()
{
    static EnchantRates instance;
    return instance;
}

void EnchantRates::Load()
{
    std::string const defaults =
        "1:100,2:100,3:100,4:90,5:80,6:70,7:60,8:50,9:40,10:35,11:30,12:25,13:20,14:17,15:14,"
        "16:10,17:8,18:6,19:4,20:3,21:2.5,22:2,23:1.5,24:1.2,25:1";

    _defaultRates = ParseRates(sConfigMgr->GetOption<std::string>("ItemEnchant.Rates.Default", defaults));
    _categoryRates.clear();

    auto loadCategory = [this](ItemCategory category, char const* key)
    {
        std::string const configured = sConfigMgr->GetOption<std::string>(key, "");
        if (!configured.empty())
            _categoryRates[category] = ParseRates(configured);
    };

    loadCategory(ItemCategory::Weapon, "ItemEnchant.Rates.Weapon");
    loadCategory(ItemCategory::Armor, "ItemEnchant.Rates.Armor");
    loadCategory(ItemCategory::Shield, "ItemEnchant.Rates.Shield");
    loadCategory(ItemCategory::Jewelry, "ItemEnchant.Rates.Jewelry");
    loadCategory(ItemCategory::Cloak, "ItemEnchant.Rates.Cloak");
    loadCategory(ItemCategory::Relic, "ItemEnchant.Rates.Relic");
    loadCategory(ItemCategory::Other, "ItemEnchant.Rates.Other");
}

uint32 EnchantRates::GetChanceBasisPoints(uint8 targetLevel, ItemCategory category, ScrollType scrollType) const
{
    if (scrollType == ScrollType::GameMaster)
        return 10000;
    if (scrollType == ScrollType::Crystal && targetLevel <= sEnchantConfig.crystalMaxLevel)
        return 10000;

    std::optional<uint32> chance;
    auto const categoryItr = _categoryRates.find(category);
    if (categoryItr != _categoryRates.end())
        chance = FindRate(categoryItr->second, targetLevel);
    if (!chance.has_value())
        chance = FindRate(_defaultRates, targetLevel);

    uint8 safeLevel = category == ItemCategory::Weapon ? sEnchantConfig.safeWeaponLevel : sEnchantConfig.safeArmorLevel;
    if (targetLevel <= safeLevel)
        return 10000;

    float multiplier = 1.0f;
    switch (scrollType)
    {
        case ScrollType::Normal: multiplier = sEnchantConfig.normalRateMultiplier; break;
        case ScrollType::Blessed: multiplier = sEnchantConfig.blessedRateMultiplier; break;
        case ScrollType::Safe: multiplier = sEnchantConfig.safeRateMultiplier; break;
        case ScrollType::Event: multiplier = sEnchantConfig.eventRateMultiplier; break;
        case ScrollType::Crystal:
        case ScrollType::GameMaster: break;
    }

    return uint32(std::lround(std::clamp(float(chance.value_or(0)) * multiplier, 0.0f, 10000.0f)));
}
}
