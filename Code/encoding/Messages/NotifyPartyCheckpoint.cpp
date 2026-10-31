#include <Messages/NotifyPartyCheckpoint.h>

void NotifyPartyCheckpoint::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, CheckpointId);
}

void NotifyPartyCheckpoint::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);
    CheckpointId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
