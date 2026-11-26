#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace labrc
{
// Stereo-linked, topology-preserving-transform biquads. For analogue low, band and
// high outputs the common denominator is s² + k*s + 1. The gain-dependent cutoff
// and output weights below give the RBJ shelves (S=1) and peaking filter (Q=0.7).
class PairEqualiser
{
  public:
    void prepare(double sampleRate) noexcept
    {
        constexpr std::array<double, 3> frequencies{100., 1000., 8000.};
        for (size_t i = 0; i < bands.size(); ++i)
            bands[i].prepare(i, sampleRate, frequencies[i]);
        enable.reset(sampleRate, 0.020);
    }

    void targets(const std::array<float, 3>& db, bool enabled, bool initial) noexcept
    {
        for (size_t i = 0; i < bands.size(); ++i)
            bands[i].target(db[i], initial);
        if (initial)
            enable.setCurrentAndTargetValue(enabled ? 1.f : 0.f);
        else
            enable.setTargetValue(enabled ? 1.f : 0.f);
    }

    void advance() noexcept
    {
        amount = enable.getNextValue();
        for (auto& band : bands)
            band.advance();
    }

    float process(int channel, float input) noexcept
    {
        double filtered = std::isfinite(input) ? input : 0.f;
        for (auto& band : bands)
            filtered = band.process(channel, filtered);

        // Both filter integrators remain warm in OUT. The stable OUT path returns
        // the original float directly, including when nonzero gains are parked.
        if (amount == 0.f || filtered == input)
            return input;
        if (amount == 1.f)
            return static_cast<float>(filtered);
        return static_cast<float>(input + static_cast<double>(amount) * (filtered - input));
    }

  private:
    struct Band
    {
        void prepare(size_t newIndex, double sr, double frequency) noexcept
        {
            index = newIndex;
            // For supported rates all three centres are strictly below Nyquist.
            prewarp = std::tan(juce::MathConstants<double>::pi * std::min(0.45, frequency / sr));
            rootA.reset(sr, 0.020);
            integrators = {};
        }

        void target(float db, bool initial) noexcept
        {
            const float boundedDb = std::isfinite(db) ? juce::jlimit(-12.f, 12.f, db) : 0.f;
            if (!initial && boundedDb == lastDb)
                return;
            lastDb = boundedDb;
            // A multiplicative ramp of 10^(dB/80) is a linear 20 ms ramp in dB.
            const double value = std::pow(10., static_cast<double>(boundedDb) / 80.);
            if (initial)
            {
                rootA.setCurrentAndTargetValue(value);
                coefficients(value);
            }
            else
                rootA.setTargetValue(value);
        }

        void advance() noexcept
        {
            if (rootA.isSmoothing())
                coefficients(rootA.getNextValue());
        }

        double process(int channel, double x) noexcept
        {
            auto& state = integrators[static_cast<size_t>(channel)];
            const double difference = x - state.low;
            const double band = a1 * state.band + a2 * difference;
            const double low = state.low + a2 * state.band + a3 * difference;
            state.band = 2. * band - state.band;
            state.low = 2. * low - state.low;
            return flat ? x : m0 * x + m1 * band + m2 * low;
        }

        void coefficients(double root) noexcept
        {
            constexpr double shelfK = 1.4142135623730950488;
            const double A = root * root;
            double g = prewarp;
            double k = shelfK;
            m0 = 1.;
            m2 = 0.;
            if (index == 0)
            {
                g /= root;
                m1 = k * (A - 1.);
                m2 = A * A - 1.;
            }
            else if (index == 1)
            {
                k = 1. / (0.7 * A);
                m1 = k * (A * A - 1.);
            }
            else
            {
                g *= root;
                m0 = A * A;
                m1 = k * (A - m0);
                m2 = 1. - m0;
            }
            a1 = 1. / (1. + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
            flat = root == 1.;
        }

        struct State
        {
            double band = 0., low = 0.;
        };
        std::array<State, 2> integrators{};
        juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> rootA{1.};
        size_t index = 0;
        float lastDb = 0.f;
        double prewarp = 0., a1 = 1., a2 = 0., a3 = 0., m0 = 1., m1 = 0., m2 = 0.;
        bool flat = true;
    };

    std::array<Band, 3> bands{};
    juce::LinearSmoothedValue<float> enable{0.f};
    float amount = 0.f;
};
} // namespace labrc
