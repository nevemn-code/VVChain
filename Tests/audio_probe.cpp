#include "../Source/DSP/ChainDSP.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <vector>

namespace
{
constexpr double kPi = 3.1415926535897932384626433832795;

constexpr int kRounds = 10;
constexpr int kCasesPerRound = 50;
constexpr int kTotalCases = kRounds * kCasesPerRound;

constexpr int kProbeSamples = 4096;
constexpr int kWarmup = 512;
constexpr int kLatencyRadius = 64;

constexpr double kFlatPhasePassDeg = 0.75;
constexpr double kResidualPhasePassDeg = 10.0;
constexpr double kRelativePhasePassDeg = 4.0;
constexpr double kUnityNullPassDb = -120.0;
constexpr double kStereoPassDb = -120.0;

struct Audio
{
    int sr = 48000;
    int block = 128;
    float freq = 1000.f;
};

struct CaseConfig
{
    Audio audio;
    int seed = 1;
    bool solidState = false;
};

struct Measurement
{
    int lag = -1;
    double corr = 0.0;
};

VVChainDSP::Parameters baseParameters()
{
    VVChainDSP::Parameters p;

    // The probe intentionally exercises the complete real-time chain.
    // masterBypass=true would replace the preceding stages with the delayed
    // dry path and make all feature phase tests invalid.
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

std::vector<float> makeProbeSignal(int n, double sr, double freq, int seed)
{
    std::vector<float> x((size_t)n);

    uint32_t state =
        static_cast<uint32_t>(
            0x12345678u
            + static_cast<uint32_t>(seed) * 2654435761u);

    for (int i = 0; i < n; ++i)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;

        const float noise =
            (static_cast<float>(state & 0x00ffffffu)
             / 16777215.0f
             * 2.0f - 1.0f)
            * 0.008f;

        const float tone =
            0.0225f
            * std::sin(
                2.0 * kPi * freq
                * static_cast<double>(i) / sr);

        x[(size_t)i] = noise + tone;
    }

    return x;
}

std::vector<float> render(
    VVChainDSP& dsp,
    const VVChainDSP::Parameters& p,
    const std::vector<float>& input,
    int block,
    bool stereo = false)
{
    dsp.reset();

    const int channels = stereo ? 2 : 1;
    std::vector<float> output(input.size(), 0.f);
    juce::AudioBuffer<float> buffer(channels, block);

    for (size_t pos = 0; pos < input.size(); pos += static_cast<size_t>(block))
    {
        const int count = static_cast<int>(
            std::min<size_t>(
                static_cast<size_t>(block),
                input.size() - pos));

        if (buffer.getNumSamples() != count)
            buffer.setSize(channels, count, false, false, true);

        buffer.clear();
        buffer.copyFrom(0, 0, input.data() + pos, count);

        if (stereo)
            buffer.copyFrom(1, 0, input.data() + pos, count);

        dsp.process(buffer, p);

        for (int n = 0; n < count; ++n)
            output[pos + static_cast<size_t>(n)] =
                buffer.getSample(0, n);
    }

    return output;
}

Measurement measureDelay(
    const std::vector<float>& input,
    const std::vector<float>& output,
    int expectedLatency)
{
    Measurement m;

    const int start = kWarmup;
    const int count = std::min(
        kProbeSamples - start - kLatencyRadius - 1,
        2048);

    const int first = std::max(0, expectedLatency - kLatencyRadius);
    const int last = std::min(
        expectedLatency + kLatencyRadius,
        static_cast<int>(output.size()) - start - count);

    double best = -2.0;

    for (int lag = first; lag <= last; ++lag)
    {
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

        const double c =
            (xx < 1.0e-20 || yy < 1.0e-20)
                ? 0.0
                : xy / std::sqrt(xx * yy);

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
            - 0.5
            * std::cos(
                2.0 * kPi
                * static_cast<double>(i)
                / static_cast<double>(count - 1));

        sum += static_cast<double>(
            audio[(size_t)(start + i)])
            * window
            * std::complex<double>(
                std::cos(phase),
                -std::sin(phase));
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
        std::min({ availableA, availableB, kProbeSamples - start });

    if (count < 1024)
        return 180.0;

    const auto A =
        projectTone(
            a, sr, freq,
            start + lagA,
            count);

    const auto B =
        projectTone(
            b, sr, freq,
            start + lagB,
            count);

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
        const double av = a[(size_t)(start + i)];
        const double bv = b[(size_t)(start + i)];
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

std::vector<float> expectedMix(
    const std::vector<float>& dry,
    const std::vector<float>& wet)
{
    const size_t n =
        std::min(dry.size(), wet.size());

    std::vector<float> result(n);

    for (size_t i = 0; i < n; ++i)
        result[i] =
            0.5f * dry[i]
            + 0.5f * wet[i];

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

    // Feed identical samples to L/R and inspect the two output channels
    // directly. A single stereo render is sufficient for this regression.
    dsp.reset();
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

    return 10.0
        * std::log10(
            std::max(diff / ref, 1.0e-30));
}

VVChainDSP::Parameters makeEqUnity()
{
    auto p = baseParameters();
    p.eqBypass = false;
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

VVChainDSP::Parameters makeAnalog(
    float amount,
    bool solidState)
{
    auto p = baseParameters();
    p.eqColorGlobalBypass = false;
    p.eqColor = { amount, amount, amount, amount };
    p.eqColorSolidState =
        { solidState, solidState, solidState, solidState };
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
        a.eqColorGlobalBypass
        && b.eqColorGlobalBypass;

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

    c.audio.sr =
        rates[i % 5];

    c.audio.block =
        blocks[(i / 5) % 5];

    c.audio.freq =
        static_cast<float>(
            freqs[(i / 25) % 10]);

    if (c.audio.sr >= 176400
        && c.audio.freq < 250.f)
        c.audio.freq = 250.f;

    if (c.audio.freq
        >= c.audio.sr * 0.40)
        c.audio.freq =
            static_cast<float>(
                c.audio.sr * 0.20);

    c.seed =
        1000 + i * 17;

    c.solidState =
        (i & 1) != 0;

    return c;
}

void updateWorst(
    double& target,
    double value)
{
    target = std::max(target, value);
}

}

int main()
{
    std::ofstream csv(
        "VVChain_audio_probe.csv");

    csv
        << "round,case,sr,block,freq,declared_latency,"
           "base_delay,base_corr,base_phase,"
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
           "stereo_full_db\n";

    int failures = 0;
    int checks = 0;

    double worstDelayError = 0.0;
    double worstBasePhase = 0.0;
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
            dsp.prepare(
                cfg.audio.sr,
                cfg.audio.block,
                1);

            const auto input =
                makeProbeSignal(
                    kProbeSamples,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    cfg.seed);

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

            auto full =
                combine(
                    combine(eqActive, analog),
                    combine(ott, tape));

            full =
                combine(full, deess);

            auto mix50 = full;
            mix50.mixBypass = false;
            mix50.dryWet = 50.f;

            const auto baseOut =
                render(dsp, base, input, cfg.audio.block);

            const int declaredLatency =
                dsp.getLatencySamples();

            const auto eqUnityOut =
                render(dsp, eqUnity, input, cfg.audio.block);

            const auto eqActiveOut =
                render(dsp, eqActive, input, cfg.audio.block);

            const auto analogOut =
                render(dsp, analog, input, cfg.audio.block);

            const auto tapeOut =
                render(dsp, tape, input, cfg.audio.block);

            const auto ottOut =
                render(dsp, ott, input, cfg.audio.block);

            const auto deessOut =
                render(dsp, deess, input, cfg.audio.block);

            const auto eqAnalogOut =
                render(dsp, eqAnalog, input, cfg.audio.block);

            const auto analogTapeOut =
                render(dsp, analogTape, input, cfg.audio.block);

            const auto eqOttOut =
                render(dsp, eqOtt, input, cfg.audio.block);

            const auto ottTapeOut =
                render(dsp, ottTape, input, cfg.audio.block);

            const auto analogDeEssOut =
                render(dsp, analogDeEss, input, cfg.audio.block);

            const auto fullOut =
                render(dsp, full, input, cfg.audio.block);

            const auto mixOut =
                render(dsp, mix50, input, cfg.audio.block);

            const Measurement baseM =
                measureDelay(
                    input, baseOut, declaredLatency);

            const Measurement eqUnityM =
                measureDelay(
                    input, eqUnityOut, declaredLatency);

            const Measurement eqActiveM =
                measureDelay(
                    input, eqActiveOut, declaredLatency);

            const Measurement analogM =
                measureDelay(
                    input, analogOut, declaredLatency);

            const Measurement tapeM =
                measureDelay(
                    input, tapeOut, declaredLatency);

            const Measurement ottM =
                measureDelay(
                    input, ottOut, declaredLatency);

            const Measurement deessM =
                measureDelay(
                    input, deessOut, declaredLatency);

            const Measurement eqAnalogM =
                measureDelay(
                    input, eqAnalogOut, declaredLatency);

            const Measurement analogTapeM =
                measureDelay(
                    input, analogTapeOut, declaredLatency);

            const Measurement eqOttM =
                measureDelay(
                    input, eqOttOut, declaredLatency);

            const Measurement ottTapeM =
                measureDelay(
                    input, ottTapeOut, declaredLatency);

            const Measurement analogDeEssM =
                measureDelay(
                    input, analogDeEssOut, declaredLatency);

            const Measurement fullM =
                measureDelay(
                    input, fullOut, declaredLatency);

            // basePhase includes the common plugin's true-peak oversampling
            // phase. All feature phase checks below compare against this
            // measured real path after the same latency alignment.
            const double basePhase =
                phaseBetweenAligned(
                    input, 0,
                    baseOut, baseM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqFlatPhase =
                phaseBetweenAligned(
                    baseOut, baseM.lag,
                    eqUnityOut, eqUnityM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqActivePhase =
                phaseBetweenAligned(
                    input, 0,
                    eqActiveOut, eqActiveM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double analogPhase =
                phaseBetweenAligned(
                    baseOut, baseM.lag,
                    analogOut, analogM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double tapePhase =
                phaseBetweenAligned(
                    baseOut, baseM.lag,
                    tapeOut, tapeM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double ottPhase =
                phaseBetweenAligned(
                    baseOut, baseM.lag,
                    ottOut, ottM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double deessPhase =
                phaseBetweenAligned(
                    baseOut, baseM.lag,
                    deessOut, deessM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqAnalogDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        eqActiveOut, eqActiveM.lag,
                        eqAnalogOut, eqAnalogM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double analogTapeDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        analogOut, analogM.lag,
                        analogTapeOut, analogTapeM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double eqOttDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        eqActiveOut, eqActiveM.lag,
                        eqOttOut, eqOttM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double ottTapeDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        ottOut, ottM.lag,
                        ottTapeOut, ottTapeM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double analogDeEssDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        analogOut, analogM.lag,
                        analogDeEssOut, analogDeEssM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double fullChainDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        baseOut, baseM.lag,
                        fullOut, fullM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const auto mixExpected =
                expectedMix(baseOut, fullOut);

            const double dryWetNull =
                nullDb(
                    mixExpected,
                    mixOut,
                    kWarmup,
                    kProbeSamples - kWarmup);

            const double eqUnityNull =
                nullDb(
                    baseOut,
                    eqUnityOut,
                    kWarmup,
                    kProbeSamples - kWarmup);

            const double stereoFull =
                stereoMismatchDb(
                    input,
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

            updateWorst(
                worstDelayError,
                static_cast<double>(localWorstDelay));

            updateWorst(
                worstBasePhase,
                std::abs(basePhase));

            updateWorst(
                worstEqFlatPhase,
                std::abs(eqFlatPhase));

            updateWorst(
                worstEqActivePhase,
                std::abs(eqActivePhase));

            updateWorst(
                worstAnalogPhase,
                std::abs(analogPhase));

            updateWorst(
                worstTapePhase,
                std::abs(tapePhase));

            updateWorst(
                worstOttPhase,
                std::abs(ottPhase));

            updateWorst(
                worstDeEssPhase,
                std::abs(deessPhase));

            updateWorst(
                worstEqAnalogDelta,
                std::abs(eqAnalogDelta));

            updateWorst(
                worstAnalogTapeDelta,
                std::abs(analogTapeDelta));

            updateWorst(
                worstEqOttDelta,
                std::abs(eqOttDelta));

            updateWorst(
                worstOttTapeDelta,
                std::abs(ottTapeDelta));

            updateWorst(
                worstAnalogDeEssDelta,
                std::abs(analogDeEssDelta));

            updateWorst(
                worstFullChainDelta,
                std::abs(fullChainDelta));

            worstDryWetNull =
                std::max(worstDryWetNull, dryWetNull);

            worstEqUnityNull =
                std::max(worstEqUnityNull, eqUnityNull);

            worstStereoFull =
                std::max(worstStereoFull, stereoFull);

            ++checks;

            const Measurement* latencyChecks[] =
            {
                &baseM,
                &eqUnityM,
                &eqActiveM,
                &analogM,
                &tapeM,
                &ottM,
                &deessM,
                &eqAnalogM,
                &analogTapeM,
                &eqOttM,
                &ottTapeM,
                &analogDeEssM,
                &fullM
            };

            bool ok = true;

            for (const auto* m : latencyChecks)
            {
                ok &= m->lag >= 0;
                ok &= m->corr > 0.999;
            }

            ok &= localWorstDelay <= 1;

            // The base path may have non-zero common linear phase from its
            // true-peak oversampling. Feature-neutrality is judged against
            // that measured base path, not against an ideal zero-phase target.
            ok &= std::abs(eqFlatPhase)
                <= kFlatPhasePassDeg;

            ok &= std::abs(analogPhase)
                <= kResidualPhasePassDeg;

            ok &= std::abs(tapePhase)
                <= kResidualPhasePassDeg;

            ok &= std::abs(ottPhase)
                <= kResidualPhasePassDeg;

            ok &= std::abs(deessPhase)
                <= kResidualPhasePassDeg;

            ok &= std::abs(eqAnalogDelta)
                <= kRelativePhasePassDeg;

            ok &= std::abs(analogTapeDelta)
                <= kRelativePhasePassDeg;

            ok &= std::abs(eqOttDelta)
                <= kRelativePhasePassDeg;

            ok &= std::abs(ottTapeDelta)
                <= kRelativePhasePassDeg;

            ok &= std::abs(analogDeEssDelta)
                <= kRelativePhasePassDeg;

            ok &= std::abs(fullChainDelta)
                <= kResidualPhasePassDeg;

            ok &= eqUnityNull
                < kUnityNullPassDb;

            ok &= dryWetNull
                < kUnityNullPassDb;

            ok &= stereoFull
                < kStereoPassDb;

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
                    << " basePhase=" << basePhase
                    << " relPhase=["
                    << eqFlatPhase << ","
                    << analogPhase << ","
                    << tapePhase << ","
                    << ottPhase << ","
                    << deessPhase << "]"
                    << " cross=["
                    << eqAnalogDelta << ","
                    << analogTapeDelta << ","
                    << eqOttDelta << ","
                    << ottTapeDelta << ","
                    << analogDeEssDelta << ","
                    << fullChainDelta << "]"
                    << " null=["
                    << eqUnityNull << ","
                    << dryWetNull << "]"
                    << " stereoFull="
                    << stereoFull
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
                << basePhase << ','
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
                << stereoFull
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
        << "worst_common_base_phase_deg: "
        << worstBasePhase << '\n'
        << "worst_EQ_flat_relative_phase_deg: "
        << worstEqFlatPhase << '\n'
        << "worst_EQ_active_phase_deg: "
        << worstEqActivePhase << '\n'
        << "worst_ANALOG_relative_phase_deg: "
        << worstAnalogPhase << '\n'
        << "worst_TAPE-A_relative_phase_deg: "
        << worstTapePhase << '\n'
        << "worst_OTT_relative_phase_deg: "
        << worstOttPhase << '\n'
        << "worst_DeEsser_relative_phase_deg: "
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
        << "worst_FULL_CHAIN_relative_phase_deg: "
        << worstFullChainDelta << '\n'
        << "worst_DRY_WET_null_dB: "
        << worstDryWetNull << '\n'
        << "worst_EQ_unity_null_dB: "
        << worstEqUnityNull << '\n'
        << "worst_stereo_full_mismatch_dB: "
        << worstStereoFull << '\n';

    return failures == 0 ? 0 : 1;
}
