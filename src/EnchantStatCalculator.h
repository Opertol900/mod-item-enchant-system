#ifndef MOD_ITEM_ENCHANT_SYSTEM_STAT_CALCULATOR_H
#define MOD_ITEM_ENCHANT_SYSTEM_STAT_CALCULATOR_H

#include "Define.h"

#include <mutex>
#include <unordered_map>

class Item;
class Player;

namespace ItemEnchant
{
enum EnchantSyntheticStat : uint32
{
    ENCHANT_STAT_ARMOR = 10000,
    ENCHANT_STAT_BLOCK = 10001,
    ENCHANT_STAT_HOLY_RESISTANCE = 10002,
    ENCHANT_STAT_FIRE_RESISTANCE = 10003,
    ENCHANT_STAT_NATURE_RESISTANCE = 10004,
    ENCHANT_STAT_FROST_RESISTANCE = 10005,
    ENCHANT_STAT_SHADOW_RESISTANCE = 10006,
    ENCHANT_STAT_ARCANE_RESISTANCE = 10007,
    ENCHANT_STAT_WEAPON_DAMAGE = 10010,
};

class EnchantStatCalculator
{
public:
    static EnchantStatCalculator& Instance();

    bool IsStatAllowed(uint32 statType) const;
    bool IsWeaponDamageAllowed() const;
    int32 Scale(int32 baseValue, uint8 enchantLevel) const;
    float Scale(float baseValue, uint8 enchantLevel) const;
    void ApplyStaticBonuses(Player* player, Item const* item, uint8 enchantLevel, bool apply) const;
    void ReconcilePlayer(Player* player) const;
    void ForgetPlayer(uint32 playerGuid) const;

private:
    struct StaticBonus
    {
        uint32 ownerGuid = 0;
        int32 armor = 0;
        int32 block = 0;
        int32 holy = 0;
        int32 fire = 0;
        int32 nature = 0;
        int32 frost = 0;
        int32 shadow = 0;
        int32 arcane = 0;

        bool operator==(StaticBonus const&) const = default;
        bool Empty() const;
    };

    EnchantStatCalculator() = default;
    float GetMultiplier(uint8 enchantLevel) const;
    bool IsSyntheticStatAllowed(uint32 statType) const;
    StaticBonus CalculateStaticBonus(Player const* player, Item const* item, uint8 enchantLevel) const;
    void ApplyStaticBonusValues(Player* player, StaticBonus const& bonus, bool apply) const;

    mutable std::mutex _staticBonusMutex;
    mutable std::unordered_map<uint32, StaticBonus> _appliedStaticBonuses;
};
}

#define sEnchantStatCalculator ItemEnchant::EnchantStatCalculator::Instance()

#endif
