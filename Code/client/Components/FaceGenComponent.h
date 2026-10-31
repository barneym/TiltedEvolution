#pragma once

#ifndef TP_INTERNAL_COMPONENTS_GUARD
#error Include Components.h instead
#endif

#include <Structs/Tints.h>

struct NiTexture;

struct FaceGenComponent
{
    FaceGenComponent() noexcept {}

    // Only compared, to notice when the head no longer shows this texture
    const NiTexture* Texture{nullptr};
    Tints FaceTints{};
};
