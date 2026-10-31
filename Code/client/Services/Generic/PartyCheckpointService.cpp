#include <Services/PartyCheckpointService.h>

#include <Services/TransportService.h>

#include <World.h>

#include <Events/DisconnectedEvent.h>
#include <Events/LocalSaveEvent.h>

#include <Messages/NotifyPartyCheckpoint.h>
#include <Messages/PartyCheckpointRequest.h>

#include <Games/Skyrim/SaveLoad.h>

namespace
{
// Checkpoints reuse a fixed set of saves, so that they don't pile up
constexpr uint32_t kCheckpointSlots = 10;
} // namespace

PartyCheckpointService::PartyCheckpointService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransportService) noexcept
    : m_world(aWorld)
    , m_transport(aTransportService)
{
    m_disconnectConnection = aDispatcher.sink<DisconnectedEvent>().connect<&PartyCheckpointService::OnDisconnected>(this);
    m_localSaveConnection = aDispatcher.sink<LocalSaveEvent>().connect<&PartyCheckpointService::OnLocalSave>(this);
    m_partyCheckpointConnection = aDispatcher.sink<NotifyPartyCheckpoint>().connect<&PartyCheckpointService::OnPartyCheckpoint>(this);
}

void PartyCheckpointService::OnDisconnected(const DisconnectedEvent& acEvent) noexcept
{
    BGSSaveLoadManager::CancelQueuedSave();
}

void PartyCheckpointService::OnLocalSave(const LocalSaveEvent& acEvent) noexcept
{
    if (!m_transport.IsOnline() || !m_world.GetPartyService().IsInParty())
        return;

    PartyCheckpointRequest request;
    m_transport.Send(request);
}

void PartyCheckpointService::OnPartyCheckpoint(const NotifyPartyCheckpoint& acMessage) noexcept
{
    spdlog::info("Party checkpoint {} received", acMessage.CheckpointId);

    BGSSaveLoadManager::QueueSave(acMessage.CheckpointId % kCheckpointSlots + 1);
}
