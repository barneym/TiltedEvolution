#include <TiltedOnlinePCH.h>

#include <SaveLoad.h>

#include <Events/LocalSaveEvent.h>
#include <Games/Skyrim/Interface/UI.h>
#include <Misc/BSFixedString.h>
#include <PlayerCharacter.h>
#include <Utils.h>
#include <World.h>

#include <atomic>
#include <cctype>
#include <chrono>

namespace
{
// Save_Impl's device: 2 is the save menu and the console, 4 is a quicksave, 3 and 5 are the engine's autosaves.
constexpr int32_t kDeviceManual = 2;
constexpr int32_t kDeviceQuick = 4;

constexpr auto kRetryDelay = std::chrono::seconds(1);
constexpr auto kRefusedRetryDelay = std::chrono::seconds(30);

// A save should not interrupt any of these
constexpr const char* kBusyMenus[] = {
    "Main Menu",      "Loading Menu", "Fader Menu",     "Mist Menu",        "Journal Menu",  "Dialogue Menu",   "Sleep/Wait Menu",
    "Console",        "TweenMenu",    "InventoryMenu",  "MagicMenu",        "ContainerMenu", "BarterMenu",      "GiftMenu",
    "Crafting Menu",  "Book Menu",    "Lockpicking Menu", "StatsMenu",      "MapMenu",       "RaceSex Menu",    "LevelUp Menu",
    "Training Menu",  "Credits Menu", "MessageBoxMenu", "Tutorial Menu",
};

TP_THIS_FUNCTION(TSaveImpl, bool, BGSSaveLoadManager, int32_t, uint32_t, const char*);
TSaveImpl* RealSaveImpl = nullptr;

// Zero when no save is queued
std::atomic<uint32_t> s_queuedSlot{0};
std::chrono::steady_clock::time_point s_nextAttempt{};

bool IsPlayerBusy() noexcept
{
    const PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer || pPlayer->IsDead() || pPlayer->IsInCombat())
        return true;

    const UI* pUI = UI::Get();
    if (!pUI)
        return true;

    for (const char* cpMenu : kBusyMenus)
    {
        if (pUI->GetMenuOpen(BSFixedString(cpMenu)))
            return true;
    }

    return false;
}

String BuildSaveName(const uint32_t aSlot) noexcept
{
    String name = "Together_";

    // Characters share the save folder, and a file name only allows so much
    if (const char* cpPlayerName = PlayerCharacter::Get()->baseForm->GetName())
    {
        for (; *cpPlayerName; ++cpPlayerName)
        {
            if (std::isalnum(static_cast<unsigned char>(*cpPlayerName)))
                name += *cpPlayerName;
        }
    }

    name += '_';
    name += std::to_string(aSlot).c_str();
    return name;
}
} // namespace

// Every save goes through here. Queued saves call RealSaveImpl, so they don't ask the party to save again.
bool TP_MAKE_THISCALL(HookSaveImpl, BGSSaveLoadManager, int32_t aDeviceId, uint32_t aOutputStats, const char* apFileName)
{
    const bool cSaved = TiltedPhoques::ThisCall(RealSaveImpl, apThis, aDeviceId, aOutputStats, apFileName);

    if (cSaved && (aDeviceId == kDeviceManual || aDeviceId == kDeviceQuick))
        World::Get().GetRunner().Trigger(LocalSaveEvent{});

    return cSaved;
}

BGSSaveLoadManager* BGSSaveLoadManager::Get() noexcept
{
    POINTER_SKYRIMSE(BGSSaveLoadManager*, s_instance, 403340);
    return *s_instance.Get();
}

bool BGSSaveLoadManager::CanSave() noexcept
{
    TP_THIS_FUNCTION(TCanSave, bool, BGSSaveLoadManager);
    POINTER_SKYRIMSE(TCanSave, s_canSave, 35734);

    return TiltedPhoques::ThisCall(s_canSave, this);
}

void BGSSaveLoadManager::QueueSave(const uint32_t aSlot) noexcept
{
    s_queuedSlot = aSlot;
}

void BGSSaveLoadManager::CancelQueuedSave() noexcept
{
    s_queuedSlot = 0;
}

void BGSSaveLoadManager::ProcessQueuedSave() noexcept
{
    uint32_t slot = s_queuedSlot;
    if (slot == 0)
        return;

    const auto cNow = std::chrono::steady_clock::now();
    if (cNow < s_nextAttempt)
        return;

    s_nextAttempt = cNow + kRetryDelay;

    BGSSaveLoadManager* pManager = Get();
    if (!pManager || IsPlayerBusy() || !pManager->CanSave())
        return;

    const String cName = BuildSaveName(slot);
    if (!TiltedPhoques::ThisCall(RealSaveImpl, pManager, kDeviceManual, 0u, cName.c_str()))
    {
        spdlog::warn("Party checkpoint save {} was refused, trying again later", cName);
        s_nextAttempt = cNow + kRefusedRetryDelay;
        return;
    }

    spdlog::info("Party checkpoint saved as {}", cName);
    Utils::ShowHudMessage("Party checkpoint saved");

    // A newer checkpoint may have arrived in the meantime
    s_queuedSlot.compare_exchange_strong(slot, 0);
}

static TiltedPhoques::Initializer s_saveManagerHooks(
    []()
    {
        POINTER_SKYRIMSE(TSaveImpl, s_saveImpl, 35727);

        RealSaveImpl = s_saveImpl.Get();

        TP_HOOK(&RealSaveImpl, HookSaveImpl);
    });
