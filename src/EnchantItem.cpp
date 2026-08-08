#include "EnchantItem.h"

#include "EnchantDatabase.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldSession.h"

namespace ItemEnchant
{
EnchantItem& EnchantItem::Instance()
{
    static EnchantItem instance;
    return instance;
}

std::string EnchantItem::GetLocalizedName(Player const* player, Item const* item) const
{
    if (!item || !item->GetTemplate())
    {
        bool const russian = player && player->GetSession() &&
            player->GetSession()->GetSessionDbLocaleIndex() == LOCALE_ruRU;
        return russian ? "предмет" : "item";
    }

    ItemTemplate const* itemTemplate = item->GetTemplate();
    std::string name = itemTemplate->Name1;
    if (player && player->GetSession())
    {
        LocaleConstant const locale = player->GetSession()->GetSessionDbLocaleIndex();
        if (locale != LOCALE_enUS)
            if (ItemLocale const* itemLocale = sObjectMgr->GetItemLocale(itemTemplate->ItemId))
                ObjectMgr::GetLocaleString(itemLocale->Name, locale, name);
    }
    return name;
}

std::string EnchantItem::GetDisplayName(Player const* player, Item const* item) const
{
    std::string const name = GetLocalizedName(player, item);
    uint8 const level = sEnchantDatabase.GetLevel(item);
    return level ? "+" + std::to_string(level) + " " + name : name;
}
}
