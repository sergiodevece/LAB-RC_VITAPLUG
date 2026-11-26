#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "EditorGeometry.h"
#include "ConsoleFaderScale.h"
#include "PairGeometry.h"
#include "DrumKitLayer.h"

class LabRCProcessor;

namespace labrc::ui
{
namespace theme
{
juce::Colour cream();
juce::Colour gold();
juce::Colour dark();
juce::Colour pairColour (int pair);
juce::Font font (float height, bool serif = false, bool bold = false);
void panel (juce::Graphics&, juce::Rectangle<float>, float scale, bool screws = true);
void patina (juce::Graphics&, juce::Rectangle<float>, float scale, int seed);
void screw (juce::Graphics&, float x, float y, float radius);
void lettering (juce::Graphics&, const juce::String&, juce::Rectangle<float>,
                float height, bool serif = false, bool bold = false,
                juce::Justification = juce::Justification::centred);
}

class VintageLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawCornerResizer (juce::Graphics&, int width, int height, bool over, bool dragging) override;
};

class VintageButton final : public juce::Button
{
public:
    enum class Kind { normal, mute, solo, room, marker };
    explicit VintageButton (juce::String name, Kind kind = Kind::normal);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void setSubtitle (juce::String text) { subtitle = std::move (text); repaint(); }
    void setAccent (juce::Colour colour) { accent = colour; repaint(); }
    void setRouteActive (bool active) { routeActive = active; repaint(); }
private:
    Kind kind;
    juce::String subtitle;
    juce::Colour accent;
    bool routeActive = true;
};

class ConsoleSlider final : public juce::Slider
{
public:
    explicit ConsoleSlider (bool fader);
    double proportionOfLengthToValue (double proportion) override;
    double valueToProportionOfLength (double value) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    const bool consoleFader;
};

class ParameterControl final : public juce::Component
{
public:
    ParameterControl (LabRCProcessor&, juce::String id, juce::String name, bool fader);
    ~ParameterControl() override;
    void layout (juce::Rectangle<int> sliderArea, juce::Rectangle<int> readoutArea);
    void refreshFromParameter();
private:
    void update (float actual);
    void change (float actual);
    juce::String formattedValue (double value) const;
    LabRCProcessor& processor;
    const juce::String parameterID, parameterName;
    ConsoleSlider slider;
    juce::Label readout;
    const std::atomic<float>* rawParameter = nullptr;
    bool dragging = false, updating = false;
};

class StereoMeter final : public juce::Component
{
public:
    StereoMeter();
    void setLevels (std::array<float, 2> linear);
    void paint (juce::Graphics&) override;
private:
    std::array<float, 2> displayDb {{ -100, -100 }};
    std::array<double, 2> clippedUntil {};
};

class RoomScene final : public juce::Component
{
public:
    RoomScene();
    void setRoom (int index);
    void setMicsVisible (bool);
    void setPairState (int pair, bool active);
    void setHighlightedPair (int);
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void(int)> onHighlight;
private:
    std::array<juce::Image, 3> images;
    DrumKitLayer drumLayer;
    std::array<std::unique_ptr<VintageButton>, 6> markers;
    int room = 0, highlightedPair = -1;
    bool mics = true;
};

class PairStrip final : public juce::Component
{
public:
    PairStrip (LabRCProcessor&, int pair);
    void resized() override;
    void paint (juce::Graphics&) override;
    void setPresentation (bool muted, bool soloed, bool active, bool bypassed, bool highlighted);
    void setLevels (std::array<float, 2> linear) { meter.setLevels (linear); }
    void refreshFromParameters();
private:
    LabRCProcessor& processor;
    const int pair;
    ParameterControl gain;
    std::array<std::unique_ptr<ParameterControl>, 3> eq;
    VintageButton mute, solo, eqIn;
    StereoMeter meter;
    juce::Image plateCache;
    float plateScale = 0;
    bool routeActive = true, bypassed = false, highlighted = false;
};
}
