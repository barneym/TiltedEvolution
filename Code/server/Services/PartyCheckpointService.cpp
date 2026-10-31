#include <Services/PartyCheckpointService.h>

#include <Components.h>
#include <GameServer.h>
#include <World.h>

#include <Events/UpdateEvent.h>

#include <Messages/NotifyPartyCheckpoint.h>
#include <Messages/PartyCheckpointRequest.h>

#include <Setting.h>

namespace
{
Console::Setting uPartyCheckpointInterval{"Gameplay:uPartyCheckpointInterval", "Minutes between automatic party checkpoints, 0 disables them", 0u};

// Members often save at about the same time, which should only count once
constexpr uint64_t kMinCheckpointGap = 30000;
} // namespace

PartyCheckpointService::PartyCheckpointService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
    , m_updateConnection(aDispatcher.sink<UpdateEvent>().connect<&PartyCheckpointService::OnUpdate>(this))
    , m_checkpointRequestConnection(aDispatcher.sink<PacketEvent<PartyCheckpointRequest>>().connect<&PartyCheckpointService::OnCheckpointRequest>(this))
{
}

void PartyCheckpointService::OnUpdate(const UpdateEvent& acEvent) noexcept
{
    const auto cCurrentTick = GameServer::Get()->GetTick();
    if (m_nextUpdate > cCurrentTick)
        return;

    // Only check once every second
    m_nextUpdate = cCurrentTick + 1000;

    for (auto itor = std::begin(m_lastCheckpoints); itor != std::end(m_lastCheckpoints);)
    {
        if (m_world.GetPartyService().GetById(itor->first))
            ++itor;
        else
            itor = m_lastCheckpoints.erase(itor);
    }

    const uint64_t cInterval = static_cast<uint64_t>(uPartyCheckpointInterval.value_as<uint32_t>()) * 60000;
    if (cInterval == 0)
        return;

    for (const Player* pPlayer : m_world.GetPlayerManager())
    {
        const auto& cPartyId = pPlayer->GetParty().JoinedPartyId;
        if (!cPartyId.has_value())
            continue;

        // The interval of a new party starts now
        const auto itor = m_lastCheckpoints.try_emplace(*cPartyId, cCurrentTick).first;
        if (cCurrentTick - itor->second >= cInterval)
            SendCheckpoint(pPlayer, nullptr);
    }
}

void PartyCheckpointService::OnCheckpointRequest(const PacketEvent<PartyCheckpointRequest>& acPacket) noexcept
{
    const auto& cPartyId = acPacket.pPlayer->GetParty().JoinedPartyId;
    if (!cPartyId.has_value())
        return;

    const auto itor = m_lastCheckpoints.find(*cPartyId);
    if (itor != std::end(m_lastCheckpoints) && GameServer::Get()->GetTick() - itor->second < kMinCheckpointGap)
        return;

    // The player who asked has just saved
    SendCheckpoint(acPacket.pPlayer, acPacket.pPlayer);
}

void PartyCheckpointService::SendCheckpoint(const Player* apPlayer, const Player* apExclude) noexcept
{
    const auto& cParty = apPlayer->GetParty();

    NotifyPartyCheckpoint notify;
    notify.CheckpointId = m_nextCheckpointId++;

    m_lastCheckpoints[*cParty.JoinedPartyId] = GameServer::Get()->GetTick();

    spdlog::info("Party {} checkpoint {}", *cParty.JoinedPartyId, notify.CheckpointId);
    GameServer::Get()->SendToParty(notify, cParty, apExclude);
}
