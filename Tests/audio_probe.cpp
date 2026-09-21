#include "../Source/DSP/ChainDSP.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

namespace
{
constexpr double kPi = 3.1415926535897932384626433832795;
constexpr int kRounds = 10;
constexpr int kCasesPerRound = 50;
constexpr int kTotalCases = kRounds * kCasesPerRound;
constexpr int kLatencySearch = 4096;
constexpr int kWarmup = 2048;
constexpr float kPhasePassDeg = 1.0f;
constexpr float kRelativePhasePassDeg = 0.75f;

struct Audio
{
    int sr = 48000;
    int block = 128;
    float freq = 1000.f;
    float amplitude = 0.1f;
};

struct Measurement
{
    int declaredLatency = 0;
    int measuredLatency = -1;
    double correlation = 0.0;
    double phaseDeg = 0.0;
    double relativePhaseDeg = 0.0;
    double gainRatio = 0.0;
    double nullDb = 0.0;
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
    uint32_t s = static_cast<uint32_t>(0x12345678u + seed * 2654435761u);
    for (float& v : x)
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        const float u = static_cast<float>(s & 0x00ffffffu) / 16777215.0f;
        v = (2.0f * u - 1.0f) * 0.035f;
    }
    return x;
}

std::vector<float> makeSine(int n, double sr, double freq, double amplitude)
{
    std::vector<float> x((size_t)n);
    for (int i = 0; i < n; ++i)
        x[(size_t)i] = static_cast<float>(
            amplitude * std::sin(2.0 * kPi * freq * static_cast<double>(i) / sr));
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

    for (size_t pos = 0; pos < input.size(); pos += static_cast<size_t>(block))
    {
        const int count = static_cast<int>(
            std::min<size_t>(static_cast<size_t>(block), input.size() - pos));

        if (buffer.getNumSamples() != count)
            buffer.setSize(2, count, false, false, true);

        buffer.clear();
        buffer.copyFrom(0, 0, input.data() + pos, count);
        buffer.copyFrom(1, 0, input.data() + pos, count);

        dsp.process(buffer, p);

        for (int n = 0; n < count; ++n)
            output[pos + static_cast<size_t>(n)] = buffer.getSample(0, n);
    }

    return output;
}

double correlationAtLag(const std::vector<float>& input,
                        const std::vector<float>& output,
                        int lag,
                        int start)
{
    const int n = static_cast<int>(std::min(input.size(), output.size()));
    if (lag < 0 || lag >= n - start - 8)
        return -1.0;

    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double syy = 0.0;
    double sxy = 0.0;

    for (int i = start + lag; i < n; ++i)
    {
        const double x = input[(size_t)(i - lag)];
        const double y = output[(size_t)i];
        sx += x;
        sy += y;
    }

    const int count = n - (start + lag);
    if (count < 512)
        return -1.0;

    sx /= count;
    sy /= count;

    for (int i = start + lag; i < n; ++i)
    {
        const double x = input[(size_t)(i - lag)] - sx;
        const double y = output[(size_t)i] - sy;
        sxx += x * x;
        syy += y * y;
        sxy += x * y;
    }

    if (sxx < 1.0e-18 || syy < 1.0e-18)
        return 0.0;

    return sxy / std::sqrt(sxx * syy);
}

int estimateDelay(const std::vector<float>& input,
                  const std::vector<float>& output)
{
    double best = -1.0;
    int bestLag = -1;

    for (int lag = 0; lag <= kLatencySearch; ++lag)
    {
        const double c = correlationAtLag(input, output, lag, kWarmup);
        if (c > best)
        {
            best = c;
            bestLag = lag;
        }
    }

    return bestLag;
}

double rms(const std::vector<float>& x, int start)
{
    if (start >= static_cast<int>(x.size()))
        return 0.0;

    double sum = 0.0;
    for (size_t i = static_cast<size_t>(start); i < x.size(); ++i)
        sum += static_cast<double>(x[i]) * x[i];

    const double count = static_cast<double>(x.size() - static_cast<size_t>(start));
    return std::sqrt(sum / std::max(1.0, count));
}

double nullDb(const std::vector<float>& a,
              const std::vector<float>& b,
              int start)
{
    const int n = static_cast<int>(std::min(a.size(), b.size()));
    double diff = 0.0;
    double ref = 0.0;

    for (int i = start; i < n; ++i)
    {
        const double d = static_cast<double>(a[(size_t)i])
                       - static_cast<double>(b[(size_t)i]);
        diff += d * d;
        ref += static_cast<double>(a[(size_t)i]) * a[(size_t)i];
    }

    const double ratio =
        std::sqrt(diff / std::max(ref, 1.0e-24));
    return 20.0 * std::log10(std::max(ratio, 1.0e-15));
}

double phaseFromSine(const std::vector<float>& y,
                     double sr,
                     double freq,
                     int start)
{
    double s = 0.0;
    double c = 0.0;
    const double w = 2.0 * kPi * freq / sr;

    for (size_t i = static_cast<size_t>(start); i < y.size(); ++i)
    {
        const double t = static_cast<double>(i);
        s += static_cast<double>(y[i]) * std::sin(w * t);
        c += static_cast<double>(y[i]) * std::cos(w * t);
    }

    return std::atan2(-c, s);
}

double wrapDeg(double deg)
{
    while (deg > 180.0) deg -= 360.0;
    while (deg < -180.0) deg += 360.0;
    return deg;
}

Measurement measureTone(VVChainDSP& dsp,
                        const VVChainDSP::Parameters& p,
                        const Audio& a,
                        const std::vector<float>& baseline)
{
    Measurement m;
    const auto input = makeSine(a.sr * 2, a.sr, a.freq, a.amplitude);
    const auto output = render(dsp, p, input, a.block);

    m.declaredLatency = dsp.getLatencySamples();
    m.measuredLatency = estimateDelay(
        makeNoise(static_cast<int>(input.size()), a.sr + a.block),
        output);

    const int lag = std::max(0, m.measuredLatency);
    const int phaseStart = std::max(kWarmup + 4096, lag + kWarmup);

    const double inPhase = phaseFromSine(input, a.sr, a.freq, phaseStart - lag);
    const double outPhase = phaseFromSine(output, a.sr, a.freq, phaseStart);
    m.phaseDeg = wrapDeg((outPhase - inPhase) * 180.0 / kPi);

    double inRms = rms(input, phaseStart - lag);
    double outRms = rms(output, phaseStart);
    m.gainRatio = outRms / std::max(inRms, 1.0e-12);

    (void) baseline;
    return m;
}

std::vector<float> renderNoise(VVChainDSP& dsp,
                               const VVChainDSP::Parameters& p,
                               int sr,
                               int block,
                               int seed)
{
    auto input = makeNoise(sr / 2, seed);
    return render(dsp, p, input, block);
}

double measureFundamentalPhase(VVChainDSP& dsp,
                               const VVChainDSP::Parameters& p,
                               const Audio& a,
                               int lag)
{
    const auto input = makeSine(a.sr * 2, a.sr, a.freq, a.amplitude);
    const auto output = render(dsp, p, input, a.block);
    const int phaseStart = std::max(kWarmup + 4096, lag + kWarmup);

    const double inPhase = phaseFromSine(
        input, a.sr, a.freq, phaseStart - lag);
    const double outPhase = phaseFromSine(
        output, a.sr, a.freq, phaseStart);

    return wrapDeg((outPhase - inPhase) * 180.0 / kPi);
}

VVChainDSP::Parameters makeEq(float gain, float colorAmount)
{
    auto p = baseParameters();
    p.eqBypass = false;
    p.eqColorGlobalBypass = colorAmount <= 0.f;
    p.gain = { gain, 0.f, 0.f, 0.f };
    p.eqColor = { colorAmount, colorAmount, colorAmount, colorAmount };
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
    p.ottDegree = { 80.f, 80.f, 80.f, 80.f };
    p.ottCompThreshold = { 0.f, 0.f, 0.f, 0.f };
    p.ottLifterThreshold = { -48.f, -48.f, -48.f, -48.f };
    p.ottCompMix = { 100.f, 100.f, 100.f, 100.f };
    p.ottLifterMix = { 100.f, 100.f, 100.f, 100.f };
    return p;
}

VVChainDSP::Parameters makeTapeA()
{
    auto p = baseParameters();
    p.atypeBypass = false;
    p.atypeMix = 100.f;
    p.atypeDegree = { 20.f, 20.f, 35.f, 35.f };
    return p;
}

VVChainDSP::Parameters makeDeEss(bool active)
{
    auto p = baseParameters();
    p.deessBypass = false;
    p.deessReferenceHz = 12500.f;
    p.deessIntensity = active ? 24.f : 0.f;
    return p;
}

bool nearlyEqualPhase(double a, double b, double limit)
{
    return std::abs(wrapDeg(a - b)) <= limit;
}

struct CaseConfig
{
    Audio audio;
    int seed = 1;
    bool ss = false;
    float eqGain = 0.f;
};

CaseConfig makeCase(int i)
{
    static constexpr int rates[] = { 44100, 48000, 88200, 96000, 192000 };
    static constexpr int blocks[] = { 32, 64, 128, 256, 512 };
    static constexpr double freqs[] =
    {
        60.0, 100.0, 250.0, 500.0, 1000.0,
        2500.0, 5000.0, 8000.0, 12000.0, 16000.0
    };

    CaseConfig c;
    c.audio.sr = rates[i % 5];
    c.audio.block = blocks[(i / 5) % 5];
    c.audio.freq = static_cast<float>(freqs[(i / 25) % 10]);
    if (c.audio.freq >= c.audio.sr * 0.45)
        c.audio.freq = static_cast<float>(c.audio.sr * 0.20);
    c.audio.amplitude = 0.01f + 0.02f * static_cast<float>(i % 8);
    c.seed = 1000 + i * 17;
    c.ss = (i & 1) != 0;
    c.eqGain = ((i % 13) - 6) * 0.75f;
    return c;
}
}

int main()
{
    std::ofstream csv("VVChain_audio_probe.csv");
    csv << "round,case,sr,block,freq,declared_latency,baseline_delay,"
           "eq_flat_phase,analog_phase,tape_phase,ott_phase,deess_phase,"
           "eq_analog_relative,analog_tape_relative,drywet_null_db,"
           "analog_gain,tape_gain\n";

    int failures = 0;
    int totalChecks = 0;
    double worstDryWetNull = -300.0;
    double worstEqAnalog = 0.0;
    double worstAnalogTape = 0.0;
    int worstDelayError = 0;

    for (int round = 0; round < kRounds; ++round)
    {
        for (int ci = 0; ci < kCasesPerRound; ++ci)
        {
            const int index = round * kCasesPerRound + ci;
            const auto cfg = makeCase(index);

            VVChainDSP dsp;
            dsp.prepare(cfg.audio.sr, cfg.audio.block, 2);

            const auto baselineParams = baseParameters();
            const auto noise = makeNoise(
                cfg.audio.sr / 2, cfg.seed);
            const auto baseline =
                render(dsp, baselineParams, noise, cfg.audio.block);

            const int baselineDelay =
                estimateDelay(noise, baseline);
            const int declaredLatency = dsp.getLatencySamples();

            const auto eqFlatParams = makeEq(0.f, 0.f);
            const auto analogParams =
                makeAnalog(35.f + static_cast<float>(ci % 4) * 10.f, cfg.ss);
            const auto tapeParams = makeTapeA();
            const auto ottParams = makeOtt();
            const auto deessParams = makeDeEss((ci % 3) == 0);

            auto eqAnalogParams =
                makeEq(cfg.eqGain, analogParams.eqColor[0]);
            eqAnalogParams.eqColorSolidState =
                { cfg.ss, cfg.ss, cfg.ss, cfg.ss };
            const auto analogTapeParams = makeAnalog(25.f, cfg.ss);
            analogTapeParams.atypeBypass = false;
            analogTapeParams.atypeMix = 100.f;
            analogTapeParams.atypeDegree = { 10.f, 15.f, 25.f, 25.f };

            const auto eqFlatOut =
                render(dsp, eqFlatParams, makeSine(
                    cfg.audio.sr * 2,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    cfg.audio.amplitude), cfg.audio.block);

            const auto analogOut =
                render(dsp, analogParams, makeSine(
                    cfg.audio.sr * 2,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    cfg.audio.amplitude), cfg.audio.block);

            const auto tapeOut =
                render(dsp, tapeParams, makeSine(
                    cfg.audio.sr * 2,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    cfg.audio.amplitude), cfg.audio.block);

            const auto ottOut =
                render(dsp, ottParams, makeSine(
                    cfg.audio.sr * 2,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    cfg.audio.amplitude), cfg.audio.block);

            const auto deessOut =
                render(dsp, deessParams, makeSine(
                    cfg.audio.sr * 2,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    cfg.audio.amplitude), cfg.audio.block);

            const int eqDelay = estimateDelay(
                noise, render(dsp, eqFlatParams, noise, cfg.audio.block));
            const int analogDelay = estimateDelay(
                noise, render(dsp, analogParams, noise, cfg.audio.block));
            const int tapeDelay = estimateDelay(
                noise, render(dsp, tapeParams, noise, cfg.audio.block));
            const int ottDelay = estimateDelay(
                noise, render(dsp, ottParams, noise, cfg.audio.block));
            const int deessDelay = estimateDelay(
                noise, render(dsp, deessParams, noise, cfg.audio.block));

            const int maxDelayError = std::max({
                std::abs(baselineDelay - declaredLatency),
                std::abs(eqDelay - baselineDelay),
                std::abs(analogDelay - baselineDelay),
                std::abs(tapeDelay - baselineDelay),
                std::abs(ottDelay - baselineDelay),
                std::abs(deessDelay - baselineDelay)
            });
            worstDelayError = std::max(worstDelayError, maxDelayError);

            const double eqFlatPhase =
                measureFundamentalPhase(
                    dsp, eqFlatParams, cfg.audio, baselineDelay);
            const double analogPhase =
                measureFundamentalPhase(
                    dsp, analogParams, cfg.audio, baselineDelay);
            const double tapePhase =
                measureFundamentalPhase(
                    dsp, tapeParams, cfg.audio, baselineDelay);
            const double ottPhase =
                measureFundamentalPhase(
                    dsp, ottParams, cfg.audio, baselineDelay);
            const double deessPhase =
                measureFundamentalPhase(
                    dsp, deessParams, cfg.audio, baselineDelay);

            const auto eqOnlyParams =
                makeEq(cfg.eqGain, 0.f);
            const auto eqOnlyOut =
                render(dsp, eqOnlyParams, makeSine(
                    cfg.audio.sr * 2,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    0.005), cfg.audio.block);
            const auto eqAnalogOut =
                render(dsp, eqAnalogParams, makeSine(
                    cfg.audio.sr * 2,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    0.005), cfg.audio.block);
            const auto tapeOnlyOut =
                render(dsp, tapeParams, makeSine(
                    cfg.audio.sr * 2,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    0.005), cfg.audio.block);
            const auto analogTapeOut =
                render(dsp, analogTapeParams, makeSine(
                    cfg.audio.sr * 2,
                    cfg.audio.sr,
                    cfg.audio.freq,
                    0.005), cfg.audio.block);

            const double eqOnlyPhase =
                phaseFromSine(eqOnlyOut, cfg.audio.sr,
                              cfg.audio.freq, kWarmup + baselineDelay);
            const double eqAnalogPhase =
                phaseFromSine(eqAnalogOut, cfg.audio.sr,
                              cfg.audio.freq, kWarmup + baselineDelay);
            const double tapeOnlyPhase =
                phaseFromSine(tapeOnlyOut, cfg.audio.sr,
                              cfg.audio.freq, kWarmup + baselineDelay);
            const double analogTapePhase =
                phaseFromSine(analogTapeOut, cfg.audio.sr,
                              cfg.audio.freq, kWarmup + baselineDelay);

            const double eqAnalogRelative =
                wrapDeg((eqAnalogPhase - eqOnlyPhase) * 180.0 / kPi);
            const double analogTapeRelative =
                wrapDeg((analogTapePhase - tapeOnlyPhase) * 180.0 / kPi);

            auto mixParams = baseParameters();
            mixParams.masterBypass = true;
            mixParams.mixBypass = false;
            mixParams.dryWet = 50.f;
            const auto mixOut =
                render(dsp, mixParams, noise, cfg.audio.block);
            const auto delayedBaseline = [&]()
            {
                std::vector<float> d(noise.size(), 0.f);
                for (size_t i = static_cast<size_t>(baselineDelay);
                     i < d.size(); ++i)
                    d[i] = noise[i - static_cast<size_t>(baselineDelay)];
                return d;
            }();
            const double mixNull =
                nullDb(mixOut, delayedBaseline, kWarmup + 512);

            worstDryWetNull = std::max(worstDryWetNull, mixNull);
            worstEqAnalog =
                std::max(worstEqAnalog,
                         std::abs(eqAnalogRelative));
            worstAnalogTape =
                std::max(worstAnalogTape,
                         std::abs(analogTapeRelative));

            ++totalChecks;
            bool ok = true;

            ok &= std::abs(baselineDelay - declaredLatency) <= 1;
            ok &= maxDelayError <= 1;
            ok &= std::abs(eqFlatPhase) <= kPhasePassDeg;
            ok &= std::abs(analogPhase) <= kPhasePassDeg;
            ok &= std::abs(tapePhase) <= kPhasePassDeg;
            ok &= std::abs(ottPhase) <= kPhasePassDeg;
            ok &= std::abs(deessPhase) <= 6.0;
            ok &= std::abs(eqAnalogRelative) <= kRelativePhasePassDeg;
            ok &= std::abs(analogTapeRelative) <= kRelativePhasePassDeg;
            ok &= mixNull < -90.0;

            if (!ok)
            {
                ++failures;
                std::cerr
                    << "FAIL round=" << round
                    << " case=" << ci
                    << " sr=" << cfg.audio.sr
                    << " block=" << cfg.audio.block
                    << " freq=" << cfg.audio.freq
                    << " baseline=" << baselineDelay
                    << " declared=" << declaredLatency
                    << " delayErr=" << maxDelayError
                    << " eqFlatPhase=" << eqFlatPhase
                    << " analogPhase=" << analogPhase
                    << " tapePhase=" << tapePhase
                    << " ottPhase=" << ottPhase
                    << " deessPhase=" << deessPhase
                    << " eqAnalogRel=" << eqAnalogRelative
                    << " analogTapeRel=" << analogTapeRelative
                    << " mixNullDb=" << mixNull
                    << std::endl;
            }

            csv << round << ',' << ci << ','
                << cfg.audio.sr << ','
                << cfg.audio.block << ','
                << cfg.audio.freq << ','
                << declaredLatency << ','
                << baselineDelay << ','
                << eqFlatPhase << ','
                << analogPhase << ','
                << tapePhase << ','
                << ottPhase << ','
                << deessPhase << ','
                << eqAnalogRelative << ','
                << analogTapeRelative << ','
                << mixNull << ','
                << rms(analogOut, kWarmup + baselineDelay)
                   / std::max(rms(
                       makeSine(cfg.audio.sr * 2,
                                cfg.audio.sr,
                                cfg.audio.freq,
                                cfg.audio.amplitude),
                       kWarmup), 1.0e-12)
                << ','
                << rms(tapeOut, kWarmup + baselineDelay)
                   / std::max(rms(
                       makeSine(cfg.audio.sr * 2,
                                cfg.audio.sr,
                                cfg.audio.freq,
                                cfg.audio.amplitude),
                       kWarmup), 1.0e-12)
                << '\n';
        }
    }

    csv.close();

    std::cout << std::fixed << std::setprecision(4)
              << "audio_monitor_rounds: " << kRounds << "\n"
              << "audio_monitor_cases_per_round: " << kCasesPerRound << "\n"
              << "audio_monitor_cases: " << totalChecks << "\n"
              << "audio_monitor_failures: " << failures << "\n"
              << "worst_declared_vs_measured_delay_error_samples: "
              << worstDelayError << "\n"
              << "worst_EQ_ANALOG_relative_phase_deg: "
              << worstEqAnalog << "\n"
              << "worst_ANALOG_TAPE-A_relative_phase_deg: "
              << worstAnalogTape << "\n"
              << "worst_DRY_WET_null_dB: "
              << worstDryWetNull << "\n";

    return failures == 0 ? 0 : 1;
}
