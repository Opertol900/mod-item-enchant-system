#ifndef MOD_ITEM_ENCHANT_SYSTEM_ITEM_H
#define MOD_ITEM_ENCHANT_SYSTEM_ITEM_H

#include <string>

class Item;
class Player;

namespace ItemEnchant
{
class EnchantItem
{
public:
    static EnchantItem& Instance();

    std::string GetLocalizedName(Player const* player, Item const* item) const;
    std::string GetDisplayName(Player const* player, Item const* item) const;

private:
    EnchantItem() = default;
};
}

#define sEnchantItem ItemEnchant::EnchantItem::Instance()

#endif
