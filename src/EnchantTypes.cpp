#include "EnchantTypes.h"

namespace ItemEnchant
{
char const* ToString(ScrollType type)
{
    switch (type)
    {
        case ScrollType::Normal: return "normal";
        case ScrollType::Blessed: return "blessed";
        case ScrollType::Safe: return "safe";
        case ScrollType::Crystal: return "crystal";
        case ScrollType::Event: return "event";
        case ScrollType::GameMaster: return "gm";
    }
    return "unknown";
}

char const* ToString(ItemCategory category)
{
    switch (category)
    {
        case ItemCategory::Weapon: return "weapon";
        case ItemCategory::Armor: return "armor";
        case ItemCategory::Shield: return "shield";
        case ItemCategory::Jewelry: return "jewelry";
        case ItemCategory::Cloak: return "cloak";
        case ItemCategory::Relic: return "relic";
        case ItemCategory::Other: return "other";
    }
    return "other";
}

char const* ToString(AttemptResult result)
{
    switch (result)
    {
        case AttemptResult::Success: return "success";
        case AttemptResult::FailedDestroyed: return "failed_destroyed";
        case AttemptResult::FailedReset: return "failed_reset";
        case AttemptResult::FailedKept: return "failed_kept";
        case AttemptResult::Recovered: return "recovered";
        case AttemptResult::Aborted: return "aborted";
    }
    return "unknown";
}
}

