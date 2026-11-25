#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace labrc::ui
{
struct Point { double x = 0, y = 0; };
struct Size { double width = 0, height = 0; };
struct Rect
{
    double x = 0, y = 0, width = 0, height = 0;
    double right() const noexcept { return x + width; }
    double bottom() const noexcept { return y + height; }
    bool empty() const noexcept { return width <= 0 || height <= 0; }
    bool contains (Point p) const noexcept { return p.x >= x && p.x <= right() && p.y >= y && p.y <= bottom(); }
    bool contains (Rect r) const noexcept { return contains (Point { r.x, r.y }) && contains (Point { r.right(), r.bottom() }); }
};
inline Rect fitContain (Rect stage, double imageWidth, double imageHeight) noexcept
{
    if (stage.empty() || imageWidth <= 0 || imageHeight <= 0
        || ! std::isfinite (imageWidth) || ! std::isfinite (imageHeight))
        return {};
    const auto scale = std::min (stage.width / imageWidth, stage.height / imageHeight);
    const auto width = imageWidth * scale, height = imageHeight * scale;
    return { stage.x + (stage.width - width) * .5, stage.y + (stage.height - height) * .5, width, height };
}

struct EditorGeometry
{
    Rect header, input, output, tabs, stage, sceneFooter, footer;
    std::array<Rect, 3> pairs;
    double scale = 1.0;
    static EditorGeometry forSize (double width, double height) noexcept
    {
        EditorGeometry result;
        result.scale = std::max (0.0, std::min (width / 1586.0, height / 992.0));
        const auto x = (width - 1586.0 * result.scale) * .5;
        const auto y = (height - 992.0 * result.scale) * .5;
        const auto rect = [=] (double rx, double ry, double rw, double rh)
        { return Rect { x + rx * result.scale, y + ry * result.scale, rw * result.scale, rh * result.scale }; };
        result.header = rect (30, 12, 1526, 80);
        result.input = rect (25, 104, 201, 564);
        result.output = rect (1358, 104, 201, 564);
        result.tabs = rect (246, 115, 1088, 60);
        result.stage = rect (244, 184, 1096, 431);
        result.sceneFooter = rect (244, 616, 1096, 40);
        result.pairs = {{ rect (25, 678, 503, 224), rect (539, 678, 489, 224), rect (1040, 678, 519, 224) }};
        result.footer = rect (25, 921, 1534, 57);
        return result;
    }
};
inline Point markerPoint (int room, int mic, Rect photo) noexcept
{
    constexpr double coordinates[3][6][2] {
        {{39,54},{58,54},{34,67},{64,67},{24,85},{75,85}},
        {{11,45},{24,45},{10,62},{29,63},{6,80},{44,81}},
        {{49,65},{66,65},{44,78},{72,78},{31,91},{84,91}}
    };
    const auto& p = coordinates[std::clamp (room, 0, 2)][std::clamp (mic, 0, 5)];
    return { photo.x + photo.width * p[0] * .01, photo.y + photo.height * p[1] * .01 };
}
}
