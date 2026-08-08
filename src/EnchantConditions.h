#ifndef MOD_ITEM_ENCHANT_SYSTEM_CONDITIONS_H
#define MOD_ITEM_ENCHANT_SYSTEM_CONDITIONS_H

#include "EnchantTypes.h"

#include <functional>
#include <shared_mutex>

class Item;
class ItemTemplate;
class Player;

namespace ItemEnchant
{
class EnchantConditions
{
public:
    using TransmogDetector = std::function<bool(Item const*)>;

    static EnchantConditions& Instance();

    ValidationResult Validate(Player const* player, Item const* item, ScrollType scrollType) const;
    ItemCategory GetCategory(ItemTemplate const* itemTemplate) const;
    uint8 GetMaxEnchant(ItemTemplate const* itemTemplate, ScrollType scrollType) const;
    bool IsScrollEntry(uint32 entry, ScrollType* type = nullptr) const;
    void SetTransmogDetector(TransmogDetector detector);

private:
    EnchantConditions() = default;
    bool IsTransmogrified(Item const* item) const;

    mutable std::shared_mutex _transmogDetectorMutex;
    TransmogDetector _transmogDetector;
};
}

#define sEnchantConditions ItemEnchant::EnchantConditions::Instance()

#endif
