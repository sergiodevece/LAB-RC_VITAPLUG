#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
void writeSnapshot(juce::AudioProcessorEditor& editor, const juce::File& output,
                   int width, int height, float scale)
{
    editor.setSize(width, height);
    const auto snapshot = editor.createComponentSnapshot(editor.getLocalBounds(), true, scale);
    if(snapshot.isNull() || snapshot.getWidth() != juce::roundToInt(width * scale)
                         || snapshot.getHeight() != juce::roundToInt(height * scale))
        throw std::runtime_error("Invalid component snapshot dimensions");
    juce::PNGImageFormat png;
    const auto stream = output.createOutputStream();
    if(!stream || !stream->setPosition(0) || stream->truncate().failed()
               || !png.writeImageToStream(snapshot, *stream))
        throw std::runtime_error("Cannot write " + output.getFullPathName().toStdString());
    stream->flush();
    if(stream->getStatus().failed())
        throw std::runtime_error("Snapshot write failed");
    std::cout << output.getFullPathName() << " " << snapshot.getWidth() << "x"
              << snapshot.getHeight() << "\n";
}
}

int main(int argc, char** argv)
{
    if(argc != 3 || juce::String(argv[1]) != "--output")
    {
        std::cerr << "Usage: labrc_render_ui --output /absolute/screenshots/directory\n";
        return 2;
    }
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        const juce::File folder(juce::String::fromUTF8(argv[2]));
        if(folder.createDirectory().failed())
            throw std::runtime_error("Cannot create screenshots directory");
        LabRCProcessor processor;
        auto* room = processor.apvts.getParameter("room");
        if(room == nullptr)
            throw std::runtime_error("Room parameter missing");
        constexpr const char* names[] {"kentucky", "center-field", "live-room"};
        for(int index = 0; index < 3; ++index)
        {
            room->setValueNotifyingHost(room->convertTo0to1(static_cast<float>(index)));
            std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
            if(editor == nullptr)
                throw std::runtime_error("Processor returned no editor");
            writeSnapshot(*editor, folder.getChildFile(juce::String(names[index]) + "-min.png"),
                          800, 500, 1.f);
            writeSnapshot(*editor, folder.getChildFile(juce::String(names[index]) + "-default.png"),
                          1180, 738, 1.f);
            writeSnapshot(*editor, folder.getChildFile(juce::String(names[index]) + "-max.png"),
                          1586, 992, 1.f);
            writeSnapshot(*editor, folder.getChildFile(juce::String(names[index]) + "-default@2x.png"),
                          1180, 738, 2.f);
        }
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr << "Render failed: " << error.what() << '\n';
        return 1;
    }
}
