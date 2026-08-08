#include "EnchantManager.h"

#include "Item.h"
#include "ItemScript.h"
#include "Player.h"

class item_mod_enchant_scroll : public ItemScript
{
public:
    item_mod_enchant_scroll() : ItemScript("item_mod_enchant_scroll") { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        sEnchantManager.ShowItemSelection(player, item);
        return true;
    }

    void OnGossipSelect(Player* player, Item* item, uint32 sender, uint32 action) override
    {
        sEnchantManager.HandleGossipSelection(player, item, sender, action);
    }
};

void AddSC_item_mod_enchant_scroll()
{
    new item_mod_enchant_scroll();
}
