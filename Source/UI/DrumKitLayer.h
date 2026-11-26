#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Resources.h"
#include "SceneComposition.h"

namespace labrc::ui
{
class DrumKitLayer final : public juce::Component
{
public:
    DrumKitLayer()
    {
        setComponentID ("boostThisLayer");
        setName ("Original BOOST THIS drum kit, visual layer");
        setInterceptsMouseClicks (false, false);
        // Read-only asset derivatives are shared between editors. Only this foreground is lit.
        static const auto layers = []
        {
            std::array<juce::Image, 3> result;
            const auto original = labrc::drumKitImage();
            constexpr float light[3][3] {{ .98f, .91f, .81f }, { .96f, .92f, .85f }, { .82f, .86f, .91f }};
            for (int room = 0; room < 3; ++room)
            {
                auto image = original.createCopy();
                juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readWrite);
                for (int y = 0; y < image.getHeight(); ++y)
                    for (int x = 0; x < image.getWidth(); ++x)
                    {
                        const auto colour = pixels.getPixelColour (x, y);
                        if (colour.getAlpha() == 0) continue;
                        const auto falloff = 1.0f - .08f * y / image.getHeight();
                        pixels.setPixelColour (x, y, juce::Colour::fromRGBA (
                            static_cast<juce::uint8> (colour.getRed() * light[room][0] * falloff),
                            static_cast<juce::uint8> (colour.getGreen() * light[room][1] * falloff),
                            static_cast<juce::uint8> (colour.getBlue() * light[room][2] * falloff), colour.getAlpha()));
                    }
                result[static_cast<std::size_t> (room)] = image;
            }
            return result;
        }();
        illuminated = layers;
    }
    void setScene (int index, Rect photoBounds)
    {
        room = juce::jlimit (0, 2, index);
        photo = photoBounds;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        if (photo.empty()) return;
        const auto bounds = drumLayerBounds (room, photo);
        const auto x = static_cast<float> (bounds.x), y = static_cast<float> (bounds.y);
        const auto w = static_cast<float> (bounds.width), h = static_cast<float> (bounds.height);
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (juce::Rectangle<float> (static_cast<float> (photo.x), static_cast<float> (photo.y),
                                                   static_cast<float> (photo.width), static_cast<float> (photo.height)).getSmallestIntegerContainer());
        const juce::Rectangle<float> contact (x + w * .035f, y + h * .935f, w * .93f, h * .105f);
        {
            juce::Graphics::ScopedSaveState shadow (g);
            g.addTransform (juce::AffineTransform::scale (contact.getWidth() * .5f, contact.getHeight() * .5f)
                                .translated (contact.getCentreX(), contact.getCentreY()));
            g.setGradientFill (juce::ColourGradient (juce::Colour (room == 2 ? 0x5d080b10 : 0x650d0804),
                                                    0, 0, juce::Colours::transparentBlack, 1, 0, true));
            g.fillEllipse (-1.0f, -1.0f, 2.0f, 2.0f);
        }
        const auto& image = illuminated[static_cast<std::size_t> (room)];
        constexpr float shear[3] { -.013f, -.022f, .014f };
        const auto inset = w * std::abs (shear[room]);
        const auto topLeft = x + (shear[room] < 0 ? inset : 0);
        const auto bottomLeft = x + (shear[room] > 0 ? inset : 0);
        const auto transform = juce::AffineTransform::fromTargetPoints (
            0, 0, topLeft, y, static_cast<float> (image.getWidth()), 0, topLeft + w - inset, y + h * .004f,
            0, static_cast<float> (image.getHeight()), bottomLeft, y + h);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageTransformed (image, transform, false);
    }
private:
    std::array<juce::Image, 3> illuminated;
    Rect photo;
    int room = 0;
};
}
