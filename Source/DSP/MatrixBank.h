#pragma once
#include "RoomMixer.h"

namespace labrc
{
class MatrixBank
{
  public:
    static constexpr int latency = 128;
    MatrixBank(int room, double sampleRate, const IRProvider&);
    ~MatrixBank();
    // Outputs 2 * pair + channel. The caller supplies a cleared destination.
    void add(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, int count) noexcept;

  private:
    class Layer;
    std::vector<std::unique_ptr<Layer>> layers;
};
} // namespace labrc
