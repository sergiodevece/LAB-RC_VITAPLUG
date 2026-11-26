#include "RoomMixer.h"
#include "PairEqualiser.h"
#include "MatrixBank.h"
#include <chrono>
#include <thread>

namespace labrc
{
namespace
{
using Buffer = juce::AudioBuffer<float>;
using Stereo = std::array<float, 2>;
using Smoother = juce::LinearSmoothedValue<float>;
static_assert(std::atomic<float>::is_always_lock_free && std::atomic<int>::is_always_lock_free);
float finite(float x) noexcept
{
    return std::isfinite(x) ? x : 0.f;
}
float bounded(float x, float fallback, float low, float high) noexcept
{
    return std::isfinite(x) ? juce::jlimit(low, high, x) : fallback;
}
float gain(float x, float fallback, float low, float high) noexcept
{
    return juce::Decibels::decibelsToGain(bounded(x, fallback, low, high));
}
} // namespace

struct RoomMixer::Impl
{
    enum Status
    {
        available,
        ready,
        playing
    };
    struct Slot
    {
        std::unique_ptr<MatrixBank> bank;
        int room = -1;
        std::atomic<int> status{available};
    };
    std::array<Slot, 2> slots;
    std::atomic<int> requested{0};
    std::atomic<bool> stopping{false};
    std::thread worker;
    IRProvider provider;
    double rate = 96000.0;
    std::atomic<double> tail{4.5};
    std::atomic<int> latency{0};
    int active = 0, incoming = -1, prime = 0, fadeIndex = 0, fadeLength = 1;
    int capacity = 0, delayIndex = 0;
    bool firstBlock = true, externallyControlledRoom = false;
    Buffer input, wet, nextWet;
    std::array<std::array<float, MatrixBank::latency>, 4> delay{};
    Smoother inputGain, outputGain, mix, bypass;
    std::array<Smoother, 3> pairGain;
    std::array<PairEqualiser, 3> pairEQ;
    std::array<std::atomic<float>, 10> meters{};

    Impl()
    {
        clearMeters();
    }
    ~Impl()
    {
        release();
    }

    void clearMeters() noexcept
    {
        for (auto& meter : meters)
            meter.store(0.f, std::memory_order_relaxed);
    }

    void release()
    {
        stopping.store(true);
        if (worker.joinable())
            worker.join();
        for (auto& slot : slots)
        {
            slot.bank.reset();
            slot.status.store(available);
            slot.room = -1;
        }
        capacity = 0;
        provider = {};
        latency.store(0);
        tail.store(4.5);
        clearMeters();
    }

    void prepare(double sampleRate, int maxBlock, int initialRoom, IRProvider source, bool externalRoom)
    {
        release();
        if (!std::isfinite(sampleRate) || sampleRate < 1. || sampleRate > 768000. || !source)
            throw std::invalid_argument("Invalid LAB-RC preparation");
        rate = sampleRate;
        externallyControlledRoom = externalRoom;
        provider = std::move(source);
        active = 0;
        incoming = -1;
        prime = 0;
        fadeIndex = 0;
        fadeLength = std::max(1, juce::roundToInt(rate * 0.05));
        slots[0].room = juce::jlimit(0, 2, initialRoom);
        slots[0].bank = std::make_unique<MatrixBank>(slots[0].room, rate, provider);
        slots[0].status.store(playing);
        requested.store(slots[0].room);
        delay = {};
        delayIndex = 0;
        capacity = juce::jlimit(1, 2048, maxBlock);
        input.setSize(2, capacity);
        wet.setSize(6, capacity);
        nextWet.setSize(6, capacity);
        inputGain.reset(rate, 0.020);
        outputGain.reset(rate, 0.020);
        mix.reset(rate, 0.020);
        bypass.reset(rate, 0.020);
        for (auto& smooth : pairGain)
            smooth.reset(rate, 0.020);
        for (auto& eq : pairEQ)
            eq.prepare(rate);
        firstBlock = true;
        latency.store(MatrixBank::latency);
        tail.store(4.5 + MatrixBank::latency / rate);
        stopping.store(false);
        worker = std::thread([this] { loadRooms(); });
    }

    void loadRooms()
    {
        int failedRoom = -1;
        while (!stopping.load(std::memory_order_acquire))
        {
            const int room = requested.load(std::memory_order_acquire);
            if (room != failedRoom)
            {
                failedRoom = -1;
                Slot* spare = nullptr;
                bool found = false;
                for (auto& slot : slots)
                {
                    auto state = slot.status.load(std::memory_order_acquire);
                    if (state == available)
                        spare = &slot;
                    else if (slot.room == room)
                        found = true;
                    else if (state == ready && slot.status.compare_exchange_strong(
                                 state, available, std::memory_order_acq_rel))
                        spare = &slot; // A newer idle request can replace an unclaimed ready bank.
                }
                if (spare && !found)
                {
                    // Only the worker assigns, clears or destroys a published bank.
                    spare->bank.reset();
                    try
                    {
                        auto bank = std::make_unique<MatrixBank>(room, rate, provider);
                        if (requested.load(std::memory_order_acquire) == room)
                        {
                            spare->bank = std::move(bank);
                            spare->room = room;
                            spare->status.store(ready, std::memory_order_release);
                        }
                    }
                    catch (...)
                    {
                        failedRoom = room;
                    } // Hold the last working room.
                }
                // Retired spectra and history do not occupy memory between requests.
                else if (spare)
                    spare->bank.reset();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    void targets(const Parameters& p, bool hostBypassed) noexcept
    {
        auto set = [this](Smoother& smooth, float value)
        {
            if (firstBlock)
                smooth.setCurrentAndTargetValue(value);
            else
                smooth.setTargetValue(value);
        };
        set(inputGain, gain(p.inputDb, 0, -18, 18));
        set(outputGain, gain(p.outputDb, 0, -18, 18));
        set(mix, bounded(p.mixPercent, 100, 0, 100) * 0.01f);
        set(bypass, (p.bypass || hostBypassed) ? 1.f : 0.f);
        const bool solo = p.solo[0] || p.solo[1] || p.solo[2];
        constexpr float defaults[] = {-6, -3, -9};
        for (int i = 0; i < 3; ++i)
            set(pairGain[i],
                p.mute[i] || (solo && !p.solo[i]) ? 0.f : gain(p.pairDb[i], defaults[i], -60, 6));
        for (size_t i = 0; i < pairEQ.size(); ++i)
            pairEQ[i].targets(p.eqDb[i], p.eqEnabled[i], firstBlock);
        firstBlock = false;
    }

    void acceptReady(int desired) noexcept
    {
        if (incoming >= 0)
            return;
        auto& candidate = slots[1 - active];
        int expected = ready;
        if (candidate.status.load(std::memory_order_relaxed) != ready ||
            !candidate.status.compare_exchange_strong(expected, playing, std::memory_order_acq_rel))
            return;
        if (candidate.room != desired)
        {
            candidate.status.store(available, std::memory_order_release);
            return;
        }
        incoming = 1 - active;
        prime = MatrixBank::latency;
        fadeIndex = 0;
    }

    void process(float* const* channels, int numInputs, int numSamples, const Parameters& p,
                 bool hostBypassed) noexcept
    {
        if (numSamples <= 0 || channels == nullptr || channels[0] == nullptr || channels[1] == nullptr)
            return;
        if (capacity == 0)
        {
            if (numInputs < 1)
                std::fill_n(channels[0], numSamples, 0.f);
            if (numInputs < 2)
                std::copy_n(channels[0], numSamples, channels[1]);
            return;
        }
        juce::ScopedNoDenormals noDenormals;
        const int blockRoom = juce::jlimit(0, 2, p.room);
        if (!externallyControlledRoom)
            requested.store(blockRoom, std::memory_order_release);
        targets(p, hostBypassed);
        std::array<float, 10> peaks{};
        for (int pos = 0; pos < numSamples;)
        {
            // A parameter snapshot can predate the latest host notification. In external mode
            // only the control publisher selects a room, including during the final callback.
            acceptReady(externallyControlledRoom ? requested.load(std::memory_order_acquire) : blockRoom);
            int n = std::min(numSamples - pos, capacity);
            if (incoming >= 0)
                n = std::min(n, prime > 0 ? prime : fadeLength - fadeIndex);
            for (int i = 0; i < n; ++i)
            {
                const float l = numInputs > 0 ? finite(channels[0][pos + i]) : 0.f;
                const float r = numInputs > 1 ? finite(channels[1][pos + i]) : l;
                const float g = inputGain.getNextValue();
                input.setSample(0, i, finite(l * g));
                input.setSample(1, i, finite(r * g));
            }
            wet.clear(0, n);
            slots[active].bank->add(input, wet, n);
            if (incoming >= 0)
            {
                nextWet.clear(0, n);
                slots[incoming].bank->add(input, nextWet, n);
            }
            for (int i = 0; i < n; ++i)
            {
                const float inputL = numInputs > 0 ? finite(channels[0][pos + i]) : 0.f;
                const Stereo original{{inputL, numInputs > 1 ? finite(channels[1][pos + i]) : inputL}};
                const float fade =
                    incoming >= 0 && prime == 0 ? float(fadeIndex + i + 1) / float(fadeLength) : 0.f;
                const float blend = mix.getNextValue(), out = outputGain.getNextValue(),
                            bypassGain = bypass.getNextValue();
                std::array<float, 3> gains;
                for (int pair = 0; pair < 3; ++pair)
                {
                    gains[pair] = pairGain[pair].getNextValue();
                    pairEQ[pair].advance();
                }
                for (int c = 0; c < 2; ++c)
                {
                    const float u = input.getSample(c, i);
                    const float alignedOriginal = delay[c][delayIndex],
                                alignedInput = delay[c + 2][delayIndex];
                    delay[c][delayIndex] = original[c];
                    delay[c + 2][delayIndex] = u;
                    peaks[c] = std::max(peaks[c], std::abs(u));
                    float ambience = 0.f;
                    for (int pair = 0; pair < 3; ++pair)
                    {
                        const int k = 2 * pair + c;
                        const float old = wet.getSample(k, i);
                        const float combined =
                            incoming < 0 ? old : (1.f - fade) * old + fade * nextWet.getSample(k, i);
                        const float level = finite(pairEQ[pair].process(c, combined) * gains[pair]);
                        ambience += level;
                        peaks[4 + k] = std::max(peaks[4 + k], std::abs(level));
                    }
                    const float processed = finite(out * ((1.f - blend) * alignedInput + blend * ambience));
                    const float result =
                        finite((1.f - bypassGain) * processed + bypassGain * alignedOriginal);
                    channels[c][pos + i] = result;
                    peaks[2 + c] = std::max(peaks[2 + c], std::abs(result));
                }
                delayIndex = (delayIndex + 1) % MatrixBank::latency;
            }
            if (incoming >= 0)
            {
                if (prime > 0)
                    prime -= n;
                else if ((fadeIndex += n) == fadeLength)
                {
                    const int retired = active;
                    active = incoming;
                    incoming = -1;
                    slots[retired].status.store(available, std::memory_order_release);
                }
            }
            pos += n;
        }
        for (size_t i = 0; i < meters.size(); ++i)
        {
            // The single audio producer publishes maxima. The UI can only reset to zero.
            auto previous = meters[i].load(std::memory_order_relaxed);
            while (previous < peaks[i] &&
                   !meters[i].compare_exchange_strong(previous, peaks[i], std::memory_order_relaxed))
            {
            }
        }
    }
};

RoomMixer::RoomMixer() : impl(std::make_unique<Impl>()) {}
RoomMixer::~RoomMixer() = default;
void RoomMixer::prepare(double sr, int block, int room, IRProvider provider, bool externallyControlledRoom)
{
    impl->prepare(sr, block, room, std::move(provider), externallyControlledRoom);
}
void RoomMixer::requestRoom(int room) noexcept
{
    impl->requested.store(juce::jlimit(0, 2, room), std::memory_order_release);
}
void RoomMixer::release()
{
    impl->release();
}
void RoomMixer::process(float* const* c, int ins, int n, const Parameters& p, bool bypass) noexcept
{
    impl->process(c, ins, n, p, bypass);
}
int RoomMixer::latencySamples() const noexcept
{
    return impl->latency.load();
}
double RoomMixer::tailSeconds() const noexcept
{
    return impl->tail.load();
}
MeterValues RoomMixer::meterSnapshot() const noexcept
{
    MeterValues result;
    for (int c = 0; c < 2; ++c)
    {
        result.input[c] = impl->meters[c].exchange(0.f, std::memory_order_relaxed);
        result.output[c] = impl->meters[2 + c].exchange(0.f, std::memory_order_relaxed);
        for (int pair = 0; pair < 3; ++pair)
            result.pairs[pair][c] = impl->meters[4 + 2 * pair + c].exchange(0.f, std::memory_order_relaxed);
    }
    return result;
}
} // namespace labrc
