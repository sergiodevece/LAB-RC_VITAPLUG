#include "Resources.h"
#include <LabRCAssets.h>
#include <stdexcept>

namespace labrc {
namespace {
constexpr const char* rooms[]{"kentucky","center_field","live_room"};
constexpr const char* pairs[]{"EARS","FRONT","FLOOR"};
const char* resource(const juce::String& name,int& size) {
    const auto* data=LabRCAssets::getNamedResource(name.toRawUTF8(),size);
    if(data==nullptr || size<=0) throw std::runtime_error("Missing embedded LAB-RC resource");
    return data;
}
}
juce::AudioBuffer<float> loadMatrixIR(int room,int pair) {
    if(room<0 || room>2 || pair<0 || pair>2) throw std::invalid_argument("Invalid LAB-RC matrix index");
    int size=0;
    const auto* data=resource(juce::String(rooms[room])+"_"+pairs[pair]+"_wet_matrix_wav",size);
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(new juce::MemoryInputStream(data,static_cast<size_t>(size),false),true));
    if(!reader || reader->sampleRate!=96000 || reader->numChannels!=4 || !reader->usesFloatingPointData)
        throw std::runtime_error("Invalid embedded LAB-RC matrix format");
    juce::AudioBuffer<float> result(4,static_cast<int>(reader->lengthInSamples));
    if(!reader->read(&result,0,result.getNumSamples(),0,true,true))
        throw std::runtime_error("Cannot decode embedded LAB-RC matrix");
    return result;
}
juce::Image roomImage(int room) {
    int size=0;
    const auto* data=resource(juce::String(rooms[juce::jlimit(0,2,room)])+"_clean_png",size);
    return juce::ImageFileFormat::loadFrom(data,static_cast<size_t>(size));
}
juce::Image drumKitImage() {
    int size=0; const auto* data=resource("boost_this_png",size);
    return juce::ImageFileFormat::loadFrom(data,static_cast<size_t>(size));
}
juce::Image metalTexture() {
    int size=0; const auto* data=resource("approved_metal_png",size);
    return juce::ImageFileFormat::loadFrom(data,static_cast<size_t>(size));
}
juce::Image screwImage() {
    int size=0; const auto* data=resource("approved_screw_png",size);
    return juce::ImageFileFormat::loadFrom(data,static_cast<size_t>(size));
}
}
