#include "../Source/DSP/ChainDSP.h"

#include <algorithm>
#include <cmath>
#include <complex>
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
constexpr int kToneSamples = 16384;
constexpr int kWarmup = 1024;
constexpr int kLatencyRadius = 64;
constexpr float kFlatPhasePassDeg = 0.25f;
constexpr float kResidualPhasePassDeg = 8.0f;
constexpr float kRelativePhasePassDeg = 3.0f;
constexpr double kUnityNullPassDb = -120.0;

struct Audio
{
    int sr = 48000;
    int block = 128;
    float freq = 1000.f;
    float amplitude = 0.05f;
};

struct Measurement
{
    int lag = -1;
    double corr = 0.0;
    double phaseDeg = 0.0;
    double gainRatio = 0.0;
};

VVChainDSP::Parameters baseParameters()
{
    VVChainDSP::Parameters p;
    p.eqBypass = true;
    p.masterBypass = true;
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
                                      + seed * 2654435761u);

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

std::vector<float> makeSine(int n, double sr, double freq, double amplitude)
{
    std::vector<float> x((size_t)n);

    for (int i = 0; i < n; ++i)
        x[(size_t)i] = static_cast<float>(
            amplitude
            * std::sin(2.0 * kPi * freq
                        * static_cast<double>(i) / sr));

    return x;
}

std::vector<float> render(VVChainDSP& dsp,
                          const VVChainDSP::Parameters& p,
                          const std::vector<float>& input,
                          int block)
{
    dsp.reset();

    std::vector<float> output(input.size(), 0.f);
    juce::AudioBuffer<float> buffer(2, block);

    for (size_t pos = 0;
         pos < input.size();
         pos += static_cast<size_t>(block))
    {
        const int count = static_cast<int>(
            std::min<size_t>(
                static_cast<size_t>(block),
                input.size() - pos));

        if (buffer.getNumSamples() != count)
            buffer.setSize(2, count, false, false, true);

        buffer.clear();
        buffer.copyFrom(
            0, 0, input.data() + pos, count);
        buffer.copyFrom(
            1, 0, input.data() + pos, count);

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
    if (lag < 0)
        return -1.0;

    if (start + lag + count > static_cast<int>(output.size()))
        return -1.0;

    if (start + count > static_cast<int>(input.size()))
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
            static_cast<double>(
                input[(size_t)(start + i)]) - sx;
        const double y =
            static_cast<double>(
                output[(size_t)(start + lag + i)]) - sy;

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
    const int count =
        std::min(
            kNoiseSamples - start - kLatencyRadius - 1,
            2048);

    const int first =
        std::max(0, expectedLatency - kLatencyRadius);
    const int last =
        std::min(expectedLatency + kLatencyRadius,
                 static_cast<int>(output.size())
                 - start - count);

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

        sum += static_cast<double>(
            audio[(size_t)(start + i)])
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
        static_cast<int>(a.size())
        - start - lagA;
    const int availableB =
        static_cast<int>(b.size())
        - start - lagB;

    const int count =
        std::min({ availableA, availableB, 12000 });

    if (count < 1024)
        return 180.0;

    const auto A =
        projectTone(a, sr, freq, start + lagA, count);
    const auto B =
        projectTone(b, sr, freq, start + lagB, count);

    if (std::abs(A) < 1.0e-12
        || std::abs(B) < 1.0e-12)
        return 180.0;

    return std::atan2(
        std::imag(B * std::conj(A)),
        std::real(B * std::conj(A)))
        * 180.0 / kPi;
}

double wrapDeg(double deg)
{
    while (deg > 180.0)
        deg -= 360.0;

    while (deg < -180.0)
        deg += 360.0;

    return deg;
}

double rms(
    const std::vector<float>& x,
    int start,
    int count)
{
    if (start < 0 || start + count > static_cast<int>(x.size()))
        return 0.0;

    double sum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const double v =
            static_cast<double>(
                x[(size_t)(start + i)]);
        sum += v * v;
    }

    return std::sqrt(
        sum / static_cast<double>(count));
}

double nullDb(
    const std::vector<float>& a,
    const std::vector<float>& b,
    int start,
    int count)
{
    const int maxCount =
        std::min(
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
        const double av =
            static_cast<double>(
                a[(size_t)(start + i)]);
        const double bv =
            static_cast<double>(
                b[(size_t)(start + i)]);

        const double d = av - bv;
        diff += d * d;
        ref += av * av;
    }

    const double ratio =
        std::sqrt(
            diff / std::max(ref, 1.0e-24));

    return 20.0
        * std::log10(
            std::max(ratio, 1.0e-15));
}

double stereoMismatchDb(
    const std::vector<float>& input,
    const VVChainDSP::Parameters& p,
    int sr,
    int block)
{
    VVChainDSP dsp;
    dsp.prepare(sr, block, 2);
    dsp.reset();

    juce::AudioBuffer<float> buffer(2, block);
    double sumDiff = 0.0;
    double sumRef = 0.0;
    size_t pos = 0;

    while (pos < input.size())
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
            sumDiff += d * d;
            sumRef += l * l;
        }

        pos += static_cast<size_t>(count);
    }

    if (sumRef < 1.0e-24)
        return -300.0;

    return 10.0
        * std::log10(
            std::max(sumDiff / sumRef, 1.0e-30));
}

VVChainDSP::Parameters makeEqUnity()
{
    auto p = baseParameters();
    p.eqBypass = false;
    p.eqColorGlobalBypass = true;
    p.gain = { 0.f, 0.f, 0.f, 0.f };
    return p;
}

VVChainDSP::Parameters makeAnalog(float amount, bool ss)
{
    auto p = baseParameters();
    p.eqColorGlobalBypass = false;
    p.eqColor = { amount, amount, amount, amount };
    p.eqColorSolidState =
        { ss, ss, ss, ss };
    return p;
}

VVChainDSP::Parameters makeOtt()
{
    auto p = baseParameters();
    p.ottBypass = false;
    p.ottMix = 100.f;
    p.ottDegree =
        { 80.f, 80.f, 80.f, 80.f };
    p.ottCompThreshold =
        { 0.f, 0.f, 0.f, 0.f };
    p.ottLifterThreshold =
        { -48.f, -48.f, -48.f, -48.f };
    p.ottCompMix =
        { 100.f, 100.f, 100.f, 100.f };
    p.ottLifterMix =
        { 100.f, 100.f, 100.f, 100.f };
    return p;
}

VVChainDSP::Parameters makeTapeA()
{
    auto p = baseParameters();
    p.atypeBypass = false;
    p.atypeMix = 100.f;
    p.atypeDegree =
        { 20.f, 20.f, 35.f, 35.f };
    return p;
}

VVChainDSP::Parameters makeDeEss(bool active)
{
    auto p = baseParameters();
    p.deessBypass = false;
    p.deessReferenceHz = 12500.f;
    p.deessIntensity =
        active ? 24.f : 0.f;
    return p;
}

struct CaseConfig
{
    Audio audio;
    int seed = 1;
    bool ss = false;
};

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
    c.audio.sr =
        rates[i % 5];

    c.audio.block =
        blocks[(i / 5) % 5];

    c.audio.freq =
        static_cast<float>(
            freqs[(i / 25) % 10]);

    if (c.audio.freq
        >= c.audio.sr * 0.45)
        c.audio.freq =
            static_cast<float>(
                c.audio.sr * 0.20);

    c.audio.amplitude =
        0.02f
        + 0.01f
        * static_cast<float>(i % 8);

    c.seed =
        1000 + i * 17;

    c.ss =
        (i & 1) != 0;

    return c;
}

struct Stage
{
    VVChainDSP::Parameters params;
    std::vector<float> noise;
    std::vector<float> tone;
    Measurement delay;
    double absPhase = 0.0;
    double relPhase = 0.0;
};

}

int main()
{
    std::ofstream csv("VVChain_audio_probe.csv");
    csv << "round,case,sr,block,freq,declared_latency,"
           "baseline_delay,baseline_corr,"
           "eq_delay,eq_corr,eq_flat_phase,eq_flat_rel,"
           "analog_delay,analog_corr,analog_phase,analog_rel,"
           "tape_delay,tape_corr,tape_phase,tape_rel,"
           "ott_delay,ott_corr,ott_phase,ott_rel,"
           "deess_delay,deess_corr,deess_phase,deess_rel,"
           "eq_analog_rel,analog_tape_rel,"
           "drywet_null_db,eq_unity_null_db,"
           "stereo_analog_db,stereo_ott_db\\n";

    int failures = 0;
    int checks = 0;

    double worstDelayError = 0.0;
    double worstBaselinePhase = 0.0;
    double worstEqFlatPhase = 0.0;
    double worstAnalogPhase = 0.0;
    double worstTapePhase = 0.0;
    double worstOttPhase = 0.0;
    double worstDeEssPhase = 0.0;
    double worstEqAnalog = 0.0;
    double worstAnalogTape = 0.0;
    double worstDryWetNull = -300.0;
    double worstEqUnityNull = -300.0;
    double worstStereoAnalog = -300.0;
    double worstStereoOtt = -300.0;

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
            dsp.prepare(
                cfg.audio.sr,
                cfg.audio.block,
                2);

            const auto inputNoise =
                makeNoise(
                    kNoiseSamples,
                    cfg.seed);

            const auto tone =
                makeSine(
                    kToneSamples,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    cfg.audio.amplitude);

            const auto base =
                baseParameters();

            const auto eq =
                makeEqUnity();

            const auto analog =
                makeAnalog(
                    35.f
                    + static_cast<float>(ci % 4) * 10.f,
                    cfg.ss);

            const auto tape =
                makeTapeA();

            const auto ott =
                makeOtt();

            const auto deess =
                makeDeEss((ci % 3) == 0);

            auto eqAnalog =
                eq;

            eqAnalog.eqColorGlobalBypass = false;
            eqAnalog.eqColor =
                { analog.eqColor[0],
                  analog.eqColor[0],
                  analog.eqColor[0],
                  analog.eqColor[0] };
            eqAnalog.eqColorSolidState =
                { cfg.ss, cfg.ss, cfg.ss, cfg.ss };

            auto analogTape =
                analog;
            analogTape.atypeBypass = false;
            analogTape.atypeMix = 100.f;
            analogTape.atypeDegree =
                { 10.f, 15.f, 25.f, 25.f };

            auto mix50 =
                base;
            mix50.mixBypass = false;
            mix50.dryWet = 50.f;

            const auto baseNoise =
                render(
                    dsp, base,
                    inputNoise,
                    cfg.audio.block);

            const int declaredLatency =
                dsp.getLatencySamples();

            const auto baseTone =
                render(
                    dsp, base,
                    tone,
                    cfg.audio.block);

            const auto eqNoise =
                render(
                    dsp, eq,
                    inputNoise,
                    cfg.audio.block);

            const auto eqTone =
                render(
                    dsp, eq,
                    tone,
                    cfg.audio.block);

            const auto analogNoise =
                render(
                    dsp, analog,
                    inputNoise,
                    cfg.audio.block);

            const auto analogTone =
                render(
                    dsp, analog,
                    tone,
                    cfg.audio.block);

            const auto tapeNoise =
                render(
                    dsp, tape,
                    inputNoise,
                    cfg.audio.block);

            const auto tapeTone =
                render(
                    dsp, tape,
                    tone,
                    cfg.audio.block);

            const auto ottNoise =
                render(
                    dsp, ott,
                    inputNoise,
                    cfg.audio.block);

            const auto ottTone =
                render(
                    dsp, ott,
                    tone,
                    cfg.audio.block);

            const auto deessNoise =
                render(
                    dsp, deess,
                    inputNoise,
                    cfg.audio.block);

            const auto deessTone =
                render(
                    dsp, deess,
                    tone,
                    cfg.audio.block);

            const auto eqAnalogTone =
                render(
                    dsp, eqAnalog,
                    tone,
                    cfg.audio.block);

            const auto analogTapeTone =
                render(
                    dsp, analogTape,
                    tone,
                    cfg.audio.block);

            const auto mixTone =
                render(
                    dsp, mix50,
                    tone,
                    cfg.audio.block);

            const Measurement baseM =
                measureDelay(
                    inputNoise,
                    baseNoise,
                    declaredLatency);

            const Measurement eqM =
                measureDelay(
                    inputNoise,
                    eqNoise,
                    declaredLatency);

            const Measurement analogM =
                measureDelay(
                    inputNoise,
                    analogNoise,
                    declaredLatency);

            const Measurement tapeM =
                measureDelay(
                    inputNoise,
                    tapeNoise,
                    declaredLatency);

            const Measurement ottM =
                measureDelay(
                    inputNoise,
                    ottNoise,
                    declaredLatency);

            const Measurement deessM =
                measureDelay(
                    inputNoise,
                    deessNoise,
                    declaredLatency);

            const double baselinePhase =
                phaseBetweenAligned(
                    tone, 0,
                    baseTone, baseM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqFlatPhase =
                phaseBetweenAligned(
                    tone, 0,
                    eqTone, eqM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double analogPhase =
                phaseBetweenAligned(
                    tone, 0,
                    analogTone, analogM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double tapePhase =
                phaseBetweenAligned(
                    tone, 0,
                    tapeTone, tapeM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double ottPhase =
                phaseBetweenAligned(
                    tone, 0,
                    ottTone, ottM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double deessPhase =
                phaseBetweenAligned(
                    tone, 0,
                    deessTone, deessM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqFlatRel =
                phaseBetweenAligned(
                    baseTone, baseM.lag,
                    eqTone, eqM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double analogRel =
                phaseBetweenAligned(
                    baseTone, baseM.lag,
                    analogTone, analogM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double tapeRel =
                phaseBetweenAligned(
                    baseTone, baseM.lag,
                    tapeTone, tapeM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double ottRel =
                phaseBetweenAligned(
                    baseTone, baseM.lag,
                    ottTone, ottM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double deessRel =
                phaseBetweenAligned(
                    baseTone, baseM.lag,
                    deessTone, deessM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqAnalogRel =
                wrapDeg(
                    phaseBetweenAligned(
                        eqTone, eqM.lag,
                        eqAnalogTone, eqM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double analogTapeRel =
                wrapDeg(
                    phaseBetweenAligned(
                        tapeTone, tapeM.lag,
                        analogTapeTone, tapeM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double eqUnityNull =
                nullDb(
                    baseNoise,
                    eqNoise,
                    kWarmup,
                    2048);

            const auto mixNoise =
                render(
                    dsp, mix50,
                    inputNoise,
                    cfg.audio.block);

            const double dryWetNull =
                nullDb(
                    baseNoise,
                    mixNoise,
                    kWarmup,
                    2048);

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

            const int delayErrors[] =
            {
                std::abs(baseM.lag - declaredLatency),
                std::abs(eqM.lag - declaredLatency),
                std::abs(analogM.lag - declaredLatency),
                std::abs(tapeM.lag - declaredLatency),
                std::abs(ottM.lag - declaredLatency),
                std::abs(deessM.lag - declaredLatency)
            };

            const int localWorstDelay =
                *std::max_element(
                    std::begin(delayErrors),
                    std::end(delayErrors));

            worstDelayError =
                std::max<double>(
                    worstDelayError,
                    localWorstDelay);

            worstBaselinePhase =
                std::max(
                    worstBaselinePhase,
                    std::abs(baselinePhase));

            worstEqFlatPhase =
                std::max(
                    worstEqFlatPhase,
                    std::abs(eqFlatPhase));

            worstAnalogPhase =
                std::max(
                    worstAnalogPhase,
                    std::abs(analogPhase));

            worstTapePhase =
                std::max(
                    worstTapePhase,
                    std::abs(tapePhase));

            worstOttPhase =
                std::max(
                    worstOttPhase,
                    std::abs(ottPhase));

            worstDeEssPhase =
                std::max(
                    worstDeEssPhase,
                    std::abs(deessPhase));

            worstEqAnalog =
                std::max(
                    worstEqAnalog,
                    std::abs(eqAnalogRel));

            worstAnalogTape =
                std::max(
                    worstAnalogTape,
                    std::abs(analogTapeRel));

            worstDryWetNull =
                std::max(
                    worstDryWetNull,
                    dryWetNull);

            worstEqUnityNull =
                std::max(
                    worstEqUnityNull,
                    eqUnityNull);

            worstStereoAnalog =
                std::max(
                    worstStereoAnalog,
                    stereoAnalog);

            worstStereoOtt =
                std::max(
                    worstStereoOtt,
                    stereoOtt);

            ++checks;

            bool ok = true;

            ok &= baseM.lag >= 0
                && eqM.lag >= 0
                && analogM.lag >= 0
                && tapeM.lag >= 0
                && ottM.lag >= 0
                && deessM.lag >= 0;

            ok &= localWorstDelay <= 1;

            // The fixed path and EQ-at-unity must have essentially no
            // phase rotation after the measured latency is removed.
            ok &= std::abs(baselinePhase) <= kFlatPhasePassDeg;
            ok &= std::abs(eqFlatPhase) <= kFlatPhasePassDeg;
            ok &= std::abs(eqFlatRel) <= kFlatPhasePassDeg;

            // Active residual processors are allowed to change the waveform,
            // but must not re-introduce a large crossover-only phase rotation.
            ok &= std::abs(analogPhase) <= kResidualPhasePassDeg;
            ok &= std::abs(tapePhase) <= kResidualPhasePassDeg;
            ok &= std::abs(ottPhase) <= kResidualPhasePassDeg;
            ok &= std::abs(deessPhase) <= kResidualPhasePassDeg;

            ok &= std::abs(analogRel) <= kRelativePhasePassDeg;
            ok &= std::abs(tapeRel) <= kRelativePhasePassDeg;
            ok &= std::abs(ottRel) <= kRelativePhasePassDeg;
            ok &= std::abs(deessRel) <= kRelativePhasePassDeg;

            // Cross-feature combinations: adding an otherwise phase-neutral
            // stage must not create a new crossover all-pass phase.
            ok &= std::abs(eqAnalogRel) <= kRelativePhasePassDeg;
            ok &= std::abs(analogTapeRel) <= kRelativePhasePassDeg;

            ok &= eqUnityNull < kUnityNullPassDb;
            ok &= dryWetNull < kUnityNullPassDb;

            // Identical stereo inputs must remain sample-identical.
            ok &= stereoAnalog < -120.0;
            ok &= stereoOtt < -120.0;

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
                    << " delays=["
                    << baseM.lag << ","
                    << eqM.lag << ","
                    << analogM.lag << ","
                    << tapeM.lag << ","
                    << ottM.lag << ","
                    << deessM.lag << "]"
                    << " phaseAbs=["
                    << baselinePhase << ","
                    << eqFlatPhase << ","
                    << analogPhase << ","
                    << tapePhase << ","
                    << ottPhase << ","
                    << deessPhase << "]"
                    << " phaseRel=["
                    << eqAnalogRel << ","
                    << analogTapeRel << "]"
                    << " null=["
                    << eqUnityNull << ","
                    << dryWetNull << "]"
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
                << eqM.lag << ','
                << eqM.corr << ','
                << eqFlatPhase << ','
                << eqFlatRel << ','
                << analogM.lag << ','
                << analogM.corr << ','
                << analogPhase << ','
                << analogRel << ','
                << tapeM.lag << ','
                << tapeM.corr << ','
                << tapePhase << ','
                << tapeRel << ','
                << ottM.lag << ','
                << ottM.corr << ','
                << ottPhase << ','
                << ottRel << ','
                << deessM.lag << ','
                << deessM.corr << ','
                << deessPhase << ','
                << deessRel << ','
                << eqAnalogRel << ','
                << analogTapeRel << ','
                << dryWetNull << ','
                << eqUnityNull << ','
                << stereoAnalog << ','
                << stereoOtt
                << '\n';
        }
    }

    csv.close();

    std::cout
        << std::fixed
        << std::setprecision(5)
        << "audio_monitor_rounds: "
        << kRounds << '\n'
        << "audio_monitor_cases_per_round: "
        << kCasesPerRound << '\n'
        << "audio_monitor_cases: "
        << checks << '\n'
        << "audio_monitor_failures: "
        << failures << '\n'
        << "worst_declared_vs_measured_delay_error_samples: "
        << worstDelayError << '\n'
        << "worst_baseline_phase_deg: "
        << worstBaselinePhase << '\n'
        << "worst_EQ_flat_phase_deg: "
        << worstEqFlatPhase << '\n'
        << "worst_ANALOG_phase_deg: "
        << worstAnalogPhase << '\n'
        << "worst_TAPE-A_phase_deg: "
        << worstTapePhase << '\n'
        << "worst_OTT_phase_deg: "
        << worstOttPhase << '\n'
        << "worst_DeEsser_phase_deg: "
        << worstDeEssPhase << '\n'
        << "worst_EQ_ANALOG_relative_phase_deg: "
        << worstEqAnalog << '\n'
        << "worst_ANALOG_TAPE-A_relative_phase_deg: "
        << worstAnalogTape << '\n'
        << "worst_DRY_WET_null_dB: "
        << worstDryWetNull << '\n'
        << "worst_EQ_unity_null_dB: "
        << worstEqUnityNull << '\n'
        << "worst_stereo_analog_mismatch_dB: "
        << worstStereoAnalog << '\n'
        << "worst_stereo_ott_mismatch_dB: "
        << worstStereoOtt << '\n';

    return failures == 0 ? 0 : 1;
}
