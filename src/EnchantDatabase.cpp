#include "EnchantDatabase.h"

#include "DatabaseEnv.h"
#include "EnchantConfig.h"
#include "Item.h"
#include "Log.h"
#include "Player.h"

#include <mutex>

namespace ItemEnchant
{
EnchantDatabase& EnchantDatabase::Instance()
{
    static EnchantDatabase instance;
    return instance;
}

void EnchantDatabase::LoadForPlayer(Player const* player)
{
    if (!player)
        return;

    uint32 const ownerGuid = player->GetGUID().GetCounter();
    if (!sEnchantConfig.useDatabaseCache)
    {
        std::unique_lock lock(_mutex);
        auto const ownerItr = _ownerItems.find(ownerGuid);
        if (ownerItr != _ownerItems.end())
        {
            for (uint32 itemGuid : ownerItr->second)
                _levels.erase(itemGuid);
            _ownerItems.erase(ownerItr);
        }
        return;
    }

    QueryResult result = CharacterDatabase.Query(
        "SELECT e.item_guid, e.enchant_level FROM mod_item_enchant e "
        "INNER JOIN item_instance i ON i.guid = e.item_guid WHERE i.owner_guid = {}", ownerGuid);

    std::unique_lock lock(_mutex);
    std::unordered_set<uint32>& ownedItems = _ownerItems[ownerGuid];
    for (uint32 itemGuid : ownedItems)
        _levels.erase(itemGuid);
    ownedItems.clear();
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        uint32 const itemGuid = fields[0].Get<uint32>();
        _levels[itemGuid] = fields[1].Get<uint8>();
        ownedItems.insert(itemGuid);
    } while (result->NextRow());
}

void EnchantDatabase::UnloadPlayer(uint32 ownerGuid)
{
    std::unique_lock lock(_mutex);
    auto const ownerItr = _ownerItems.find(ownerGuid);
    if (ownerItr == _ownerItems.end())
        return;
    for (uint32 itemGuid : ownerItr->second)
        _levels.erase(itemGuid);
    _ownerItems.erase(ownerItr);
}

uint8 EnchantDatabase::GetLevel(Item const* item) const
{
    if (!item)
        return 0;

    uint32 const itemGuid = item->GetGUID().GetCounter();
    uint8 const level = GetLevel(itemGuid);
    uint32 const ownerGuid = item->GetOwnerGUID().GetCounter();
    std::unique_lock lock(_mutex);
    _ownerItems[ownerGuid].insert(itemGuid);
    return level;
}

uint8 EnchantDatabase::GetLevel(uint32 itemGuid) const
{
    if (!itemGuid)
        return 0;
    {
        std::shared_lock lock(_mutex);
        auto const itr = _levels.find(itemGuid);
        if (itr != _levels.end())
            return itr->second;
    }

    QueryResult result = CharacterDatabase.Query(
        "SELECT enchant_level FROM mod_item_enchant WHERE item_guid = {}", itemGuid);
    if (!result)
    {
        std::unique_lock lock(_mutex);
        _levels[itemGuid] = 0;
        return 0;
    }

    uint8 const level = result->Fetch()[0].Get<uint8>();
    std::unique_lock lock(_mutex);
    _levels[itemGuid] = level;
    return level;
}

void EnchantDatabase::SetCachedLevel(uint32 itemGuid, uint32 ownerGuid, uint8 level)
{
    std::unique_lock lock(_mutex);
    _levels[itemGuid] = level;
    _ownerItems[ownerGuid].insert(itemGuid);
}

void EnchantDatabase::EraseCachedLevel(uint32 itemGuid)
{
    std::unique_lock lock(_mutex);
    _levels.erase(itemGuid);
}

bool EnchantDatabase::SetLevel(uint32 itemGuid, uint32 ownerGuid, uint32 itemEntry, uint8 level)
{
    if (!itemGuid)
        return false;
    if (!level)
        return RemoveLevel(itemGuid);

    CharacterDatabase.DirectExecute(
        "INSERT INTO mod_item_enchant (item_guid, owner_guid, item_entry, enchant_level) "
        "VALUES ({}, {}, {}, {}) ON DUPLICATE KEY UPDATE owner_guid=VALUES(owner_guid), "
        "item_entry=VALUES(item_entry), enchant_level=VALUES(enchant_level), revision=revision+1", 
        itemGuid, ownerGuid, itemEntry, uint32(level));

    QueryResult verify = CharacterDatabase.Query(
        "SELECT enchant_level FROM mod_item_enchant WHERE item_guid = {}", itemGuid);
    if (!verify || verify->Fetch()[0].Get<uint8>() != level)
        return false;

    std::unique_lock lock(_mutex);
    _levels[itemGuid] = level;
    _ownerItems[ownerGuid].insert(itemGuid);
    return true;
}

bool EnchantDatabase::RemoveLevel(uint32 itemGuid)
{
    CharacterDatabase.DirectExecute("DELETE FROM mod_item_enchant WHERE item_guid = {}", itemGuid);
    QueryResult verify = CharacterDatabase.Query(
        "SELECT COUNT(*) FROM mod_item_enchant WHERE item_guid = {}", itemGuid);
    if (!verify || verify->Fetch()[0].Get<uint64>() != 0)
        return false;
    std::unique_lock lock(_mutex);
    _levels[itemGuid] = 0;
    return true;
}

bool EnchantDatabase::BeginPending(PendingOperation const& operation)
{
    CharacterDatabase.DirectExecute(
        "INSERT INTO mod_item_enchant_pending "
        "(operation_id, owner_guid, item_guid, item_entry, scroll_guid, scroll_entry, scroll_type, old_level, "
        "new_level, destroy_item, success, chance_bp, roll_value, status) VALUES "
        "({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, 'pending')",
        operation.operationId, operation.ownerGuid, operation.itemGuid, operation.itemEntry, operation.scrollGuid,
        operation.scrollEntry, uint32(operation.scrollType), uint32(operation.oldLevel), uint32(operation.newLevel),
        operation.destroyItem ? 1 : 0, operation.success ? 1 : 0, operation.chanceBasisPoints, operation.roll);

    QueryResult verify = CharacterDatabase.Query(
        "SELECT operation_id FROM mod_item_enchant_pending WHERE operation_id={} AND owner_guid={} "
        "AND item_guid={} AND scroll_guid={} AND status='pending'",
        operation.operationId, operation.ownerGuid, operation.itemGuid, operation.scrollGuid);
    return bool(verify);
}

bool EnchantDatabase::CommitOperation(Player* player, PendingOperation const& operation, AttemptResult result,
    uint8 finalLevel, std::string const& reason)
{
    if (!player)
        return false;

    CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
    if (finalLevel && !operation.destroyItem)
    {
        transaction->Append(
            "INSERT INTO mod_item_enchant (item_guid, owner_guid, item_entry, enchant_level) "
            "VALUES ({}, {}, {}, {}) ON DUPLICATE KEY UPDATE owner_guid=VALUES(owner_guid), "
            "item_entry=VALUES(item_entry), enchant_level=VALUES(enchant_level), revision=revision+1",
            operation.itemGuid, operation.ownerGuid, operation.itemEntry, uint32(finalLevel));
    }
    else
        transaction->Append("DELETE FROM mod_item_enchant WHERE item_guid={}", operation.itemGuid);

    if (sEnchantConfig.logsEnabled)
    {
        transaction->Append(
            "INSERT INTO mod_item_enchant_history "
            "(operation_id, owner_guid, item_guid, item_entry, scroll_entry, scroll_type, old_level, new_level, "
            "chance_bp, roll_value, result, reason) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, '{}', '{}')",
            operation.operationId, operation.ownerGuid, operation.itemGuid, operation.itemEntry, operation.scrollEntry,
            uint32(operation.scrollType), uint32(operation.oldLevel), uint32(finalLevel),
            operation.chanceBasisPoints, operation.roll, ToString(result), reason);
    }
    transaction->Append(
        "UPDATE mod_item_enchant_pending SET status='completed', completed_at=NOW() WHERE operation_id={}",
        operation.operationId);

    player->SaveToDB(transaction, false, false);
    CharacterDatabase.DirectCommitTransaction(transaction);

    QueryResult verify = CharacterDatabase.Query(
        "SELECT status FROM mod_item_enchant_pending WHERE operation_id={}", operation.operationId);
    if (!verify || verify->Fetch()[0].Get<std::string>() != "completed")
        return false;

    SetCachedLevel(operation.itemGuid, operation.ownerGuid, operation.destroyItem ? 0 : finalLevel);
    return true;
}

void EnchantDatabase::CompletePending(uint64 operationId, char const* status)
{
    CharacterDatabase.DirectExecute(
        "UPDATE mod_item_enchant_pending SET status='{}', completed_at=NOW() WHERE operation_id={}",
        status, operationId);
}

std::vector<PendingOperation> EnchantDatabase::LoadPending(uint32 ownerGuid) const
{
    std::vector<PendingOperation> operations;
    QueryResult result = CharacterDatabase.Query(
        "SELECT operation_id, owner_guid, item_guid, item_entry, scroll_guid, scroll_entry, scroll_type, "
        "old_level, new_level, destroy_item, success, chance_bp, roll_value "
        "FROM mod_item_enchant_pending WHERE owner_guid={} AND status='pending' ORDER BY operation_id", ownerGuid);
    if (!result)
        return operations;

    do
    {
        Field* fields = result->Fetch();
        PendingOperation operation;
        operation.operationId = fields[0].Get<uint64>();
        operation.ownerGuid = fields[1].Get<uint32>();
        operation.itemGuid = fields[2].Get<uint32>();
        operation.itemEntry = fields[3].Get<uint32>();
        operation.scrollGuid = fields[4].Get<uint32>();
        operation.scrollEntry = fields[5].Get<uint32>();
        operation.scrollType = ScrollType(fields[6].Get<uint8>());
        operation.oldLevel = fields[7].Get<uint8>();
        operation.newLevel = fields[8].Get<uint8>();
        operation.destroyItem = fields[9].Get<bool>();
        operation.success = fields[10].Get<bool>();
        operation.chanceBasisPoints = fields[11].Get<uint32>();
        operation.roll = fields[12].Get<uint32>();
        operations.push_back(operation);
    } while (result->NextRow());
    return operations;
}

void EnchantDatabase::LogAttempt(PendingOperation const& operation, AttemptResult result, std::string const& reason)
{
    if (!sEnchantConfig.logsEnabled)
        return;

    CharacterDatabase.DirectExecute(
        "INSERT INTO mod_item_enchant_history "
        "(operation_id, owner_guid, item_guid, item_entry, scroll_entry, scroll_type, old_level, new_level, "
        "chance_bp, roll_value, result, reason) VALUES ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, '{}', '{}')",
        operation.operationId, operation.ownerGuid, operation.itemGuid, operation.itemEntry, operation.scrollEntry,
        uint32(operation.scrollType), uint32(operation.oldLevel), uint32(operation.newLevel),
        operation.chanceBasisPoints, operation.roll, ToString(result), reason);
}

void EnchantDatabase::CleanupOrphans()
{
    CharacterDatabase.DirectExecute(
        "DELETE e FROM mod_item_enchant e LEFT JOIN item_instance i ON i.guid=e.item_guid WHERE i.guid IS NULL");
    CharacterDatabase.DirectExecute(
        "UPDATE mod_item_enchant_pending SET status='abandoned', completed_at=NOW() "
        "WHERE status='pending' AND created_at < DATE_SUB(NOW(), INTERVAL 7 DAY)");
}

EnchantStatistics EnchantDatabase::GetStatistics() const
{
    EnchantStatistics statistics;
    if (QueryResult result = CharacterDatabase.Query(
        "SELECT COUNT(*), COALESCE(SUM(result='success'), 0), COALESCE(SUM(result LIKE 'failed%'), 0), "
        "COALESCE(SUM(result='failed_destroyed'), 0) "
        "FROM mod_item_enchant_history"))
    {
        Field* fields = result->Fetch();
        statistics.total = fields[0].Get<uint64>();
        statistics.successful = fields[1].Get<uint64>();
        statistics.failed = fields[2].Get<uint64>();
        statistics.destroyed = fields[3].Get<uint64>();
    }
    if (QueryResult result = CharacterDatabase.Query(
        "SELECT COUNT(*) FROM mod_item_enchant_pending WHERE status='pending'"))
        statistics.pending = result->Fetch()[0].Get<uint64>();
    return statistics;
}
}
