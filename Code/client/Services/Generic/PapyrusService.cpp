#include <TiltedOnlinePCH.h>

#include <Events/PapyrusFunctionRegisterEvent.h>

#include <Services/PapyrusService.h>

namespace
{
// Papyrus names ignore case, and the engine's string table keeps whichever spelling it saw first: on 1.6.1179 the
// Game script registers its natives under "game".
String MakeKey(const String& acNamespace, const String& acFunction) noexcept
{
    String key = acNamespace + "::" + acFunction;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return key;
}
} // namespace

PapyrusService::PapyrusService(entt::dispatcher& aDispatcher) noexcept
{
    m_papyrusFunctionRegisterConnection = aDispatcher.sink<PapyrusFunctionRegisterEvent>().connect<&PapyrusService::HandlePapyrusFunctionEvent>(this);
}

const void* PapyrusService::Get(const String& acNamespace, const String& acFunction) const noexcept
{
    const auto itor = m_functions.find(MakeKey(acNamespace, acFunction));
    if (itor != std::end(m_functions))
        return itor->second;

    return nullptr;
}

void PapyrusService::HandlePapyrusFunctionEvent(const PapyrusFunctionRegisterEvent& acEvent) noexcept
{
    m_functions[MakeKey(acEvent.Namespace, acEvent.Name)] = acEvent.Function;
}
