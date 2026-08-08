#ifndef MOD_ITEM_ENCHANT_SYSTEM_TYPES_H
#define MOD_ITEM_ENCHANT_SYSTEM_TYPES_H

#include "Define.h"

#include <string>

namespace ItemEnchant
{
enum class ScrollType : uint8
{
    Normal = 1,
    Blessed = 2,
    Safe = 3,
    Crystal = 4,
    Event = 5,
    GameMaster = 6,
};

enum class ItemCategory : uint8
{
    Other = 0,
    Weapon = 1,
    Armor = 2,
    Shield = 3,
    Jewelry = 4,
    Cloak = 5,
    Relic = 6,
};

enum class StatMode : uint8
{
    MainStatsOnly = 1,
    AllStats = 2,
    Custom = 3,
};

enum class FormulaMode : uint8
{
    LinearPercent = 1,
    Fixed = 2,
    TablePercent = 3,
    Exponential = 4,
};

enum class FailureMode : uint8
{
    Destroy = 1,
    Reset = 2,
    Keep = 3,
};

enum class AttemptResult : uint8
{
    Success = 1,
    FailedDestroyed = 2,
    FailedReset = 3,
    FailedKept = 4,
    Recovered = 5,
    Aborted = 6,
};

struct ValidationResult
{
    bool allowed = false;
    std::string reason;
};

struct PendingOperation
{
    uint64 operationId = 0;
    uint32 ownerGuid = 0;
    uint32 itemGuid = 0;
    uint32 itemEntry = 0;
    uint32 scrollGuid = 0;
    uint32 scrollEntry = 0;
    ScrollType scrollType = ScrollType::Normal;
    uint8 oldLevel = 0;
    uint8 newLevel = 0;
    bool destroyItem = false;
    bool success = false;
    uint32 chanceBasisPoints = 0;
    uint32 roll = 0;
};

char const* ToString(ScrollType type);
char const* ToString(ItemCategory category);
char const* ToString(AttemptResult result);
}

#endif

