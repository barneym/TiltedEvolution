#include <Services/DebugService.h>

#include <Services/PapyrusService.h>
#include <Services/PartyService.h>
#include <Services/TransportService.h>

#include <Messages/TeleportCommandRequest.h>

#include <Components.h>
#include <PlayerCharacter.h>
#include <Utils.h>
#include <World.h>

#include <imgui.h>

// Solo testing aid: follow mode.
//
// Toggle it with F8 in the follower's window, or with the checkbox in the debug Party window. While it is on, the
// local player is handed to the game AI (Game.SetPlayerAIDriven) and told to stay close to the party leader's
// character (Actor.KeepOffsetFromActor). If the leader is out of range or in another cell, the follower teleports to
// them through the normal teleport-to-player request. Toggling it off gives control back.
//
// Non-master builds only, so it never ships in a release.

#if (!IS_MASTER)

namespace
{
constexpr double kTickInterval = 0.5;       // seconds between follow updates
constexpr double kTeleportCooldown = 5.0;   // seconds between teleport requests
constexpr float kTeleportDistance = 3000.f; // game units (about 43 m) before snapping to the leader
constexpr float kFollowBehind = -150.f;     // offset behind the leader, in the leader's local space
constexpr float kCatchUpRadius = 400.f;     // run to catch up past this distance
constexpr float kFollowRadius = 150.f;      // stop moving inside this distance

const void* GetNative(const char* acpScript, const char* acpFunction) noexcept
{
    const void* pFunction = World::Get().ctx().at<PapyrusService>().Get(acpScript, acpFunction);
    if (!pFunction)
        spdlog::warn("Follow mode: Papyrus native {}.{} not found", acpScript, acpFunction);
    return pFunction;
}

bool SetPlayerAIDriven(bool aDriven) noexcept
{
    static const void* s_pAddress = GetNative("Game", "SetPlayerAIDriven");
    if (!s_pAddress)
        return false;

    // Game is a global script: the native takes no self, so pass null where PapyrusFunction puts it.
    static PapyrusFunction<void, void, bool> s_setPlayerAIDriven(s_pAddress);
    s_setPlayerAIDriven(nullptr, aDriven);
    return true;
}

bool KeepOffsetFromActor(Actor* apActor, Actor* apTarget) noexcept
{
    static const void* s_pAddress = GetNative("Actor", "KeepOffsetFromActor");
    if (!s_pAddress)
        return false;

    static PapyrusFunction<void, Actor, Actor*, float, float, float, float, float, float, float, float> s_keepOffset(s_pAddress);
    s_keepOffset(apActor, apTarget, 0.f, kFollowBehind, 0.f, 0.f, 0.f, 0.f, kCatchUpRadius, kFollowRadius);
    return true;
}

void ClearKeepOffsetFromActor(Actor* apActor) noexcept
{
    static const void* s_pAddress = GetNative("Actor", "ClearKeepOffsetFromActor");
    if (!s_pAddress)
        return;

    static PapyrusFunction<void, Actor> s_clearKeepOffset(s_pAddress);
    s_clearKeepOffset(apActor);
}

// The leader's character as this client sees it, or null when it isn't loaded here.
Actor* FindPlayerActor(World& aWorld, uint32_t aPlayerId) noexcept
{
    auto view = aWorld.view<FormIdComponent, PlayerComponent>();
    for (const auto entity : view)
    {
        if (view.get<PlayerComponent>(entity).Id != aPlayerId)
            continue;

        return Cast<Actor>(TESForm::GetById(view.get<FormIdComponent>(entity).Id));
    }

    return nullptr;
}
} // namespace

void DebugService::ToggleFollowLeader() noexcept
{
    if (m_followLeader)
    {
        StopFollowingLeader();
        Utils::ShowHudMessage("Follow mode off");
        return;
    }

    const auto& partyService = m_world.GetPartyService();
    if (!partyService.IsInParty() || partyService.IsLeader())
    {
        Utils::ShowHudMessage("Follow mode needs a party, with someone else as leader");
        return;
    }

    m_followLeader = true;
    m_followTargetFormId = 0;
    m_followTickTimer = 0.0;
    m_followTeleportCooldown = 0.0;
    Utils::ShowHudMessage("Follow mode on");
    spdlog::info("Follow mode on");
}

void DebugService::StopFollowingLeader() noexcept
{
    if (!m_followLeader)
        return;

    m_followLeader = false;
    m_followTargetFormId = 0;

    if (auto* pPlayer = PlayerCharacter::Get())
        ClearKeepOffsetFromActor(pPlayer);
    SetPlayerAIDriven(false);

    spdlog::info("Follow mode off");
}

void DebugService::UpdateFollowLeader(double aDelta) noexcept
{
    if (!m_followLeader)
        return;

    m_followTeleportCooldown = std::max(0.0, m_followTeleportCooldown - aDelta);
    m_followTickTimer -= aDelta;
    if (m_followTickTimer > 0.0)
        return;
    m_followTickTimer = kTickInterval;

    const auto& partyService = m_world.GetPartyService();
    if (!m_transport.IsConnected() || !partyService.IsInParty() || partyService.IsLeader())
    {
        StopFollowingLeader();
        Utils::ShowHudMessage("Follow mode off: no longer following a leader");
        return;
    }

    auto* pPlayer = PlayerCharacter::Get();
    if (!pPlayer || pPlayer->IsDead())
        return;

    const uint32_t leaderId = partyService.GetLeaderPlayerId();
    Actor* pLeader = FindPlayerActor(m_world, leaderId);

    const bool sameCell = pLeader && pLeader->GetParentCellEx() == pPlayer->GetParentCellEx();
    const bool sameWorldSpace = pLeader && pLeader->GetWorldSpace() && pLeader->GetWorldSpace() == pPlayer->GetWorldSpace();
    const bool nearby = pLeader && (sameCell || sameWorldSpace) && glm::distance(glm::vec3(pLeader->position), glm::vec3(pPlayer->position)) <= kTeleportDistance;

    if (!nearby)
    {
        // Leader is far away, in another cell, or not loaded here: snap to them, then follow on the next ticks.
        if (m_followTeleportCooldown > 0.0)
            return;

        const auto& players = partyService.GetPlayers();
        const auto it = players.find(leaderId);
        if (it == players.end())
            return;

        TeleportCommandRequest request{};
        request.TargetPlayer = it->second;
        m_transport.Send(request);

        m_followTeleportCooldown = kTeleportCooldown;
        m_followTargetFormId = 0; // Re-apply the follow after arriving.
        return;
    }

    if (m_followTargetFormId == pLeader->formID)
        return;

    // New or changed target: hand the player to the AI and point it at the leader.
    if (!SetPlayerAIDriven(true) || !KeepOffsetFromActor(pPlayer, pLeader))
    {
        StopFollowingLeader();
        Utils::ShowHudMessage("Follow mode unavailable, see the client log");
        return;
    }

    m_followTargetFormId = pLeader->formID;
    spdlog::info("Follow mode: following leader {:X}", pLeader->formID);
}

void DebugService::DrawFollowLeaderControls() noexcept
{
    const auto& partyService = m_world.GetPartyService();
    if (!partyService.IsInParty() || partyService.IsLeader())
        return;

    bool follow = m_followLeader;
    if (ImGui::Checkbox("Follow leader (F8)", &follow) && follow != m_followLeader)
        ToggleFollowLeader();
}

#endif
