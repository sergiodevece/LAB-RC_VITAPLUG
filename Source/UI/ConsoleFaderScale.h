#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace labrc::ui
{
struct ConsoleFaderScale
{
    inline static constexpr std::array<double, 6> positions { 0.0, .15, .36, .65, .86, 1.0 };
    inline static constexpr std::array<double, 6> decibels { -60.0, -43.0, -27.0, -10.0, 0.0, 6.0 };
    static double decibelsForPosition (double position) noexcept
    { return interpolate (std::isfinite (position) ? position : 0.0, positions, decibels); }
    static double positionForDecibels (double db) noexcept
    { return interpolate (std::isfinite (db) ? db : -60.0, decibels, positions); }
private:
    static double interpolate (double value, const std::array<double, 6>& from,
                               const std::array<double, 6>& to) noexcept
    {
        value = std::clamp (value, from.front(), from.back());
        for (std::size_t i = 1; i < from.size(); ++i)
            if (value <= from[i])
                return to[i - 1] + (value - from[i - 1]) / (from[i] - from[i - 1]) * (to[i] - to[i - 1]);
        return to.back();
    }
};
}
