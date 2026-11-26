#pragma once
#include "EditorGeometry.h"

namespace labrc::ui
{
inline Rect drumLayerBounds (int room, Rect photo) noexcept
{
    constexpr double positions[3][3] {{ .355, .475, .31 }, { .095, .39, .195 }, { .47, .61, .29 }};
    const auto& p = positions[std::clamp (room, 0, 2)];
    const auto width = photo.width * p[2];
    return { photo.x + photo.width * p[0], photo.y + photo.height * p[1], width, width * 1086.0 / 1448.0 };
}
}
