#include "../Source/DSP/ChainDSP.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr double kPi = 3.1415926535897932384626433832795;

constexpr int kRounds = 10;
constexpr int kCasesPerRound = 50;
constexpr int kTotalCases = kRounds * kCasesPerRound;

constexpr int kNoiseSamples = 4096;
constexpr int kToneSamples = 4096;
constexpr int kWarmup = 512;
constexpr int kLatencyRadius = 64;

constexpr double kFlatPhasePassDeg = 0.50;
constexpr double kResidualPhasePassDeg = 10.0;
constexpr double kRelativePhasePassDeg = 4.0;
constexpr double kUnityNullPassDb = -120.0;
constexpr double kStereoPassDb = -120.0;

struct Audio
{
    int sr = 48000;
    int block = 128;
    float freq = 1000.f;
    float amplitude = 0.035f;
};

struct Measurement
{
    int lag = -1;
    double corr = 0.0;
};

struct CaseConfig
{
    Audio audio;
    int seed = 1;
    bool solidState = false;
};

struct StageResult
{
    VVChainDSP::Parameters params;
    std::vector<float> noise;
    std::vector<float> tone;
    Measurement delay;
    double phase = 0.0;
};

VVChainDSP::Parameters baseParameters()
{
    VVChainDSP::Parameters p;

    // The probe must exercise the real DSP chain. A master-bypass setting
    // here would replace all preceding processing with the dry path and make
    // the phase/latency probe meaningless.
    p.eqBypass = true;
    p.masterBypass = false;
    p.ottBypass = true;
    p.atypeBypass = true;
    p.deessBypass = true;
    p.mixBypass = true;
    p.eqColorGlobalBypass = true;
    p.dryWet = 100.f;
    p.outputDb = 0.f;

    p.gain = { 0.f, 0.f, 0.f, 0.f };
    p.q = { 0.707f, 0.707f, 0.707f, 0.707f };
    p.eqColor = { 35.f, 35.f, 35.f, 35.f };
    p.eqColorBypass = { false, false, false, false };
    p.eqColorSolidState = { false, false, false, false };

    p.ottBandBypass = { false, false, false, false };
    p.ottDegree = { 0.f, 0.f, 0.f, 0.f };
    p.ottCompThreshold = { 0.f, 0.f, 0.f, 0.f };
    p.ottLifterThreshold = { -48.f, -48.f, -48.f, -48.f };
    p.ottCompMix = { 100.f, 100.f, 100.f, 100.f };
    p.ottLifterMix = { 100.f, 100.f, 100.f, 100.f };
    p.ottBandLevelDb = { 0.f, 0.f, 0.f, 0.f };
    p.ottMix = 100.f;
    p.ottInputGainDb = 0.f;
    p.ottOutputGainDb = 0.f;
    p.ottGateThresholdDb = -80.f;
    p.ottClipper = false;

    p.atypeBandBypass = { false, false, false, false };
    p.atypeDegree = { 0.f, 0.f, 0.f, 0.f };
    p.atypeBandLevelDb = { 0.f, 0.f, 0.f, 0.f };
    p.atypeAttackMs = 10.f;
    p.atypeReleaseMs = 120.f;
    p.atypeMix = 100.f;
    p.atypeInputGainDb = 0.f;
    p.atypeOutputGainDb = 0.f;

    p.deessReferenceHz = 12500.f;
    p.deessIntensity = 0.f;
    p.deessAverageOffset = 0.f;

    p.ottX1 = 120.f;
    p.ottX2 = 1000.f;
    p.ottX3 = 7000.f;
    p.ottXoverOverlap = 50.f;

    return p;
}

std::vector<float> makeNoise(int n, int seed)
{
    std::vector<float> x((size_t)n);
    uint32_t s = static_cast<uint32_t>(0x12345678u
                                      + static_cast<uint32_t>(seed)
                                      * 2654435761u);

    for (float& v : x)
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;

        const float u =
            static_cast<float>(s & 0x00ffffffu) / 16777215.0f;

        v = (2.0f * u - 1.0f) * 0.035f;
    }

    return x;
}

std::vector<float> makeSine(int n,
                            double sr,
                            double freq,
                            double amplitude)
{
    std::vector<float> x((size_t)n);

    for (int i = 0; i < n; ++i)
    {
        x[(size_t)i] = static_cast<float>(
            amplitude
            * std::sin(
                2.0 * kPi * freq
                * static_cast<double>(i) / sr));
    }

    return x;
}

std::vector<float> render(VVChainDSP& dsp,
                          const VVChainDSP::Parameters& p,
                          const std::vector<float>& input,
                          int block)
{
    dsp.reset();

    std::vector<float> output(input.size(), 0.f);
    juce::AudioBuffer<float> buffer(1, block);

    for (size_t pos = 0; pos < input.size(); pos += static_cast<size_t>(block))
    {
        const int count = static_cast<int>(
            std::min<size_t>(
                static_cast<size_t>(block),
                input.size() - pos));

        if (buffer.getNumSamples() != count)
            buffer.setSize(1, count, false, false, true);

        buffer.clear();
        buffer.copyFrom(0, 0, input.data() + pos, count);

        dsp.process(buffer, p);

        for (int n = 0; n < count; ++n)
            output[pos + static_cast<size_t>(n)] =
                buffer.getSample(0, n);
    }

    return output;
}

double normalizedCorrelationAtLag(
    const std::vector<float>& input,
    const std::vector<float>& output,
    int lag,
    int start,
    int count)
{
    if (lag < 0
        || start + lag + count > static_cast<int>(output.size())
        || start + count > static_cast<int>(input.size()))
        return -1.0;

    double sx = 0.0;
    double sy = 0.0;

    for (int i = 0; i < count; ++i)
    {
        sx += input[(size_t)(start + i)];
        sy += output[(size_t)(start + lag + i)];
    }

    sx /= static_cast<double>(count);
    sy /= static_cast<double>(count);

    double xx = 0.0;
    double yy = 0.0;
    double xy = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const double x =
            static_cast<double>(input[(size_t)(start + i)]) - sx;
        const double y =
            static_cast<double>(output[(size_t)(start + lag + i)]) - sy;

        xx += x * x;
        yy += y * y;
        xy += x * y;
    }

    if (xx < 1.0e-20 || yy < 1.0e-20)
        return 0.0;

    return xy / std::sqrt(xx * yy);
}

Measurement measureDelay(
    const std::vector<float>& input,
    const std::vector<float>& output,
    int expectedLatency)
{
    Measurement m;

    const int start = kWarmup;
    const int count = std::min(
        kNoiseSamples - start - kLatencyRadius - 1,
        2048);

    const int first = std::max(0, expectedLatency - kLatencyRadius);
    const int last = std::min(
        expectedLatency + kLatencyRadius,
        static_cast<int>(output.size()) - start - count);

    double best = -2.0;

    for (int lag = first; lag <= last; ++lag)
    {
        const double c =
            normalizedCorrelationAtLag(
                input, output, lag, start, count);

        if (c > best)
        {
            best = c;
            m.lag = lag;
        }
    }

    m.corr = best;
    return m;
}

std::complex<double> projectTone(
    const std::vector<float>& audio,
    double sr,
    double freq,
    int start,
    int count)
{
    std::complex<double> sum(0.0, 0.0);

    for (int i = 0; i < count; ++i)
    {
        const double phase =
            2.0 * kPi * freq
            * static_cast<double>(i) / sr;

        const double window =
            0.5
            - 0.5 * std::cos(
                2.0 * kPi
                * static_cast<double>(i)
                / static_cast<double>(count - 1));

        const double c = std::cos(phase);
        const double s = std::sin(phase);

        sum += static_cast<double>(audio[(size_t)(start + i)])
            * window
            * std::complex<double>(c, -s);
    }

    return sum;
}

double phaseBetweenAligned(
    const std::vector<float>& a,
    int lagA,
    const std::vector<float>& b,
    int lagB,
    double sr,
    double freq)
{
    const int start = kWarmup;

    const int availableA =
        static_cast<int>(a.size()) - start - lagA;
    const int availableB =
        static_cast<int>(b.size()) - start - lagB;

    const int count =
        std::min({ availableA, availableB, kToneSamples - start });

    if (count < 1024)
        return 180.0;

    const auto A =
        projectTone(a, sr, freq, start + lagA, count);

    const auto B =
        projectTone(b, sr, freq, start + lagB, count);

    if (std::abs(A) < 1.0e-12 || std::abs(B) < 1.0e-12)
        return 180.0;

    const double deg =
        std::atan2(
            std::imag(B * std::conj(A)),
            std::real(B * std::conj(A)))
        * 180.0 / kPi;

    return deg;
}

double wrapDeg(double deg)
{
    while (deg > 180.0)
        deg -= 360.0;

    while (deg < -180.0)
        deg += 360.0;

    return deg;
}

double nullDb(
    const std::vector<float>& a,
    const std::vector<float>& b,
    int start,
    int count)
{
    const int maxCount = std::min(
        count,
        std::min(
            static_cast<int>(a.size()) - start,
            static_cast<int>(b.size()) - start));

    if (maxCount <= 0)
        return 0.0;

    double diff = 0.0;
    double ref = 0.0;

    for (int i = 0; i < maxCount; ++i)
    {
        const double av = static_cast<double>(a[(size_t)(start + i)]);
        const double bv = static_cast<double>(b[(size_t)(start + i)]);

        const double d = av - bv;
        diff += d * d;
        ref += av * av;
    }

    const double ratio =
        std::sqrt(diff / std::max(ref, 1.0e-24));

    return 20.0 * std::log10(std::max(ratio, 1.0e-15));
}

std::vector<float> expectedMix(
    const std::vector<float>& dry,
    const std::vector<float>& wet,
    double mix)
{
    const size_t n = std::min(dry.size(), wet.size());
    std::vector<float> result(n);

    const float m = static_cast<float>(mix);
    for (size_t i = 0; i < n; ++i)
        result[i] =
            dry[i] * (1.0f - m)
            + wet[i] * m;

    return result;
}

double stereoMismatchDb(
    const std::vector<float>& input,
    const VVChainDSP::Parameters& p,
    int sr,
    int block)
{
    VVChainDSP dsp;
    dsp.prepare(sr, block, 2);

    juce::AudioBuffer<float> buffer(2, block);
    double diff = 0.0;
    double ref = 0.0;

    for (size_t pos = 0; pos < input.size(); pos += static_cast<size_t>(block))
    {
        const int count = static_cast<int>(
            std::min<size_t>(
                static_cast<size_t>(block),
                input.size() - pos));

        if (buffer.getNumSamples() != count)
            buffer.setSize(2, count, false, false, true);

        buffer.clear();
        buffer.copyFrom(0, 0, input.data() + pos, count);
        buffer.copyFrom(1, 0, input.data() + pos, count);

        dsp.process(buffer, p);

        for (int n = 0; n < count; ++n)
        {
            const double l = buffer.getSample(0, n);
            const double r = buffer.getSample(1, n);
            const double d = l - r;
            diff += d * d;
            ref += l * l;
        }
    }

    if (ref < 1.0e-24)
        return -300.0;

    return 10.0 * std::log10(std::max(diff / ref, 1.0e-30));
}

VVChainDSP::Parameters makeEqUnity()
{
    auto p = baseParameters();
    p.eqBypass = false;
    p.gain = { 0.f, 0.f, 0.f, 0.f };
    return p;
}

VVChainDSP::Parameters makeEqActive()
{
    auto p = baseParameters();
    p.eqBypass = false;
    p.gain = { 1.25f, -1.00f, 0.75f, -0.60f };
    p.freq = { 80.f, 420.f, 2800.f, 10000.f };
    p.q = { 0.707f, 0.9f, 1.1f, 0.85f };
    return p;
}

VVChainDSP::Parameters makeAnalog(float amount, bool ss)
{
    auto p = baseParameters();
    p.eqColorGlobalBypass = false;
    p.eqColor = { amount, amount, amount, amount };
    p.eqColorSolidState = { ss, ss, ss, ss };
    return p;
}

VVChainDSP::Parameters makeOtt()
{
    auto p = baseParameters();
    p.ottBypass = false;
    p.ottMix = 100.f;
    p.ottDegree = { 55.f, 60.f, 65.f, 60.f };
    p.ottCompThreshold = { -18.f, -18.f, -18.f, -18.f };
    p.ottLifterThreshold = { -42.f, -42.f, -42.f, -42.f };
    p.ottCompMix = { 100.f, 100.f, 100.f, 100.f };
    p.ottLifterMix = { 100.f, 100.f, 100.f, 100.f };
    return p;
}

VVChainDSP::Parameters makeTapeA()
{
    auto p = baseParameters();
    p.atypeBypass = false;
    p.atypeMix = 100.f;
    p.atypeDegree = { 15.f, 20.f, 30.f, 25.f };
    return p;
}

VVChainDSP::Parameters makeDeEss()
{
    auto p = baseParameters();
    p.deessBypass = false;
    p.deessReferenceHz = 12500.f;
    p.deessIntensity = 18.f;
    return p;
}

VVChainDSP::Parameters combine(
    const VVChainDSP::Parameters& a,
    const VVChainDSP::Parameters& b)
{
    auto p = baseParameters();

    const auto& eqSource =
        !a.eqBypass ? a : b;

    p.eqBypass =
        a.eqBypass && b.eqBypass;

    if (!p.eqBypass)
    {
        p.gain = eqSource.gain;
        p.freq = eqSource.freq;
        p.q = eqSource.q;
    }

    const auto& colorSource =
        !a.eqColorGlobalBypass ? a : b;

    p.eqColorGlobalBypass =
        a.eqColorGlobalBypass && b.eqColorGlobalBypass;

    if (!p.eqColorGlobalBypass)
    {
        p.eqColor = colorSource.eqColor;
        p.eqColorBypass = colorSource.eqColorBypass;
        p.eqColorSolidState = colorSource.eqColorSolidState;
    }

    const auto& ottSource =
        !a.ottBypass ? a : b;

    p.ottBypass =
        a.ottBypass && b.ottBypass;

    if (!p.ottBypass)
    {
        p.ottBandBypass = ottSource.ottBandBypass;
        p.ottDegree = ottSource.ottDegree;
        p.ottCompThreshold = ottSource.ottCompThreshold;
        p.ottLifterThreshold = ottSource.ottLifterThreshold;
        p.ottCompMix = ottSource.ottCompMix;
        p.ottLifterMix = ottSource.ottLifterMix;
        p.ottBandLevelDb = ottSource.ottBandLevelDb;
        p.ottMix = ottSource.ottMix;
        p.ottInputGainDb = ottSource.ottInputGainDb;
        p.ottOutputGainDb = ottSource.ottOutputGainDb;
        p.ottGateThresholdDb = ottSource.ottGateThresholdDb;
        p.ottClipper = ottSource.ottClipper;
    }

    const auto& tapeSource =
        !a.atypeBypass ? a : b;

    p.atypeBypass =
        a.atypeBypass && b.atypeBypass;

    if (!p.atypeBypass)
    {
        p.atypeBandBypass = tapeSource.atypeBandBypass;
        p.atypeDegree = tapeSource.atypeDegree;
        p.atypeBandLevelDb = tapeSource.atypeBandLevelDb;
        p.atypeAttackMs = tapeSource.atypeAttackMs;
        p.atypeReleaseMs = tapeSource.atypeReleaseMs;
        p.atypeMix = tapeSource.atypeMix;
        p.atypeInputGainDb = tapeSource.atypeInputGainDb;
        p.atypeOutputGainDb = tapeSource.atypeOutputGainDb;
    }

    const auto& deessSource =
        !a.deessBypass ? a : b;

    p.deessBypass =
        a.deessBypass && b.deessBypass;

    if (!p.deessBypass)
    {
        p.deessReferenceHz = deessSource.deessReferenceHz;
        p.deessIntensity = deessSource.deessIntensity;
        p.deessAverageOffset = deessSource.deessAverageOffset;
    }

    p.masterBypass = false;
    p.mixBypass = true;
    p.outputDb = 0.f;

    return p;
}

CaseConfig makeCase(int i)
{
    static constexpr int rates[] =
        { 44100, 48000, 88200, 96000, 192000 };

    static constexpr int blocks[] =
        { 32, 64, 128, 256, 512 };

    static constexpr double freqs[] =
    {
        60.0, 100.0, 250.0, 500.0, 1000.0,
        2500.0, 5000.0, 8000.0, 12000.0, 16000.0
    };

    CaseConfig c;
    c.audio.sr = rates[i % 5];
    c.audio.block = blocks[(i / 5) % 5];
    c.audio.freq =
        static_cast<float>(freqs[(i / 25) % 10]);

    if (c.audio.freq >= c.audio.sr * 0.40)
        c.audio.freq =
            static_cast<float>(c.audio.sr * 0.20);

    c.audio.amplitude =
        0.0225f + 0.0025f * static_cast<float>(i % 8);

    c.seed = 1000 + i * 17;
    c.solidState = (i & 1) != 0;

    return c;
}

template <typename T>
void updateWorst(T& target, T value)
{
    target = std::max(target, value);
}

}

int main()
{
    std::ofstream csv("VVChain_audio_probe.csv");
    csv
        << "round,case,sr,block,freq,declared_latency,"
           "base_delay,base_corr,"
           "eq_unity_delay,eq_unity_corr,eq_flat_phase,"
           "eq_active_delay,eq_active_corr,eq_active_phase,"
           "analog_delay,analog_corr,analog_phase,"
           "tape_delay,tape_corr,tape_phase,"
           "ott_delay,ott_corr,ott_phase,"
           "deess_delay,deess_corr,deess_phase,"
           "eq_analog_phase_delta,analog_tape_phase_delta,"
           "eq_ott_phase_delta,ott_tape_phase_delta,"
           "analog_deess_phase_delta,full_chain_phase_delta,"
           "drywet_null_db,eq_unity_null_db,"
           "stereo_analog_db,stereo_ott_db,stereo_full_db\n";

    int failures = 0;
    int checks = 0;

    double worstDelayError = 0.0;
    double worstBaselinePhase = 0.0;
    double worstEqFlatPhase = 0.0;
    double worstEqActivePhase = 0.0;
    double worstAnalogPhase = 0.0;
    double worstTapePhase = 0.0;
    double worstOttPhase = 0.0;
    double worstDeEssPhase = 0.0;
    double worstEqAnalogDelta = 0.0;
    double worstAnalogTapeDelta = 0.0;
    double worstEqOttDelta = 0.0;
    double worstOttTapeDelta = 0.0;
    double worstAnalogDeEssDelta = 0.0;
    double worstFullChainDelta = 0.0;
    double worstDryWetNull = -300.0;
    double worstEqUnityNull = -300.0;
    double worstStereoAnalog = -300.0;
    double worstStereoOtt = -300.0;
    double worstStereoFull = -300.0;

    for (int round = 0; round < kRounds; ++round)
    {
        std::cout
            << "audio_round "
            << (round + 1)
            << "/"
            << kRounds
            << std::endl;

        for (int ci = 0; ci < kCasesPerRound; ++ci)
        {
            const int index =
                round * kCasesPerRound + ci;

            const CaseConfig cfg =
                makeCase(index);

            VVChainDSP dsp;
            dsp.prepare(cfg.audio.sr, cfg.audio.block, 1);

            const auto inputNoise =
                makeNoise(kNoiseSamples, cfg.seed);

            const auto tone =
                makeSine(
                    kToneSamples,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    cfg.audio.amplitude);

            const auto base =
                baseParameters();

            const auto eqUnity =
                makeEqUnity();

            const auto eqActive =
                makeEqActive();

            const auto analog =
                makeAnalog(
                    35.f + static_cast<float>(ci % 4) * 10.f,
                    cfg.solidState);

            const auto tape =
                makeTapeA();

            const auto ott =
                makeOtt();

            const auto deess =
                makeDeEss();

            const auto eqAnalog =
                combine(eqActive, analog);

            const auto analogTape =
                combine(analog, tape);

            const auto eqOtt =
                combine(eqActive, ott);

            const auto ottTape =
                combine(ott, tape);

            const auto analogDeEss =
                combine(analog, deess);

            const auto full =
                [&]()
                {
                    auto p = combine(
                        combine(eqActive, analog),
                        combine(ott, tape));

                    const auto fullWithDeEss =
                        combine(p, deess);

                    return fullWithDeEss;
                }();

            auto mix50 = full;
            mix50.mixBypass = false;
            mix50.dryWet = 50.f;

            const auto baseNoise =
                render(dsp, base, inputNoise, cfg.audio.block);
            const auto baseTone =
                render(dsp, base, tone, cfg.audio.block);

            const int declaredLatency =
                dsp.getLatencySamples();

            const auto eqUnityNoise =
                render(dsp, eqUnity, inputNoise, cfg.audio.block);
            const auto eqUnityTone =
                render(dsp, eqUnity, tone, cfg.audio.block);

            const auto eqActiveNoise =
                render(dsp, eqActive, inputNoise, cfg.audio.block);
            const auto eqActiveTone =
                render(dsp, eqActive, tone, cfg.audio.block);

            const auto analogNoise =
                render(dsp, analog, inputNoise, cfg.audio.block);
            const auto analogTone =
                render(dsp, analog, tone, cfg.audio.block);

            const auto tapeNoise =
                render(dsp, tape, inputNoise, cfg.audio.block);
            const auto tapeTone =
                render(dsp, tape, tone, cfg.audio.block);

            const auto ottNoise =
                render(dsp, ott, inputNoise, cfg.audio.block);
            const auto ottTone =
                render(dsp, ott, tone, cfg.audio.block);

            const auto deessNoise =
                render(dsp, deess, inputNoise, cfg.audio.block);
            const auto deessTone =
                render(dsp, deess, tone, cfg.audio.block);

            const auto eqAnalogNoise =
                render(dsp, eqAnalog, inputNoise, cfg.audio.block);
            const auto eqAnalogTone =
                render(dsp, eqAnalog, tone, cfg.audio.block);

            const auto analogTapeNoise =
                render(dsp, analogTape, inputNoise, cfg.audio.block);
            const auto analogTapeTone =
                render(dsp, analogTape, tone, cfg.audio.block);

            const auto eqOttNoise =
                render(dsp, eqOtt, inputNoise, cfg.audio.block);
            const auto eqOttTone =
                render(dsp, eqOtt, tone, cfg.audio.block);

            const auto ottTapeNoise =
                render(dsp, ottTape, inputNoise, cfg.audio.block);
            const auto ottTapeTone =
                render(dsp, ottTape, tone, cfg.audio.block);

            const auto analogDeEssNoise =
                render(dsp, analogDeEss, inputNoise, cfg.audio.block);
            const auto analogDeEssTone =
                render(dsp, analogDeEss, tone, cfg.audio.block);

            const auto fullNoise =
                render(dsp, full, inputNoise, cfg.audio.block);
            const auto fullTone =
                render(dsp, full, tone, cfg.audio.block);

            const auto mixTone =
                render(dsp, mix50, tone, cfg.audio.block);

            const Measurement baseM =
                measureDelay(inputNoise, baseNoise, declaredLatency);

            const Measurement eqUnityM =
                measureDelay(inputNoise, eqUnityNoise, declaredLatency);

            const Measurement eqActiveM =
                measureDelay(inputNoise, eqActiveNoise, declaredLatency);

            const Measurement analogM =
                measureDelay(inputNoise, analogNoise, declaredLatency);

            const Measurement tapeM =
                measureDelay(inputNoise, tapeNoise, declaredLatency);

            const Measurement ottM =
                measureDelay(inputNoise, ottNoise, declaredLatency);

            const Measurement deessM =
                measureDelay(inputNoise, deessNoise, declaredLatency);

            const Measurement eqAnalogM =
                measureDelay(inputNoise, eqAnalogNoise, declaredLatency);

            const Measurement analogTapeM =
                measureDelay(inputNoise, analogTapeNoise, declaredLatency);

            const Measurement eqOttM =
                measureDelay(inputNoise, eqOttNoise, declaredLatency);

            const Measurement ottTapeM =
                measureDelay(inputNoise, ottTapeNoise, declaredLatency);

            const Measurement analogDeEssM =
                measureDelay(inputNoise, analogDeEssNoise, declaredLatency);

            const Measurement fullM =
                measureDelay(inputNoise, fullNoise, declaredLatency);

            const double basePhase =
                phaseBetweenAligned(
                    tone, 0,
                    baseTone, baseM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqFlatPhase =
                phaseBetweenAligned(
                    baseTone, baseM.lag,
                    eqUnityTone, eqUnityM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqActivePhase =
                phaseBetweenAligned(
                    tone, 0,
                    eqActiveTone, eqActiveM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double analogPhase =
                phaseBetweenAligned(
                    baseTone, baseM.lag,
                    analogTone, analogM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double tapePhase =
                phaseBetweenAligned(
                    baseTone, baseM.lag,
                    tapeTone, tapeM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double ottPhase =
                phaseBetweenAligned(
                    baseTone, baseM.lag,
                    ottTone, ottM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double deessPhase =
                phaseBetweenAligned(
                    baseTone, baseM.lag,
                    deessTone, deessM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqAnalogPhase =
                phaseBetweenAligned(
                    eqActiveTone, eqActiveM.lag,
                    eqAnalogTone, eqAnalogM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double analogTapePhase =
                phaseBetweenAligned(
                    analogTone, analogM.lag,
                    analogTapeTone, analogTapeM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqOttPhase =
                phaseBetweenAligned(
                    eqActiveTone, eqActiveM.lag,
                    eqOttTone, eqOttM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double ottTapePhase =
                phaseBetweenAligned(
                    ottTone, ottM.lag,
                    ottTapeTone, ottTapeM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double analogDeEssPhase =
                phaseBetweenAligned(
                    analogTone, analogM.lag,
                    analogDeEssTone, analogDeEssM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double fullVsBase =
                phaseBetweenAligned(
                    baseTone, baseM.lag,
                    fullTone, fullM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqAnalogDelta =
                wrapDeg(eqAnalogPhase);

            const double analogTapeDelta =
                wrapDeg(analogTapePhase);

            const double eqOttDelta =
                wrapDeg(eqOttPhase);

            const double ottTapeDelta =
                wrapDeg(ottTapePhase);

            const double analogDeEssDelta =
                wrapDeg(analogDeEssPhase);

            const double fullChainDelta =
                wrapDeg(fullVsBase);

            const auto mixExpected =
                expectedMix(
                    baseTone,
                    fullTone,
                    0.5);

            const double dryWetNull =
                nullDb(
                    mixExpected,
                    mixTone,
                    kWarmup,
                    kToneSamples - kWarmup);

            const double eqUnityNull =
                nullDb(
                    baseNoise,
                    eqUnityNoise,
                    kWarmup,
                    kNoiseSamples - kWarmup);

            const double stereoAnalog =
                stereoMismatchDb(
                    inputNoise,
                    analog,
                    cfg.audio.sr,
                    cfg.audio.block);

            const double stereoOtt =
                stereoMismatchDb(
                    inputNoise,
                    ott,
                    cfg.audio.sr,
                    cfg.audio.block);

            const double stereoFull =
                stereoMismatchDb(
                    inputNoise,
                    full,
                    cfg.audio.sr,
                    cfg.audio.block);

            const int delayErrors[] =
            {
                std::abs(baseM.lag - declaredLatency),
                std::abs(eqUnityM.lag - declaredLatency),
                std::abs(eqActiveM.lag - declaredLatency),
                std::abs(analogM.lag - declaredLatency),
                std::abs(tapeM.lag - declaredLatency),
                std::abs(ottM.lag - declaredLatency),
                std::abs(deessM.lag - declaredLatency),
                std::abs(eqAnalogM.lag - declaredLatency),
                std::abs(analogTapeM.lag - declaredLatency),
                std::abs(eqOttM.lag - declaredLatency),
                std::abs(ottTapeM.lag - declaredLatency),
                std::abs(analogDeEssM.lag - declaredLatency),
                std::abs(fullM.lag - declaredLatency)
            };

            const int localWorstDelay =
                *std::max_element(
                    std::begin(delayErrors),
                    std::end(delayErrors));

            updateWorst(worstDelayError, static_cast<double>(localWorstDelay));
            updateWorst(worstBaselinePhase, std::abs(basePhase));
            updateWorst(worstEqFlatPhase, std::abs(eqFlatPhase));
            updateWorst(worstEqActivePhase, std::abs(eqActivePhase));
            updateWorst(worstAnalogPhase, std::abs(analogPhase));
            updateWorst(worstTapePhase, std::abs(tapePhase));
            updateWorst(worstOttPhase, std::abs(ottPhase));
            updateWorst(worstDeEssPhase, std::abs(deessPhase));
            updateWorst(worstEqAnalogDelta, std::abs(eqAnalogDelta));
            updateWorst(worstAnalogTapeDelta, std::abs(analogTapeDelta));
            updateWorst(worstEqOttDelta, std::abs(eqOttDelta));
            updateWorst(worstOttTapeDelta, std::abs(ottTapeDelta));
            updateWorst(worstAnalogDeEssDelta, std::abs(analogDeEssDelta));
            updateWorst(worstFullChainDelta, std::abs(fullChainDelta));
            worstDryWetNull =
                std::max(worstDryWetNull, dryWetNull);
            worstEqUnityNull =
                std::max(worstEqUnityNull, eqUnityNull);
            worstStereoAnalog =
                std::max(worstStereoAnalog, stereoAnalog);
            worstStereoOtt =
                std::max(worstStereoOtt, stereoOtt);
            worstStereoFull =
                std::max(worstStereoFull, stereoFull);

            ++checks;

            bool ok = true;

            const Measurement* latencyChecks[] =
            {
                &baseM, &eqUnityM, &eqActiveM, &analogM, &tapeM,
                &ottM, &deessM, &eqAnalogM, &analogTapeM, &eqOttM,
                &ottTapeM, &analogDeEssM, &fullM
            };

            for (const auto* m : latencyChecks)
                ok &= m->lag >= 0 && m->corr > 0.999;

            ok &= localWorstDelay <= 1;

            // basePhase / eqActivePhase are informational: the true-peak
            // oversampling path has a frequency-dependent linear phase which
            // is part of the common plugin path. Feature neutrality is judged
            // against the measured base output after latency alignment.
            ok &= std::abs(eqFlatPhase) <= kFlatPhasePassDeg;
            ok &= std::abs(eqAnalogDelta) <= kRelativePhasePassDeg;

            ok &= std::abs(analogPhase) <= kResidualPhasePassDeg;
            ok &= std::abs(tapePhase) <= kResidualPhasePassDeg;
            ok &= std::abs(ottPhase) <= kResidualPhasePassDeg;
            ok &= std::abs(deessPhase) <= kResidualPhasePassDeg;

            ok &= std::abs(eqOttDelta) <= kRelativePhasePassDeg;
            ok &= std::abs(analogTapeDelta) <= kRelativePhasePassDeg;
            ok &= std::abs(ottTapeDelta) <= kRelativePhasePassDeg;
            ok &= std::abs(analogDeEssDelta) <= kRelativePhasePassDeg;
            ok &= std::abs(fullChainDelta) <= kResidualPhasePassDeg;

            ok &= eqUnityNull < kUnityNullPassDb;
            ok &= dryWetNull < kUnityNullPassDb;

            ok &= stereoAnalog < kStereoPassDb;
            ok &= stereoOtt < kStereoPassDb;
            ok &= stereoFull < kStereoPassDb;

            if (!ok)
            {
                ++failures;

                std::cerr
                    << "FAIL round=" << round
                    << " case=" << ci
                    << " sr=" << cfg.audio.sr
                    << " block=" << cfg.audio.block
                    << " freq=" << cfg.audio.freq
                    << " latency=" << declaredLatency
                    << " delayWorst=" << localWorstDelay
                    << " phases=["
                    << basePhase << ","
                    << eqFlatPhase << ","
                    << eqActivePhase << ","
                    << analogPhase << ","
                    << tapePhase << ","
                    << ottPhase << ","
                    << deessPhase << "]"
                    << " deltas=["
                    << eqAnalogDelta << ","
                    << analogTapeDelta << ","
                    << eqOttDelta << ","
                    << ottTapeDelta << ","
                    << analogDeEssDelta << ","
                    << fullChainDelta << "]"
                    << " null=["
                    << eqUnityNull << ","
                    << dryWetNull << "]"
                    << " stereo=["
                    << stereoAnalog << ","
                    << stereoOtt << ","
                    << stereoFull << "]"
                    << std::endl;
            }

            csv
                << round << ','
                << ci << ','
                << cfg.audio.sr << ','
                << cfg.audio.block << ','
                << cfg.audio.freq << ','
                << declaredLatency << ','
                << baseM.lag << ','
                << baseM.corr << ','
                << eqUnityM.lag << ','
                << eqUnityM.corr << ','
                << eqFlatPhase << ','
                << eqActiveM.lag << ','
                << eqActiveM.corr << ','
                << eqActivePhase << ','
                << analogM.lag << ','
                << analogM.corr << ','
                << analogPhase << ','
                << tapeM.lag << ','
                << tapeM.corr << ','
                << tapePhase << ','
                << ottM.lag << ','
                << ottM.corr << ','
                << ottPhase << ','
                << deessM.lag << ','
                << deessM.corr << ','
                << deessPhase << ','
                << eqAnalogDelta << ','
                << analogTapeDelta << ','
                << eqOttDelta << ','
                << ottTapeDelta << ','
                << analogDeEssDelta << ','
                << fullChainDelta << ','
                << dryWetNull << ','
                << eqUnityNull << ','
                << stereoAnalog << ','
                << stereoOtt << ','
                << stereoFull
                << '\n';
        }
    }

    csv.close();

    std::cout
        << std::fixed
        << std::setprecision(5)
        << "audio_monitor_rounds: " << kRounds << '\n'
        << "audio_monitor_cases_per_round: " << kCasesPerRound << '\n'
        << "audio_monitor_cases: " << checks << '\n'
        << "audio_monitor_failures: " << failures << '\n'
        << "worst_declared_vs_measured_delay_error_samples: "
        << worstDelayError << '\n'
        << "worst_baseline_phase_deg: "
        << worstBaselinePhase << '\n'
        << "worst_EQ_flat_phase_deg: "
        << worstEqFlatPhase << '\n'
        << "worst_EQ_active_phase_deg: "
        << worstEqActivePhase << '\n'
        << "worst_ANALOG_phase_deg: "
        << worstAnalogPhase << '\n'
        << "worst_TAPE-A_phase_deg: "
        << worstTapePhase << '\n'
        << "worst_OTT_phase_deg: "
        << worstOttPhase << '\n'
        << "worst_DeEsser_phase_deg: "
        << worstDeEssPhase << '\n'
        << "worst_EQ_ANALOG_phase_delta_deg: "
        << worstEqAnalogDelta << '\n'
        << "worst_ANALOG_TAPE-A_phase_delta_deg: "
        << worstAnalogTapeDelta << '\n'
        << "worst_EQ_OTT_phase_delta_deg: "
        << worstEqOttDelta << '\n'
        << "worst_OTT_TAPE-A_phase_delta_deg: "
        << worstOttTapeDelta << '\n'
        << "worst_ANALOG_DeEsser_phase_delta_deg: "
        << worstAnalogDeEssDelta << '\n'
        << "worst_FULL_CHAIN_phase_delta_deg: "
        << worstFullChainDelta << '\n'
        << "worst_DRY_WET_null_dB: "
        << worstDryWetNull << '\n'
        << "worst_EQ_unity_null_dB: "
        << worstEqUnityNull << '\n'
        << "worst_stereo_analog_mismatch_dB: "
        << worstStereoAnalog << '\n'
        << "worst_stereo_ott_mismatch_dB: "
        << worstStereoOtt << '\n'
        << "worst_stereo_full_mismatch_dB: "
        << worstStereoFull << '\n';

    return failures == 0 ? 0 : 1;
}
