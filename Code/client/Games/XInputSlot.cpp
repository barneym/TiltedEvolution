#include <TiltedOnlinePCH.h>

#include <Xinput.h>

#include <cstdlib>
#include <string_view>

// Solo testing aid: choose which gamepad this client reads.
//
// Skyrim reads XInput slot 0, and every process sees every pad. With two clients side by side on one machine
// (two Wine prefixes, CrossOver bottles, or plain Windows), both follow the same pad. Set ST_XINPUT_SLOT in a
// client's environment to give it its own pad:
//
//   ST_XINPUT_SLOT=1     this client uses the second pad (slots 0 and 1 are swapped for the game)
//   ST_XINPUT_SLOT=none  this client sees no pads (for the keyboard-and-mouse client)
//
// When the variable is unset, no hook is installed and behaviour is unchanged. Non-master builds only.

#if (!IS_MASTER)

namespace
{
constexpr DWORD kMaxSlots = XUSER_MAX_COUNT;
constexpr DWORD kNoPads = static_cast<DWORD>(-1);

DWORD s_slot = 0;

// Swap slot 0 with the chosen slot, so the game's "first pad" is the one picked for this client.
bool RemapSlot(DWORD& aUserIndex) noexcept
{
    if (s_slot == kNoPads)
        return false;

    if (aUserIndex == 0)
        aUserIndex = s_slot;
    else if (aUserIndex == s_slot)
        aUserIndex = 0;

    return true;
}

bool ParseSlot(std::string_view aValue, DWORD& aSlot) noexcept
{
    if (aValue == "none" || aValue == "-1")
    {
        aSlot = kNoPads;
        return true;
    }

    if (aValue.size() == 1 && aValue[0] >= '0' && aValue[0] < static_cast<char>('0' + kMaxSlots))
    {
        aSlot = static_cast<DWORD>(aValue[0] - '0');
        return true;
    }

    return false;
}
} // namespace

using TXInputGetState = decltype(&XInputGetState);
using TXInputSetState = decltype(&XInputSetState);
using TXInputGetCapabilities = decltype(&XInputGetCapabilities);

static TXInputGetState RealXInputGetState = nullptr;
static TXInputSetState RealXInputSetState = nullptr;
static TXInputGetCapabilities RealXInputGetCapabilities = nullptr;

static DWORD WINAPI HookXInputGetState(DWORD aUserIndex, XINPUT_STATE* apState)
{
    if (!RemapSlot(aUserIndex))
        return ERROR_DEVICE_NOT_CONNECTED;

    return RealXInputGetState(aUserIndex, apState);
}

static DWORD WINAPI HookXInputSetState(DWORD aUserIndex, XINPUT_VIBRATION* apVibration)
{
    if (!RemapSlot(aUserIndex))
        return ERROR_DEVICE_NOT_CONNECTED;

    return RealXInputSetState(aUserIndex, apVibration);
}

static DWORD WINAPI HookXInputGetCapabilities(DWORD aUserIndex, DWORD aFlags, XINPUT_CAPABILITIES* apCapabilities)
{
    if (!RemapSlot(aUserIndex))
        return ERROR_DEVICE_NOT_CONNECTED;

    return RealXInputGetCapabilities(aUserIndex, aFlags, apCapabilities);
}

static TiltedPhoques::Initializer s_xinputSlotHooks(
    []()
    {
        const char* pValue = std::getenv("ST_XINPUT_SLOT");
        if (!pValue || !*pValue)
            return;

        if (!ParseSlot(pValue, s_slot))
        {
            spdlog::warn("ST_XINPUT_SLOT=\"{}\" is not 0-3 or \"none\"; gamepad slot left unchanged", pValue);
            return;
        }

        if (s_slot == 0)
            return; // Nothing to remap.

        // Skyrim SE imports XInput by name from xinput1_3.dll.
        const char* cModuleName = "xinput1_3.dll";

        TP_HOOK_IAT(XInputGetState, cModuleName);
        TP_HOOK_IAT(XInputSetState, cModuleName);
        TP_HOOK_IAT(XInputGetCapabilities, cModuleName);

        if (!RealXInputGetState)
        {
            spdlog::warn("ST_XINPUT_SLOT is set but XInputGetState is not imported from {}; gamepad slot left unchanged", cModuleName);
            return;
        }

        if (s_slot == kNoPads)
            spdlog::info("ST_XINPUT_SLOT: this client ignores all gamepads");
        else
            spdlog::info("ST_XINPUT_SLOT: this client reads gamepad slot {}", s_slot);
    });

#endif
