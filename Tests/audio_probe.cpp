#include "../Source/DSP/ChainDSP.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <vector>

namespace
{
constexpr double kPi = 3.1415926535897932384626433832795;

constexpr int kRounds = 10;
constexpr int kCasesPerRound = 50;
constexpr int kTotalCases = kRounds * kCasesPerRound;

constexpr int kProbeSamples = 4096;
constexpr int kWarmup = 512;
constexpr int kLatencyRadius = 4;

constexpr double kFlatPhasePassDeg = 0.75;
constexpr double kResidualPhasePassDeg = 10.0;
constexpr double kRelativePhasePassDeg = 4.0;
constexpr double kLatencyCorrelationPass = 0.999;
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

struct StageResult
{
    std::vector<float> latencyOut;
    std::vector<float> phaseOut;
    Measurement latency;
};

VVChainDSP::Parameters baseParameters()
{
    VVChainDSP::Parameters p;

    // Never use masterBypass for the probe: it intentionally bypasses the
    // preceding feature chain and would invalidate feature-path testing.
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
    p.ottCompThreshold = { -18.f, -18.f, -18.f, -18.f };
    p.ottLifterThreshold = { -42.f, -42.f, -42.f, -42.f };
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

std::vector<float> makeLatencyNoise(int n, int seed)
{
    std::vector<float> x((size_t)n);

    uint32_t state =
        static_cast<uint32_t>(
            0x12345678u
            + static_cast<uint32_t>(seed) * 2654435761u);

    for (float& value : x)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;

        const float bipolar =
            static_cast<float>(state & 0x00ffffffu)
            / 16777215.0f * 2.0f - 1.0f;

        // Well below OTT / dynamics thresholds. This is intentionally a
        // timing code, not a musical signal.
        value = bipolar * 0.0015f;
    }

    return x;
}

std::vector<float> makeTone(
    int n,
    double sr,
    double freq)
{
    std::vector<float> x((size_t)n);

    constexpr float amplitude = 0.02f;

    for (int i = 0; i < n; ++i)
    {
        x[(size_t)i] =
            amplitude
            * std::sin(
                2.0 * kPi
                * freq
                * static_cast<double>(i) / sr);
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

    const int channelCount = stereo ? 2 : 1;
    std::vector<float> output(input.size(), 0.f);
    juce::AudioBuffer<float> buffer(channelCount, block);

    for (size_t pos = 0;
         pos < input.size();
         pos += static_cast<size_t>(block))
    {
        const int count =
            static_cast<int>(
                std::min<size_t>(
                    static_cast<size_t>(block),
                    input.size() - pos));

        if (buffer.getNumSamples() != count)
        {
            buffer.setSize(
                channelCount,
                count,
                false,
                false,
                true);
        }

        buffer.clear();
        buffer.copyFrom(
            0, 0,
            input.data() + pos,
            count);

        if (stereo)
        {
            buffer.copyFrom(
                1, 0,
                input.data() + pos,
                count);
        }

        dsp.process(buffer, p);

        for (int n = 0; n < count; ++n)
        {
            output[pos + static_cast<size_t>(n)] =
                buffer.getSample(0, n);
        }
    }

    return output;
}

StageResult renderStage(
    VVChainDSP& dsp,
    const VVChainDSP::Parameters& p,
    const std::vector<float>& latencyInput,
    const std::vector<float>& phaseInput,
    int block,
    int declaredLatency)
{
    StageResult result;

    result.latencyOut =
        render(
            dsp,
            p,
            latencyInput,
            block);

    result.phaseOut =
        render(
            dsp,
            p,
            phaseInput,
            block);

    result.latency =
        [&]()
        {
            Measurement m;

            const int start = kWarmup;
            const int available =
                static_cast<int>(result.latencyOut.size())
                - start
                - declaredLatency
                - kLatencyRadius
                - 1;

            // Leave enough output samples after the candidate lag. The prior
            // version consumed the entire tail, which silently collapsed the
            // correlation search to lag=0 and falsely reported huge latency
            // errors.
            const int count =
                std::min(
                    2048,
                    std::max(512, available));

            const int first =
                std::max(
                    0,
                    declaredLatency - kLatencyRadius);

            const int last =
                std::min(
                    declaredLatency + kLatencyRadius,
                    static_cast<int>(result.latencyOut.size())
                    - start
                    - count);

            double best = -2.0;

            for (int lag = first; lag <= last; ++lag)
            {
                double sx = 0.0;
                double sy = 0.0;

                for (int i = 0; i < count; ++i)
                {
                    sx += latencyInput[
                        static_cast<size_t>(start + i)];

                    sy += result.latencyOut[
                        static_cast<size_t>(start + lag + i)];
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
                            latencyInput[
                                static_cast<size_t>(start + i)])
                        - sx;

                    const double y =
                        static_cast<double>(
                            result.latencyOut[
                                static_cast<size_t>(start + lag + i)])
                        - sy;

                    xx += x * x;
                    yy += y * y;
                    xy += x * y;
                }

                const double corr =
                    (xx < 1.0e-24 || yy < 1.0e-24)
                        ? 0.0
                        : xy / std::sqrt(xx * yy);

                if (corr > best)
                {
                    best = corr;
                    m.lag = lag;
                }
            }

            m.corr = best;
            return m;
        }();

    return result;
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
            2.0 * kPi
            * freq
            * static_cast<double>(i) / sr;

        const double window =
            0.5
            - 0.5
            * std::cos(
                2.0 * kPi
                * static_cast<double>(i)
                / static_cast<double>(count - 1));

        sum +=
            static_cast<double>(
                audio[
                    static_cast<size_t>(start + i)])
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
        static_cast<int>(a.size())
        - start
        - lagA;

    const int availableB =
        static_cast<int>(b.size())
        - start
        - lagB;

    const int count =
        std::min(
            { availableA,
              availableB,
              kProbeSamples - start });

    if (count < 1024)
        return 180.0;

    const auto A =
        projectTone(
            a,
            sr,
            freq,
            start + lagA,
            count);

    const auto B =
        projectTone(
            b,
            sr,
            freq,
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

double wrapDeg(double value)
{
    while (value > 180.0)
        value -= 360.0;

    while (value < -180.0)
        value += 360.0;

    return value;
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
    double reference = 0.0;

    for (int i = 0; i < maxCount; ++i)
    {
        const double av =
            a[static_cast<size_t>(start + i)];

        const double bv =
            b[static_cast<size_t>(start + i)];

        const double d = av - bv;

        diff += d * d;
        reference += av * av;
    }

    const double ratio =
        std::sqrt(
            diff / std::max(reference, 1.0e-24));

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

    juce::AudioBuffer<float> buffer(2, block);

    double diff = 0.0;
    double reference = 0.0;

    for (size_t pos = 0;
         pos < input.size();
         pos += static_cast<size_t>(block))
    {
        const int count =
            static_cast<int>(
                std::min<size_t>(
                    static_cast<size_t>(block),
                    input.size() - pos));

        if (buffer.getNumSamples() != count)
        {
            buffer.setSize(
                2,
                count,
                false,
                false,
                true);
        }

        buffer.clear();
        buffer.copyFrom(
            0, 0,
            input.data() + pos,
            count);
        buffer.copyFrom(
            1, 0,
            input.data() + pos,
            count);

        dsp.process(buffer, p);

        for (int n = 0; n < count; ++n)
        {
            const double left =
                buffer.getSample(0, n);

            const double right =
                buffer.getSample(1, n);

            const double d = left - right;

            diff += d * d;
            reference += left * left;
        }
    }

    if (reference < 1.0e-24)
        return -300.0;

    return 10.0
        * std::log10(
            std::max(
                diff / reference,
                1.0e-30));
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

VVChainDSP::Parameters makeOttFlat()
{
    auto p = baseParameters();
    p.ottBypass = false;
    p.ottMix = 100.f;
    p.ottDegree = { 0.f, 0.f, 0.f, 0.f };
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

VVChainDSP::Parameters makeDeEssFlat()
{
    auto p = baseParameters();
    p.deessBypass = false;
    p.deessReferenceHz = 12500.f;
    p.deessIntensity = 0.f;
    return p;
}

VVChainDSP::Parameters makeAllNeutral()
{
    auto p = baseParameters();

    p.eqBypass = false;
    p.gain = { 0.f, 0.f, 0.f, 0.f };

    p.eqColorGlobalBypass = false;
    p.eqColor = { 0.f, 0.f, 0.f, 0.f };

    p.ottBypass = false;
    p.ottDegree = { 0.f, 0.f, 0.f, 0.f };

    p.atypeBypass = false;
    p.atypeDegree = { 0.f, 0.f, 0.f, 0.f };

    p.deessBypass = false;
    p.deessIntensity = 0.f;

    p.masterBypass = false;
    p.mixBypass = true;

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
        p.eqColorSolidState =
            colorSource.eqColorSolidState;
    }

    const auto& ottSource =
        !a.ottBypass ? a : b;

    p.ottBypass =
        a.ottBypass && b.ottBypass;

    if (!p.ottBypass)
    {
        p.ottBandBypass = ottSource.ottBandBypass;
        p.ottDegree = ottSource.ottDegree;
        p.ottCompThreshold =
            ottSource.ottCompThreshold;
        p.ottLifterThreshold =
            ottSource.ottLifterThreshold;
        p.ottCompMix = ottSource.ottCompMix;
        p.ottLifterMix = ottSource.ottLifterMix;
        p.ottBandLevelDb =
            ottSource.ottBandLevelDb;
        p.ottMix = ottSource.ottMix;
        p.ottInputGainDb =
            ottSource.ottInputGainDb;
        p.ottOutputGainDb =
            ottSource.ottOutputGainDb;
        p.ottGateThresholdDb =
            ottSource.ottGateThresholdDb;
        p.ottClipper = ottSource.ottClipper;
    }

    const auto& tapeSource =
        !a.atypeBypass ? a : b;

    p.atypeBypass =
        a.atypeBypass && b.atypeBypass;

    if (!p.atypeBypass)
    {
        p.atypeBandBypass =
            tapeSource.atypeBandBypass;
        p.atypeDegree =
            tapeSource.atypeDegree;
        p.atypeBandLevelDb =
            tapeSource.atypeBandLevelDb;
        p.atypeAttackMs =
            tapeSource.atypeAttackMs;
        p.atypeReleaseMs =
            tapeSource.atypeReleaseMs;
        p.atypeMix = tapeSource.atypeMix;
        p.atypeInputGainDb =
            tapeSource.atypeInputGainDb;
        p.atypeOutputGainDb =
            tapeSource.atypeOutputGainDb;
    }

    const auto& deessSource =
        !a.deessBypass ? a : b;

    p.deessBypass =
        a.deessBypass && b.deessBypass;

    if (!p.deessBypass)
    {
        p.deessReferenceHz =
            deessSource.deessReferenceHz;
        p.deessIntensity =
            deessSource.deessIntensity;
        p.deessAverageOffset =
            deessSource.deessAverageOffset;
    }

    p.masterBypass = false;
    p.mixBypass = true;
    p.outputDb = 0.f;

    return p;
}

CaseConfig makeCase(int index)
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
        rates[index % 5];

    c.audio.block =
        blocks[(index / 5) % 5];

    c.audio.freq =
        static_cast<float>(
            freqs[(index / 25) % 10]);

    if (c.audio.freq
        >= c.audio.sr * 0.40)
    {
        c.audio.freq =
            static_cast<float>(
                c.audio.sr * 0.20);
    }

    if (c.audio.sr >= 176400
        && c.audio.freq < 250.f)
    {
        c.audio.freq = 250.f;
    }

    c.seed =
        1000 + index * 17;

    c.solidState =
        (index & 1) != 0;

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
           "base_lag,base_corr,base_phase,"
           "eq_unity_lag,eq_unity_corr,eq_flat_phase,"
           "eq_active_lag,eq_active_corr,eq_active_phase,"
           "analog_lag,analog_corr,analog_phase,"
           "tape_lag,tape_corr,tape_phase,"
           "ott_lag,ott_corr,ott_phase,"
           "ott_flat_lag,ott_flat_corr,ott_flat_phase,"
           "deess_lag,deess_corr,deess_phase,"
           "deess_flat_lag,deess_flat_corr,deess_flat_phase,"
           "all_neutral_lag,all_neutral_corr,all_neutral_phase,"
           "eq_analog_phase_delta,analog_tape_phase_delta,"
           "eq_ott_phase_delta,eq_ott_flat_phase_delta,"
           "ott_tape_phase_delta,"
           "analog_deess_phase_delta,analog_deess_flat_phase_delta,"
           "full_chain_phase_delta,drywet_null_db,eq_unity_null_db,"
           "stereo_full_mismatch_db\n";

    int failures = 0;
    int checks = 0;

    double worstDelayError = 0.0;
    double worstBasePhase = 0.0;
    double worstEqFlatPhase = 0.0;
    double worstEqActivePhase = 0.0;
    double worstAnalogPhase = 0.0;
    double worstTapePhase = 0.0;
    double worstOttPhase = 0.0;
    double worstOttFlatPhase = 0.0;
    double worstDeEssPhase = 0.0;
    double worstDeEssFlatPhase = 0.0;
    double worstAllNeutralPhase = 0.0;
    double worstEqAnalogDelta = 0.0;
    double worstAnalogTapeDelta = 0.0;
    double worstEqOttDelta = 0.0;
    double worstEqOttFlatDelta = 0.0;
    double worstOttTapeDelta = 0.0;
    double worstAnalogDeEssDelta = 0.0;
    double worstAnalogDeEssFlatDelta = 0.0;
    double worstFullChainDelta = 0.0;
    double worstDryWetNull = -300.0;
    double worstEqUnityNull = -300.0;
    double worstStereoFull = -300.0;

    for (int round = 0;
         round < kRounds;
         ++round)
    {
        std::cout
            << "audio_round "
            << (round + 1)
            << "/"
            << kRounds
            << std::endl;

        for (int ci = 0;
             ci < kCasesPerRound;
             ++ci)
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

            const auto latencyInput =
                makeLatencyNoise(
                    kProbeSamples,
                    cfg.seed);

            const auto phaseInput =
                makeTone(
                    kProbeSamples,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const auto base =
                baseParameters();

            const auto eqUnity =
                makeEqUnity();

            const auto eqActive =
                makeEqActive();

            const auto analog =
                makeAnalog(
                    35.f
                    + static_cast<float>(ci % 4) * 10.f,
                    cfg.solidState);

            const auto tape =
                makeTapeA();

            const auto ott =
                makeOtt();

            const auto ottFlat =
                makeOttFlat();

            const auto deess =
                makeDeEss();

            const auto deessFlat =
                makeDeEssFlat();

            const auto allNeutral =
                makeAllNeutral();

            const auto eqAnalog =
                combine(eqActive, analog);

            const auto analogTape =
                combine(analog, tape);

            const auto eqOtt =
                combine(eqActive, ott);

            const auto eqOttFlat =
                combine(eqActive, ottFlat);

            const auto ottTape =
                combine(ott, tape);

            const auto analogDeEss =
                combine(analog, deess);

            const auto analogDeEssFlat =
                combine(analog, deessFlat);

            auto full =
                combine(
                    combine(eqActive, analog),
                    combine(ott, tape));

            full =
                combine(full, deess);

            auto mixFlat = base;
            mixFlat.mixBypass = false;
            mixFlat.dryWet = 50.f;

            const int declaredLatency =
                dsp.getLatencySamples();

            const auto baseResult =
                renderStage(
                    dsp, base,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto eqUnityResult =
                renderStage(
                    dsp, eqUnity,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto eqActiveResult =
                renderStage(
                    dsp, eqActive,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto analogResult =
                renderStage(
                    dsp, analog,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto tapeResult =
                renderStage(
                    dsp, tape,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto ottResult =
                renderStage(
                    dsp, ott,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto ottFlatResult =
                renderStage(
                    dsp, ottFlat,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto deessResult =
                renderStage(
                    dsp, deess,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto deessFlatResult =
                renderStage(
                    dsp, deessFlat,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto allNeutralResult =
                renderStage(
                    dsp, allNeutral,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto eqAnalogResult =
                renderStage(
                    dsp, eqAnalog,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto analogTapeResult =
                renderStage(
                    dsp, analogTape,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto eqOttResult =
                renderStage(
                    dsp, eqOtt,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto eqOttFlatResult =
                renderStage(
                    dsp, eqOttFlat,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto ottTapeResult =
                renderStage(
                    dsp, ottTape,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto analogDeEssResult =
                renderStage(
                    dsp, analogDeEss,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto analogDeEssFlatResult =
                renderStage(
                    dsp, analogDeEssFlat,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto fullResult =
                renderStage(
                    dsp, full,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto mixResult =
                renderStage(
                    dsp, mixFlat,
                    latencyInput,
                    phaseInput,
                    cfg.audio.block,
                    declaredLatency);

            const auto& baseM =
                baseResult.latency;

            const auto& eqUnityM =
                eqUnityResult.latency;

            const auto& eqActiveM =
                eqActiveResult.latency;

            const auto& analogM =
                analogResult.latency;

            const auto& tapeM =
                tapeResult.latency;

            const auto& ottM =
                ottResult.latency;

            const auto& ottFlatM =
                ottFlatResult.latency;

            const auto& deessM =
                deessResult.latency;

            const auto& deessFlatM =
                deessFlatResult.latency;

            const auto& allNeutralM =
                allNeutralResult.latency;

            const auto& eqAnalogM =
                eqAnalogResult.latency;

            const auto& analogTapeM =
                analogTapeResult.latency;

            const auto& eqOttM =
                eqOttResult.latency;

            const auto& eqOttFlatM =
                eqOttFlatResult.latency;

            const auto& ottTapeM =
                ottTapeResult.latency;

            const auto& analogDeEssM =
                analogDeEssResult.latency;

            const auto& analogDeEssFlatM =
                analogDeEssFlatResult.latency;

            const auto& fullM =
                fullResult.latency;

            const double basePhase =
                phaseBetweenAligned(
                    phaseInput,
                    0,
                    baseResult.phaseOut,
                    baseM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqFlatPhase =
                phaseBetweenAligned(
                    baseResult.phaseOut,
                    baseM.lag,
                    eqUnityResult.phaseOut,
                    eqUnityM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqActivePhase =
                phaseBetweenAligned(
                    phaseInput,
                    0,
                    eqActiveResult.phaseOut,
                    eqActiveM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double analogPhase =
                phaseBetweenAligned(
                    baseResult.phaseOut,
                    baseM.lag,
                    analogResult.phaseOut,
                    analogM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double tapePhase =
                phaseBetweenAligned(
                    baseResult.phaseOut,
                    baseM.lag,
                    tapeResult.phaseOut,
                    tapeM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double ottPhase =
                phaseBetweenAligned(
                    baseResult.phaseOut,
                    baseM.lag,
                    ottResult.phaseOut,
                    ottM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double ottFlatPhase =
                phaseBetweenAligned(
                    baseResult.phaseOut,
                    baseM.lag,
                    ottFlatResult.phaseOut,
                    ottFlatM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double deessPhase =
                phaseBetweenAligned(
                    baseResult.phaseOut,
                    baseM.lag,
                    deessResult.phaseOut,
                    deessM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double deessFlatPhase =
                phaseBetweenAligned(
                    baseResult.phaseOut,
                    baseM.lag,
                    deessFlatResult.phaseOut,
                    deessFlatM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double allNeutralPhase =
                phaseBetweenAligned(
                    baseResult.phaseOut,
                    baseM.lag,
                    allNeutralResult.phaseOut,
                    allNeutralM.lag,
                    cfg.audio.sr,
                    cfg.audio.freq);

            const double eqAnalogDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        eqActiveResult.phaseOut,
                        eqActiveM.lag,
                        eqAnalogResult.phaseOut,
                        eqAnalogM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double analogTapeDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        analogResult.phaseOut,
                        analogM.lag,
                        analogTapeResult.phaseOut,
                        analogTapeM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double eqOttDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        eqActiveResult.phaseOut,
                        eqActiveM.lag,
                        eqOttResult.phaseOut,
                        eqOttM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double eqOttFlatDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        eqActiveResult.phaseOut,
                        eqActiveM.lag,
                        eqOttFlatResult.phaseOut,
                        eqOttFlatM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double ottTapeDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        ottResult.phaseOut,
                        ottM.lag,
                        ottTapeResult.phaseOut,
                        ottTapeM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double analogDeEssDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        analogResult.phaseOut,
                        analogM.lag,
                        analogDeEssResult.phaseOut,
                        analogDeEssM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double analogDeEssFlatDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        analogResult.phaseOut,
                        analogM.lag,
                        analogDeEssFlatResult.phaseOut,
                        analogDeEssFlatM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double fullChainDelta =
                wrapDeg(
                    phaseBetweenAligned(
                        baseResult.phaseOut,
                        baseM.lag,
                        fullResult.phaseOut,
                        fullM.lag,
                        cfg.audio.sr,
                        cfg.audio.freq));

            const double dryWetNull =
                nullDb(
                    baseResult.phaseOut,
                    mixResult.phaseOut,
                    kWarmup,
                    kProbeSamples - kWarmup);

            const double eqUnityNull =
                nullDb(
                    baseResult.phaseOut,
                    eqUnityResult.phaseOut,
                    kWarmup,
                    kProbeSamples - kWarmup);

            const double stereoFull =
                stereoMismatchDb(
                    phaseInput,
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
                std::abs(ottFlatM.lag - declaredLatency),
                std::abs(deessM.lag - declaredLatency),
                std::abs(deessFlatM.lag - declaredLatency),
                std::abs(allNeutralM.lag - declaredLatency),
                std::abs(eqAnalogM.lag - declaredLatency),
                std::abs(analogTapeM.lag - declaredLatency),
                std::abs(eqOttM.lag - declaredLatency),
                std::abs(eqOttFlatM.lag - declaredLatency),
                std::abs(ottTapeM.lag - declaredLatency),
                std::abs(analogDeEssM.lag - declaredLatency),
                std::abs(analogDeEssFlatM.lag - declaredLatency),
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
                worstOttFlatPhase,
                std::abs(ottFlatPhase));

            updateWorst(
                worstDeEssPhase,
                std::abs(deessPhase));

            updateWorst(
                worstDeEssFlatPhase,
                std::abs(deessFlatPhase));

            updateWorst(
                worstAllNeutralPhase,
                std::abs(allNeutralPhase));

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
                worstEqOttFlatDelta,
                std::abs(eqOttFlatDelta));

            updateWorst(
                worstOttTapeDelta,
                std::abs(ottTapeDelta));

            updateWorst(
                worstAnalogDeEssDelta,
                std::abs(analogDeEssDelta));

            updateWorst(
                worstAnalogDeEssFlatDelta,
                std::abs(analogDeEssFlatDelta));

            updateWorst(
                worstFullChainDelta,
                std::abs(fullChainDelta));

            worstDryWetNull =
                std::max(
                    worstDryWetNull,
                    dryWetNull);

            worstEqUnityNull =
                std::max(
                    worstEqUnityNull,
                    eqUnityNull);

            worstStereoFull =
                std::max(
                    worstStereoFull,
                    stereoFull);

            ++checks;

            const Measurement* latencyChecks[] =
            {
                &baseM,
                &eqUnityM,
                &eqActiveM,
                &analogM,
                &tapeM,
                &ottM,
                &ottFlatM,
                &deessM,
                &deessFlatM,
                &allNeutralM,
                &eqAnalogM,
                &analogTapeM,
                &eqOttM,
                &eqOttFlatM,
                &ottTapeM,
                &analogDeEssM,
                &analogDeEssFlatM,
                &fullM
            };

            bool ok = true;

            for (const auto* m : latencyChecks)
            {
                ok &= m->lag >= 0;
                ok &= m->corr >= kLatencyCorrelationPass;
            }

            ok &= localWorstDelay <= 1;

            // Common plugin-path phase is informational. Feature neutrality
            // is tested only against a measured base path.
            ok &= std::abs(eqFlatPhase)
                <= kFlatPhasePassDeg;

            ok &= std::abs(analogPhase)
                <= kResidualPhasePassDeg;

            ok &= std::abs(tapePhase)
                <= kResidualPhasePassDeg;

            // Structural unity tests: no dynamic work, no color, no gain
            // change. These must not add a new phase signature.
            ok &= std::abs(ottFlatPhase)
                <= kFlatPhasePassDeg;

            ok &= std::abs(deessFlatPhase)
                <= kFlatPhasePassDeg;

            ok &= std::abs(allNeutralPhase)
                <= kFlatPhasePassDeg;

            // Cross-feature structural tests.
            ok &= std::abs(eqAnalogDelta)
                <= kRelativePhasePassDeg;

            ok &= std::abs(analogTapeDelta)
                <= kRelativePhasePassDeg;

            ok &= std::abs(eqOttFlatDelta)
                <= kRelativePhasePassDeg;

            ok &= std::abs(analogDeEssFlatDelta)
                <= kRelativePhasePassDeg;

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
                    << "FAIL round="
                    << round
                    << " case="
                    << ci
                    << " sr="
                    << cfg.audio.sr
                    << " block="
                    << cfg.audio.block
                    << " freq="
                    << cfg.audio.freq
                    << " latency="
                    << declaredLatency
                    << " delayWorst="
                    << localWorstDelay
                    << " basePhase="
                    << basePhase
                    << " flat=["
                    << eqFlatPhase
                    << ","
                    << ottFlatPhase
                    << ","
                    << deessFlatPhase
                    << ","
                    << allNeutralPhase
                    << "]"
                    << " crossFlat=["
                    << eqAnalogDelta
                    << ","
                    << analogTapeDelta
                    << ","
                    << eqOttFlatDelta
                    << ","
                    << analogDeEssFlatDelta
                    << "]"
                    << " active=["
                    << eqActivePhase
                    << ","
                    << analogPhase
                    << ","
                    << tapePhase
                    << ","
                    << ottPhase
                    << ","
                    << deessPhase
                    << "]"
                    << " null=["
                    << eqUnityNull
                    << ","
                    << dryWetNull
                    << "]"
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
                << ottFlatM.lag << ','
                << ottFlatM.corr << ','
                << ottFlatPhase << ','
                << deessM.lag << ','
                << deessM.corr << ','
                << deessPhase << ','
                << deessFlatM.lag << ','
                << deessFlatM.corr << ','
                << deessFlatPhase << ','
                << allNeutralM.lag << ','
                << allNeutralM.corr << ','
                << allNeutralPhase << ','
                << eqAnalogDelta << ','
                << analogTapeDelta << ','
                << eqOttDelta << ','
                << eqOttFlatDelta << ','
                << ottTapeDelta << ','
                << analogDeEssDelta << ','
                << analogDeEssFlatDelta << ','
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
        << kRounds
        << '\n'
        << "audio_monitor_cases_per_round: "
        << kCasesPerRound
        << '\n'
        << "audio_monitor_cases: "
        << checks
        << '\n'
        << "audio_monitor_failures: "
        << failures
        << '\n'
        << "worst_declared_vs_measured_delay_error_samples: "
        << worstDelayError
        << '\n'
        << "worst_common_base_phase_deg: "
        << worstBasePhase
        << '\n'
        << "worst_EQ_flat_relative_phase_deg: "
        << worstEqFlatPhase
        << '\n'
        << "worst_EQ_active_phase_deg: "
        << worstEqActivePhase
        << '\n'
        << "worst_ANALOG_relative_phase_deg: "
        << worstAnalogPhase
        << '\n'
        << "worst_TAPE-A_relative_phase_deg: "
        << worstTapePhase
        << '\n'
        << "worst_OTT_active_relative_phase_deg: "
        << worstOttPhase
        << '\n'
        << "worst_OTT_flat_relative_phase_deg: "
        << worstOttFlatPhase
        << '\n'
        << "worst_DeEsser_active_relative_phase_deg: "
        << worstDeEssPhase
        << '\n'
        << "worst_DeEsser_flat_relative_phase_deg: "
        << worstDeEssFlatPhase
        << '\n'
        << "worst_ALL_FEATURES_neutral_phase_deg: "
        << worstAllNeutralPhase
        << '\n'
        << "worst_EQ_ANALOG_phase_delta_deg: "
        << worstEqAnalogDelta
        << '\n'
        << "worst_ANALOG_TAPE-A_phase_delta_deg: "
        << worstAnalogTapeDelta
        << '\n'
        << "worst_EQ_OTT_active_phase_delta_deg: "
        << worstEqOttDelta
        << '\n'
        << "worst_EQ_OTT_neutral_phase_delta_deg: "
        << worstEqOttFlatDelta
        << '\n'
        << "worst_OTT_TAPE-A_phase_delta_deg: "
        << worstOttTapeDelta
        << '\n'
        << "worst_ANALOG_DeEsser_active_phase_delta_deg: "
        << worstAnalogDeEssDelta
        << '\n'
        << "worst_ANALOG_DeEsser_neutral_phase_delta_deg: "
        << worstAnalogDeEssFlatDelta
        << '\n'
        << "worst_FULL_CHAIN_active_phase_delta_deg: "
        << worstFullChainDelta
        << '\n'
        << "worst_DRY_WET_null_dB: "
        << worstDryWetNull
        << '\n'
        << "worst_EQ_unity_null_dB: "
        << worstEqUnityNull
        << '\n'
        << "worst_stereo_full_mismatch_dB: "
        << worstStereoFull
        << '\n';

    return failures == 0 ? 0 : 1;
}
