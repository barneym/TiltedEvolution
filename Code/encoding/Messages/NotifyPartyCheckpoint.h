#pragma once

#include "Message.h"

struct NotifyPartyCheckpoint final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyPartyCheckpoint;

    NotifyPartyCheckpoint()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyPartyCheckpoint& acRhs) const noexcept
    {
        return GetOpcode() == acRhs.GetOpcode() && CheckpointId == acRhs.CheckpointId;
    }

    uint32_t CheckpointId{};
};
