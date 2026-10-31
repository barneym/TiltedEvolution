#include <Messages/PartyCheckpointRequest.h>

void PartyCheckpointRequest::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
}

void PartyCheckpointRequest::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);
}
