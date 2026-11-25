#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_graphics/juce_graphics.h>
namespace labrc {
juce::AudioBuffer<float> loadMatrixIR(int room,int pair);
juce::Image roomImage(int room);
}
