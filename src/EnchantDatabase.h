#ifndef MOD_ITEM_ENCHANT_SYSTEM_DATABASE_H
#define MOD_ITEM_ENCHANT_SYSTEM_DATABASE_H

#include "EnchantTypes.h"

#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Item;
class Player;

namespace ItemEnchant
{
struct EnchantStatistics
{
    uint64 total = 0;
    uint64 successful = 0;
    uint64 failed = 0;
    uint64 destroyed = 0;
    uint64 pending = 0;
};

class EnchantDatabase
{
public:
    static EnchantDatabase& Instance();

    void LoadForPlayer(Player const* player);
    void UnloadPlayer(uint32 ownerGuid);
    uint8 GetLevel(Item const* item) const;
    uint8 GetLevel(uint32 itemGuid) const;
    void SetCachedLevel(uint32 itemGuid, uint32 ownerGuid, uint8 level);
    void EraseCachedLevel(uint32 itemGuid);
    bool SetLevel(uint32 itemGuid, uint32 ownerGuid, uint32 itemEntry, uint8 level);
    bool RemoveLevel(uint32 itemGuid);

    bool BeginPending(PendingOperation const& operation);
    bool CommitOperation(Player* player, PendingOperation const& operation, AttemptResult result,
        uint8 finalLevel, std::string const& reason = {});
    void CompletePending(uint64 operationId, char const* status);
    std::vector<PendingOperation> LoadPending(uint32 ownerGuid) const;
    void LogAttempt(PendingOperation const& operation, AttemptResult result, std::string const& reason = {});
    void CleanupOrphans();
    EnchantStatistics GetStatistics() const;

private:
    EnchantDatabase() = default;

    mutable std::shared_mutex _mutex;
    mutable std::unordered_map<uint32, uint8> _levels;
    mutable std::unordered_map<uint32, std::unordered_set<uint32>> _ownerItems;
};
}

#define sEnchantDatabase ItemEnchant::EnchantDatabase::Instance()

#endif
