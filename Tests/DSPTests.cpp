#include "DSP/RoomMixer.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <complex>
#include <numeric>
#if JUCE_MAC
#include <mach/mach.h>
#include <dlfcn.h>
#endif
#include <random>
#include <stdexcept>

namespace
{
using Buffer = juce::AudioBuffer<float>;
using labrc::Parameters;
using labrc::RoomMixer;

Buffer loadAsset(int room, int pair, bool monoReference = false)
{
    const char* rooms[] = {"kentucky", "center_field", "live_room"};
    const char* pairs[] = {"EARS", "FRONT", "FLOOR"};
    const auto file = juce::File(juce::String::fromUTF8(LABRC_KIT_ROOT))
                          .getChildFile("assets/ir")
                          .getChildFile(monoReference ? "v05" : "v06")
                          .getChildFile(juce::String(rooms[room]) + "_" + pairs[pair] +
                                        (monoReference ? "_wet.wav" : "_wet_matrix.wav"));
    juce::WavAudioFormat wav;
    auto stream = file.createInputStream();
    if (!stream)
        throw std::runtime_error("Cannot open " + file.getFullPathName().toStdString());
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(stream.release(), true));
    if (reader == nullptr)
        throw std::runtime_error("Missing WAV: " + file.getFullPathName().toStdString());
    Buffer ir(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&ir, 0, ir.getNumSamples(), 0, true, true))
        throw std::runtime_error("Cannot read WAV");
    return ir;
}

constexpr std::array<int, 10> blocks{{1, 32, 64, 127, 128, 256, 512, 1024, 2048, 2801}};

void render(RoomMixer& mixer, Buffer& buffer, int inputs, const Parameters& p, bool bypass = false)
{
    int pos = 0, index = 0;
    while (pos < buffer.getNumSamples())
    {
        const int n = std::min(blocks[index++ % blocks.size()], buffer.getNumSamples() - pos);
        float* data[] = {buffer.getWritePointer(0, pos), buffer.getWritePointer(1, pos)};
        mixer.process(data, inputs, n, p, bypass);
        pos += n;
    }
}

class DspTests : public juce::UnitTest
{
  public:
    DspTests() : UnitTest("Room matrices, controls and scheduling", "DSP") {}
    void runTest() override
    {
        beginTest("Room request during idle prepares the selected bank on the worker without a callback");
        {
            std::array<std::atomic<int>,3> loaded {};
            RoomMixer idle;
            idle.prepare(96000.,64,0,[&](int room,int)
            {
                Buffer ir(4,1); ir.clear();
                ++loaded[room];
                return ir;
            });
            for (int target : {1, 2}) {
                idle.requestRoom(target);
                for(int i=0;i<100 && loaded[target].load()!=3;++i) juce::Thread::sleep(2);
                expectEquals(loaded[target].load(),3);
                juce::Thread::sleep(5); // Give the first preloaded bank time to publish while idle.
            }
        }
        beginTest("The final audio callback with an old room snapshot cannot cancel an external idle request");
        {
            // Hold the worker in a previous load, publish the final selection, then run the stale
            // callback and stop audio. All ordering is explicit; timeouts only bound failed runs.
            std::mutex lock;
            std::condition_variable changed;
            bool inPreviousLoad = false, releasePrevious = false, finalRoomLoaded = false;
            RoomMixer external;
            external.prepare(96000., 64, 0, [&](int room, int pair) {
                std::unique_lock<std::mutex> guard(lock);
                if (room == 1 && pair == 0) {
                    inPreviousLoad = true;
                    changed.notify_one();
                    changed.wait(guard, [&] { return releasePrevious; });
                }
                if (room == 2 && pair == 2) {
                    finalRoomLoaded = true;
                    changed.notify_one();
                }
                Buffer identity(4, 1);
                identity.clear();
                identity.setSample(0, 0, static_cast<float>(room + 1));
                identity.setSample(3, 0, static_cast<float>(room + 1));
                return identity;
            }, true);
            external.requestRoom(1);
            {
                std::unique_lock<std::mutex> guard(lock);
                expect(changed.wait_for(guard, std::chrono::seconds(2), [&] { return inPreviousLoad; }));
            }
            Parameters stale;
            stale.room = 0;
            stale.pairDb.fill(0.f);
            stale.solo[0] = true;
            external.requestRoom(2);
            Buffer block(2, 64);
            block.clear();
            external.process(block.getArrayOfWritePointers(), 2, 64, stale);
            {
                std::unique_lock<std::mutex> guard(lock);
                releasePrevious = true;
                changed.notify_one();
                expect(changed.wait_for(guard, std::chrono::seconds(2), [&] { return finalRoomLoaded; }),
                       "Final room must reach the worker before audio resumes");
            }
            float finalLevel = 0.f;
            const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            do {
                for (int c = 0; c < 2; ++c)
                    std::fill_n(block.getWritePointer(c), 64, 1.f);
                external.process(block.getArrayOfWritePointers(), 2, 64, stale);
                finalLevel = block.getSample(1, 63);
                std::this_thread::yield();
            } while (std::abs(finalLevel - 3.f) > 2e-5f && std::chrono::steady_clock::now() < until);
            expectWithinAbsoluteError(finalLevel, 3.f, 2e-5f);
        }
        beginTest("Two independent stereo inputs sum through four distinct FIRs");
        RoomMixer mixer;
        auto fixture = [](int, int)
        {
            Buffer ir(4, 7);
            ir.clear();
            for (int c = 0; c < 4; ++c)
                ir.setSample(c, c + 1, 0.25f * (c + 1));
            return ir;
        };
        Parameters p;
        p.pairDb.fill(0.f);
        p.solo[0] = true;
        mixer.prepare(96000, 64, 0, fixture);
        const int delay = mixer.latencySamples();
        Buffer block(2, 4096 + delay);
        block.clear();
        std::mt19937 gen(1234);
        std::uniform_real_distribution<float> noise(-0.5f, 0.5f);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 4096; ++i)
                block.setSample(c, i, noise(gen));
        Buffer input(block);
        render(mixer, block, 2, p);
        float error = 0;
        for (int i = 4; i < 4096; ++i)
        {
            const float l = input.getSample(0, i - 1) * 0.25f + input.getSample(1, i - 3) * 0.75f;
            const float r = input.getSample(0, i - 2) * 0.5f + input.getSample(1, i - 4);
            error = std::max({error, std::abs(l - block.getSample(0, i + delay)),
                              std::abs(r - block.getSample(1, i + delay))});
        }
        expect(error < 2e-5f, "Maximum stereo FIR error " + juce::String(error, 8));
        expect(mixer.tailSeconds() >= 4.5 + delay / 96000.0);
        beginTest("L-only, R-only and mono traverse every sample of all nine approved matrices");
        float maxAssetError = 0.f;
        for (int room = 0; room < 3; ++room)
            for (int pair = 0; pair < 3; ++pair)
            {
                auto ir = loadAsset(room, pair);
                auto mono = loadAsset(room, pair, true);
                for (int mode = 0; mode < 3; ++mode)
                {
                    // Isolate the pair through the production controls; silent fixtures avoid
                    // repeatedly preparing the other two multi-second kernels.
                    mixer.prepare(96000, 127, room,
                                  [&](int, int index)
                                  {
                                      if (index == pair)
                                          return Buffer(ir);
                                      Buffer silent(4, 1);
                                      silent.clear();
                                      return silent;
                                  });
                    const int latency = mixer.latencySamples();
                    Buffer out(2, ir.getNumSamples() + latency + 256);
                    out.clear();
                    out.setSample(mode == 1 ? 1 : 0, 0, 1.f);
                    p.room = room;
                    p.solo.fill(false);
                    p.solo[pair] = true;
                    render(mixer, out, mode == 2 ? 1 : 2, p);
                    float worst = 0;
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < out.getNumSamples(); ++i)
                        {
                            const int t = i - latency;
                            const float ref =
                                t < 0 || t >= ir.getNumSamples()
                                    ? 0.f
                                    : (mode == 2 ? mono.getSample(c, t) : ir.getSample(2 * mode + c, t));
                            worst = std::max(worst, std::abs(ref - out.getSample(c, i)));
                        }
                    maxAssetError = std::max(maxAssetError, worst);
                    expect(worst <= 2e-5f, juce::String("room/pair/mode ") + juce::String(room) + "/" +
                                               juce::String(pair) + "/" + juce::String(mode) + " error " +
                                               juce::String(worst, 9));
                }
            }
        logMessage("Maximum sample error (L/R/mono, all 9 pairs): " + juce::String(maxAssetError, 10));
        beginTest("All 64 solo/mute combinations preserve independent pair gains");
        auto identities = [](int room, int pair)
        {
            Buffer impulse(4, 1);
            impulse.clear();
            for (int c : {0, 3})
                impulse.setSample(c, 0, float((room + 1) * (1 << pair)));
            return impulse;
        };
        mixer.prepare(96000, 64, 0, identities);
        p = {};
        p.pairDb.fill(0.f);
        for (int solo = 0; solo < 8; ++solo)
            for (int mute = 0; mute < 8; ++mute)
            {
                for (int i = 0; i < 3; ++i)
                {
                    p.solo[i] = (solo & (1 << i)) != 0;
                    p.mute[i] = (mute & (1 << i)) != 0;
                }
                Buffer ones(2, 3072);
                for (int c = 0; c < 2; ++c)
                    std::fill_n(ones.getWritePointer(c), 3072, 1.f);
                render(mixer, ones, 2, p);
                const float expected = float((solo == 0 ? 7 : solo) & (~mute & 7));
                expectWithinAbsoluteError(ones.getSample(1, 3071), expected, 2e-5f);
                mixer.meterSnapshot();
                for (int c = 0; c < 2; ++c)
                    std::fill_n(ones.getWritePointer(c), 3072, 1.f);
                render(mixer, ones, 2, p);
                expectWithinAbsoluteError(mixer.meterSnapshot().output[1], expected, 2e-5f);
            }
        beginTest(
            "Dry routing respects trims; bypass ramps to original unity; peak above full scale survives");
        p = {};
        p.mixPercent = 0;
        p.inputDb = 6;
        p.outputDb = 6;
        mixer.prepare(96000, 64, 0, identities);
        auto constant = [&](int n, bool hostBypass = false)
        {
            Buffer b(2, n);
            for (int c = 0; c < 2; ++c)
                std::fill_n(b.getWritePointer(c), n, 1.f);
            render(mixer, b, 2, p, hostBypass);
            return b;
        };
        const float trimmed = std::pow(10.f, 12.f / 20.f);
        auto dry = constant(3000);
        expectWithinAbsoluteError(dry.getSample(0, 2999), trimmed, 2e-5f);
        auto fade = constant(2000, true);
        expectWithinAbsoluteError(fade.getSample(0, 959), (trimmed + 1) * 0.5f, 2e-4f);
        expectWithinAbsoluteError(fade.getSample(0, 1919), 1.f, 2e-5f);
        p.bypass = true;
        auto bypass = constant(32);
        expectWithinAbsoluteError(bypass.getSample(1, 31), 1.f, 2e-5f);
        p.bypass = false;
        p.mixPercent = 100;
        p.inputDb = p.outputDb = 0;
        p.solo[0] = true;
        p.pairDb[0] = -60;
        auto floor = constant(4096);
        expectWithinAbsoluteError(floor.getSample(0, 4095), 0.001f, 0.000001f);
        beginTest("Nonfinite samples and controls leave following buffers usable");
        p.inputDb = std::numeric_limits<float>::quiet_NaN();
        p.mixPercent = std::numeric_limits<float>::infinity();
        Buffer poison(2, 256);
        poison.clear();
        poison.setSample(0, 0, p.inputDb);
        poison.setSample(1, 0, p.mixPercent);
        render(mixer, poison, 2, p);
        auto clean = constant(4096);
        expect(std::isfinite(clean.getSample(0, 4095)) && std::isfinite(clean.getSample(1, 4095)));
        beginTest("Room requests keep previous output until new bank is ready and latest room wins");
        p = {};
        p.pairDb.fill(0.f);
        p.solo[0] = true;
        mixer.prepare(96000, 64, 0, identities);
        constant(1024);
        p.room = 1;
        constant(31);
        p.room = 2;
        float previous = 1.f, largestStep = 0.f;
        for (int i = 0; i < 1500 && previous < 2.99999f; ++i)
        {
            auto data = constant(128);
            for (int k = 0; k < 128; ++k)
            {
                const float v = data.getSample(0, k);
                largestStep = std::max(largestStep, std::abs(v - previous));
                previous = v;
            }
            juce::Thread::sleep(1);
        }
        expectWithinAbsoluteError(previous, 3.f, 2e-5f);
        expect(largestStep <= 2.f / 4800.f + 2e-5f, "Largest fade step " + juce::String(largestStep, 7));
        beginTest("Fader, mute, solo, trims and mix ramp at sample resolution; excluded tail remains hot");
        p = {};
        p.pairDb.fill(0.f);
        p.solo[0] = true;
        mixer.prepare(96000., 32, 0, identities);
        constant(2048);
        p.mute[0] = true;
        auto muted = constant(1920);
        expectWithinAbsoluteError(muted.getSample(0, 959), 0.5f, 0.0001f);
        expectWithinAbsoluteError(muted.getSample(0, 1919), 0.f, 2e-5f);
        p.mute[0] = false;
        constant(1920);
        p.solo[1] = true;
        auto twoSolos = constant(1920);
        expectWithinAbsoluteError(twoSolos.getSample(0, 959), 2.f, 0.0001f);
        auto meter = mixer.meterSnapshot();
        expectWithinAbsoluteError(meter.pairs[0][0], 1.f, 2e-5f);
        expectWithinAbsoluteError(meter.pairs[1][0], 2.f, 2e-5f);
        expectWithinAbsoluteError(meter.pairs[2][1], 0.f, 2e-5f);
        p.pairDb[1] = -6.0206f;
        auto fader = constant(1920);
        expectWithinAbsoluteError(fader.getSample(0, 959), 2.5f, 0.0002f);
        p.mixPercent = 0;
        auto blend = constant(1920);
        expectWithinAbsoluteError(blend.getSample(0, 959), 1.5f, 0.0002f);
        p.inputDb = 6.0206f;
        auto inputTrim = constant(1920 + mixer.latencySamples());
        expectWithinAbsoluteError(inputTrim.getSample(0, 959 + mixer.latencySamples()), 1.5f, 0.0002f);
        p.outputDb = -6.0206f;
        auto outputTrim = constant(1920);
        expectWithinAbsoluteError(outputTrim.getSample(0, 959), 1.5f, 0.0002f);
        constexpr int lateTap = 6144;
        mixer.prepare(96000., 64, 0,
                      [](int, int)
                      {
                          Buffer ir(4, lateTap + 1);
                          ir.clear();
                          ir.setSample(0, lateTap, 0.75f);
                          ir.setSample(3, lateTap, -0.25f);
                          return ir;
                      });
        p = {};
        p.pairDb.fill(0.f);
        p.solo[0] = true;
        p.mute[0] = true;
        p.bypass = true;
        Buffer hit(2, 2048);
        hit.clear();
        hit.setSample(0, 0, 1.f);
        render(mixer, hit, 1, p);
        p.mute[0] = false;
        p.bypass = false;
        Buffer tail(2, lateTap);
        tail.clear();
        render(mixer, tail, 2, p);
        expectWithinAbsoluteError(tail.getSample(0, lateTap - 2048 + mixer.latencySamples()), 0.75f, 2e-5f);
        expectWithinAbsoluteError(tail.getSample(1, lateTap - 2048 + mixer.latencySamples()), -0.25f, 2e-5f);
        beginTest("Multiple requests during an audible transition converge on the last selection");
        mixer.prepare(96000., 32, 0, identities);
        p = {};
        p.pairDb.fill(0.f);
        p.solo[0] = true;
        constant(1024);
        p.room = 1;
        float first = 1.f;
        for (int tick = 0; tick < 1000 && first < 1.1f; ++tick)
        {
            first = constant(64).getSample(0, 63);
            juce::Thread::sleep(1);
        }
        expect(first >= 1.1f && first < 1.2f);
        p.room = 2;
        constant(64);
        p.room = 0;
        constant(128);
        p.room = 2;
        float last = first;
        for (int tick = 0; tick < 1500 && last < 2.99999f; ++tick)
        {
            last = constant(128).getSample(0, 127);
            juce::Thread::sleep(1);
        }
        expectWithinAbsoluteError(last, 3.f, 2e-5f);
        beginTest("A sub-frame over is retained across silent callbacks until read by the UI");
        mixer.prepare(96000., 32, 0, identities);
        p = {};
        p.solo[0] = true;
        p.pairDb.fill(0.f);
        Buffer over(2, 512);
        over.clear();
        over.setSample(0, 0, 1.75f);
        render(mixer, over, 1, p);
        Buffer silence(2, 4096);
        silence.clear();
        render(mixer, silence, 2, p);
        const auto held = mixer.meterSnapshot();
        expectWithinAbsoluteError(held.input[0], 1.75f, 2e-5f);
        expectWithinAbsoluteError(held.output[1], 1.75f, 2e-5f);
        expectWithinAbsoluteError(held.pairs[0][1], 1.75f, 2e-5f);
        expectWithinAbsoluteError(mixer.meterSnapshot().output[1], 0.f, 1e-7f);
        beginTest("A failed off-thread IR load preserves the sounding room and permits another selection");
        std::atomic<bool> attempted{false};
        mixer.prepare(96000., 32, 0,
                      [&](int room, int pair)
                      {
                          if (room == 1)
                          {
                              attempted.store(true);
                              throw std::runtime_error("Test provider rejected room");
                          }
                          return identities(room, pair);
                      });
        p.room = 0;
        constant(1024);
        p.room = 1;
        float stillPlaying = 0.f;
        for (int k = 0; k < 1000 && !attempted.load(); ++k)
        {
            stillPlaying = constant(128).getSample(0, 127);
            juce::Thread::sleep(1);
        }
        expect(attempted.load());
        expectWithinAbsoluteError(stillPlaying, 1.f, 2e-5f);
        p.room = 2;
        for (int k = 0; k < 1000 && stillPlaying < 2.99999f; ++k)
        {
            stillPlaying = constant(128).getSample(0, 127);
            juce::Thread::sleep(1);
        }
        expectWithinAbsoluteError(stillPlaying, 3.f, 2e-5f);
        beginTest("Transfer magnitude and phase at all host rates, including delayed high-frequency content");
        for (double sr : {44100., 48000., 88200., 96000., 176400., 192000.})
        {
            // A narrow Gaussian burst keeps its analytical continuous-time shape under resampling.
            auto gaussian = [](double t)
            {
                const double x = (t - 0.003) / 0.00016;
                return std::exp(-0.5 * x * x) *
                       std::cos(2 * juce::MathConstants<double>::pi * 7300 * (t - 0.003));
            };
            mixer.prepare(sr, 32, 0,
                          [&](int, int)
                          {
                              Buffer ir(4, 641);
                              ir.clear();
                              for (int i = 0; i < 641; ++i)
                                  ir.setSample(0, i, float(gaussian(i / 96000.)));
                              return ir;
                          });
            p = {};
            p.pairDb.fill(0.f);
            p.solo[0] = true;
            const int n = int(sr * 0.013) + mixer.latencySamples();
            Buffer b(2, n);
            b.clear();
            b.setSample(0, 0, 0.25f);
            b.setSample(0, 13, -0.42f);
            b.setSample(0, 29, 0.17f);
            render(mixer, b, 2, p);
            double worst = 0;
            for (int i = mixer.latencySamples(); i < n; ++i)
            {
                const double j = i - mixer.latencySamples();
                const double reference = (0.25 * gaussian(j / sr) - 0.42 * gaussian((j - 13) / sr) +
                                          0.17 * gaussian((j - 29) / sr)) *
                                         96000. / sr;
                worst = std::max(worst, std::abs(b.getSample(0, i) - reference));
            }
            expect(worst < 2e-5, "rate " + juce::String(sr) + " error " + juce::String(worst, 9));
        }
        beginTest(
            "Broadband stereo random burst vs double-precision time-domain convolution at every sample rate");
        for (double sr : {44100., 48000., 88200., 96000., 176400., 192000.})
        {
            auto component = [](int route, double t, double frequency)
            {
                const double centre = 0.0021 + 0.00023 * route;
                const double delta = t - centre, width = 0.00021;
                return 0.19 * (route + 1) * std::exp(-0.5 * delta * delta / (width * width)) *
                       std::cos(2 * juce::MathConstants<double>::pi * frequency * delta + 0.37 * route);
            };
            constexpr int irLength = 613;
            auto ideal = [&](int route, double t, bool lowRate)
            {
                const double low = component(route, t, 1250) + component(route, t, 17500);
                return low + (lowRate ? 0. : component(route, t, 32000));
            };
            mixer.prepare(sr, 32, 0,
                          [&](int, int)
                          {
                              Buffer ir(4, irLength);
                              for (int c = 0; c < 4; ++c)
                                  for (int i = 0; i < irLength; ++i)
                                      ir.setSample(c, i, float(ideal(c, i / 96000., false)));
                              return ir;
                          });
            p = {};
            p.solo[0] = true;
            p.pairDb.fill(0.f);
            constexpr int burstLength = 73;
            const int length = int(std::ceil(irLength * sr / 96000.));
            Buffer burst(2, burstLength), out(2, length + burstLength + mixer.latencySamples());
            out.clear();
            std::mt19937 random(6642);
            std::uniform_real_distribution<float> amp(-0.13f, 0.13f);
            for (int c = 0; c < 2; ++c)
            {
                for (int i = 0; i < burstLength; ++i)
                    burst.setSample(c, i, amp(random));
                out.copyFrom(c, 0, burst, c, 0, burstLength);
            }
            render(mixer, out, 2, p);
            double worst = 0.;
            for (int c = 0; c < 2; ++c)
                for (int t = 0; t < length + burstLength; ++t)
                {
                    double ref = 0.;
                    for (int in = 0; in < 2; ++in)
                        for (int k = 0; k < burstLength; ++k)
                        {
                            const double time = (t - k) / sr;
                            if (t >= k && t - k < length)
                                ref += burst.getSample(in, k) * ideal(2 * in + c, time, sr <= 48000.) *
                                       96000. / sr;
                        }
                    worst = std::max(worst, std::abs(ref - out.getSample(c, t + mixer.latencySamples())));
                }
            logMessage("Fs " + juce::String(sr, 0) + " Hz, max time-domain error " + juce::String(worst, 9));
            expect(worst < 0.00008, "Independent random convolution with anti-alias rejection at 32 kHz");
        }
        beginTest("Nine recorded matrices preserve complex transfer across the six host sample rates");
        constexpr std::array<double, 5> frequencies{{51., 411., 1753., 7300., 16000.}};
        using Complex = std::complex<double>;
        auto response = [](const float* samples, int count, double fs, double frequency)
        {
            const Complex delta = std::polar(1.0, -2 * juce::MathConstants<double>::pi * frequency / fs);
            Complex z(1, 0), result(0, 0);
            for (int i = 0; i < count; ++i)
            {
                result += double(samples[i]) * z;
                z *= delta;
            }
            return result;
        };
        for (double sr : {44100., 48000., 88200., 96000., 176400., 192000.})
        {
            double worst = 0.;
            for (int room = 0; room < 3; ++room)
            {
                std::array<Buffer, 3> matrices;
                for (int pair = 0; pair < 3; ++pair)
                    matrices[pair] = loadAsset(room, pair);
                mixer.prepare(sr, 127, room, [&](int, int pair) { return Buffer(matrices[pair]); });
                p = {};
                p.room = room;
                p.pairDb.fill(0.f);
                for (int pair = 0; pair < 3; ++pair)
                {
                    p.solo.fill(false);
                    p.solo[pair] = true;
                    Buffer settle(2, int(sr * 0.021) + 256);
                    settle.clear();
                    render(mixer, settle, 2, p);
                    const int len = int(std::ceil(matrices[pair].getNumSamples() * sr / 96000.));
                    Buffer actual(2, len + 73 + mixer.latencySamples());
                    actual.clear();
                    Buffer burst(2, 73);
                    std::mt19937 random(4253 + pair * 18 + room);
                    std::uniform_real_distribution<float> amp(-0.15f, 0.15f);
                    for (int c = 0; c < 2; ++c)
                    {
                        for (int i = 0; i < 73; ++i)
                            burst.setSample(c, i, amp(random));
                        actual.copyFrom(c, 0, burst, c, 0, 73);
                    }
                    render(mixer, actual, 2, p);
                    for (double hz : frequencies)
                        for (int c = 0; c < 2; ++c)
                        {
                            Complex expected{};
                            for (int in = 0; in < 2; ++in)
                                expected += response(burst.getReadPointer(in), 73, sr, hz) *
                                            response(matrices[pair].getReadPointer(2 * in + c),
                                                     matrices[pair].getNumSamples(), 96000, hz);
                            const auto referenceAligned = actual.getReadPointer(c, mixer.latencySamples());
                            const auto measured = response(referenceAligned, len + 73, sr, hz);
                            worst = std::max(worst, std::abs(measured - expected));
                        }
                }
            }
            logMessage("Fs " + juce::String(sr, 0) + " Hz, max absolute complex transfer error " +
                       juce::String(worst, 9));
            expect(worst < 0.00015, "Complex transfer at five frequencies / all nine pairs");
        }
    }
};
class DspBenchmark : public juce::UnitTest
{
  public:
    DspBenchmark() : UnitTest("Live Room callback timings", "Benchmark") {}
    void runTest() override
    {
        using Clock = std::chrono::steady_clock;
        using Micros = std::chrono::duration<double, std::micro>;
#if JUCE_MAC
        using Start = void (*)();
        using Stop = unsigned long long (*)();
        const auto startAudit = reinterpret_cast<Start>(dlsym(RTLD_DEFAULT, "labrc_start_heap_audit"));
        const auto stopAudit = reinterpret_cast<Stop>(dlsym(RTLD_DEFAULT, "labrc_stop_heap_audit"));
        if (startAudit && stopAudit)
        {
            beginTest("Positive control for the optional allocator interposer");
            startAudit();
            auto* volatile pointer = std::malloc(145);
            std::free(pointer);
            expect(stopAudit() >= 2);
        }
#endif
        for (double rate : {48000., 96000., 192000.})
            for (int block : {1, 64, 256})
            {
                RoomMixer mixer;
                auto load = [](int room, int pair) { return loadAsset(room, pair); };
                Parameters p;
                p.room = 2;
                const auto prepareStart = Clock::now();
                mixer.prepare(rate, block, p.room, load, true);
                const double prepareMs = Micros(Clock::now() - prepareStart).count() / 1000.;
                beginTest("Fs " + juce::String(rate, 0) + " | block " + juce::String(block));
                Buffer audio(2, block);
                std::vector<double> timings;
                const int count = static_cast<int>(rate * 6 / block);
                timings.reserve(static_cast<size_t>(count));
                std::minstd_rand random(92473);
                std::uniform_real_distribution<float> uniform(-0.25f, 0.25f);
                unsigned long long heapCalls = 0;
                double checksum = 0.;
                for (int b = 0; b < count; ++b)
                {
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < block; ++i)
                            audio.setSample(c, i, uniform(random));
#if JUCE_MAC
                    if (startAudit && stopAudit)
                        startAudit();
#endif
                    const auto start = Clock::now();
                    mixer.process(audio.getArrayOfWritePointers(), 2, block, p);
                    const auto end = Clock::now();
#if JUCE_MAC
                    if (startAudit && stopAudit)
                        heapCalls += stopAudit();
#endif
                    timings.push_back(Micros(end - start).count());
                    checksum += audio.getSample(1, block - 1);
                }
                std::sort(timings.begin(), timings.end());
                const double mean = std::accumulate(timings.begin(), timings.end(), 0.) / count;
                const auto misses = std::count_if(timings.begin(), timings.end(),
                                                  [=](double t) { return t > block * 1e6 / rate; });
                juce::String report = "callback us mean/p95/p99/max " + juce::String(mean, 2) + "/" +
                                      juce::String(timings[count * 95 / 100], 2) + "/" +
                                      juce::String(timings[count * 99 / 100], 2) + "/" +
                                      juce::String(timings.back(), 2) + "; deadline misses " +
                                      juce::String(int(misses)) + "/" + juce::String(count) + "; budget us " +
                                      juce::String(block * 1e6 / rate, 2) + "; prepare ms " +
                                      juce::String(prepareMs, 1);
#if JUCE_MAC
                mach_task_basic_info_data_t info{};
                mach_msg_type_number_t length = MACH_TASK_BASIC_INFO_COUNT;
                if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info),
                              &length) == KERN_SUCCESS)
                    report +=
                        "; process RSS MiB " + juce::String(double(info.resident_size) / (1024 * 1024), 1);
                if (startAudit && stopAudit)
                {
                    report += "; callback heap calls " + juce::String(static_cast<juce::int64>(heapCalls));
                    expect(heapCalls == 0);
                }
#else
                juce::ignoreUnused(heapCalls);
#endif
                logMessage(report);
                expect(std::isfinite(checksum) && checksum != 0.);
            }
        for (double rate : {96000., 192000.})
        {
            constexpr int block = 64;
            const int total = int(rate * 5);
            const int frames = (total / block) * block;
            RoomMixer mixer;
            Parameters p;
            p.room = 2;
            Buffer source(2, frames), reference(2, frames);
            std::minstd_rand rng(333);
            std::uniform_real_distribution<float> uniform(-0.35f, 0.35f);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < frames; ++i)
                    source.setSample(c, i, uniform(rng));
            reference.makeCopyOf(source);
            auto load = [](int room, int pair) { return loadAsset(room, pair); };
            mixer.prepare(rate, block, 2, load, true);
            for (int i = 0; i < frames; i += block)
            {
                float* channel[] = {reference.getWritePointer(0, i), reference.getWritePointer(1, i)};
                mixer.process(channel, 2, block, p);
            }
            beginTest("Two-bank fade, Fs " + juce::String(rate, 0) + " | block 64");
            std::vector<double> timings;
            unsigned long long heapCalls = 0;
            double maxRss = 0;
            for (int run = 0; run < 4; ++run)
            {
                Buffer actual(source);
                p.room = 2;
                mixer.prepare(rate, block, 2, load, true);
                int startFade = -1;
                for (int i = 0; i < frames; i += block)
                {
                    if (p.room == 2 && i >= rate * 0.5)
                    {
                        p.room = 1;
                        mixer.requestRoom(1);
                    }
                    float* channel[] = {actual.getWritePointer(0, i), actual.getWritePointer(1, i)};
#if JUCE_MAC
                    if (startAudit && stopAudit)
                        startAudit();
#endif
                    const auto start = Clock::now();
                    mixer.process(channel, 2, block, p);
                    const auto end = Clock::now();
#if JUCE_MAC
                    if (startAudit && stopAudit)
                        heapCalls += stopAudit();
#endif
                    // Compare with a separate old-room render. The first difference locates
                    // the 50 ms interval sample-accurately, despite asynchronous loading.
                    if (startFade < 0 && i >= rate * 0.5)
                        for (int k = 0; k < block; ++k)
                            if (channel[0][k] != reference.getSample(0, i + k))
                            {
                                startFade = i + k;
                                break;
                            }
                    if (startFade >= 0)
                    {
                        if (i >= startFade + int(rate * 0.05))
                            break;
                        timings.push_back(Micros(end - start).count());
#if JUCE_MAC
                        if (i % 1024 == 0)
                        {
                            mach_task_basic_info_data_t info{};
                            mach_msg_type_number_t length = MACH_TASK_BASIC_INFO_COUNT;
                            if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                                          reinterpret_cast<task_info_t>(&info), &length) == KERN_SUCCESS)
                                maxRss = std::max(maxRss, double(info.resident_size) / (1024 * 1024));
                        }
#endif
                    }
                }
                expect(startFade >= 0 && startFade + int(rate * 0.05) < frames, "Observed entire transition");
            }
            if (!timings.empty())
            {
                std::sort(timings.begin(), timings.end());
                const auto count = timings.size();
                const double budget = 1e6 * block / rate;
                const double mean = std::accumulate(timings.begin(), timings.end(), 0.) / double(count);
                const auto overruns =
                    std::count_if(timings.begin(), timings.end(), [=](double t) { return t > budget; });
                logMessage("fade callbacks " + juce::String(int(count)) + "; us mean/p95/p99/max " +
                           juce::String(mean, 2) + "/" + juce::String(timings[count * 95 / 100], 2) + "/" +
                           juce::String(timings[count * 99 / 100], 2) + "/" +
                           juce::String(timings.back(), 2) + "; deadline misses " +
                           juce::String(int(overruns)) + "; sampled process RSS MiB " +
                           juce::String(maxRss, 1));
            }
#if JUCE_MAC
            if (startAudit && stopAudit)
            {
                logMessage("Crossfade-series heap calls " +
                           juce::String(static_cast<juce::int64>(heapCalls)));
                expect(heapCalls == 0);
            }
#else
            juce::ignoreUnused(heapCalls, maxRss);
#endif
        }
    }
};
DspBenchmark dspBenchmark;
DspTests dspTests;
} // namespace
