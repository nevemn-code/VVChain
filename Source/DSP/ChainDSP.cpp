#include "ChainDSP.h"
#include "../VVChain_DynEQ_Engine.h"

namespace
{
static constexpr float kLifterRatio = 4.0f;
static constexpr float kCompressorRatio = 66.7f;
static constexpr float kGateRatio = 6.0f;
static constexpr float kLifterKneeDb = 6.0f;
static constexpr float kCompressorKneeDb = 6.0f;
static constexpr float kGateKneeDb = 9.0f;
static constexpr double kReferenceSampleRate = 44100.0;
static constexpr double kTwoPi = 6.28318530717958647692;
static constexpr int kMasterBypassRampSamples = 64;

float crossoverQFromOverlap(float overlap)
{
    const float t = juce::jlimit(0.f, 100.f, overlap) / 100.f;
    return 0.90f - 0.35f * t;
}

float dynamicThresholdFromDynamics(float dynamics) noexcept
{
    const float signedDynamics =
        juce::jlimit(-100.f, 100.f, dynamics) * 0.01f;
    // DYNAMICS uses a truly linear bipolar map:
    // -100% = -24 dB threshold, 0% = -12 dB, +100% = 0 dB.
    // Keeping the signed amount itself linear avoids a nonlinear jump
    // when crossing 0%.
    return juce::jlimit(
        -60.f, 0.f,
        -12.f + signedDynamics * 12.f);
}

    // Dynamic EQ threshold is intentionally not a stored parameter.
    // DYNAMICS is the single macro that defines both depth and threshold.

}

void VVChainDSP::updateAnalogPeak(Biquad& filter, double fs, double f0, double gainDb, double q)
{
    // Orfanidis de-cramped parametric EQ. G0 is unity at DC; G1 is
    // calculated to match the equivalent analog response at Nyquist.
    // Q is mapped to the familiar ~3 dB bandwidth convention.
    const double safeF = juce::jlimit(20.0, fs * 0.45, f0);
    const double safeQ = std::max(0.1, q);
    const double w0 = juce::MathConstants<double>::twoPi * safeF / fs;
    const double dw = juce::jlimit(
        1.0e-5,
        juce::MathConstants<double>::pi * 0.98,
        w0 / safeQ);

    if (std::abs(gainDb) < 1.0e-5)
    {
        filter.updateCoefficients(1.0, 0.0, 0.0, 0.0, 0.0);
        return;
    }

    const double G0 = 1.0;
    const double G = std::pow(10.0, gainDb / 20.0);
    const double GB = std::sqrt(std::max(1.0e-12, G));

    const double G2 = G * G;
    const double G0_2 = G0 * G0;
    const double GB2 = GB * GB;

    const double F = std::abs(G2 - GB2);
    const double F00 = std::abs(GB2 - G0_2);
    const double F00safe = std::max(F00, 1.0e-12);
    const double Fsafe = std::max(F, 1.0e-12);

    const double w0pi = w0 * w0 - juce::MathConstants<double>::pi
                                      * juce::MathConstants<double>::pi;

    const double numerator =
        G0_2 * w0pi * w0pi
        + G2 * F00 * juce::MathConstants<double>::pi
            * juce::MathConstants<double>::pi * dw * dw / Fsafe;
    const double denominator =
        w0pi * w0pi
        + F00 * juce::MathConstants<double>::pi
            * juce::MathConstants<double>::pi * dw * dw / Fsafe;

    const double G1 = std::sqrt(std::max(1.0e-18, numerator
                                                     / std::max(1.0e-18, denominator)));

    const double G01 = std::abs(G2 - G0 * G1);
    const double G11 = std::abs(G2 - G1 * G1);
    const double F01 = std::abs(GB2 - G0 * G1);
    const double F11 = std::abs(GB2 - G1 * G1);

    const double tanHalf = std::tan(w0 * 0.5);
    const double W2 = std::sqrt(std::max(1.0e-18, G11
                                           / F00safe)) * tanHalf * tanHalf;

    const double DW =
        (1.0 + std::sqrt(std::max(1.0e-18, F00safe / std::max(F11, 1.0e-12))) * W2)
        * std::tan(dw * 0.5);

    const double C =
        F11 * DW * DW
        - 2.0 * W2
            * (F01 - std::sqrt(std::max(0.0, F00safe * std::max(F11, 0.0))));
    const double D =
        2.0 * W2
        * (G01 - std::sqrt(std::max(0.0, F00safe * std::max(G11, 0.0))));

    const double A =
        std::sqrt(std::max(1.0e-18, (C + D) / Fsafe));
    const double B =
        std::sqrt(std::max(1.0e-18, (G2 * C + GB2 * D) / Fsafe));

    const double norm = 1.0 / std::max(1.0e-12, 1.0 + W2 + A);
    const double b0 = (G1 + G0 * W2 + B) * norm;
    const double b1 = -2.0 * (G1 - G0 * W2) * norm;
    const double b2 = (G1 - B + G0 * W2) * norm;
    const double a1 = -2.0 * (1.0 - W2) * norm;
    const double a2 = (1.0 + W2 - A) * norm;

    // Orfanidis reduces continuously to the conventional design when G1=G0.
    filter.updateCoefficients(b0, b1, b2, a1, a2);
}

void VVChainDSP::updateDynamicPeak(Biquad& filter, double fs, double f0,
                                      double gainDb, double q)
{
    const double safeF = juce::jlimit(10.0, fs * 0.45, f0);
    const double safeQ = juce::jlimit(0.05, 30.0, q);
    const double A = std::pow(
        10.0, juce::jlimit(-36.0, 36.0, gainDb) / 40.0);
    const double w0 = juce::MathConstants<double>::twoPi * safeF / fs;
    const double c = std::cos(w0);
    const double s = std::sin(w0);
    const double alpha = s / (2.0 * safeQ);

    const double b0 = 1.0 + alpha * A;
    const double b1 = -2.0 * c;
    const double b2 = 1.0 - alpha * A;
    const double a0 = 1.0 + alpha / A;
    const double a1 = -2.0 * c;
    const double a2 = 1.0 - alpha / A;
    const double invA0 = 1.0 / std::max(1.0e-12, a0);

    filter.updateCoefficients(
        b0 * invA0, b1 * invA0, b2 * invA0,
        a1 * invA0, a2 * invA0);
}

void VVChainDSP::updateDynamicDetector(Biquad& filter, double fs, double f0, double q)
{
    // Constant-peak-gain band-pass detector: approximately unity at the
    // target frequency so Threshold behaves like an audio level control.
    const double safeF = juce::jlimit(10.0, fs * 0.45, f0);
    const double safeQ = juce::jlimit(0.05, 30.0, q);
    const double w0 = juce::MathConstants<double>::twoPi * safeF / fs;
    const double c = std::cos(w0);
    const double s = std::sin(w0);
    const double alpha = s / (2.0 * safeQ);

    const double b0 = alpha;
    const double b1 = 0.0;
    const double b2 = -alpha;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * c;
    const double a2 = 1.0 - alpha;
    const double invA0 = 1.0 / std::max(1.0e-12, a0);

    filter.updateCoefficients(
        b0 * invA0, b1 * invA0, b2 * invA0,
        a1 * invA0, a2 * invA0);
}

void VVChainDSP::updateAnalogHighPass(Biquad& filter, double fs, double f0, double q)
{
    const double safeF = juce::jlimit(10.0, fs * 0.45, f0);
    const double K = std::tan(juce::MathConstants<double>::pi * safeF / fs);
    const double Q = std::max(0.05, q);

    const double a0 = 1.0 + K / Q + K * K;
    const double b0 = 1.0;
    const double b1 = -2.0;
    const double b2 = 1.0;
    const double a1 = 2.0 * (K * K - 1.0);
    const double a2 = 1.0 - K / Q + K * K;

    filter.updateCoefficients(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
}

void VVChainDSP::updateLowPass(Biquad& filter, double fs, double f0, double q)
{
    const double safeF = juce::jlimit(10.0, fs * 0.45, f0);
    const double K = std::tan(juce::MathConstants<double>::pi * safeF / fs);
    const double K2 = K * K;
    const double Q = std::max(0.05, q);

    const double a0 = 1.0 + K / Q + K2;
    const double b0 = K2;
    const double b1 = 2.0 * K2;
    const double b2 = K2;
    const double a1 = 2.0 * (K2 - 1.0);
    const double a2 = 1.0 - K / Q + K2;

    filter.updateCoefficients(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
}

void VVChainDSP::updateHighPass(Biquad& filter, double fs, double f0, double q)
{
    updateAnalogHighPass(filter, fs, f0, q);
}

void VVChainDSP::updateCrossover(Crossover4th& xover, double fs, double f0, double q)
{
    updateLowPass(xover.lp1, fs, f0, q);
    updateLowPass(xover.lp2, fs, f0, q);
    updateHighPass(xover.hp1, fs, f0, q);
    updateHighPass(xover.hp2, fs, f0, q);
}

void VVChainDSP::updateCrossover2nd(Crossover2nd& xover, double fs, double f0, double q)
{
    updateLowPass(xover.lp, fs, f0, q);
    updateHighPass(xover.hp, fs, f0, q);
}

float VVChainDSP::dynamicMidGainChangeDb(int band) const noexcept
{
    if (band < 0 || band >= 4)
        return 0.f;
    return dynMidGainChangeDb[(size_t) band].load(std::memory_order_relaxed);
}

float VVChainDSP::dynamicSideGainChangeDb(int band) const noexcept
{
    if (band < 0 || band >= 4)
        return 0.f;
    return dynSideGainChangeDb[(size_t) band].load(std::memory_order_relaxed);
}

float VVChainDSP::dynamicAverageGainChangeDb(int band) const noexcept
{
    if (band < 0 || band >= 4)
        return 0.f;
    return 0.5f * (
        dynamicMidGainChangeDb(band)
        + dynamicSideGainChangeDb(band));
}

float VVChainDSP::dbToGain(float db) noexcept
{
    return juce::Decibels::decibelsToGain(db);
}

float VVChainDSP::gainToDb(float gain) noexcept
{
    return juce::Decibels::gainToDecibels(std::max(gain, 1.0e-9f));
}

float VVChainDSP::timeCoeff(double sampleRate, float ms) noexcept
{
    return std::exp(-1.0f / (0.001f * std::max(ms, 0.1f) * static_cast<float>(sampleRate)));
}

void VVChainDSP::processChebyshevAnalog(
    juce::dsp::AudioBlock<float>& block,
    float drive,
    float amount)
{
    if (amount <= 0.0f || drive <= 0.0f || block.getNumSamples() == 0)
        return;

    const auto numSamples = block.getNumSamples();
    const float safeDrive = juce::jmax(0.0f, drive);
    const float safeAmount = juce::jlimit(0.0f, 1.0f, amount);

    // One-channel scratch, preallocated in prepare(); no realtime allocation.
    jassert(analogTempBuffer.getNumChannels() >= 1);
    jassert(static_cast<size_t>(analogTempBuffer.getNumSamples()) >= numSamples);

    auto* tempPtr = analogTempBuffer.getWritePointer(0);

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
    {
        auto* channelData = block.getChannelPointer(ch);

        // 1. Calculate input RMS.
        double inputSumSquares = 0.0;

        for (size_t i = 0; i < numSamples; ++i)
        {
            const double v = static_cast<double>(channelData[i]);
            inputSumSquares += v * v;
        }

        const float inputRms =
            static_cast<float>(std::sqrt(
                inputSumSquares / static_cast<double>(numSamples)));

        if (inputRms < 0.0001f)
            continue;

        // 2. High-purity analogue colouring.
        double outputSumSquares = 0.0;

        for (size_t i = 0; i < numSamples; ++i)
        {
            const float x = channelData[i];

            // Gentle soft clipping.
            const float xDriven = std::tanh(x * safeDrive);

            // Pure harmonic extraction requested by the final recipe.
            const float evenHarmonics =
                2.0f * xDriven * xDriven - 1.0f;

            const float oddHarmonics =
                4.0f * xDriven * xDriven * xDriven;

            // Fundamental-preserving source + requested harmonic colour.
            const float shaped =
                xDriven
                + 0.25f * evenHarmonics
                + 0.15f * oddHarmonics;

            tempPtr[i] = shaped;
            outputSumSquares +=
                static_cast<double>(shaped) * static_cast<double>(shaped);
        }

        // 3. 100% Auto-Gain Match.
        const float outputRms =
            static_cast<float>(std::sqrt(
                outputSumSquares / static_cast<double>(numSamples)));

        const float gainComp =
            outputRms > 0.0001f
                ? inputRms / outputRms
                : 1.0f;

        // 4. Serial routing in VVChain: original + pure Delta * amount.
        // No parallel-path route currently exists in the production signal
        // path, so the requested isParallelPath branch is intentionally
        // represented by the existing serial path only.
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float delta =
                (tempPtr[i] * gainComp) - channelData[i];

            channelData[i] =
                channelData[i] + (delta * safeAmount);
        }
    }
}

void VVChainDSP::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    sr = std::max(8000.0, sampleRate);
    channels = juce::jlimit(1, 2, numChannels);

    const int maxBlock = juce::jmax(1, samplesPerBlock);
    dryBuffer.setSize(channels, maxBlock, false, true, true);
    alignedDryBuffer.setSize(channels, maxBlock, false, true, true);
    analogTempBuffer.setSize(1, maxBlock * 4, false, true, true);
    dynamicDetectorInput.setSize(channels, maxBlock * 4, false, true, true);

    eqOversampler.reset();
    limiterOversampler.reset();
    eqOversampler.initProcessing(static_cast<size_t>(maxBlock));
    limiterOversampler.initProcessing(static_cast<size_t>(maxBlock));

    eqLatencySamples =
        static_cast<int>(std::lround(eqOversampler.getLatencyInSamples()));
    limiterOversamplingLatencySamples =
        static_cast<int>(std::lround(limiterOversampler.getLatencyInSamples()));
    limiterLookaheadSamples =
        juce::jmax(1, static_cast<int>(std::lround(sr * 0.003)));

    totalLatencySamples =
        eqLatencySamples
        + limiterOversamplingLatencySamples
        + limiterLookaheadSamples;

    juce::dsp::ProcessSpec drySpec
    {
        sr,
        static_cast<juce::uint32>(maxBlock),
        static_cast<juce::uint32>(channels)
    };

    eqDryDelay.prepare(drySpec);
    eqDryDelay.setDelay(static_cast<float>(eqLatencySamples));

    juce::dsp::ProcessSpec limiterSpec
    {
        sr * 4.0,
        static_cast<juce::uint32>(maxBlock * 4),
        static_cast<juce::uint32>(channels)
    };

    limiterLookahead.prepare(limiterSpec);
    limiterLookahead.setDelay(static_cast<float>(limiterLookaheadSamples * 4));

    masterDryDelay.prepare(drySpec);
    masterDryDelay.setDelay(static_cast<float>(
        limiterOversamplingLatencySamples + limiterLookaheadSamples));

    reset();
}

void VVChainDSP::reset()
{
    for (auto& b : eq) b.reset();
    for (auto& b : dynMidEq) b.reset();
    for (auto& b : dynSideEq) b.reset();
    for (auto& b : dynMidDetectors) b.reset();
    for (auto& b : dynSideDetectors) b.reset();
    dynMidEnvelopeDb = { -120.f, -120.f, -120.f, -120.f };
    dynSideEnvelopeDb = { -120.f, -120.f, -120.f, -120.f };
    dynMidSlowDb = { -120.f, -120.f, -120.f, -120.f };
    dynSideSlowDb = { -120.f, -120.f, -120.f, -120.f };
    dynMidActivation = { 0.f, 0.f, 0.f, 0.f };
    dynSideActivation = { 0.f, 0.f, 0.f, 0.f };
    for (auto& v : dynMidGainChangeDb)
        v.store(0.f, std::memory_order_relaxed);
    for (auto& v : dynSideGainChangeDb)
        v.store(0.f, std::memory_order_relaxed);
    soloPreXover1.reset(); soloPreXover2.reset(); soloPreXover3.reset();
    soloPostXover1.reset(); soloPostXover2.reset(); soloPostXover3.reset();
    soloBlend = 0.f;
    lastSoloBand = -2;
    lastSoloPost = false;

    ottXover1.reset();
    ottXover2.reset();
    ottXover3.reset();
    ottPhase2_B1.reset();
    ottPhase3_B1.reset();
    ottPhase3_B2.reset();

    for (auto& b : ottDynamics)
    {
        b.gateEnvDb = { 0.f, 0.f };
        b.lifterEnv = { 1.f, 1.f };
        b.compEnvDb = { 0.f, 0.f };
        b.upRmsPower = { 0.f, 0.f };
        b.upSlowRmsPower = { 0.f, 0.f };
        b.downRmsPower = { 0.f, 0.f };
        b.downSlowRmsPower = { 0.f, 0.f };
    }

    typeXover1.reset();
    typeXover2.reset();
    typeXover3.reset();

    deessSplit.reset();

    eqOversampler.reset();
    limiterOversampler.reset();
    eqDryDelay.reset();
    limiterLookahead.reset();
    masterDryDelay.reset();

    masterBypassBlend = 0.f;
    limiterGain = 1.f;

    for (size_t band = 0; band < 4; ++band)
    {
        typeFastEnv[band] = { 0.f, 0.f };
        typeSlowEnv[band] = { 0.f, 0.f };
        typeDc[band] = { 0.f, 0.f };
    }

    for (auto& state : deess)
    {
        state.sidechainHP.reset();
        state.fastEnv = 0.f;
        state.slowEnv = 0.f;
        state.gainDb = 0.f;
    }

    gateEnvDb = { 0.f, 0.f };
    limiterEnvDb = { 0.f, 0.f };
    dryBuffer.clear();
    alignedDryBuffer.clear();
    dynamicDetectorInput.clear();
    analogTempBuffer.clear();
}

float VVChainDSP::rmsDetectPDR(float input,
                                  float& fastPower,
                                  float& slowPower,
                                  float attackMs,
                                  float releaseMs,
                                  double sampleRate,
                                  float& programReleaseMs,
                                  float attackCoeffOverride,
                                  float releaseCoeffOverride) noexcept
{
    const float target = input * input;

    const float fastAttack =
        attackCoeffOverride >= 0.0f
            ? juce::jlimit(0.0f, 1.0f, attackCoeffOverride)
            : timeCoeff(sampleRate, attackMs);
    const float fastRelease =
        releaseCoeffOverride >= 0.0f
            ? juce::jlimit(0.0f, 1.0f, releaseCoeffOverride)
            : timeCoeff(sampleRate, juce::jmax(0.5f, releaseMs * 0.35f));
    const float slowAttack =
        timeCoeff(sampleRate, juce::jmax(attackMs * 4.0f, 5.0f));
    const float slowRelease =
        timeCoeff(sampleRate, juce::jmax(releaseMs * 1.75f, 20.0f));

    fastPower = (target > fastPower ? fastAttack : fastRelease) * fastPower
              + (1.0f - (target > fastPower ? fastAttack : fastRelease)) * target;

    slowPower = (target > slowPower ? slowAttack : slowRelease) * slowPower
              + (1.0f - (target > slowPower ? slowAttack : slowRelease)) * target;

    const float fastDb = gainToDb(std::sqrt(std::max(fastPower, 1.0e-12f)));
    const float slowDb = gainToDb(std::sqrt(std::max(slowPower, 1.0e-12f)));
    const float crestDb = fastDb - slowDb;

    const float transientBlend =
        juce::jlimit(0.0f, 1.0f, (crestDb - 1.0f) / 8.0f);

    // Large crest / transient -> short release. Sustained programme -> longer release.
    programReleaseMs = releaseMs
        * juce::jlimit(0.20f, 2.0f, 2.0f - 1.80f * transientBlend);

    const float finalPower =
        fastPower * transientBlend
        + slowPower * (1.0f - transientBlend);

    return gainToDb(std::sqrt(std::max(finalPower, 1.0e-12f)));
}

float VVChainDSP::applyLifterFromDetectorDb(float input, float detectorDb,
                                             float& env, float thresholdDb,
                                             float attackMs, float releaseMs,
                                             float mix, double sampleRate,
                                             float ratio)
{
    const float safeRatio = juce::jmax(1.0f, ratio);
    const float slope = 1.0f - (1.0f / safeRatio);
    const float kneeStart = thresholdDb - kLifterKneeDb * 0.5f;
    const float kneeEnd = thresholdDb + kLifterKneeDb * 0.5f;

    float targetGainDb = 0.f;
    if (detectorDb < kneeStart)
        targetGainDb = (thresholdDb - detectorDb) * slope;
    else if (detectorDb < kneeEnd)
    {
        const float x = kneeEnd - detectorDb;
        targetGainDb = slope / (2.0f * kLifterKneeDb) * x * x;
    }

    // Hard upward-gain ceiling: +12dB = 3.981x, prevents noise-floor lift.
    const float targetGainDbClamped =
        juce::jmin(12.0f, juce::jmax(0.0f, targetGainDb));
    const float targetLinear = dbToGain(targetGainDbClamped);
    const float attack = timeCoeff(sampleRate, attackMs);
    const float release = timeCoeff(sampleRate, releaseMs);
    const float alpha = targetLinear > env ? attack : release;
    env = alpha * env + (1.0f - alpha) * targetLinear;

    const float wet = input * env;
    const float m = juce::jlimit(0.f, 1.f, mix / 100.f);
    return wet * m + input * (1.0f - m);
}

float VVChainDSP::applyCompressorFromDetectorDb(float input, float detectorDb,
                                                float& envDb, float thresholdDb,
                                                float attackMs, float releaseMs,
                                                float mix, double sampleRate,
                                                float ratio,
                                                float attackCoeffOverride,
                                                float releaseCoeffOverride)
{
    const float slope = 1.0f - (1.0f / juce::jmax(1.0f, ratio));
    const float kneeStart = thresholdDb - kCompressorKneeDb * 0.5f;
    const float kneeEnd = thresholdDb + kCompressorKneeDb * 0.5f;

    float targetReductionDb = 0.f;
    if (detectorDb > kneeEnd)
        targetReductionDb = (detectorDb - thresholdDb) * slope;
    else if (detectorDb > kneeStart)
    {
        const float x = detectorDb - kneeStart;
        targetReductionDb = slope / (2.0f * kCompressorKneeDb) * x * x;
    }

    const float currentReductionDb = -envDb;
    const float alpha = targetReductionDb > currentReductionDb
        ? (attackCoeffOverride >= 0.0f
            ? juce::jlimit(0.0f, 1.0f, attackCoeffOverride)
            : timeCoeff(sampleRate, attackMs))
        : (releaseCoeffOverride >= 0.0f
            ? juce::jlimit(0.0f, 1.0f, releaseCoeffOverride)
            : timeCoeff(sampleRate, releaseMs));
    const float smoothedReduction =
        alpha * currentReductionDb + (1.0f - alpha) * targetReductionDb;

    envDb = -juce::jlimit(0.f, 60.f, smoothedReduction);

    const float wet = input * dbToGain(envDb);
    const float m = juce::jlimit(0.f, 1.f, mix / 100.f);
    return wet * m + input * (1.0f - m);
}

float VVChainDSP::applyGate(float input, float& envDb, float thresholdDb,
                            double sampleRate)
{
    const float knee = kGateKneeDb;
    const float ratioSlope = kGateRatio - 1.f;
    const float kneeStart = thresholdDb - knee * 0.5f;
    const float kneeEnd = thresholdDb + knee * 0.5f;

    const float magnitude = std::max(std::abs(input), 0.000001f);
    const float inputDb = juce::Decibels::gainToDecibels(magnitude);

    // A gate must never create positive gain. The previous knee formula
    // accidentally turned the gate into an expander/booster around the
    // threshold, which could make OTT jump by many dB on quiet material.
    float targetGainDb = 0.f;
    if (inputDb < kneeStart)
    {
        targetGainDb = (inputDb - thresholdDb) * ratioSlope;
    }
    else if (inputDb < kneeEnd)
    {
        const float t = juce::jlimit(0.f, 1.f, (inputDb - kneeStart) / knee);
        const float hardGainDb = (inputDb - thresholdDb) * ratioSlope;
        targetGainDb = hardGainDb * (1.f - t) * (1.f - t);
    }

    targetGainDb = std::min(0.f, targetGainDb);

    const float attack = timeCoeff(sampleRate, 100.f);
    const float release = timeCoeff(sampleRate, 30.f);
    const float alpha = targetGainDb < envDb ? attack : release;
    envDb = alpha * envDb + (1.f - alpha) * targetGainDb;

    return input * (0.90f * dbToGain(envDb) + 0.10f);
}

float VVChainDSP::applyLimiter(float input, float& envDb, double sampleRate)
{
    constexpr float ceilingDb = -0.8f;
    const float inputDb =
        juce::Decibels::gainToDecibels(std::max(std::abs(input), 1.0e-9f));

    const float targetReductionDb =
        inputDb > ceilingDb
            ? -juce::jlimit(0.f, 24.f, inputDb - ceilingDb)
            : 0.f;

    const float attack = timeCoeff(sampleRate, 0.05f);
    const float release = timeCoeff(sampleRate, 85.f);
    const float alpha = targetReductionDb < envDb ? attack : release;
    envDb = alpha * envDb
        + (1.f - alpha) * targetReductionDb;

    return input * dbToGain(envDb);
}

void VVChainDSP::applyEq(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    juce::dsp::AudioBlock<const float> inputBlock(buffer);
    juce::dsp::AudioBlock<float> outputBlock(buffer);
    auto osBlock = eqOversampler.processSamplesUp(inputBlock);

    const double osSr =
        sr * static_cast<double>(eqOversampler.getOversamplingFactor());
    const int osSamples = static_cast<int>(osBlock.getNumSamples());
    const int osChannels = static_cast<int>(osBlock.getNumChannels());
    constexpr float invSqrt2 = 0.7071067811865475f;
    constexpr float dynKneeDb = 10.0f;

    // Sonnox-style Dynamic EQ:
    // Offset = p.gain. DYN_TARGET defines the maximum dynamic span around that EQ offset.
    // DYNAMICS is signed: negative = downward compression, positive = upward
    // expansion; 0% = no dynamic movement. A 10 dB soft knee controls drive.
    // Detection remains feed-forward from pristine pre-EQ audio.
    for (int ch = 0; ch < osChannels; ++ch)
    {
        auto* dst = dynamicDetectorInput.getWritePointer(ch);
        const auto* src =
            osBlock.getChannelPointer(static_cast<size_t>(ch));
        std::copy(src, src + osSamples, dst);
    }

    if (!p.eqBypass)
    {
        for (size_t band = 0; band < 4; ++band)
        {
            const double frequency = juce::jlimit(
                20.0, osSr * 0.45, static_cast<double>(p.freq[band]));
            const float offsetGain =
                juce::jlimit(-18.f, 18.f, p.gain[band]);
            const float dynamicRangeDb = 18.0f;
            const float dynamicsDirection =
                p.dynDynamics[band] < 0.f ? -1.f : 1.f;
            const float dynamicDeltaDb =
                dynamicsDirection * dynamicRangeDb;
            const double baseQ = juce::jlimit(
                0.1, 18.0, static_cast<double>(p.q[band]));

            updateDynamicDetector(
                dynMidDetectors[band], osSr, frequency, baseQ);
            updateDynamicDetector(
                dynSideDetectors[band], osSr, frequency, baseQ);

            // Single user-facing DYNAMICS macro:
            //   -100..0 = compression
            //     0..100 = expansion
            //
            // One macro controls BOTH:
            // 1) dynamic depth via dynamicsAmount, and
            // 2) detector threshold via the same signed amount.
            //
            // Compression moves the threshold downward as depth increases;
            // expansion moves it upward so the same control remains intuitive.
            const float dynamicsSigned =
                juce::jlimit(-100.f, 100.f, p.dynDynamics[band]) * 0.01f;
            const float dynamicsAmount = std::abs(dynamicsSigned);
            const float thresholdDb =
                dynamicThresholdFromDynamics(
                    p.dynDynamics[band]);
            const float attackCoeff = timeCoeff(
                osSr, juce::jlimit(0.1f, 200.f, p.dynAttack[band]));
            const float releaseCoeff = timeCoeff(
                osSr, juce::jlimit(5.f, 2000.f, p.dynRelease[band]));
            const float onsetSlowCoeff =
                timeCoeff(osSr, 120.0f);

            const float midPercent =
                juce::jlimit(0.f, 100.f, p.dynMSBalance[band]);
            const float sidePercent = 100.f - midPercent;
            const float midWeight =
                juce::jlimit(0.f, 1.f, midPercent / 50.f);
            const float sideWeight =
                juce::jlimit(0.f, 1.f, sidePercent / 50.f);

            float& midEnv = dynMidEnvelopeDb[band];
            float& sideEnv = dynSideEnvelopeDb[band];
            float& midSlow = dynMidSlowDb[band];
            float& sideSlow = dynSideSlowDb[band];
            float& midActivation = dynMidActivation[band];
            float& sideActivation = dynSideActivation[band];

            const float gainDeltaDb = dynamicDeltaDb;

            for (int sample = 0; sample < osSamples; ++sample)
            {
                const float srcL =
                    dynamicDetectorInput.getSample(0, sample);
                const float srcR = osChannels > 1
                    ? dynamicDetectorInput.getSample(1, sample)
                    : 0.f;

                const float sourceMid = osChannels > 1
                    ? (srcL + srcR) * invSqrt2
                    : srcL;
                const float sourceSide = osChannels > 1
                    ? (srcL - srcR) * invSqrt2
                    : 0.f;

                const float detectorMid =
                    dynMidDetectors[band].process(sourceMid, false);
                const float detectorSide = osChannels > 1
                    ? dynSideDetectors[band].process(sourceSide, false)
                    : 0.f;

                const float midDb =
                    gainToDb(std::abs(detectorMid) + 1.0e-6f);
                const float sideDb = osChannels > 1
                    ? gainToDb(std::abs(detectorSide) + 1.0e-6f)
                    : -120.f;

                // Fast envelope drives the response time. A slower envelope is
                // retained so Onsets detection reacts to sudden increases but
                // is comparatively insensitive to sustained level.
                const float midAlpha =
                    midDb > midEnv ? attackCoeff : releaseCoeff;
                const float sideAlpha =
                    sideDb > sideEnv ? attackCoeff : releaseCoeff;

                midEnv = midAlpha * midEnv
                    + (1.f - midAlpha) * midDb;

                if (osChannels > 1)
                {
                    sideEnv = sideAlpha * sideEnv
                        + (1.f - sideAlpha) * sideDb;
                }

                midSlow = onsetSlowCoeff * midSlow
                    + (1.f - onsetSlowCoeff) * midDb;
                if (osChannels > 1)
                {
                    sideSlow = onsetSlowCoeff * sideSlow
                        + (1.f - onsetSlowCoeff) * sideDb;
                }

                auto activationFor = [thresholdDb, dynamicsAmount](
                    float detectorDb,
                    bool triggerBelow) noexcept
                {
                    constexpr float knee = dynKneeDb;
                    float a = 0.f;

                    if (!triggerBelow)
                    {
                        const float lo = thresholdDb - knee;
                        if (detectorDb <= lo)
                            a = 0.f;
                        else if (detectorDb >= thresholdDb)
                            a = 1.f;
                        else
                        {
                            const float t =
                                (detectorDb - lo) / knee;
                            a = t * t * (3.f - 2.f * t);
                        }
                    }
                    else
                    {
                        const float hi = thresholdDb + knee;
                        if (detectorDb >= hi)
                            a = 0.f;
                        else if (detectorDb <= thresholdDb)
                            a = 1.f;
                        else
                        {
                            const float t =
                                (hi - detectorDb) / knee;
                            a = t * t * (3.f - 2.f * t);
                        }
                    }

                    return juce::jlimit(0.f, 1.f, a * dynamicsAmount);
                };

                float midTriggerDb = midEnv;
                float sideTriggerDb = sideEnv;

                if (p.dynDetectOnsets[band])
                {
                    const float midRise =
                        juce::jmax(0.f, midDb - midSlow);
                    const float sideRise =
                        juce::jmax(0.f, sideDb - sideSlow);

                    midTriggerDb += juce::jlimit(0.f, 12.f, midRise * 1.5f);
                    sideTriggerDb += juce::jlimit(0.f, 12.f, sideRise * 1.5f);
                }

                const float midTargetActivation = activationFor(
                    midTriggerDb, p.dynTriggerBelow[band]);
                const float sideTargetActivation = activationFor(
                    sideTriggerDb, p.dynTriggerBelow[band]);

                // Attack / release are applied to the dynamic gain movement
                // itself, so the band approaches Target and returns to Offset.
                const float midActCoeff =
                    midTargetActivation > midActivation
                        ? attackCoeff : releaseCoeff;
                const float sideActCoeff =
                    sideTargetActivation > sideActivation
                        ? attackCoeff : releaseCoeff;

                midActivation = midActCoeff * midActivation
                    + (1.f - midActCoeff) * midTargetActivation;
                if (osChannels > 1)
                {
                    sideActivation = sideActCoeff * sideActivation
                        + (1.f - sideActCoeff) * sideTargetActivation;
                }

                const float midEnvRatio =
                    juce::jlimit(0.f, 1.f, midActivation * midWeight);
                const float sideEnvRatio =
                    juce::jlimit(0.f, 1.f, sideActivation * sideWeight);

                const float midTotalGain =
                    VVChain_DynEQ_Engine::getTargetGainDB(
                        offsetGain, gainDeltaDb, midEnvRatio);
                const float sideTotalGain =
                    VVChain_DynEQ_Engine::getTargetGainDB(
                        offsetGain, gainDeltaDb, sideEnvRatio);

                const float safeMidTotalGain =
                    juce::jlimit(-18.f, 18.f, midTotalGain);
                const float safeSideTotalGain =
                    juce::jlimit(-18.f, 18.f, sideTotalGain);
                const float midGainChange =
                    safeMidTotalGain - offsetGain;
                const float sideGainChange =
                    safeSideTotalGain - offsetGain;

                dynMidGainChangeDb[band].store(
                    midGainChange, std::memory_order_relaxed);
                dynSideGainChangeDb[band].store(
                    sideGainChange, std::memory_order_relaxed);

                // Oxford Type-3-style gain/Q interaction:
                // as gain moves farther from 0 dB, Q reduces and the
                // effective bandwidth becomes wider / softer.
                const double midQ = juce::jlimit(
                    0.1, 18.0,
                    baseQ / (1.0 + 0.045
                        * std::abs(static_cast<double>(safeMidTotalGain))));
                const double sideQ = juce::jlimit(
                    0.1, 18.0,
                    baseQ / (1.0 + 0.045
                        * std::abs(static_cast<double>(safeSideTotalGain))));

                if ((sample & 3) == 0)
                {
                    updateDynamicPeak(
                        dynMidEq[band], osSr, frequency, midTotalGain, midQ);
                    updateDynamicPeak(
                        dynSideEq[band], osSr, frequency, sideTotalGain, sideQ);
                }

                auto* left = osBlock.getChannelPointer(0);
                const float leftIn = left[sample];

                if (osChannels > 1)
                {
                    auto* right = osBlock.getChannelPointer(1);
                    const float rightIn = right[sample];

                    float currentMid =
                        (leftIn + rightIn) * invSqrt2;
                    float currentSide =
                        (leftIn - rightIn) * invSqrt2;

                    currentMid =
                        dynMidEq[band].process(currentMid, false);
                    currentSide =
                        dynSideEq[band].process(currentSide, false);

                    left[sample] =
                        (currentMid + currentSide) * invSqrt2;
                    right[sample] =
                        (currentMid - currentSide) * invSqrt2;
                }
                else
                {
                    left[sample] =
                        dynMidEq[band].process(leftIn, false);
                }
            }

            if (!p.eqColorGlobalBypass
                && !p.eqColorBypass[band])
            {
                const float amount =
                    juce::jlimit(0.f, 100.f, p.eqColor[band]) / 100.f;
                const float drive =
                    p.eqColorSolidState[band] ? 1.15f : 0.95f;

                // Keep Analog Color in conventional L/R so M/S weighting
                // belongs only to the Dynamic EQ section.
                processChebyshevAnalog(osBlock, drive, amount);
            }
        }
    }

    eqOversampler.processSamplesDown(outputBlock);
}
void VVChainDSP::applyOtt(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    // Four independent OTT bands. Each band has its own detector state and
    // runs downward compression first, then upward compression, followed by
    // per-band makeup. The gate is also applied after the crossover so it
    // cannot make one frequency band modulate another.
    const float x1 = juce::jlimit(80.f, 900.f, p.ottX1);
    const float x2 = juce::jlimit(x1 + 80.f, 5000.f, p.ottX2);
    const float x3 = juce::jlimit(x2 + 200.f, static_cast<float>(sr * 0.42), p.ottX3);

    const float xoverQ = crossoverQFromOverlap(p.ottXoverOverlap);

    updateCrossover(ottXover1, sr, x1, xoverQ);
    updateCrossover(ottXover2, sr, x2, xoverQ);
    updateCrossover(ottXover3, sr, x3, xoverQ);

    // Equalize the number of crossover sections traversed by each branch.
    // B1: X1 -> add all-pass X2 + X3. B2: X1+X2 -> add all-pass X3.
    updateCrossover(ottPhase2_B1, sr, x2, xoverQ);
    updateCrossover(ottPhase3_B1, sr, x3, xoverQ);
    updateCrossover(ottPhase3_B2, sr, x3, xoverQ);

    const float inputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.ottInputGainDb));
    const float globalMix =
        juce::jlimit(0.f, 1.f, p.ottMix / 100.f);
    const float outputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.ottOutputGainDb));

    float amountSum = 0.0f;
    int amountCount = 0;
    for (int band = 0; band < 4; ++band)
    {
        if (p.ottBandBypass[(size_t) band])
            continue;
        amountSum += juce::jlimit(
            0.0f, 100.0f, p.ottDegree[(size_t) band]) / 100.0f;
        ++amountCount;
    }

    const float averageAmount =
        amountCount > 0 ? amountSum / static_cast<float>(amountCount) : 0.0f;
    // Nominal level compensation: 70% Amount -> -1.75dB.
    const float autoTrimDb = -2.5f * averageAmount;
    const float autoTrimGain = dbToGain(autoTrimDb);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float original = data[n];
            const float x = original * inputGain;

            float low = ottXover1.low(x, right);
            const float x1High = ottXover1.high(x, right);
            float lowMid = ottXover2.low(x1High, right);
            const float x2High = ottXover2.high(x1High, right);
            const float midHigh = ottXover3.low(x2High, right);
            const float top = ottXover3.high(x2High, right);

            // LP4 + HP4 compensation restores the phase path for skipped
            // crossovers without adding host/plugin latency.
            low = ottPhase2_B1.allPass(low, right);
            low = ottPhase3_B1.allPass(low, right);
            lowMid = ottPhase3_B2.allPass(lowMid, right);

            float bands[4] = { low, lowMid, midHigh, top };

            for (int band = 0; band < 4; ++band)
            {
                if (p.ottBandBypass[(size_t) band])
                    continue;

                const float degree =
                    juce::jlimit(0.f, 100.f, p.ottDegree[(size_t) band]);

                if (degree <= 0.0001f)
                    continue;

                auto& state = ottDynamics[(size_t) band];
                float& gateEnv = state.gateEnvDb[(size_t) ch];
                float& lifterEnv = state.lifterEnv[(size_t) ch];
                float& compEnv = state.compEnvDb[(size_t) ch];

                // The existing GATE control is now independent per frequency band.
                float v = applyGate(
                    bands[band], gateEnv,
                    p.ottGateThresholdDb, sr);

                // Degree=0 means true unity ratio. Degree=100 reaches the
                // OTT-style maximum ratios while preserving the user's
                // existing per-band controls.
                const float depth = degree / 100.f;

                // Classic OTT-style scaling: upward reaches 4:1.
                // Downward is intentionally much stronger, matching the
                // documented Ableton/Xfer family character. The top band
                // uses the slightly harder target.
                const float downMaxRatio = band == 3 ? 100.f : kCompressorRatio;
                const float downRatio =
                    1.f + depth * (downMaxRatio - 1.f);
                const float upRatio =
                    1.f + depth * (kLifterRatio - 1.f);

                const float compMix =
                    juce::jlimit(0.f, 100.f, p.ottCompMix[(size_t) band]);
                const float lifterMix =
                    juce::jlimit(0.f, 100.f, p.ottLifterMix[(size_t) band]);

                // Standard OTT order: downward first, upward second.
                // The user's Attack is automatically lengthened as OTT Amount
                // (degree) rises, reducing high-depth click / transient tearing.
                const float amount = depth;

                // The Compressor Attack parameter is the actual user base time.
                // Defaults are Low=15ms, LowMid=8ms, HighMid=3ms, High=1ms.
                const float baseAttackMs =
                    juce::jlimit(0.1f, 120.0f,
                                 p.ottCompAttack[(size_t) band]);

                // Amount^2 curve: Amount=70% maps exactly to 120ms.
                const float targetLimitMs = 120.0f;
                const float k =
                    juce::jmax(0.0f,
                               (targetLimitMs - baseAttackMs) / 0.49f);
                const float dynamicAttackMs =
                    baseAttackMs + k * (amount * amount);

                const float minAttackLimit =
                    (band == 0) ? 15.0f
                                : ((band == 1) ? 8.0f : 1.0f);

                const float finalAttackMs =
                    juce::jmax(minAttackLimit, dynamicAttackMs);

                const float baseReleaseMs =
                    juce::jlimit(10.0f, 2500.0f,
                                 p.ottCompRelease[(size_t) band]);
                const float dynamicReleaseMs =
                    baseReleaseMs + (amount * 100.0f);
                const float finalReleaseMs =
                    juce::jmax(20.0f, dynamicReleaseMs);

                const float attackCoef =
                    std::exp(-1000.0f
                             / (finalAttackMs * static_cast<float>(sr)));
                const float releaseCoef =
                    std::exp(-1000.0f
                             / (finalReleaseMs * static_cast<float>(sr)));

                // Each stage has its own RMS detector state for this band/channel.
                float downReleaseMs = p.ottCompRelease[(size_t) band];
                const float downDb = rmsDetectPDR(
                    v,
                    state.downRmsPower[(size_t) ch],
                    state.downSlowRmsPower[(size_t) ch],
                    finalAttackMs,
                    finalReleaseMs,
                    sr,
                    downReleaseMs,
                    attackCoef,
                    releaseCoef);

                v = applyCompressorFromDetectorDb(
                    v, downDb, compEnv,
                    p.ottCompThreshold[(size_t) band],
                    finalAttackMs,
                    finalReleaseMs,
                    compMix, sr, downRatio,
                    attackCoef,
                    releaseCoef);

                float upReleaseMs = p.ottLifterRelease[(size_t) band];
                const float upDb = rmsDetectPDR(
                    v,
                    state.upRmsPower[(size_t) ch],
                    state.upSlowRmsPower[(size_t) ch],
                    p.ottLifterAttack[(size_t) band],
                    p.ottLifterRelease[(size_t) band],
                    sr,
                    upReleaseMs);

                const float liftThreshold =
                    juce::jmax(p.ottLifterThreshold[(size_t) band], -48.f);

                v = applyLifterFromDetectorDb(
                    v, upDb, lifterEnv,
                    liftThreshold,
                    p.ottLifterAttack[(size_t) band],
                    upReleaseMs,
                    lifterMix, sr, upRatio);

                v *= dbToGain(
                    juce::jlimit(-24.f, 12.f,
                        p.ottBandLevelDb[(size_t) band]));

                bands[band] = v;
            }

            float wet = bands[0] + bands[1] + bands[2] + bands[3];

            if (p.ottClipper)
                wet = std::tanh(wet * 1.7f);

            wet *= outputGain;

            // Do not clip the OTT reconstruction here. The final true-peak
            // lookahead limiter operates on the complete mixed programme.
            data[n] =
                (original + globalMix * (wet - original))
                * autoTrimGain;
        }
    }
}

void VVChainDSP::applyAType(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    // TAPE-A is intentionally stateless: no envelope/attack/release dependent
    // gain jump is allowed when a new transient enters the processor.
    // The transfer curve is normalized so a 0 dBFS sample (|x| = 1) remains
    // at unity. The degree control determines both drive depth and wet amount.
    const float inputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.atypeInputGainDb));
    const float outputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.atypeOutputGainDb));
    const float mix =
        juce::jlimit(0.f, 1.f, p.atypeMix / 100.f);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float original = data[n];
            const float x = original * inputGain;

            const float b1 = typeXover1.low(x, ch == 1);
            const float b3 = typeXover2.high(x, ch == 1);
            const float b4 = typeXover3.high(x, ch == 1);
            const float b2 = x - b1 - b3;
            const float bands[4] = { b1, b2, b3, b4 };

            float enhancement = 0.f;

            for (int band = 0; band < 4; ++band)
            {
                if (p.atypeBandBypass[static_cast<size_t>(band)])
                    continue;

                const float depth =
                    juce::jlimit(0.f, 1.f,
                                 p.atypeDegree[static_cast<size_t>(band)] / 100.f);
                if (depth <= 0.f)
                    continue;

                // Conservative drive mapping: defaults stay subtle, while 100%
                // reaches a materially stronger tape-like transfer curve.
                const float rawDriveParam = 1.0f + 1.5f * depth;
                const float driveParam =
                    juce::jmax(1.0f, rawDriveParam);

                float makeupDenominator = std::tanh(driveParam);
                makeupDenominator =
                    juce::jmax(makeupDenominator, 1.0e-6f);

                const float staticMakeupMultiplier =
                    1.0f / makeupDenominator;

                // Normalized tanh: an input of +1/-1 maps exactly to +1/-1.
                // No envelope state, Attack or Release participates in the
                // TAPE-A gain path, so the first incoming transient cannot
                // acquire a state-dependent startup boost.
                const float driven =
                    std::tanh(bands[band] * driveParam)
                    * staticMakeupMultiplier;

                const float bandTrim =
                    dbToGain(juce::jlimit(
                        -6.f, 6.f,
                        p.atypeBandLevelDb[static_cast<size_t>(band)]));

                const float processed = driven * bandTrim;

                // Degree also controls wetness: 0% is mathematically
                // transparent, avoiding any hidden coloration at zero.
                enhancement +=
                    (processed - bands[band]) * depth;
            }

            const float processed =
                (x + enhancement * mix) * outputGain;

            data[n] = processed;
        }
    }
}

void VVChainDSP::processDeEsser(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    if (p.deessBypass || p.deessIntensity <= 0.0f)
        return;

    const float referenceHz =
        juce::jlimit(6000.f, 18000.f, p.deessReferenceHz);
    const float q = 0.70710678f;

    // Split-band de-essing: only the high band is gain-reduced. The low band
    // is carried through untouched and recombined, matching the standard
    // split-band approach used in professional de-essers.
    updateCrossover(deessSplit, sr, referenceHz, q);

    const float fastAttack = timeCoeff(sr, 0.25f);
    const float fastRelease = timeCoeff(sr, 45.f);
    const float slowAttack = timeCoeff(sr, 75.f);
    const float slowRelease = timeCoeff(sr, 260.f);
    const float gainAttack = timeCoeff(sr, 0.35f);
    const float gainRelease = timeCoeff(sr, 60.f);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        auto& state = deess[(size_t) ch];
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float input = data[n];
            const float lowBand = deessSplit.low(input, right);
            const float highBand = deessSplit.high(input, right);
            const float detector = std::abs(highBand);

            const float fastCoeff =
                detector > state.fastEnv ? fastAttack : fastRelease;
            state.fastEnv =
                fastCoeff * state.fastEnv
                + (1.f - fastCoeff) * detector;

            const float slowCoeff =
                detector > state.slowEnv ? slowAttack : slowRelease;
            state.slowEnv =
                slowCoeff * state.slowEnv
                + (1.f - slowCoeff) * detector;

            const float fastDb = gainToDb(state.fastEnv);
            const float slowDb = gainToDb(state.slowEnv);
            const float excessDb =
                fastDb - slowDb - 2.0f - p.deessAverageOffset;

            const float trigger =
                juce::jlimit(0.f, 1.f, excessDb / 6.f);

            const float targetReductionDb =
                juce::jlimit(0.f, 8.f, p.deessIntensity) * trigger;

            const float gainCoeff =
                targetReductionDb > state.gainDb
                    ? gainAttack
                    : gainRelease;

            state.gainDb =
                gainCoeff * state.gainDb
                + (1.f - gainCoeff) * targetReductionDb;

            data[n] =
                lowBand + highBand * dbToGain(-state.gainDb);
        }
    }
}

void VVChainDSP::processMasterLimiter(juce::AudioBuffer<float>& buffer,
                                         bool active)
{
    juce::dsp::AudioBlock<const float> inputBlock(buffer);
    juce::dsp::AudioBlock<float> outputBlock(buffer);
    auto osBlock = limiterOversampler.processSamplesUp(inputBlock);

    const double osSr =
        sr * static_cast<double>(limiterOversampler.getOversamplingFactor());

    constexpr float ceilingDb = -1.0f;
    const float ceiling = dbToGain(ceilingDb);
    const float attack =
        timeCoeff(osSr, 0.05f);
    const float release =
        timeCoeff(osSr, 50.f);

    for (size_t n = 0; n < osBlock.getNumSamples(); ++n)
    {
        float peak = 0.f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float* data = osBlock.getChannelPointer(
                static_cast<size_t>(ch));
            peak = std::max(peak, std::abs(data[n]));
        }

        const float targetGain =
            active && peak > ceiling
                ? ceiling / std::max(peak, 1.0e-9f)
                : 1.0f;

        // Gain is smoothed in dB/log space, not as a raw linear amplitude.
        const float targetDb = gainToDb(targetGain);
        const float currentDb = gainToDb(
            juce::jmax(limiterGain, 1.0e-9f));
        const float alpha =
            targetDb < currentDb ? attack : release;

        const float smoothedDb =
            alpha * currentDb + (1.f - alpha) * targetDb;

        limiterGain = dbToGain(smoothedDb);

        for (int ch = 0; ch < channels; ++ch)
        {
            float* data = osBlock.getChannelPointer(
                static_cast<size_t>(ch));

            limiterLookahead.pushSample(ch, data[n]);
            data[n] =
                limiterLookahead.popSample(ch) * limiterGain;
        }
    }

    limiterOversampler.processSamplesDown(outputBlock);
}

void VVChainDSP::alignDryBuffer(int numSamples)
{
    const int nCh = channels;

    for (int ch = 0; ch < nCh; ++ch)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const float input = dryBuffer.getSample(ch, n);
            eqDryDelay.pushSample(ch, input);

            alignedDryBuffer.setSample(
                ch, n, eqDryDelay.popSample(ch));
        }
    }
}

void VVChainDSP::process(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    const int nCh = std::min(buffer.getNumChannels(), channels);
    const int numSamples = buffer.getNumSamples();

    jassert(nCh <= dryBuffer.getNumChannels());
    jassert(numSamples <= dryBuffer.getNumSamples());

    if (nCh > dryBuffer.getNumChannels() || numSamples > dryBuffer.getNumSamples())
        return;

    for (int ch = 0; ch < nCh; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // Always traverse the fixed-latency EQ oversampling path. When EQ is
    // bypassed this is a latency-only pass-through, keeping PDC stable.
    applyEq(buffer, p);
    alignDryBuffer(numSamples);

    if (!p.ottBypass)
        applyOtt(buffer, p);
    if (!p.atypeBypass)
        applyAType(buffer, p);

    processDeEsser(buffer, p);

    // Dry/Wet is performed after the EQ latency has been matched.
    if (!p.mixBypass)
    {
        const float mix = juce::jlimit(0.f, 1.f, p.dryWet / 100.f);
        const float out = dbToGain(
            juce::jlimit(-24.f, 12.f, p.outputDb));

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const auto* original =
                alignedDryBuffer.getReadPointer(ch);

            for (int n = 0; n < numSamples; ++n)
                wet[n] =
                    (original[n] + mix * (wet[n] - original[n])) * out;
        }
    }

    const float soloX1 =
        juce::jlimit(40.f, 1000.f, p.ottX1);
    const float soloX2 =
        juce::jlimit(soloX1 + 80.f, 5000.f, p.ottX2);
    const float soloX3 =
        juce::jlimit(soloX2 + 200.f,
                     static_cast<float>(sr * 0.42),
                     p.ottX3);
    const float soloQ = crossoverQFromOverlap(
        p.ottXoverOverlap);

    updateCrossover(
        soloPreXover1, sr, soloX1, soloQ);
    updateCrossover(
        soloPreXover2, sr, soloX2, soloQ);
    updateCrossover(
        soloPreXover3, sr, soloX3, soloQ);
    updateCrossover(
        soloPostXover1, sr, soloX1, soloQ);
    updateCrossover(
        soloPostXover2, sr, soloX2, soloQ);
    updateCrossover(
        soloPostXover3, sr, soloX3, soloQ);

    const bool soloEnabled =
        p.soloBand >= 0 && p.soloBand < 4;

    if (p.soloBand != lastSoloBand
        || p.soloPost != lastSoloPost)
    {
        soloBlend = 0.f;
        lastSoloBand = p.soloBand;
        lastSoloPost = p.soloPost;
    }

    auto splitBand =
        [](float x,
           Crossover4th& x1,
           Crossover4th& x2,
           Crossover4th& x3,
           int band,
           bool right)
    {
        const float low = x1.low(x, right);
        const float high1 = x1.high(x, right);
        const float lowMid = x2.low(high1, right);
        const float high2 = x2.high(high1, right);
        const float midHigh = x3.low(high2, right);
        const float top = x3.high(high2, right);

        switch (band)
        {
            case 0: return low;
            case 1: return lowMid;
            case 2: return midHigh;
            default: return top;
        }
    };

    const int safeSoloBand =
        juce::jlimit(0, 3, p.soloBand);

    for (int n = 0; n < numSamples; ++n)
    {
        if (soloEnabled)
            soloBlend =
                std::min(1.f, soloBlend + 1.f / 64.f);
        else
            soloBlend =
                std::max(0.f, soloBlend - 1.f / 64.f);

        if (soloBlend <= 0.f)
            continue;

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const bool right = ch == 1;

            const float preSolo = splitBand(
                alignedDryBuffer.getSample(ch, n),
                soloPreXover1,
                soloPreXover2,
                soloPreXover3,
                safeSoloBand,
                right);

            const float postSolo = splitBand(
                wet[n],
                soloPostXover1,
                soloPostXover2,
                soloPostXover3,
                safeSoloBand,
                right);

            const float solo =
                p.soloPost ? postSolo : preSolo;

            wet[n] =
                wet[n] * (1.f - soloBlend)
                + solo * soloBlend;
        }
    }

    // Final 4x true-peak lookahead limiter. Its output already contains the
    // fixed limiter latency, so the master-bypass dry path is delayed by the
    // same amount below.
    processMasterLimiter(buffer, true);

    const float target =
        p.masterBypass ? 1.f : 0.f;
    const float step =
        1.f / static_cast<float>(kMasterBypassRampSamples);

    for (int n = 0; n < numSamples; ++n)
    {
        if (masterBypassBlend < target)
            masterBypassBlend =
                std::min(target,
                         masterBypassBlend + step);
        else if (masterBypassBlend > target)
            masterBypassBlend =
                std::max(target,
                         masterBypassBlend - step);

        const float blend = masterBypassBlend;

        for (int ch = 0; ch < nCh; ++ch)
        {
            masterDryDelay.pushSample(
                ch,
                alignedDryBuffer.getSample(ch, n));

            const float delayedDry =
                masterDryDelay.popSample(ch);

            auto* wet = buffer.getWritePointer(ch);
            wet[n] =
                wet[n] * (1.f - blend)
                + delayedDry * blend;

            // Delta is the processed output minus the dry signal that is
            // already aligned to the same final-limiter latency. No extra
            // lookahead/delay is introduced by Delta monitoring.
            if (p.deltaMonitor)
                wet[n] = wet[n] - delayedDry;
        }
    }

    for (int ch = nCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}


