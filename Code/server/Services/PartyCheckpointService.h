#pragma once

#include <Events/PacketEvent.h>

struct World;
struct UpdateEvent;
struct PartyCheckpointRequest;

/**
 * @brief Tells parties when to save, so that their members end up with matching saves.
 *
 * The server stores no game state: each client writes its own save when told to.
 */
struct PartyCheckpointService
{
    PartyCheckpointService(World& aWorld, entt::dispatcher& aDispatcher) noexcept;
    ~PartyCheckpointService() noexcept = default;

    TP_NOCOPYMOVE(PartyCheckpointService);

protected:
    void OnUpdate(const UpdateEvent& acEvent) noexcept;
    void OnCheckpointRequest(const PacketEvent<PartyCheckpointRequest>& acPacket) noexcept;

private:
    void SendCheckpoint(const Player* apPlayer, const Player* apExclude) noexcept;

    World& m_world;

    // Tick of each party's last checkpoint
    TiltedPhoques::Map<uint32_t, uint64_t> m_lastCheckpoints;
    uint32_t m_nextCheckpointId{1};
    uint64_t m_nextUpdate{0};

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_checkpointRequestConnection;
};
