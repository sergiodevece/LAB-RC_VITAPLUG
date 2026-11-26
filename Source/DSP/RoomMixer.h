#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <functional>
#include <memory>

namespace labrc
{
struct Parameters
{
    int room = 0;
    float inputDb = 0, outputDb = 0, mixPercent = 100;
    std::array<float, 3> pairDb{{-6, -3, -9}};
    std::array<bool, 3> mute{}, solo{};
    bool bypass = false;
    std::array<std::array<float, 3>, 3> eqDb{}; // pair, then LOW/MID/HIGH
    std::array<bool, 3> eqEnabled{};
};

struct MeterValues
{
    std::array<float, 2> input{}, output{};
    std::array<std::array<float, 2>, 3> pairs{};
};

using IRProvider = std::function<juce::AudioBuffer<float>(int room, int pair)>;

class RoomMixer
{
  public:
    RoomMixer();
    ~RoomMixer();
    // In external mode requestRoom selects the room. Otherwise each Parameters block selects it.
    void prepare(double sampleRate, int maxBlockSize, int initialRoom, IRProvider,
                 bool externallyControlledRoom = false);
    // Control-thread/automation hint. Fully prepared banks can wait while transport is idle.
    void requestRoom(int room) noexcept;
    void release();
    void process(float* const* channels, int numInputChannels, int numSamples, const Parameters&,
                 bool hostBypassed = false) noexcept;
    int latencySamples() const noexcept;
    double tailSeconds() const noexcept;
    // Single UI reader: returns per-channel peaks since the previous read, then clears them.
    MeterValues meterSnapshot() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace labrc
