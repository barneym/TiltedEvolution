#pragma once

struct World;
struct TransportService;
struct DisconnectedEvent;
struct LocalSaveEvent;
struct NotifyPartyCheckpoint;

/**
 * @brief Saves together with the party: a local save asks the party to save, and the server says when to.
 */
struct PartyCheckpointService
{
    PartyCheckpointService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransportService) noexcept;
    ~PartyCheckpointService() = default;

    TP_NOCOPYMOVE(PartyCheckpointService);

protected:
    void OnDisconnected(const DisconnectedEvent& acEvent) noexcept;
    void OnLocalSave(const LocalSaveEvent& acEvent) noexcept;
    void OnPartyCheckpoint(const NotifyPartyCheckpoint& acMessage) noexcept;

private:
    World& m_world;
    TransportService& m_transport;

    entt::scoped_connection m_disconnectConnection;
    entt::scoped_connection m_localSaveConnection;
    entt::scoped_connection m_partyCheckpointConnection;
};
