#pragma once
#include "EditorGeometry.h"

namespace labrc::ui
{
struct PairGeometry
{
    Rect title, description, gainReadout, fader, meter, mute, solo, eqIn, status;
    std::array<Rect, 3> eqLabels, eqKnobs, eqReadouts;
    static PairGeometry forSize (double width, double height)
    {
        PairGeometry result;
        const auto sx = width / 503.0, sy = height / 370.0;
        const auto rect = [=] (double x, double y, double w, double h)
        { return Rect { x * sx, y * sy, w * sx, h * sy }; };
        result.title = rect (35, 21, 137, 45);
        result.description = rect (35, 71, 139, 43);
        result.gainReadout = rect (35, 133, 118, 32);
        result.gainReadout.height = std::max (22.0, result.gainReadout.height);
        result.mute = rect (35, 185, 51, 42);
        result.solo = rect (95, 185, 51, 42);
        result.mute.height = result.solo.height = std::max (30.0, 42 * sy);
        result.eqIn = rect (35, 247, 112, 42);
        result.eqIn.height = std::max (30.0, result.eqIn.height);
        result.status = rect (35, 327, 137, 24);
        result.fader = rect (281, 41, 119, 285);
        result.meter = rect (411, 44, 76, 288);
        const auto bandHeight = (height - 24 * sy) / 3;
        const auto labelHeight = std::max (12.0, 18 * sy);
        const auto readoutHeight = std::max (22.0, 26 * sy);
        for (int i = 0; i < 3; ++i)
        {
            const auto top = 12 * sy + i * bandHeight;
            result.eqLabels[static_cast<std::size_t> (i)] = { 171 * sx, top, 105 * sx, labelHeight };
            result.eqKnobs[static_cast<std::size_t> (i)] = { 178 * sx, top + labelHeight, 91 * sx,
                                                           bandHeight - labelHeight - readoutHeight - 4 };
            result.eqReadouts[static_cast<std::size_t> (i)] = { 172 * sx, top + bandHeight - readoutHeight - 2,
                                                              103 * sx, readoutHeight };
        }
        return result;
    }
};
}
