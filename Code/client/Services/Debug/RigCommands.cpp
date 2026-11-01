#if !IS_MASTER

#include <Services/DebugService.h>

#include <AI/AIProcess.h>
#include <Forms/TESNPC.h>
#include <Forms/TESObjectCELL.h>
#include <Forms/TESPackage.h>

#include <PlayerCharacter.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

// Lets the test rig drive a running client: ~/bin/rig-cmd drops commands in logs/rig-cmd.in and reads logs/rig-cmd.out.
// Off unless ST_RIG_CMD is set. Commands: "con <console command>", "inspect <hex form id>".
namespace
{
constexpr double kPollInterval = 0.25;
constexpr uint32_t kScriptFormType = 0x13;

void RunConsoleCommand(const std::string& acCommand) noexcept
{
    using TCreateForm = void*(void*);
    using TCompileAndRun = void(void*, void*, int32_t, void*);

    POINTER_SKYRIMSE(void*, s_formFactories, 400508);
    POINTER_SKYRIMSE(TCompileAndRun, s_compileAndRun, 441582);

    void* pFactory = s_formFactories.Get()[kScriptFormType];
    if (!pFactory)
        return;

    auto* pCreate = reinterpret_cast<TCreateForm*>((*static_cast<void***>(pFactory))[1]);
    auto* pScript = static_cast<char*>(pCreate(pFactory));
    if (!pScript)
        return;

    // The script form is left allocated: freeing it would free the text with the game's allocator.
    *reinterpret_cast<const char**>(pScript + 0x38) = acCommand.c_str();

    char compiler{};
    constexpr int32_t kSystemWindowCompiler = 1;
    s_compileAndRun.Get()(pScript, &compiler, kSystemWindowCompiler, nullptr);

    *reinterpret_cast<const char**>(pScript + 0x38) = nullptr;
}

void InspectActor(uint32_t aFormId, std::ofstream& aOutput) noexcept
{
    Actor* pActor = Cast<Actor>(TESForm::GetById(aFormId));
    if (!pActor)
    {
        aOutput << "inspect " << std::hex << aFormId << ": not an actor in memory\n";
        return;
    }

    const auto* pNpc = Cast<TESNPC>(pActor->baseForm);
    const auto* pLeft = pActor->GetEquippedWeapon(0);
    const auto* pRight = pActor->GetEquippedWeapon(1);
    const auto* pPackage = pActor->currentProcess ? pActor->currentProcess->package : nullptr;

    aOutput << std::hex << "inspect " << aFormId << ": name=" << (pNpc && pNpc->fullName.value.AsAscii() ? pNpc->fullName.value.AsAscii() : "?")
            << " remote=" << pActor->GetExtension()->IsRemote() << " cell=" << (pActor->parentCell ? pActor->parentCell->formID : 0)
            << " flags1=" << pActor->actorState.flags1 << " flags2=" << pActor->actorState.flags2 << " weapon_state=" << (pActor->actorState.flags2 >> 5 & 7)
            << " sit=" << (pActor->actorState.flags1 >> 14 & 0xF) << " package=" << (pPackage ? pPackage->formID : 0)
            << " left=" << (pLeft ? pLeft->formID : 0) << " right=" << (pRight ? pRight->formID : 0) << std::dec
            << " pos=" << pActor->position.x << "," << pActor->position.y << "," << pActor->position.z << " graph_ready=" << pActor->animationGraphHolder.IsReady();

    for (const char* cpName : {"bEquipOk", "IsEquipping", "IsUnequipping", "bInMoveState", "bAllowRotation", "IsAttacking", "bAnimationDriven", "bMotionDriven", "IsSneaking"})
    {
        BSFixedString name(cpName);
        bool value = false;
        aOutput << " " << cpName << "=";
        if (pActor->animationGraphHolder.GetVariableBool(&name, &value))
            aOutput << value;
        else
            aOutput << "n/a";
    }
    for (const char* cpName : {"Speed", "SpeedSampled", "Direction"})
    {
        BSFixedString name(cpName);
        float value = 0.f;
        aOutput << " " << cpName << "=";
        if (pActor->animationGraphHolder.GetVariableFloat(&name, &value))
            aOutput << value;
        else
            aOutput << "n/a";
    }
    for (const char* cpName : {"iState", "iLeftHandType", "iRightHandType"})
    {
        BSFixedString name(cpName);
        uint32_t value = 0;
        aOutput << " " << cpName << "=";
        if (pActor->animationGraphHolder.GetVariableInt(&name, &value))
            aOutput << value;
        else
            aOutput << "n/a";
    }
    aOutput << "\n";
}
} // namespace

void DebugService::UpdateRigCommands(double aDelta) noexcept
{
    static const bool s_enabled = std::getenv("ST_RIG_CMD") != nullptr;
    static double s_elapsed = 0.0;

    if (!s_enabled)
        return;

    s_elapsed += aDelta;
    if (s_elapsed < kPollInterval)
        return;
    s_elapsed = 0.0;

    const auto cLogs = TiltedPhoques::GetPath() / "logs";
    const auto cInput = cLogs / "rig-cmd.in";

    std::error_code error;
    if (!std::filesystem::exists(cInput, error))
        return;

    std::ifstream input(cInput);
    std::ofstream output(cLogs / "rig-cmd.out", std::ios::app);

    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        output << "> " << line << "\n";

        if (line.rfind("con ", 0) == 0)
        {
            RunConsoleCommand(line.substr(4));
            output << "ran\n";
        }
        else if (line.rfind("inspect ", 0) == 0)
        {
            InspectActor(static_cast<uint32_t>(std::strtoul(line.c_str() + 8, nullptr, 16)), output);
        }
        else
        {
            output << "unknown command\n";
        }
    }
    output.close();
    input.close();

    std::filesystem::remove(cInput, error);
}

#endif
