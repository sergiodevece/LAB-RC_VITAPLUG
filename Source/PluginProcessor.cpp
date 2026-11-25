#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Resources.h"
#include <cmath>
#include <cstdlib>

namespace {
constexpr std::array<const char*,14> ids{{"room","inputDb","outputDb","mixPercent","earsDb","frontDb","floorDb","earsMute","frontMute","floorMute","earsSolo","frontSolo","floorSolo","bypass"}};

class RoomChoice final : public juce::AudioParameterChoice {
public:
    explicit RoomChoice(labrc::RoomMixer& destination)
        : AudioParameterChoice(juce::ParameterID{"room",1},"Room",
              juce::StringArray{"Kentucky","Center Field","Live Room"},0), mixer(destination) {}
private:
    // JUCE invokes this synchronously on every parameter set, even with the transport stopped.
    void valueChanged(int room) override { mixer.requestRoom(room); }
    labrc::RoomMixer& mixer;
};

class ParameterChange final : public juce::UndoableAction {
public:
    ParameterChange(juce::RangedAudioParameter& p,float before,float after,bool gestureOpen)
        : parameter(p),from(before),to(after),skipFirstGesture(gestureOpen) {}
    bool perform() override {
        apply(to,!skipFirstGesture);skipFirstGesture=false;return true;
    }
    bool undo() override { apply(from,true);return true; }
    juce::UndoableAction* createCoalescedAction(juce::UndoableAction* next) override {
        if(auto* change=dynamic_cast<ParameterChange*>(next); change && &change->parameter==&parameter)
            return new ParameterChange(parameter,from,change->to,false);
        return nullptr;
    }
private:
    void apply(float value,bool gesture) {
        if(gesture) parameter.beginChangeGesture();
        parameter.setValueNotifyingHost(value);
        if(gesture) parameter.endChangeGesture();
    }
    juce::RangedAudioParameter& parameter;
    const float from,to;
    bool skipFirstGesture;
};

class MicsChange final : public juce::UndoableAction {
public:
    MicsChange(std::atomic<bool>& value,bool next) : target(value),from(value.load()),to(next) {}
    bool perform() override {target.store(to);return true;}
    bool undo() override {target.store(from);return true;}
private:
    std::atomic<bool>& target;
    const bool from,to;
};

double number(const juce::var& value,double fallback) {
    const auto text=value.toString().trim();
    if(text.isEmpty()) return fallback;
    const auto* start=text.toRawUTF8();char* end=nullptr;
    const double parsed=std::strtod(start,&end);
    return end!=start && end && *end=='\0' && std::isfinite(parsed) ? parsed : fallback;
}
}
juce::AudioProcessorValueTreeState::ParameterLayout LabRCProcessor::makeLayout(labrc::RoomMixer& mixer) {
    using ID=juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<RoomChoice>(mixer));
    l.add(std::make_unique<juce::AudioParameterFloat>(ID{"inputDb",1},"Input",juce::NormalisableRange<float>{-18,18,0.1f},0));
    l.add(std::make_unique<juce::AudioParameterFloat>(ID{"outputDb",1},"Output",juce::NormalisableRange<float>{-18,18,0.1f},0));
    l.add(std::make_unique<juce::AudioParameterFloat>(ID{"mixPercent",1},"Mix",juce::NormalisableRange<float>{0,100,0.1f},100));
    constexpr std::array<float,3> initial{{-6,-3,-9}};
    for(size_t i=0;i<3;++i)
        l.add(std::make_unique<juce::AudioParameterFloat>(ID{ids[4+i],1},ids[4+i],juce::NormalisableRange<float>{-60,6,0.1f},initial[i]));
    for(size_t i=7;i<14;++i)
        l.add(std::make_unique<juce::AudioParameterBool>(ID{ids[i],1},ids[i],false));
    return l;
}
LabRCProcessor::LabRCProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      apvts(*this,nullptr,"PARAMETERS",makeLayout(mixer)) {
    for(size_t i=0;i<raw.size();++i) raw[i]=apvts.getRawParameterValue(ids[i]);
}
LabRCProcessor::~LabRCProcessor() {
    undoManager.clearUndoHistory();
}
void LabRCProcessor::prepareToPlay(double sampleRate,int maximumBlock) {
    const juce::ScopedLock audioGuard(getCallbackLock()), stateGuard(userStateLock);
    prepared=false;
    audioStarted.store(false,std::memory_order_release);
    preparedRate=sampleRate; preparedBlock=maximumBlock; initialRoom=readParameters().room;
    mixer.prepare(sampleRate,maximumBlock,initialRoom,labrc::loadMatrixIR,true);
    setLatencySamples(mixer.latencySamples());
    prepared=true;
}
void LabRCProcessor::releaseResources() {
    const juce::ScopedLock audioGuard(getCallbackLock()), stateGuard(userStateLock);
    prepared=false;
    audioStarted.store(false,std::memory_order_release);
    mixer.release();
}
labrc::Parameters LabRCProcessor::readParameters() const noexcept {
    const auto load=[this](size_t i) {return raw[i]->load(std::memory_order_relaxed);};
    labrc::Parameters p;
    const float selected=load(0);
    p.room=std::isfinite(selected)?juce::roundToInt(juce::jlimit(0.f,2.f,selected)):0;
    p.inputDb=load(1);p.outputDb=load(2);p.mixPercent=load(3);
    for(size_t i=0;i<3;++i) {
        p.pairDb[i]=load(4+i);p.mute[i]=load(7+i)>=0.5f;p.solo[i]=load(10+i)>=0.5f;
    }
    p.bypass=load(13)>=0.5f;
    return p;
}
void LabRCProcessor::process(juce::AudioBuffer<float>& b,bool hostBypassed) noexcept {
    juce::ScopedNoDenormals noDenormals;
    if(!prepared || b.getNumChannels()<2) {b.clear();return;}
    if(b.getNumSamples()==0) return;
    audioStarted.store(true,std::memory_order_release);
    mixer.process(b.getArrayOfWritePointers(),getTotalNumInputChannels(),b.getNumSamples(),readParameters(),hostBypassed);
}
void LabRCProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) {process(b,false);}
void LabRCProcessor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer&) {process(b,true);}
bool LabRCProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo() &&
        (b.getMainInputChannelSet()==juce::AudioChannelSet::mono() || b.getMainInputChannelSet()==juce::AudioChannelSet::stereo());
}
juce::AudioProcessorParameter* LabRCProcessor::getBypassParameter() const { return apvts.getParameter("bypass"); }
juce::AudioProcessorEditor* LabRCProcessor::createEditor() { return new LabRCEditor(*this); }
double LabRCProcessor::getTailLengthSeconds() const {
    return std::max(mixer.tailSeconds(),4.5+getLatencySamples()/std::max(1.0,getSampleRate()));
}
labrc::MeterValues LabRCProcessor::meterSnapshot() const noexcept { return mixer.meterSnapshot(); }
bool LabRCProcessor::micsVisible() const noexcept { return showMics.load(); }
void LabRCProcessor::closeActiveGesture() {
    if(auto* parameter=apvts.getParameter(activeUserParameter)) parameter->endChangeGesture();
    activeUserParameter.clear();
}
void LabRCProcessor::reconcileUndo() {
    if(!undoResetPending) return;
    undoResetPending=false;
    if(activeUserParameter.isNotEmpty()) {
        cancelledDrag=activeUserParameter;
        closeActiveGesture();
    }
    undoManager.clearUndoHistory();
}
bool LabRCProcessor::canUndoUser() {
    const juce::ScopedLock guard(userStateLock);
    reconcileUndo(); return undoManager.canUndo();
}
bool LabRCProcessor::canRedoUser() {
    const juce::ScopedLock guard(userStateLock);
    reconcileUndo(); return undoManager.canRedo();
}
bool LabRCProcessor::undoUser() {
    const juce::ScopedLock guard(userStateLock);
    reconcileUndo();
    if(activeUserParameter.isNotEmpty()) { cancelledDrag=activeUserParameter; closeActiveGesture(); }
    return undoManager.undo();
}
bool LabRCProcessor::redoUser() {
    const juce::ScopedLock guard(userStateLock);
    reconcileUndo();
    if(activeUserParameter.isNotEmpty()) { cancelledDrag=activeUserParameter; closeActiveGesture(); }
    return undoManager.redo();
}
void LabRCProcessor::setMicsVisible(bool visible) {
    const juce::ScopedLock guard(userStateLock);
    reconcileUndo();
    if(visible==showMics.load()) return;
    beginUserGesture("Microphone markers");
    undoManager.perform(new MicsChange(showMics,visible));
}
void LabRCProcessor::setEditorSize(int width,int height) {
    const int w=juce::jlimit(800,1586,width);
    int h=juce::jlimit(500,992,height);
    if(std::abs(static_cast<double>(w)/h-1586.0/992.0)>0.015)
        h=juce::jlimit(500,992,juce::roundToInt(w*992.0/1586.0));
    viewSize.store((static_cast<std::uint64_t>(h)<<32u)|static_cast<std::uint32_t>(w));
}
juce::Point<int> LabRCProcessor::editorSize() const {
    const auto packed=viewSize.load();
    return {static_cast<int>(packed&0xffffffffu),static_cast<int>(packed>>32u)};
}
void LabRCProcessor::resetParameters() {
    const juce::ScopedLock guard(userStateLock);
    reconcileUndo();
    if(activeUserParameter.isNotEmpty()) endParameterGesture(activeUserParameter);
    beginUserGesture("Reset LAB-RC");
    for(const auto* id:ids) {
        auto& parameter=*apvts.getParameter(id);
        if(parameter.getValue()!=parameter.getDefaultValue())
            undoManager.perform(new ParameterChange(parameter,parameter.getValue(),parameter.getDefaultValue(),false));
    }
    if(!showMics.load()) undoManager.perform(new MicsChange(showMics,true));
}
void LabRCProcessor::beginUserGesture(const juce::String& name) {
    const juce::ScopedLock guard(userStateLock);
    reconcileUndo();
    undoManager.beginNewTransaction(name);
}
void LabRCProcessor::beginParameterGesture(const juce::String& id,const juce::String& name) {
    const juce::ScopedLock guard(userStateLock);
    reconcileUndo();
    if(activeUserParameter.isNotEmpty()) endParameterGesture(activeUserParameter);
    cancelledDrag.clear();
    if(auto* parameter=apvts.getParameter(id)) {
        beginUserGesture(name.isNotEmpty()?name:parameter->getName(128));
        activeUserParameter=id;
        parameter->beginChangeGesture();
    }
}
void LabRCProcessor::setParameterFromUser(const juce::String& id,float actualValue) {
    const juce::ScopedLock guard(userStateLock);
    reconcileUndo();
    if(cancelledDrag==id || !std::isfinite(actualValue)) return;
    if(auto* parameter=apvts.getParameter(id)) {
        const float next=parameter->convertTo0to1(actualValue);
        if(next==parameter->getValue()) return;
        const bool open=activeUserParameter==id;
        if(!open) beginUserGesture(parameter->getName(128));
        undoManager.perform(new ParameterChange(*parameter,parameter->getValue(),next,open));
    }
}
void LabRCProcessor::endParameterGesture(const juce::String& id) {
    const juce::ScopedLock guard(userStateLock);
    reconcileUndo();
    if(cancelledDrag==id) cancelledDrag.clear();
    if(activeUserParameter==id && id.isNotEmpty()) closeActiveGesture();
}
void LabRCProcessor::getStateInformation(juce::MemoryBlock& block) {
    const juce::ScopedLock guard(userStateLock);
    juce::ValueTree state("LABRC_STATE");
    state.setProperty("version",1,nullptr);
    state.setProperty("micsVisible",showMics.load(),nullptr);
    const auto size=editorSize();
    state.setProperty("editorWidth",size.x,nullptr);
    state.setProperty("editorHeight",size.y,nullptr);
    state.addChild(apvts.copyState(),-1,nullptr);
    if(auto xml=state.createXml()) copyXmlToBinary(*xml,block);
}
void LabRCProcessor::setStateInformation(const void* data,int bytes) {
    if(data==nullptr || bytes<=0) return;
    const auto xml=getXmlFromBinary(data,bytes);
    if(!xml) return;
    const auto saved=juce::ValueTree::fromXml(*xml);
    const bool envelope=saved.hasType("LABRC_STATE");
    if(!envelope && !saved.hasType("PARAMETERS")) return;
    if(envelope && (saved.getProperty("version").toString().trim()!="1" || saved.getNumChildren()!=1)) return;
    const auto parameters=envelope?saved.getChild(0):saved;
    if(!parameters.hasType("PARAMETERS")) return;
    for(const auto& child:parameters) if(!child.hasType("PARAM")) return;
    juce::ValueTree clean("PARAMETERS");
    for(const auto* id:ids) {
        const auto& parameter=*apvts.getParameter(id);
        const double fallback=parameter.convertFrom0to1(parameter.getDefaultValue());
        auto node=parameters.getChildWithProperty("id",id);
        const double actual=number(node.getProperty("value"),fallback);
        const auto& range=parameter.getNormalisableRange();
        const auto bounded=static_cast<float>(juce::jlimit(static_cast<double>(range.start),static_cast<double>(range.end),actual));
        const float clamped=parameter.convertFrom0to1(parameter.convertTo0to1(bounded));
        juce::ValueTree entry("PARAM");
        entry.setProperty("id",id,nullptr);entry.setProperty("value",clamped,nullptr);
        clean.addChild(entry,-1,nullptr);
    }
    const auto apply=[&] {
        apvts.replaceState(clean);
        showMics.store(envelope?number(saved.getProperty("micsVisible"),1.0)>=0.5:true);
        const auto width=envelope?number(saved.getProperty("editorWidth"),1180):1180;
        const auto height=envelope?number(saved.getProperty("editorHeight"),738):738;
        setEditorSize(static_cast<int>(juce::jlimit(800.0,1586.0,width)),static_cast<int>(juce::jlimit(500.0,992.0,height)));
        undoResetPending=true;
        const auto* mm=juce::MessageManager::getInstanceWithoutCreating();
        if(mm==nullptr || mm->isThisTheMessageThread()) reconcileUndo();
    };
    {
        const juce::ScopedLock stateGuard(userStateLock);
        if(audioStarted.load(std::memory_order_acquire)) {
            apply(); // Normal live recall. The worker services the new room with the existing crossfade.
            return;
        }
    }
    // The AU/VST/standalone wrappers serialise process against this same callback lock.
    // Take it only on the cold/control path, then recheck in case the first block won.
    const juce::ScopedLock audioGuard(getCallbackLock()), stateGuard(userStateLock);
    const auto room=static_cast<int>(clean.getChildWithProperty("id","room").getProperty("value"));
    if(prepared && !audioStarted.load(std::memory_order_acquire) && room!=initialRoom) {
        prepared=false;
        mixer.prepare(preparedRate,preparedBlock,room,labrc::loadMatrixIR,true);
        initialRoom=room; prepared=true;
    }
    apply();

}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LabRCProcessor(); }
