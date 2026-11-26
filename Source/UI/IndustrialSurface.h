#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Resources.h"
#include <cmath>

namespace labrc::ui::industrial
{
inline float hash (int x, int y, int seed = 31) noexcept
{
    auto n = static_cast<juce::uint32> (x) * 374761393u + static_cast<juce::uint32> (y) * 668265263u
             + static_cast<juce::uint32> (seed) * 2246822519u;
    n = (n ^ (n >> 13u)) * 1274126177u;
    return static_cast<float> ((n ^ (n >> 16u)) & 65535u) / 65535.0f;
}
inline float noise (float x, float y, int seed) noexcept
{
    const auto ix = static_cast<int> (std::floor (x)), iy = static_cast<int> (std::floor (y));
    auto fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    return juce::jmap (fy, juce::jmap (fx, hash (ix, iy, seed), hash (ix + 1, iy, seed)),
                           juce::jmap (fx, hash (ix, iy + 1, seed), hash (ix + 1, iy + 1, seed)));
}
inline const juce::Image& surfaceTexture()
{
    static const auto texture = []
    {
        const auto sample = labrc::metalTexture();
        juce::Image result (juce::Image::RGB, 640, 512, false);
        juce::Image::BitmapData data (result, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < result.getHeight(); ++y)
            for (int x = 0; x < result.getWidth(); ++x)
            {
                const auto sx = x % (sample.getWidth() * 2);
                const auto sy = y % (sample.getHeight() * 2);
                const auto pixel = sample.getPixelAt (sx < sample.getWidth() ? sx : sample.getWidth() * 2 - sx - 1,
                                                      sy < sample.getHeight() ? sy : sample.getHeight() * 2 - sy - 1);
                const auto broad = noise (x / 74.0f, y / 59.0f, 19);
                const auto medium = noise (x / 11.0f, y / 13.0f, 43);
                const auto grain = hash (x, y, 89);
                const auto delta = (broad - .5f) * 13 + (medium - .5f) * 6 + (grain - .5f) * 3;
                const auto oxide = std::max (0.0f, broad - .63f) * 13;
                data.setPixelColour (x, y, juce::Colour::fromRGB (
                    static_cast<juce::uint8> (juce::jlimit (0, 255, juce::roundToInt (pixel.getRed() + delta + oxide))),
                    static_cast<juce::uint8> (juce::jlimit (0, 255, juce::roundToInt (pixel.getGreen() + delta * .8f))),
                    static_cast<juce::uint8> (juce::jlimit (0, 255, juce::roundToInt (pixel.getBlue() + delta * .65f - oxide)))));
            }
        return result;
    }();
    return texture;
}
inline void wear (juce::Graphics& g, juce::Rectangle<float> area, float scale, int seed)
{
    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip; clip.addRoundedRectangle (area, 8 * scale); g.reduceClipRegion (clip);
    for (int edge = 0; edge < 4; ++edge)
    {
        const auto length = edge < 2 ? area.getWidth() : area.getHeight();
        for (float along = 0; along < length; along += 1.2f * scale)
        {
            const auto unit = along / scale;
            const auto cloud = noise (unit / 18, edge * 7.0f, seed);
            const auto particle = hash (static_cast<int> (unit * 3), edge, seed);
            if (cloud + particle * .44f < .61f) continue;
            const auto inset = (hash (static_cast<int> (unit), edge, seed + 1) * 5.0f + .4f) * scale;
            const auto size = (.35f + particle * 2.5f) * scale;
            const auto gold = particle > .68f ? juce::Colour (0xffc39a5c) : juce::Colour (0xff77502c);
            g.setColour (gold.withAlpha (.20f + particle * .42f));
            if (edge < 2)
            {
                const auto y = edge == 0 ? area.getY() + inset : area.getBottom() - inset;
                g.fillEllipse (area.getX() + along, y, size, (.35f + particle) * scale);
            }
            else
            {
                const auto x = edge == 2 ? area.getX() + inset : area.getRight() - inset;
                g.fillEllipse (x, area.getY() + along, (.35f + particle) * scale, size);
            }
        }
    }
    juce::Random random (static_cast<juce::int64> (seed));
    const auto scratches = static_cast<int> (area.getWidth() * area.getHeight() / std::max (1.0f, 12000 * scale * scale));
    for (int i = 0; i < scratches; ++i)
    {
        const auto x = area.getX() + random.nextFloat() * area.getWidth();
        const auto y = area.getY() + random.nextFloat() * area.getHeight();
        g.setColour (juce::Colour (0xffa58b58).withAlpha (.06f + random.nextFloat() * .08f));
        g.drawLine (x, y, x + (3 + random.nextFloat() * 23) * scale, y - (1 + random.nextFloat() * 4) * scale, .45f * scale);
    }
}
inline void screw (juce::Graphics& g, float x, float y, float radius)
{
    static const auto head = labrc::screwImage();
    const juce::Rectangle<float> bounds (x - radius, y - radius, radius * 2, radius * 2);
    g.setColour (juce::Colour (0xc0000000));
    g.fillEllipse (bounds.translated (radius * .08f, radius * .16f).expanded (radius * .07f));
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (head, bounds, juce::RectanglePlacement::stretchToFit);
}
inline void panel (juce::Graphics& g, juce::Rectangle<float> bounds, float scale, bool screws)
{
    const auto r = 11 * scale;
    g.setColour (juce::Colour (0xdd050805));
    g.fillRoundedRectangle (bounds.translated (0, 4 * scale).expanded (2 * scale), r + scale);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc7ad79), bounds.getX(), bounds.getY(),
                                            juce::Colour (0xff30281b), bounds.getRight(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, r);
    const auto face = bounds.reduced (3 * scale);
    juce::Path facePath; facePath.addRoundedRectangle (face, r - 2 * scale);
    g.setTiledImageFill (surfaceTexture(), static_cast<int> (bounds.getX()), static_cast<int> (bounds.getY()), 1.0f);
    g.fillPath (facePath);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x147b8668), face.getX(), face.getY(),
                                            juce::Colour (0x60070d08), face.getRight(), face.getBottom(), false));
    g.fillPath (facePath);
    g.setColour (juce::Colour (0xff080c08));
    g.drawRoundedRectangle (bounds.reduced (4 * scale), r - 3 * scale, 2 * scale);
    g.setColour (juce::Colour (0x756e7559));
    g.drawRoundedRectangle (bounds.reduced (6 * scale), r - 4 * scale, .7f * scale);
    wear (g, bounds.reduced (scale), scale, static_cast<int> (bounds.getWidth() * 11 + bounds.getHeight() * 7));
    if (screws)
        for (const auto x : { bounds.getX() + 15 * scale, bounds.getRight() - 15 * scale })
            for (const auto y : { bounds.getY() + 15 * scale, bounds.getBottom() - 15 * scale })
                screw (g, x, y, 8 * scale);
}
inline void metalKnob (juce::Graphics& g, float cx, float cy, float radius, float angle)
{
    g.setColour (juce::Colour (0xb3000000));
    g.fillEllipse (cx - radius - 3, cy - radius + 2, radius * 2 + 6, radius * 2 + 6);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0d5a9), cx - radius, cy - radius,
                                            juce::Colour (0xff3d3426), cx + radius, cy + radius, false));
    g.fillEllipse (cx - radius, cy - radius, radius * 2, radius * 2);
    const auto face = radius * .91f;
    constexpr int sectors = 512;
    for (int i = 0; i < sectors; ++i)
    {
        const auto a = i * juce::MathConstants<float>::twoPi / sectors;
        const auto b = (i + 1.8f) * juce::MathConstants<float>::twoPi / sectors;
        const auto light = .70f + .18f * std::cos (2 * a + .45f) + .06f * std::cos (a - .8f);
        const auto fine = (hash (i, 13) - .5f) * .045f;
        g.setColour (juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, light + fine + .12f),
                                                 juce::jlimit (0.0f, 1.0f, light + fine + .04f),
                                                 juce::jlimit (0.0f, 1.0f, light + fine - .09f), 1));
        juce::Path wedge; wedge.startNewSubPath (cx, cy);
        wedge.lineTo (cx + std::sin (a) * face, cy - std::cos (a) * face);
        wedge.lineTo (cx + std::sin (b) * face, cy - std::cos (b) * face); wedge.closeSubPath();
        g.fillPath (wedge);
    }
    juce::Path mask; mask.addEllipse (cx - face, cy - face, face * 2, face * 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x42fff0cf), cx - face * .45f, cy - face * .55f,
                                            juce::Colour (0x55050a05), cx + face * .65f, cy + face * .9f, false));
    g.fillPath (mask);
    for (float ring = 3; ring < face; ring += .85f)
    {
        g.setColour (juce::Colour (0x16070503));
        g.drawEllipse (cx - ring, cy - ring, ring * 2, ring * 2, .2f);
    }
    g.setColour (juce::Colour (0xffecdab6));
    g.drawEllipse (cx - radius, cy - radius, radius * 2, radius * 2, .75f);
    g.setColour (juce::Colour (0xff171a12));
    g.drawLine (cx + std::sin (angle) * radius * .24f, cy - std::cos (angle) * radius * .24f,
                cx + std::sin (angle) * radius * .83f, cy - std::cos (angle) * radius * .83f, std::max (1.6f, radius * .07f));
}
inline void bakeliteKnob (juce::Graphics& g, float cx, float cy, float radius, float angle)
{
    g.setColour (juce::Colour (0xb9000000));
    g.fillEllipse (cx - radius - 1, cy - radius + 3, radius * 2 + 2, radius * 2 + 2);
    juce::Path flutes;
    for (int i = 0; i < 128; ++i)
    {
        const auto a = i * juce::MathConstants<float>::twoPi / 128;
        const auto r = radius * (i % 4 < 2 ? 1.0f : .93f);
        const auto x = cx + std::sin (a) * r, y = cy - std::cos (a) * r;
        if (i == 0) flutes.startNewSubPath (x, y); else flutes.lineTo (x, y);
    }
    flutes.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff555950), cx - radius, cy - radius,
                                            juce::Colour (0xff080b09), cx + radius, cy + radius, false));
    g.fillPath (flutes);
    for (int i = 0; i < 32; ++i)
    {
        const auto a = i * juce::MathConstants<float>::twoPi / 32;
        g.setColour (juce::Colour (0xff777b6b).withAlpha (.14f + .14f * std::max (0.0f, std::cos (a + .7f))));
        g.drawLine (cx + std::sin (a) * radius * .80f, cy - std::cos (a) * radius * .80f,
                    cx + std::sin (a) * radius * .97f, cy - std::cos (a) * radius * .97f, .8f);
    }
    const auto face = radius * .78f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c3029), cx - face * .6f, cy - face * .7f,
                                            juce::Colour (0xff080b08), cx + face, cy + face, true));
    g.fillEllipse (cx - face, cy - face, face * 2, face * 2);
    g.setColour (juce::Colour (0x66595d4d)); g.drawEllipse (cx - face, cy - face, face * 2, face * 2, .65f);
    g.setColour (juce::Colour (0xffe6debd));
    g.drawLine (cx + std::sin (angle) * radius * .30f, cy - std::cos (angle) * radius * .30f,
                cx + std::sin (angle) * radius * .81f, cy - std::cos (angle) * radius * .81f, std::max (1.6f, radius * .09f));
}
}
