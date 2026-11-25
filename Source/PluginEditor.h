#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "UI/VintageComponents.h"

class LabRCProcessor;

class LabRCEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LabRCEditor (LabRCProcessor&);
    ~LabRCEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
private:
    void timerCallback() override;
    void refreshPresentation();
    void selectRoom (int);
    void changeParameter (const juce::String&, float, const juce::String&);
    LabRCProcessor& processor;
    labrc::ui::VintageLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltip;
    labrc::ui::ParameterControl input, output, mix;
    labrc::ui::StereoMeter inputMeter, outputMeter;
    labrc::ui::RoomScene scene;
    std::array<std::unique_ptr<labrc::ui::VintageButton>, 3> rooms;
    std::array<std::unique_ptr<labrc::ui::PairStrip>, 3> pairs;
    labrc::ui::VintageButton bypass, mics, reset, undo, redo;
    int selectedRoom = 0, highlightedPair = -1;
    bool restoringSize = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabRCEditor)
};
