#pragma once

#include "Message.h"

struct PartyCheckpointRequest final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kPartyCheckpointRequest;

    PartyCheckpointRequest()
        : ClientMessage(Opcode)
    {
    }

    virtual ~PartyCheckpointRequest() = default;

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const PartyCheckpointRequest& achRhs) const noexcept { return GetOpcode() == achRhs.GetOpcode(); }
};
