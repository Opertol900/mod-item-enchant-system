#ifndef MOD_ITEM_ENCHANT_SYSTEM_RATES_H
#define MOD_ITEM_ENCHANT_SYSTEM_RATES_H

#include "EnchantTypes.h"

#include <unordered_map>

namespace ItemEnchant
{
class EnchantRates
{
public:
    static EnchantRates& Instance();

    void Load();
    uint32 GetChanceBasisPoints(uint8 targetLevel, ItemCategory category, ScrollType scrollType) const;

private:
    EnchantRates() = default;

    std::unordered_map<uint8, uint32> _defaultRates;
    std::unordered_map<ItemCategory, std::unordered_map<uint8, uint32>> _categoryRates;
};
}

#define sEnchantRates ItemEnchant::EnchantRates::Instance()

#endif

