#include "ChainDSP.h"

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

float VVChainDSP::analogColor(float x, float amount01, bool solidState,
                                    float& previousInput, float& evenDc,
                                    float& levelPower, double sampleRate) noexcept
{
    const float a = juce::jlimit(0.f, 1.f, amount01);
    const float safeRate = static_cast<float>(std::max(8000.0, sampleRate));

    if (a <= 0.0f)
    {
        previousInput = x;
        return x;
    }

    const float alpha = std::exp(-1.0f / (0.020f * safeRate));
    levelPower = alpha * levelPower + (1.0f - alpha) * (x * x);
    const float level = std::max(
        0.03f,
        std::sqrt(std::max(levelPower * 2.0f, 1.0e-10f)));

    const float amount = std::pow(a, 0.85f);
    const float previous = previousInput;
    float colourDelta = 0.0f;

    for (int i = 1; i <= 4; ++i)
    {
        const float t = static_cast<float>(i) * 0.25f;
        const float sub = previous + (x - previous) * t;
        const float u = juce::jlimit(-1.15f, 1.15f, sub / level);
        float shaped = u;

        if (solidState)
        {
            const float drive = 1.15f + 2.15f * amount;
            const float norm = std::tanh(drive);
            shaped = norm > 1.0e-6f
                ? std::tanh(u * drive) / norm
                : u;
            shaped += 0.0125f * u * u * u;
        }
        else
        {
            const float drive = 0.95f + 1.75f * amount;
            const float asymmetric = u + 0.055f * u * u;
            const float norm = std::atan(drive);
            shaped = norm > 1.0e-6f
                ? std::atan(asymmetric * drive) / norm
                : u;
        }

        colourDelta += (shaped - u) * level;
    }

    colourDelta *= 0.25f;

    const float dcAlpha = std::exp(-1.0f / (0.200f * safeRate));
    evenDc = dcAlpha * evenDc + (1.0f - dcAlpha) * colourDelta;
    colourDelta -= evenDc;

    float delta = amount * colourDelta;

    if ((x > 0.f && delta > 0.f) || (x < 0.f && delta < 0.f))
    {
        const float headroom = 0.985f - std::abs(x);
        if (headroom <= 0.f)
            delta = 0.f;
        else
            delta = std::copysign(
                std::min(std::abs(delta), headroom), delta);
    }

    previousInput = x;
    return x + delta;
}

void VVChainDSP::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    sr = std::max(8000.0, sampleRate);
    channels = juce::jlimit(1, 2, numChannels);

    const int maxBlock = juce::jmax(1, samplesPerBlock);
    dryBuffer.setSize(channels, maxBlock, false, true, true);
    alignedDryBuffer.setSize(channels, maxBlock, false, true, true);

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

    reset();
}

void VVChainDSP::reset()
{
    for (auto& b : eq) b.reset();
    for (auto& state : analogPreviousInput)
        state = { 0.f, 0.f };
    for (auto& state : analogEvenDc)
        state = { 0.f, 0.f };
    for (auto& state : analogLevelPower)
        state = { 0.f, 0.f };

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
}

float VVChainDSP::rmsDetectPDR(float input,
                                  float& fastPower,
                                  float& slowPower,
                                  float attackMs,
                                  float releaseMs,
                                  double sampleRate,
                                  float& programReleaseMs) noexcept
{
    const float target = input * input;

    const float fastAttack = timeCoeff(sampleRate, attackMs);
    const float fastRelease =
        timeCoeff(sampleRate, juce::jmax(0.5f, releaseMs * 0.35f));
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

    const float targetLinear = dbToGain(juce::jlimit(0.f, 9.f, targetGainDb));
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
                                                float ratio)
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
        ? timeCoeff(sampleRate, attackMs)
        : timeCoeff(sampleRate, releaseMs);
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

    if (!p.eqBypass)
    {
        for (size_t i = 0; i < eq.size(); ++i)
        {
            updateAnalogPeak(
                eq[i], osSr,
                juce::jlimit(20.0, osSr * 0.45, static_cast<double>(p.freq[i])),
                juce::jlimit(-24.0, 24.0, static_cast<double>(p.gain[i])),
                juce::jlimit(0.1, 18.0, static_cast<double>(p.q[i])));
        }
    }

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = osBlock.getChannelPointer(static_cast<size_t>(ch));
        const bool right = ch == 1;

        for (size_t n = 0; n < osBlock.getNumSamples(); ++n)
        {
            float y = data[n];

            if (!p.eqBypass)
            {
                for (size_t band = 0; band < eq.size(); ++band)
                {
                    y = eq[band].process(y, right);

                    if (!p.eqColorGlobalBypass && !p.eqColorBypass[band])
                    {
                        const float amount =
                            juce::jlimit(0.f, 100.f, p.eqColor[band]) / 100.f;

                        y = analogColor(
                            y,
                            amount,
                            p.eqColorSolidState[band],
                            analogPreviousInput[band][static_cast<size_t>(ch)],
                            analogEvenDc[band][static_cast<size_t>(ch)],
                            analogLevelPower[band][static_cast<size_t>(ch)],
                            osSr);
                    }
                }
            }

            data[n] = y;
        }
    }

    // Even when EQ is bypassed, keep the fixed oversampling latency stable.
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
                // Each stage has its own RMS detector state for this band/channel.
                float downReleaseMs = p.ottCompRelease[(size_t) band];
                const float downDb = rmsDetectPDR(
                    v,
                    state.downRmsPower[(size_t) ch],
                    state.downSlowRmsPower[(size_t) ch],
                    p.ottCompAttack[(size_t) band],
                    p.ottCompRelease[(size_t) band],
                    sr,
                    downReleaseMs);

                v = applyCompressorFromDetectorDb(
                    v, downDb, compEnv,
                    p.ottCompThreshold[(size_t) band],
                    p.ottCompAttack[(size_t) band],
                    downReleaseMs,
                    compMix, sr, downRatio);

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
                original + globalMix * (wet - original);
        }
    }
}

void VVChainDSP::applyAType(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    const float ax1 = 80.f;
    const float ax2 = 3000.f;
    const float ax3 = 9000.f;
    const float typeQ = 0.70710678f;

    updateCrossover2nd(typeXover1, sr, ax1, typeQ);
    updateCrossover2nd(typeXover2, sr, ax2, typeQ);
    updateCrossover2nd(typeXover3, sr, ax3, typeQ);

    const float inputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.atypeInputGainDb));
    const float outputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.atypeOutputGainDb));
    const float attackCoeff =
        timeCoeff(sr, juce::jlimit(1.f, 100.f, p.atypeAttackMs));
    const float releaseCoeff =
        timeCoeff(sr, juce::jlimit(20.f, 500.f, p.atypeReleaseMs));
    const float mix =
        juce::jlimit(0.f, 1.f, p.atypeMix / 100.f);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float original = data[n];
            const float x = original * inputGain;

            const float b1 = typeXover1.low(x, right);
            const float b3 = typeXover2.high(x, right);
            const float b4 = typeXover3.high(x, right);
            const float b2 = x - b1 - b3;

            const float bands[4] = { b1, b2, b3, b4 };
            float enhancement = 0.f;

            for (int band = 0; band < 4; ++band)
            {
                if (p.atypeBandBypass[(size_t) band])
                    continue;

                const float degree =
                    juce::jlimit(0.f, 100.f, p.atypeDegree[(size_t) band]);
                if (degree <= 0.f)
                    continue;

                const float magnitude = std::abs(bands[band]);
                float& fastEnv = typeFastEnv[(size_t) band][(size_t) ch];
                float& slowEnv = typeSlowEnv[(size_t) band][(size_t) ch];
                float& gainState = typeDc[(size_t) band][(size_t) ch];

                const float fastAlpha =
                    magnitude > fastEnv ? attackCoeff : releaseCoeff;
                fastEnv = fastAlpha * fastEnv
                    + (1.f - fastAlpha) * magnitude;

                const float slowAlpha =
                    magnitude > slowEnv ? attackCoeff : releaseCoeff;
                slowEnv = slowAlpha * slowEnv
                    + (1.f - slowAlpha) * magnitude;

                const float levelDb =
                    gainToDb(std::max(slowEnv, 1.0e-7f));
                const float depth = degree / 100.f;

                const float thresholdDb =
                    -56.f + 20.f * std::sqrt(depth);
                const float ratio =
                    1.f + 15.f * std::sqrt(depth);
                const float slope =
                    1.f - 1.f / juce::jmax(1.f, ratio);

                const float kneeStart = thresholdDb - 3.f;
                const float kneeEnd = thresholdDb + 3.f;

                float targetGainDb = 0.f;
                if (levelDb < kneeStart)
                    targetGainDb =
                        (thresholdDb - levelDb) * slope;
                else if (levelDb < kneeEnd)
                {
                    const float xk = kneeEnd - levelDb;
                    targetGainDb =
                        slope / 12.f * xk * xk;
                }

                targetGainDb =
                    juce::jlimit(0.f, 9.f, targetGainDb * depth);

                const float gainAlpha =
                    targetGainDb > gainState
                        ? attackCoeff
                        : releaseCoeff;
                gainState =
                    gainAlpha * gainState
                    + (1.f - gainAlpha) * targetGainDb;

                const float bandTrim =
                    dbToGain(juce::jlimit(
                        -6.f, 6.f,
                        p.atypeBandLevelDb[(size_t) band]));

                const float processed =
                    bands[band]
                    * dbToGain(gainState)
                    * bandTrim;

                enhancement += processed - bands[band];
            }

            float delta = enhancement * mix;

            if ((x > 0.f && delta > 0.f) || (x < 0.f && delta < 0.f))
            {
                const float headroom = 0.985f - std::abs(x);
                if (headroom <= 0.f)
                    delta = 0.f;
                else
                    delta = std::copysign(
                        std::min(std::abs(delta), headroom), delta);
            }

            data[n] = (x + delta) * outputGain;
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

    // prepareToPlay() owns all DSP memory allocation. The host contract supplies
    // blocks no larger than the prepared maximum; never resize on the audio thread.
    if (nCh > dryBuffer.getNumChannels() || numSamples > dryBuffer.getNumSamples())
        return;

    for (int ch = 0; ch < nCh; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    if (!p.eqBypass)
        applyEq(buffer, p);
    if (!p.ottBypass)
        applyOtt(buffer, p);
    if (!p.atypeBypass)
        applyAType(buffer, p);

    // Intentionally no FFT, lookahead or plugin PDC.
    processDeEsser(buffer, p);

    if (!p.mixBypass)
    {
        const float mix = juce::jlimit(0.f, 1.f, p.dryWet / 100.f);
        const float out = dbToGain(juce::jlimit(-24.f, 12.f, p.outputDb));

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const auto* original = dryBuffer.getReadPointer(ch);

            for (int n = 0; n < numSamples; ++n)
                wet[n] = (original[n] + mix * (wet[n] - original[n])) * out;
        }
    }

    const float soloX1 = juce::jlimit(40.f, 1000.f, p.ottX1);
    const float soloX2 = juce::jlimit(soloX1 + 80.f, 5000.f, p.ottX2);
    const float soloX3 = juce::jlimit(
        soloX2 + 200.f,
        static_cast<float>(sr * 0.42),
        p.ottX3);
    const float soloQ = crossoverQFromOverlap(p.ottXoverOverlap);

    updateCrossover(soloPreXover1, sr, soloX1, soloQ);
    updateCrossover(soloPreXover2, sr, soloX2, soloQ);
    updateCrossover(soloPreXover3, sr, soloX3, soloQ);
    updateCrossover(soloPostXover1, sr, soloX1, soloQ);
    updateCrossover(soloPostXover2, sr, soloX2, soloQ);
    updateCrossover(soloPostXover3, sr, soloX3, soloQ);

    const bool soloEnabled = p.soloBand >= 0 && p.soloBand < 4;
    if (p.soloBand != lastSoloBand || p.soloPost != lastSoloPost)
    {
        soloBlend = 0.f;
        lastSoloBand = p.soloBand;
        lastSoloPost = p.soloPost;
    }

    auto splitBand = [](float x,
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

    // One crossfade counter per audio sample, shared by both channels.
    // Switching SOLO or PRE/POST therefore cannot make a channel-dependent click.
    const int safeSoloBand = juce::jlimit(0, 3, p.soloBand);
    for (int n = 0; n < numSamples; ++n)
    {
        if (soloEnabled)
            soloBlend = std::min(1.f, soloBlend + 1.f / 64.f);
        else
            soloBlend = std::max(0.f, soloBlend - 1.f / 64.f);

        if (soloBlend <= 0.f)
            continue;

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const bool right = ch == 1;

            const float preSolo = splitBand(
                dryBuffer.getSample(ch, n),
                soloPreXover1, soloPreXover2, soloPreXover3,
                safeSoloBand, right);
            const float postSolo = splitBand(
                wet[n],
                soloPostXover1, soloPostXover2, soloPostXover3,
                safeSoloBand, right);

            const float solo = p.soloPost ? postSolo : preSolo;
            wet[n] = wet[n] * (1.f - soloBlend) + solo * soloBlend;
        }
    }

    // Soft master bypass. No host latency is introduced.
    const float target = p.masterBypass ? 1.f : 0.f;
    const float step = 1.f / static_cast<float>(kMasterBypassRampSamples);

    for (int n = 0; n < numSamples; ++n)
    {
        if (masterBypassBlend < target)
            masterBypassBlend = std::min(target, masterBypassBlend + step);
        else if (masterBypassBlend > target)
            masterBypassBlend = std::max(target, masterBypassBlend - step);

        const float blend = masterBypassBlend;
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const float original = dryBuffer.getSample(ch, n);
            wet[n] = wet[n] * (1.f - blend) + original * blend;
        }
    }

    for (int ch = nCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}

