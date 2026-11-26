// Private regression fixture. Build the same translation unit against each version's shared code.
#include "PluginProcessor.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <thread>
#if JUCE_MAC
#include <mach/mach.h>
#endif

namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
constexpr int rates[]{44100,48000,88200,96000,176400,192000};
constexpr int blocks[]{1,32,64,127,128,256,512,1024,2048,2801};
constexpr const char* pairs[]{"ears", "front", "floor"};
void require(bool ok,const std::string& error) {if(!ok) throw std::runtime_error(error);}
void set(LabRCProcessor& p,const juce::String& id,float v) {
    auto* param=p.apvts.getParameter(id); require(param!=nullptr,"Missing "+id.toStdString());
    param->setValueNotifyingHost(param->convertTo0to1(v));
}
void eqMode(LabRCProcessor& p,bool flat) {
    for (auto prefix:pairs) for (auto suffix:{"EqLowDb","EqMidDb","EqHighDb","EqEnabled"}) {
        const juce::String id=juce::String(prefix)+suffix;
        auto* raw=p.apvts.getRawParameterValue(id);
        if (!raw) {require(!flat,"EQ parameters missing from flat-on build");continue;}
        require(raw->load()==0.f,"Nonzero EQ default after construction/recall: "+id.toStdString());
        if (flat && juce::String(suffix)=="EqEnabled") set(p,id,1.f);
    }
}
void configure(LabRCProcessor& p,int rate,bool mono,int maxBlock=256) {
    auto buses=p.getBusesLayout(); buses.inputBuses.set(0,mono?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
    require(p.setBusesLayout(buses),"Layout rejected");
    p.setRateAndBufferSizeDetails(rate,maxBlock); p.prepareToPlay(rate,maxBlock);
    require(p.getLatencySamples()==128,"Latency changed");
}
float noise(std::uint32_t frame,int ch) {
    auto x=frame+1u+std::uint32_t(ch)*0x9e3779b9u;
    x ^= x>>16; x *= 0x7feb352du; x ^= x>>15; x *= 0x846ca68bu; x ^= x>>16;
    return float(int(x>>9)-4194304)/16777216.f;
}
enum class Input{left,right,mono,burst,continuous};
struct Writer {
    std::ofstream data,table;
    Writer(const fs::path& dir):data(dir/"samples.f32",std::ios::binary|std::ios::trunc),table(dir/"segments.tsv") {
        require(bool(data)&&bool(table),"Cannot open render outputs");
        table<<"name\trate\tframes\tlatency\toffset_bytes\n";
    }
    void segment(const std::string& name,int rate,int frames,int latency) {
        table<<name<<'\t'<<rate<<'\t'<<frames<<'\t'<<latency<<'\t'<<data.tellp()<<'\n';
    }
};
struct Stream {
    LabRCProcessor& p; int rate; juce::AudioBuffer<float> work{2,2801}; juce::MidiBuffer midi;
    std::array<float,5602> interleaved{};
    void range(Writer& w,int first,int count,Input kind,bool hostBypass=false) {
        int end=first+count, blockIndex=0;
        for (int pos=first;pos<end;) {
            const int n=std::min(end-pos,blocks[blockIndex++%std::size(blocks)]);
            float* const channels[]{work.getWritePointer(0),work.getWritePointer(1)};
            juce::AudioBuffer<float> view(channels,2,n);
            for (int c=0;c<2;++c) for (int j=0;j<n;++j) {
                const int t=pos+j;
                if (kind==Input::continuous || (kind==Input::burst && t<4096)) channels[c][j]=noise(t,c);
                else if(kind==Input::left || kind==Input::right || kind==Input::mono)
                    channels[c][j]=(t==0 && ((kind==Input::right)==(c==1)))?1.f:0.f;
                else channels[c][j]=0.f;
                if(kind==Input::mono && c==1) channels[c][j]=.375f; // Unused host channel is deliberately nonzero.
            }
            if(hostBypass) p.processBlockBypassed(view,midi); else p.processBlock(view,midi);
            for(int j=0;j<n;++j) for(int c=0;c<2;++c) {
                const float v=channels[c][j]; require(std::isfinite(v),"Nonfinite render"); interleaved[2*j+c]=v;
            }
            w.data.write(reinterpret_cast<const char*>(interleaved.data()),n*2*sizeof(float));
            require(bool(w.data),"Writing samples failed"); pos+=n;
        }
    }
};
void writeLegacy(const fs::path& path) {
    LabRCProcessor old;
    require(old.apvts.getParameter("earsEqEnabled")==nullptr,"Capture the legacy fixture using 0.1.0");
    set(old,"room",2);set(old,"inputDb",3.3f);set(old,"outputDb",-1.8f);set(old,"mixPercent",62.7f);
    set(old,"earsDb",-9.7f);set(old,"frontDb",0.5f);set(old,"floorDb",-12.2f);
    set(old,"frontSolo",1);set(old,"floorMute",1); old.setMicsVisible(false); old.setEditorSize(1280,800);
    juce::MemoryBlock blob; old.getStateInformation(blob); std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out.write(static_cast<const char*>(blob.getData()),static_cast<std::streamsize>(blob.getSize())); require(bool(out),"Cannot save legacy blob");
}
void render(const fs::path& dir,const fs::path& legacy,bool flat) {
    fs::create_directories(dir); Writer w(dir); std::ifstream file(legacy,std::ios::binary);
    std::vector<char> state((std::istreambuf_iterator<char>(file)),{}); require(!state.empty(),"Missing 0.1 state");
    for (int rate:rates) {
        for(int room=0;room<3;++room) for(int pair=0;pair<3;++pair) {
            for(auto kind:{Input::left,Input::right,Input::mono,Input::burst}) {
                LabRCProcessor p; eqMode(p,flat); set(p,"room",float(room));
                set(p,juce::String(pairs[pair])+"Db",0);set(p,juce::String(pairs[pair])+"Solo",1);
                configure(p,rate,kind==Input::mono); Stream audio{p,rate};
                const int frames=int(std::ceil(rate*4.5))+128+(kind==Input::burst?4096:1)+256;
                const char* names[]{"L","R","mono","stereo-burst"};
                w.segment(std::to_string(room)+"-"+pairs[pair]+"-"+names[static_cast<int>(kind)],rate,frames,p.getLatencySamples());
                audio.range(w,0,frames,kind);
            }
        }
        {
            LabRCProcessor p; configure(p,rate,false); p.setStateInformation(state.data(),static_cast<int>(state.size())); eqMode(p,flat);
            require(!p.micsVisible(),"Lost recalled MICS preference");
            constexpr struct {const char* id;float value;} recalled[]={{"room",2},{"inputDb",3.3f},{"outputDb",-1.8f},{"mixPercent",62.7f},{"earsDb",-9.7f},{"frontDb",.5f},{"floorDb",-12.2f},{"frontSolo",1},{"floorMute",1}};
            for(const auto& value:recalled) require(std::abs(p.apvts.getRawParameterValue(value.id)->load()-value.value)<.001f,std::string("Recall changed ")+value.id);
            Stream audio{p,rate}; const int step=rate/8;w.segment("legacy-recall-controls",rate,9*step,128);
            audio.range(w,0,step,Input::continuous);set(p,"earsSolo",1);set(p,"frontMute",1);
            audio.range(w,step,step,Input::continuous);set(p,"mixPercent",0);
            audio.range(w,2*step,step,Input::continuous);set(p,"mixPercent",100);set(p,"floorMute",0);set(p,"frontMute",0);set(p,"frontSolo",0);set(p,"earsSolo",0);
            audio.range(w,3*step,step,Input::continuous);set(p,"bypass",1);audio.range(w,4*step,step,Input::continuous);
            set(p,"bypass",0);audio.range(w,5*step,step,Input::continuous,true);audio.range(w,6*step,3*step,Input::continuous);
        }
        // Keep requests on fixed sample boundaries and give the production worker time to finish
        // while rendering is parked. Two independent repetitions detect nondeterministic timing.
        for (int repeat=0;repeat<2;++repeat) {
            LabRCProcessor p;eqMode(p,flat);configure(p,rate,false);Stream audio{p,rate};
            const int step=rate/4;w.segment("room-switch-"+std::to_string(repeat),rate,4*step,128);
            for(int i=0;i<4;++i) {
                if(i>0) {const int rooms[]{0,1,2,0};set(p,"room",float(rooms[i]));std::this_thread::sleep_for(std::chrono::milliseconds(750));}
                audio.range(w,i*step,step,Input::continuous);
            }
        }
        std::cout<<"Completed "<<rate<<" Hz\n"<<std::flush;
    }
}
void bench(int rate,int n,bool flat) {
    LabRCProcessor p;eqMode(p,flat);set(p,"room",2);configure(p,rate,false,n);
    juce::AudioBuffer<float> block(2,n);juce::MidiBuffer midi;std::vector<double> us;
    const int total=6*rate/n;us.reserve(total);double checksum=0;
    for(int b=0;b<total;++b){for(int c=0;c<2;++c)for(int j=0;j<n;++j)block.setSample(c,j,noise(b*n+j,c));
        auto start=Clock::now();p.processBlock(block,midi);auto end=Clock::now();
        us.push_back(std::chrono::duration<double,std::micro>(end-start).count());checksum+=block.getSample(0,n-1);}
    require(std::isfinite(checksum),"Nonfinite benchmark");std::sort(us.begin(),us.end());double rss=0;
#if JUCE_MAC
    mach_task_basic_info_data_t info{};mach_msg_type_number_t count=MACH_TASK_BASIC_INFO_COUNT;
    if(task_info(mach_task_self(),MACH_TASK_BASIC_INFO,reinterpret_cast<task_info_t>(&info),&count)==KERN_SUCCESS)rss=info.resident_size/1048576.;
#endif
    std::cout<<rate<<'\t'<<n<<'\t'<<total<<'\t'<<std::accumulate(us.begin(),us.end(),0.0)/total<<'\t'<<us[total*95/100]<<'\t'<<us[total*99/100]<<'\t'<<us.back()<<'\t'<<std::count_if(us.begin(),us.end(),[=](double t){return t>n*1e6/rate;})<<'\t'<<rss<<'\n';
}
}
int main(int argc,char** argv) {
    try {juce::ScopedJuceInitialiser_GUI init;
        if(argc==3 && std::string(argv[1])=="legacy") writeLegacy(argv[2]);
        else if(argc==5 && std::string(argv[1])=="render")render(argv[2],argv[3],std::string(argv[4])=="flat");
        else if(argc==5 && std::string(argv[1])=="bench")bench(std::stoi(argv[2]),std::stoi(argv[3]),std::string(argv[4])=="flat");
        else throw std::runtime_error("legacy FILE | render OUT_DIR LEGACY_FILE off|flat | bench RATE BLOCK off|flat");
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
