#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/RoomMixer.h"

class LabRCProcessor final : public juce::AudioProcessor {
    // RoomChoice points here for its entire lifetime, including APVTS setup and teardown.
    labrc::RoomMixer mixer;
public:
    LabRCProcessor();
    ~LabRCProcessor() override;
    void prepareToPlay(double,int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    bool supportsDoublePrecisionProcessing() const override { return false; }
    juce::AudioProcessorParameter* getBypassParameter() const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "LAB-RC"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    labrc::MeterValues meterSnapshot() const noexcept;
    bool micsVisible() const noexcept;
    void setMicsVisible(bool);
    void setEditorSize(int width,int height);
    juce::Point<int> editorSize() const;
    bool canUndoUser();
    bool canRedoUser();
    bool undoUser();
    bool redoUser();
    void resetParameters();
    void beginUserGesture(const juce::String& name);
    void beginParameterGesture(const juce::String& parameterID,const juce::String& name = {});
    void setParameterFromUser(const juce::String& parameterID,float actualValue);
    void endParameterGesture(const juce::String& parameterID);
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout(labrc::RoomMixer&);
    labrc::Parameters readParameters() const noexcept;
    void process(juce::AudioBuffer<float>&,bool) noexcept;
    void reconcileUndo();
    void closeActiveGesture();
    juce::CriticalSection userStateLock; // Control/GUI only. Never acquired by process/automation.
    double preparedRate=0;
    int preparedBlock=0, initialRoom=0;
    std::atomic<bool> audioStarted{false};
    std::array<std::atomic<float>*,14> raw{};
    std::atomic<bool> showMics{true};
    std::atomic<std::uint64_t> viewSize{(std::uint64_t{738} << 32u) | 1180u};
    bool undoResetPending=false; // Protected by userStateLock.
    juce::String activeUserParameter, cancelledDrag;
    bool prepared=false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabRCProcessor)
};
