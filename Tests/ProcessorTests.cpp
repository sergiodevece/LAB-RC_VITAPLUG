#include "PluginProcessor.h"
#include "Resources.h"
#include <limits>
#include <thread>

namespace {
class ProcessorTests final : public juce::UnitTest {
public:
    ProcessorTests() : UnitTest("Processor, state and user undo", "Processor") {}
    void runTest() override {
        beginTest("Embedded matrices retain all four original channels and samples");
        constexpr const char* roomNames[]{"kentucky","center_field","live_room"};
        constexpr const char* pairNames[]{"EARS","FRONT","FLOOR"};
        juce::WavAudioFormat format;
        for(int room=0;room<3;++room) for(int pair=0;pair<3;++pair) {
            const auto path=juce::File(juce::String::fromUTF8(LABRC_KIT_ROOT))
                .getChildFile("assets/ir/v06/").getChildFile(juce::String(roomNames[room])+"_"+pairNames[pair]+"_wet_matrix.wav");
            std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(path.createInputStream().release(),true));
            expect(reader!=nullptr);
            if(!reader) continue;
            auto embedded=labrc::loadMatrixIR(room,pair);
            expectEquals(embedded.getNumChannels(),4);
            expectEquals(embedded.getNumSamples(),static_cast<int>(reader->lengthInSamples));
            if(embedded.getNumChannels()!=4 || embedded.getNumSamples()!=reader->lengthInSamples) continue;
            juce::AudioBuffer<float> original(4,static_cast<int>(reader->lengthInSamples));
            reader->read(&original,0,original.getNumSamples(),0,true,true);
            float error=0;
            for(int c=0;c<4;++c) for(int n=0;n<original.getNumSamples();++n)
                error=std::max(error,std::abs(embedded.getSample(c,n)-original.getSample(c,n)));
            expectEquals(error,0.0f);
        }
        beginTest("Embedded clean room photographs and transparent drum kit decode the approved pixels");
        const auto photos=juce::File(juce::String::fromUTF8(LABRC_FINAL_ASSET_ROOT));
        for(int resource=0;resource<4;++resource) {
            const bool kit=resource==3;
            const auto file=kit?juce::String("boost_this.png"):juce::String(roomNames[resource])+"_clean.png";
            const auto original=juce::ImageFileFormat::loadFrom(photos.getChildFile(file));
            auto embedded=kit?labrc::drumKitImage():labrc::roomImage(resource);
            expect(embedded.isValid());
            expectEquals(embedded.getWidth(),original.getWidth());
            expectEquals(embedded.getHeight(),original.getHeight());
            if(embedded.getBounds()!=original.getBounds()) continue;
            bool identical=true;
            for(int y=0;y<original.getHeight() && identical;++y) for(int x=0;x<original.getWidth();++x)
                if(original.getPixelAt(x,y)!=embedded.getPixelAt(x,y)) {identical=false;break;}
            expect(identical);
        }
        beginTest("Mono to stereo and stereo to stereo only");
        LabRCProcessor p;
        auto layout = p.getBusesLayout();
        layout.inputBuses.set(0, juce::AudioChannelSet::mono());
        layout.outputBuses.set(0, juce::AudioChannelSet::stereo());
        expect(p.isBusesLayoutSupported(layout));
        layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
        expect(p.isBusesLayoutSupported(layout));
        layout.outputBuses.set(0, juce::AudioChannelSet::mono());
        expect(!p.isBusesLayoutSupported(layout));
        expect(!p.supportsDoublePrecisionProcessing());

        beginTest("Persistent parameter defaults are usable without preparing audio");
        const std::array<std::pair<const char*,float>,14> defaults{{
            {"room",0},{"inputDb",0},{"outputDb",0},{"mixPercent",100},
            {"earsDb",-6},{"frontDb",-3},{"floorDb",-9},
            {"earsMute",0},{"frontMute",0},{"floorMute",0},
            {"earsSolo",0},{"frontSolo",0},{"floorSolo",0},{"bypass",0}
        }};
        for (const auto& item: defaults) {
            auto* raw = p.apvts.getRawParameterValue(item.first);
            expect(raw != nullptr, item.first);
            if (raw) expectWithinAbsoluteError(raw->load(),item.second,0.0001f, item.first);
        }

        beginTest("User drag creates one undo action; host automation creates none");
        p.beginParameterGesture("inputDb", "Input");
        p.setParameterFromUser("inputDb", 3.2f);
        p.setParameterFromUser("inputDb", 9.1f);
        p.endParameterGesture("inputDb");
        expect(p.undoManager.canUndo());
        expectWithinAbsoluteError(p.apvts.getRawParameterValue("inputDb")->load(),9.1f,0.001f);
        expect(p.undoManager.undo());
        expectWithinAbsoluteError(p.apvts.getRawParameterValue("inputDb")->load(),0.0f,0.001f);
        expect(!p.undoManager.canUndo());
        expect(p.undoManager.redo());
        expectWithinAbsoluteError(p.apvts.getRawParameterValue("inputDb")->load(),9.1f,0.001f);
        p.undoManager.clearUndoHistory();
        auto* mix = p.apvts.getParameter("mixPercent");
        mix->setValueNotifyingHost(mix->convertTo0to1(26.7f));
        auto flushed = p.apvts.copyState();
        juce::ignoreUnused(flushed);
        expect(!p.undoManager.canUndo(), "host automation must not enter user history");

        beginTest("Recall includes DSP and per-instance UI preferences");
        p.setMicsVisible(false);
        p.setEditorSize(1400,876);
        p.beginParameterGesture("room", "Room");
        p.setParameterFromUser("room",2);
        p.endParameterGesture("room");
        juce::MemoryBlock saved;
        p.getStateInformation(saved);
        LabRCProcessor copy;
        copy.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
        expectWithinAbsoluteError(copy.apvts.getRawParameterValue("mixPercent")->load(),26.7f,0.001f);
        expectEquals(static_cast<int>(copy.apvts.getRawParameterValue("room")->load()),2);
        expect(!copy.micsVisible());
        expectEquals(copy.editorSize().x,1400);

        beginTest("Reset and its inverse are a single user transaction");
        copy.resetParameters();
        expect(copy.micsVisible());
        for (const auto& item: defaults)
            expectWithinAbsoluteError(copy.apvts.getRawParameterValue(item.first)->load(),item.second,0.0001f);
        expectEquals(copy.editorSize().x,1400);
        expect(copy.undoManager.undo());
        expect(!copy.micsVisible());
        expectEquals(static_cast<int>(copy.apvts.getRawParameterValue("room")->load()),2);
        expectWithinAbsoluteError(copy.apvts.getRawParameterValue("mixPercent")->load(),26.7f,0.001f);
        expect(!copy.undoManager.canUndo());
        expect(copy.undoManager.redo());
        expect(copy.micsVisible());

        beginTest("Legacy partial states clamp ranges and default malformed or missing data");
        const auto restore = [&copy](const juce::String& xml) {
            auto parsed = juce::XmlDocument::parse(xml);
            juce::MemoryBlock blob;
            juce::AudioProcessor::copyXmlToBinary(*parsed,blob);
            copy.setStateInformation(blob.getData(),static_cast<int>(blob.getSize()));
        };
        restore("<PARAMETERS><PARAM id=\"inputDb\" value=\"100\"/><PARAM id=\"earsDb\" value=\"nan\"/><PARAM id=\"floorDb\" value=\"nonsense\"/><PARAM id=\"room\" value=\"999\"/></PARAMETERS>");
        expectWithinAbsoluteError(copy.apvts.getRawParameterValue("inputDb")->load(),18.f,0.0001f);
        expectWithinAbsoluteError(copy.apvts.getRawParameterValue("earsDb")->load(),-6.f,0.0001f);
        expectWithinAbsoluteError(copy.apvts.getRawParameterValue("floorDb")->load(),-9.f,0.0001f);
        expectWithinAbsoluteError(copy.apvts.getRawParameterValue("mixPercent")->load(),100.f,0.0001f);
        expectEquals(static_cast<int>(copy.apvts.getRawParameterValue("room")->load()),2);
        expect(copy.micsVisible());
        const char invalid[]="broken state";
        copy.setStateInformation(invalid,sizeof(invalid));
        expectWithinAbsoluteError(copy.apvts.getRawParameterValue("inputDb")->load(),18.f,0.0001f);
        restore("<LABRC_STATE version=\"1\" micsVisible=\"0\" editorWidth=\"99999\" editorHeight=\"1\"><PARAMETERS/></LABRC_STATE>");
        expect(!copy.micsVisible());
        expect(copy.editorSize().x<=1586 && copy.editorSize().y>=684);
        expect(!copy.undoManager.canUndo());
        beginTest("A different instance remains untouched by state and undo");
        expectWithinAbsoluteError(p.apvts.getRawParameterValue("mixPercent")->load(),26.7f,0.001f);
        expect(!p.micsVisible());
    }
};
static ProcessorTests processorTests;

class RecallTests final : public juce::UnitTest
{
public:
    RecallTests() : UnitTest("Recall lifecycle and validation", "Processor") {}

    static juce::MemoryBlock packed(const juce::String& xml)
    {
        juce::MemoryBlock result;
        juce::AudioProcessor::copyXmlToBinary(*juce::XmlDocument::parse(xml),result);
        return result;
    }

    static void restore(LabRCProcessor& processor, const juce::MemoryBlock& state)
    {
        processor.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
    }

    static void restoreOnHost(LabRCProcessor& processor,const juce::MemoryBlock& state)
    {
        std::thread host([&]{restore(processor,state);});
        host.join();
    }

    void runTest() override
    {
        beginTest("An initial recall on either side of prepare renders the same first impulse for nine pairs");
        const char* solos[]={"earsSolo","frontSolo","floorSolo"};
        for(int room=0;room<3;++room)
            for(int pair=0;pair<3;++pair)
            {
                auto state=packed("<PARAMETERS><PARAM id=\"room\" value=\""+juce::String(room)
                    +"\"/><PARAM id=\""+solos[pair]+"\" value=\"1\"/></PARAMETERS>");
                auto bounce=[&](bool recallFirst)
                {
                    LabRCProcessor processor;
                    processor.setNonRealtime(true);
                    processor.setRateAndBufferSizeDetails(192000.,64);
                    if(recallFirst) restore(processor,state);
                    processor.prepareToPlay(192000.,64);
                    if(!recallFirst) restore(processor,state);
                    juce::AudioBuffer<float> output(2,19200), block(2,64);
                    juce::MidiBuffer midi;
                    for(int k=0;k<output.getNumSamples();k+=64)
                    {
                        block.clear();
                        if(k==0) { block.setSample(0,0,1.f); block.setSample(1,0,-0.375f); }
                        processor.processBlock(block,midi);
                        for(int c=0;c<2;++c) output.copyFrom(c,k,block,c,0,64);
                    }
                    return output;
                };
                auto before=bounce(true),after=bounce(false);
                expect(before.getMagnitude(0,19200)>1e-5f);
                for(int c=0;c<2;++c) after.addFrom(c,0,before,c,0,19200,-1.f);
                expectEquals(after.getMagnitude(0,19200),0.f,
                    "room/pair "+juce::String(room)+"/"+juce::String(pair));
            }
        beginTest("An immediate GUI undo or redo cannot replay history preceding a host restore");
        const auto saved=packed("<PARAMETERS><PARAM id=\"inputDb\" value=\"-12\"/></PARAMETERS>");
        for(int op=0;op<4;++op)
        {
            LabRCProcessor p;
            p.setParameterFromUser("inputDb",6.f);
            if(op%2!=0) p.undoUser();
            restoreOnHost(p,saved);
            bool stale=false;
            switch(op)
            {
                case 0: stale=p.undoUser(); break;
                case 1: stale=p.redoUser(); break;
                case 2: stale=p.canUndoUser(); break;
                case 3: stale=p.canRedoUser(); break;
            }
            expect(!stale);
            expectWithinAbsoluteError(p.apvts.getRawParameterValue("inputDb")->load(),-12.f,0.001f);
        }
        beginTest("Host recall during a drag balances the gesture and ignores old drag moves");
        {
            LabRCProcessor p;
            struct Gestures : juce::AudioProcessorParameter::Listener
            {
                int start=0,end=0;
                void parameterValueChanged(int,float) override {}
                void parameterGestureChanged(int,bool starting) override { if(starting) ++start; else ++end; }
            } gestures;
            auto* db=p.apvts.getParameter("inputDb");
            db->addListener(&gestures);
            p.beginParameterGesture("inputDb");
            p.setParameterFromUser("inputDb",6);
            restoreOnHost(p,saved);
            p.setParameterFromUser("inputDb",14);
            p.endParameterGesture("inputDb");
            expectEquals(gestures.start,1); expectEquals(gestures.end,1);
            expectWithinAbsoluteError(p.apvts.getRawParameterValue("inputDb")->load(),-12.f,0.001f);
            expect(!p.canUndoUser());
            p.beginParameterGesture("inputDb"); p.setParameterFromUser("inputDb",4); p.endParameterGesture("inputDb");
            expect(p.undoUser());
            expectWithinAbsoluteError(p.apvts.getRawParameterValue("inputDb")->load(),-12.f,0.001f);
            db->removeListener(&gestures);
        }
        beginTest("Malformed envelopes and incompatible versions preserve all live state and user history");
        for(const auto* invalid : {"<OTHER/>",
                "<LABRC_STATE version=\"1\"><broken/></LABRC_STATE>",
                "<LABRC_STATE version=\"2\"><PARAMETERS/></LABRC_STATE>",
                "<LABRC_STATE version=\"1.1\"><PARAMETERS/></LABRC_STATE>",
                "<LABRC_STATE version=\"nonsense\"><PARAMETERS/></LABRC_STATE>",
                "<LABRC_STATE><PARAMETERS/></LABRC_STATE>",
                "<LABRC_STATE version=\"1\"><PARAMETERS><WRONG id=\"inputDb\" value=\"-2\"/></PARAMETERS></LABRC_STATE>"})
        {
            LabRCProcessor p;
            p.setMicsVisible(false); p.setEditorSize(1024,684); p.setParameterFromUser("inputDb",10);
            restoreOnHost(p,packed(invalid));
            expectWithinAbsoluteError(p.apvts.getRawParameterValue("inputDb")->load(),10.f,0.001f);
            expect(!p.micsVisible()); expectEquals(p.editorSize().x,1024);
            expect(p.undoUser()); expectWithinAbsoluteError(p.apvts.getRawParameterValue("inputDb")->load(),0.f,0.001f);
        }
    }
};
static RecallTests recallTests;

class EQStateTests final : public juce::UnitTest
{
  public:
    EQStateTests() : UnitTest("Final Touch parameter identity and recall", "Processor") {}
    void runTest() override
    {
        beginTest(
            "New parameter IDs are unique, separate from the original AU version group and automatable");
        LabRCProcessor p;
        expectEquals(p.getParameters().size(), 26);
        std::array<juce::String, 12> names;
        int index = 0;
        bool complete = true;
        for (const auto* pair : {"ears", "front", "floor"})
            for (const auto* suffix : {"EqLowDb", "EqMidDb", "EqHighDb", "EqEnabled"})
            {
                const auto name = juce::String(pair) + suffix;
                names[index++] = name;
                auto* parameter = p.apvts.getParameter(name);
                expect(parameter != nullptr, name);
                if (parameter == nullptr)
                {
                    complete = false;
                    continue;
                }
                expect(parameter->isAutomatable());
                expectEquals(parameter->getVersionHint(), 2);
                expectWithinAbsoluteError(p.apvts.getRawParameterValue(name)->load(), 0.f, 1e-5f);
                if (!name.endsWith("Enabled"))
                {
                    const auto& r = parameter->getNormalisableRange();
                    expectEquals(r.start, -12.f);
                    expectEquals(r.end, 12.f);
                    expectWithinAbsoluteError(r.interval, 0.1f, 1e-6f);
                }
            }
        if (!complete)
            return;
        for (const auto* name :
             {"room", "inputDb", "outputDb", "mixPercent", "earsDb", "frontDb", "floorDb", "earsMute",
              "frontMute", "floorMute", "earsSolo", "frontSolo", "floorSolo", "bypass"})
            expectEquals(p.apvts.getParameter(name)->getVersionHint(), 1);
        beginTest("Host automation and user edits return each EQ band to an exact zero");
        for (size_t k = 0; k < names.size(); ++k)
        {
            if (k % 4 == 3)
                continue;
            auto& parameter = *p.apvts.getParameter(names[k]);
            const auto* raw = p.apvts.getRawParameterValue(names[k]);
            parameter.setValueNotifyingHost(parameter.convertTo0to1(12.f));
            parameter.setValueNotifyingHost(parameter.convertTo0to1(0.f));
            expectEquals(raw->load(), 0.f, names[k] + " host return to flat");
            p.setParameterFromUser(names[k], -12.f);
            p.setParameterFromUser(names[k], 0.f);
            expectEquals(raw->load(), 0.f, names[k] + " edit return to flat");
        }
        beginTest("All twelve store, duplicate and reset in one reversible transaction");
        std::array<float, 12> expected{};
        for (size_t k = 0; k < names.size(); ++k)
        {
            expected[k] = (k % 4 == 3) ? 1.f : (-11.2f + float(k));
            p.setParameterFromUser(names[k], expected[k]);
        }
        p.setParameterFromUser("room", 2.f);
        juce::MemoryBlock saved;
        p.getStateInformation(saved);
        LabRCProcessor restored;
        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        auto verify = [&](const LabRCProcessor& proc, const std::array<float, 12>& values)
        {
            for (size_t k = 0; k < names.size(); ++k)
                expectWithinAbsoluteError(proc.apvts.getRawParameterValue(names[k])->load(), values[k],
                                          0.001f, names[k]);
        };
        verify(restored, expected);
        restored.setParameterFromUser("room", 1.f);
        verify(restored, expected);
        restored.resetParameters();
        verify(restored, {});
        expect(restored.undoUser());
        verify(restored, expected);
        verify(p, expected);
        expect(restored.redoUser());
        verify(restored, {});
        beginTest("Legacy partial schemas explicitly restore flat/out even after a previous non-flat preset");
        for (const auto& xmlText :
             {juce::String("<PARAMETERS><PARAM id=\"inputDb\" value=\"7.3\"/></PARAMETERS>"),
              juce::String("<LABRC_STATE version=\"1\" micsVisible=\"0\"><PARAMETERS><PARAM id=\"inputDb\" "
                           "value=\"7.3\"/></PARAMETERS></LABRC_STATE>")})
        {
            restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
            auto xml = juce::XmlDocument::parse(xmlText);
            juce::MemoryBlock legacy;
            juce::AudioProcessor::copyXmlToBinary(*xml, legacy);
            restored.setStateInformation(legacy.getData(), static_cast<int>(legacy.getSize()));
            verify(restored, {});
            expectWithinAbsoluteError(restored.apvts.getRawParameterValue("inputDb")->load(), 7.3f, 1e-4f);
            expect(!restored.canUndoUser());
        }
        beginTest("A 0.1 editor width migrates once and a Final Touch editor width recalls exactly");
        auto restoreSize = [&](int width, int height)
        {
            auto xml = juce::XmlDocument::parse("<LABRC_STATE version='1' editorWidth='" +
                                                juce::String(width) + "' editorHeight='" +
                                                juce::String(height) + "'><PARAMETERS/></LABRC_STATE>");
            juce::MemoryBlock block;
            juce::AudioProcessor::copyXmlToBinary(*xml, block);
            restored.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
        };
        restoreSize(1180, 738);
        expect(restored.editorSize() == juce::Point<int>(1280, 854));
        restoreSize(800, 500);
        expect(restored.editorSize() == juce::Point<int>(1024, 684));
        restoreSize(1586, 992);
        expect(restored.editorSize() == juce::Point<int>(1586, 1058));
        restored.setEditorSize(1240, 827);
        restored.getStateInformation(saved);
        LabRCProcessor resized;
        resized.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        expect(resized.editorSize() == juce::Point<int>(1240, 827));
        beginTest(
            "The processor routes host EQ automation to an individual wet pair without adding user undo");
        for (int pair = 0; pair < 3; ++pair)
        {
            auto capture = [&](bool eq)
            {
                LabRCProcessor proc;
                auto set = [&](const juce::String& name, float v)
                {
                    auto* param = proc.apvts.getParameter(name);
                    param->setValueNotifyingHost(param->convertTo0to1(v));
                };
                const auto prefix = juce::String(pair == 0 ? "ears" : pair == 1 ? "front" : "floor");
                set(prefix + "Solo", 1);
                set(prefix + "Db", 0);
                set(prefix + "EqEnabled", eq ? 1.f : 0.f);
                set(prefix + "EqLowDb", 12);
                set(prefix + "EqMidDb", 12);
                set(prefix + "EqHighDb", 12);
                proc.setRateAndBufferSizeDetails(96000, 127);
                proc.prepareToPlay(96000, 127);
                expectEquals(proc.getLatencySamples(), 128);
                juce::AudioBuffer<float> b(2, 24000);
                b.clear();
                b.setSample(0, 0, 1);
                juce::MidiBuffer midi;
                proc.processBlock(b, midi);
                expect(!proc.canUndoUser());
                return b;
            };
            auto direct = capture(false), coloured = capture(true);
            for (int c = 0; c < 2; ++c)
                coloured.addFrom(c, 0, direct, c, 0, direct.getNumSamples(), -1.f);
            expect(coloured.getMagnitude(0, coloured.getNumSamples()) > 0.001f);
        }
    }
};
EQStateTests eqStateTests;

class ProcessorAudioTests final : public juce::UnitTest {
public:
    ProcessorAudioTests() : UnitTest("Processor audio integration", "Processor") {}
    void runTest() override {
        beginTest("Unprepared processor starts silent");
        LabRCProcessor p;
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> b(2,257);
        for(int c=0;c<2;++c) std::fill_n(b.getWritePointer(c),257,0.5f);
        p.processBlock(b,midi);
        expectEquals(b.getMagnitude(0,257),0.0f);

        beginTest("Mono dry duplicates at unity with trims and computational alignment");
        auto layout=p.getBusesLayout();
        layout.inputBuses.set(0,juce::AudioChannelSet::mono());
        expect(p.setBusesLayout(layout));
        auto set=[&p](const char* id,float actual) {
            auto* parameter=p.apvts.getParameter(id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(actual));
        };
        set("mixPercent",0);set("inputDb",6);set("outputDb",6);
        p.setRateAndBufferSizeDetails(96000,64);
        p.prepareToPlay(96000,64);
        const int delay=p.getLatencySamples();
        expect(delay>=0 && delay<257);
        b.clear();b.setSample(0,0,1);
        p.processBlock(b,midi);
        if(delay>=0 && delay<257) {
            expectWithinAbsoluteError(b.getSample(0,delay),3.981071706f,2e-5f);
            expectWithinAbsoluteError(b.getSample(1,delay),3.981071706f,2e-5f);
        }
        expect(p.getTailLengthSeconds()>=4.5+delay/96000.0);
        expect(!p.undoManager.canUndo());

        beginTest("Host bypass ignores trims after ramp and retains latency");
        juce::AudioBuffer<float> warm(2,2048);warm.clear();
        p.processBlockBypassed(warm,midi);
        b.clear();b.setSample(0,0,1);
        p.processBlockBypassed(b,midi);
        if(delay>=0 && delay<257) {
            expectWithinAbsoluteError(b.getSample(0,delay),1.f,2e-5f);
            expectWithinAbsoluteError(b.getSample(1,delay),1.f,2e-5f);
        }
        expectEquals(p.getLatencySamples(),delay);
        p.releaseResources();
        beginTest("Prepare release at another host rate resets history");
        p.setRateAndBufferSizeDetails(48000,32);
        p.prepareToPlay(48000,32);
        warm.clear();p.processBlock(warm,midi);
        expectEquals(warm.getMagnitude(0,warm.getNumSamples()),0.f);
        expect(p.getTailLengthSeconds()>=4.5+p.getLatencySamples()/48000.0);
        p.releaseResources();
    }
};
static ProcessorAudioTests processorAudioTests;
}
