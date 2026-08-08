#ifndef MOD_ITEM_ENCHANT_SYSTEM_MANAGER_H
#define MOD_ITEM_ENCHANT_SYSTEM_MANAGER_H

#include "EnchantTypes.h"

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Item;
class Player;

namespace ItemEnchant
{
class EnchantManager
{
public:
    static EnchantManager& Instance();

    bool ShowItemSelection(Player* player, Item* scroll);
    void HandleGossipSelection(Player* player, Item* scroll, uint32 sender, uint32 action);
    bool TryEnchant(Player* player, Item* scroll, Item* target, bool recovery = false);
    bool SetLevelByCommand(Player* player, Item* target, uint8 newLevel);
    void RecoverPending(Player* player);
    void RefreshAuras(Player* player, Item const* excludedItem = nullptr) const;
    void ReapplyEquipmentModifiers(Player* player, bool apply) const;
    void ClearPlayer(uint32 playerGuid);

    Item* FindItem(Player* player, uint32 itemGuid) const;
    std::vector<Item*> GetEligibleItems(Player* player, ScrollType scrollType) const;

private:
    struct SelectionSession
    {
        uint32 scrollGuid = 0;
        ScrollType scrollType = ScrollType::Normal;
        uint32 page = 0;
        std::vector<uint32> itemGuids;
        std::chrono::steady_clock::time_point openedAt;
    };

    EnchantManager() = default;

    bool RenderSelection(Player* player, Item* scroll);
    bool ApplyOperation(Player* player, Item* scroll, Item* target, PendingOperation const& operation,
        AttemptResult result, bool recovery);
    bool AcquireAttempt(uint32 playerGuid);
    void ReleaseAttempt(uint32 playerGuid);
    void ClearSelection(uint32 playerGuid);
    void SendResult(Player* player, AttemptResult result, std::string const& itemName, uint8 level) const;
    void SendValidationError(Player* player, std::string const& reason) const;
    uint64 NextOperationId() const;

    mutable std::mutex _mutex;
    std::unordered_map<uint32, SelectionSession> _sessions;
    std::unordered_map<uint32, uint32> _lastAttemptMs;
    std::unordered_set<uint32> _busyPlayers;
    mutable std::unordered_map<uint32, uint32> _activeAuraByPlayer;
};
}

#define sEnchantManager ItemEnchant::EnchantManager::Instance()

#endif
