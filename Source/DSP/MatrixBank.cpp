#include "MatrixBank.h"
#include <complex>
#include <numeric>
#include <stdexcept>
#ifndef LABRC_USE_ACCELERATE
#define LABRC_USE_ACCELERATE (JUCE_MAC || JUCE_IOS)
#endif
#if LABRC_USE_ACCELERATE
#include <Accelerate/Accelerate.h>
#endif

namespace labrc
{
namespace
{
using Buffer = juce::AudioBuffer<float>;
constexpr double sourceRate = 96000.0;
constexpr double pi = juce::MathConstants<double>::pi;

// A centred, band-limited interpolator. Zero extension preserves the WAV origin.
// Scale by 1/ratio to preserve convolution transfer gain when the sample grid changes.
Buffer resample(Buffer source, double fs)
{
    if (source.getNumChannels() != 4 || source.getNumSamples() < 1)
        throw std::invalid_argument("LAB-RC requires a four-channel matrix IR");
    if (fs == sourceRate)
        return source;
    const double ratio = fs / sourceRate;
    const double cutoff = std::min(1.0, ratio);
    const int radius = static_cast<int>(std::ceil(40.0 / cutoff));
    const int taps = 2 * radius;
    const int outLength = std::max(1, static_cast<int>(std::ceil(source.getNumSamples() * ratio)));
    const int integerRate = static_cast<int>(std::round(fs));
    const bool exactGrid = fs == integerRate;
    const int phases = exactGrid ? std::min(4096, integerRate / std::gcd(integerRate, 96000)) : 4096;
    std::vector<float> weights(static_cast<size_t>(phases + 1) * taps);
    for (int phase = 0; phase <= phases; ++phase)
        for (int k = 0; k < taps; ++k)
        {
            const double distance = k - radius + 1.0 - static_cast<double>(phase) / phases;
            const double a = cutoff * distance;
            const double sinc = std::abs(a) < 1e-12 ? 1.0 : std::sin(pi * a) / (pi * a);
            const double angle = pi * distance / radius;
            const double blackman = 0.42 + 0.5 * std::cos(angle) + 0.08 * std::cos(2.0 * angle);
            weights[static_cast<size_t>(phase) * taps + k] =
                static_cast<float>(cutoff * sinc * blackman / ratio);
        }
    Buffer out(4, outLength);
    // Contiguous zero padding makes every dot product identical at the two boundaries.
    std::vector<float> padded(static_cast<size_t>(source.getNumSamples() + 2 * radius));
    for (int c = 0; c < 4; ++c)
    {
        std::copy_n(source.getReadPointer(c), source.getNumSamples(), padded.data() + radius);
        auto* dest = out.getWritePointer(c);
        for (int i = 0; i < outLength; ++i)
        {
            const double sourcePosition = i / ratio;
            const int base = static_cast<int>(sourcePosition);
            const int phase = static_cast<int>(std::round((sourcePosition - base) * phases));
            const auto* weight = weights.data() + static_cast<size_t>(phase) * taps;
            const auto* x = padded.data() + base + 1;
#if LABRC_USE_ACCELERATE
            vDSP_dotpr(x, 1, weight, 1, dest + i, static_cast<vDSP_Length>(taps));
#else
            float sum = 0.f;
            for (int k = 0; k < taps; ++k)
                sum += x[k] * weight[k];
            dest[i] = sum;
#endif
        }
    }
    return out;
}

// Half spectra have independent DC/Nyquist bins and aligned real/imaginary planes.
// Every plan and scratch buffer is private to one layer; the portable FFT has no
// workspace allocation or internal locking during a transform.
class RealFFT
{
  public:
    explicit RealFFT(int halfLength)
        : half(halfLength), size(2 * half), stride(half + 4),
          logSize(juce::jlimit(1, 30, static_cast<int>(std::log2(size))))
#if LABRC_USE_ACCELERATE
          ,
          setup(vDSP_create_fftsetup(static_cast<vDSP_Length>(logSize), kFFTRadix2))
#else
          ,
          work(size), twiddles(half), reversed(size)
#endif
    {
#if LABRC_USE_ACCELERATE
        if (!setup)
            throw std::bad_alloc();
#else
        for (int i = 0; i < half; ++i)
            twiddles[i] = std::polar(1.f, static_cast<float>(-2 * pi * i / size));
        for (int i = 1; i < size; ++i)
            reversed[i] = (reversed[i / 2] / 2) + ((i & 1) * half);
#endif
    }
    ~RealFFT()
    {
#if LABRC_USE_ACCELERATE
        vDSP_destroy_fftsetup(setup);
#endif
    }
    int floats() const noexcept
    {
        return 2 * stride;
    }

    void forward(const float* time, float* dest) noexcept
    {
#if LABRC_USE_ACCELERATE
        DSPSplitComplex z{dest, dest + stride};
        vDSP_ctoz(reinterpret_cast<const DSPComplex*>(time), 2, &z, 1, half);
        vDSP_fft_zrip(setup, &z, 1, logSize, FFT_FORWARD);
        dest[half] = dest[stride];
        dest[stride] = 0.f;
        dest[stride + half] = 0.f;
        const float scale = 0.5f;
        vDSP_vsmul(dest, 1, &scale, dest, 1, floats());
#else
        for (int i = 0; i < size; ++i)
            work[i] = {time[i], 0};
        transform(false);
        for (int i = 0; i <= half; ++i)
        {
            dest[i] = work[i].real();
            dest[stride + i] = work[i].imag();
        }
#endif
    }

    // The input accumulator can be reused immediately after the inverse transform.
    void inverse(float* spectrum, float* time) noexcept
    {
#if LABRC_USE_ACCELERATE
        spectrum[stride] = spectrum[half];
        DSPSplitComplex z{spectrum, spectrum + stride};
        vDSP_fft_zrip(setup, &z, 1, logSize, FFT_INVERSE);
        vDSP_ztoc(&z, 1, reinterpret_cast<DSPComplex*>(time), 2, half);
        const float scale = 1.f / static_cast<float>(size);
        vDSP_vsmul(time, 1, &scale, time, 1, size);
#else
        for (int i = 0; i <= half; ++i)
            work[i] = {spectrum[i], spectrum[stride + i]};
        for (int i = 1; i < half; ++i)
            work[size - i] = std::conj(work[i]);
        transform(true);
        for (int i = 0; i < size; ++i)
            time[i] = work[i].real() / static_cast<float>(size);
#endif
    }
    void multiplyAdd(const float* x, const float* h, float* sum) const noexcept
    {
#if LABRC_USE_ACCELERATE
        // Accelerate's split-complex API uses mutable pointer types for input views.
        DSPSplitComplex a{const_cast<float*>(x), const_cast<float*>(x + stride)};
        DSPSplitComplex b{const_cast<float*>(h), const_cast<float*>(h + stride)};
        DSPSplitComplex s{sum, sum + stride};
        vDSP_zvma(&a, 1, &b, 1, &s, 1, &s, 1, static_cast<vDSP_Length>(half + 1));
#else
        for (int i = 0; i <= half; ++i)
        {
            sum[i] += x[i] * h[i] - x[stride + i] * h[stride + i];
            sum[stride + i] += x[i] * h[stride + i] + x[stride + i] * h[i];
        }
#endif
    }

  private:
    const int half, size, stride, logSize;
#if LABRC_USE_ACCELERATE
    FFTSetup setup;
#else
    std::vector<std::complex<float>> work, twiddles;
    std::vector<int> reversed;
    void transform(bool inverseTransform) noexcept
    {
        for (int i = 0; i < size; ++i)
            if (i < reversed[i])
                std::swap(work[i], work[reversed[i]]);
        for (int width = 2; width <= size; width *= 2)
            for (int base = 0; base < size; base += width)
                for (int j = 0; j < width / 2; ++j)
                {
                    const auto w = twiddles[j * (size / width)];
                    const auto t = work[base + j + width / 2] * (inverseTransform ? std::conj(w) : w);
                    const auto u = work[base + j];
                    work[base + j] = u + t;
                    work[base + j + width / 2] = u - t;
                }
    }
#endif
};
} // namespace

class MatrixBank::Layer
{
    struct Job
    {
        int dest, source, partition;
        size_t kernelOffset;
    };

  public:
    Layer(int hopSize, int offset, int end, const std::array<Buffer, 3>& matrices)
        : hop(hopSize), start(offset), partitions((end - start + hop - 1) / hop), ringSize(4 * hop), fft(hop),
          spectrumSize(fft.floats()), history(2 * partitions * spectrumSize), input(2, 2 * hop),
          outputRing(6, ringSize), time(2 * hop), sum(spectrumSize)
    {
        input.clear();
        outputRing.clear();
        // Reserve once on the loader to avoid doubling large tails transiently.
        size_t maxKernels = 0;
        for (const auto& ir : matrices)
        {
            const int samples = std::max(0, std::min(ir.getNumSamples(), end) - start);
            maxKernels += 4 * static_cast<size_t>((samples + hop - 1) / hop);
        }
        kernels.reserve(maxKernels * static_cast<size_t>(spectrumSize));
        for (int dest = 0; dest < 6; ++dest)
        {
            const auto& ir = matrices[dest / 2];
            const auto jobStart = jobs.size();
            for (int part = 0; part < partitions; ++part)
                for (int source = 0; source < 2; ++source)
                {
                    const int position = start + part * hop;
                    const int n = std::min({hop, end - position, ir.getNumSamples() - position});
                    if (n <= 0)
                        continue;
                    const float* channel = ir.getReadPointer(2 * source + dest % 2, position);
                    if (std::all_of(channel, channel + n, [](float v) { return v == 0.f; }))
                        continue;
                    std::fill(time.begin(), time.end(), 0.f);
                    std::copy_n(channel, n, time.begin());
                    const auto index = kernels.size();
                    kernels.resize(index + spectrumSize);
                    fft.forward(time.data(), kernels.data() + index);
                    jobs.push_back({dest, source, part, index});
                }
            if (jobs.size() != jobStart)
                jobs.push_back({dest, -1, 0, 0});
        }
    }

    void add(const Buffer& in, Buffer& wet, int count) noexcept
    {
        int pos = 0;
        while (pos < count)
        {
            const int n = std::min(count - pos, latency - (fill % latency));
            // Ring and input boundaries are all multiples of the base hop.
            for (int c = 0; c < 2; ++c)
                input.copyFrom(c, fill, in, c, pos, n);
            for (int c = 0; c < 6; ++c)
            {
                wet.addFrom(c, pos, outputRing, c, ringPosition, n);
                outputRing.clear(c, ringPosition, n);
            }
            fill += n;
            pos += n;
            ringPosition = (ringPosition + n) % ringSize;
            if (fill % latency != 0)
                continue;
            if (fill == hop)
            {
                fill = 0;
                newest = (newest + 1) % partitions;
                for (int c = 0; c < 2; ++c)
                    fft.forward(input.getReadPointer(c),
                                history.data() + (c * partitions + newest) * spectrumSize);
                writePosition = (ringPosition + start + latency - hop) % ringSize;
                jobStep = 0;
                nextJob = 0;
                running = true;
            }
            if (running)
                advance();
        }
    }

  private:
    void advance() noexcept
    {
        // An offset of 2*hop in each tail tier gives one full hop to spread the
        // frequency products and six inverse transforms over 128-sample slices.
        ++jobStep;
        const size_t limit = jobs.size() * static_cast<size_t>(jobStep) / (hop / latency);
        while (nextJob < limit)
        {
            const auto& job = jobs[nextJob++];
            if (job.source >= 0)
            {
                int index = newest - job.partition;
                if (index < 0)
                    index += partitions;
                fft.multiplyAdd(history.data() + (job.source * partitions + index) * spectrumSize,
                                kernels.data() + job.kernelOffset, sum.data());
            }
            else
            {
                fft.inverse(sum.data(), time.data());
                const int first = std::min(2 * hop, ringSize - writePosition);
                outputRing.addFrom(job.dest, writePosition, time.data(), first);
                if (first < 2 * hop)
                    outputRing.addFrom(job.dest, 0, time.data() + first, 2 * hop - first);
                std::fill(sum.begin(), sum.end(), 0.f);
            }
        }
        running = jobStep < hop / latency;
    }
    const int hop, start, partitions, ringSize;
    RealFFT fft;
    int spectrumSize;
    std::vector<float> history;
    Buffer input, outputRing;
    std::vector<float> time, sum, kernels;
    std::vector<Job> jobs;
    int newest = -1, fill = 0, ringPosition = 0, writePosition = 0, jobStep = 0;
    size_t nextJob = 0;
    bool running = false;
};

MatrixBank::MatrixBank(int room, double rate, const IRProvider& provider)
{
    std::array<Buffer, 3> matrices;
    int length = 0;
    for (int pair = 0; pair < 3; ++pair)
    {
        matrices[pair] = resample(provider(room, pair), rate);
        length = std::max(length, matrices[pair].getNumSamples());
    }
    constexpr int hops[] = {latency, 512, 2048, 8192};
    for (int i = 0; i < 4; ++i)
    {
        const int start = i == 0 ? 0 : 2 * hops[i];
        if (start >= length)
            break;
        const int end = i == 3 ? length : std::min(length, 2 * hops[i + 1]);
        layers.push_back(std::make_unique<Layer>(hops[i], start, end, matrices));
    }
}

MatrixBank::~MatrixBank() = default;
void MatrixBank::add(const Buffer& input, Buffer& output, int count) noexcept
{
    for (auto& layer : layers)
        layer->add(input, output, count);
}
} // namespace labrc
