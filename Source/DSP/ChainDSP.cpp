void VVChainDSP::updateCrossover(Crossover4th& xover, double fs, double f0, double q)
{
    updateLowPass(xover.lp1, fs, f0, q);
    updateLowPass(xover.lp2, fs, f0, q);
    updateHighPass(xover.hp1, fs, f0, q);
    updateHighPass(xover.hp2, fs, f0, q);
}

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
    const double safeF = juce::jlimit(20.0, fs * 0.45, f0);
    const double A = std::pow(10.0, gainDb / 40.0);
    const double K = std::tan(juce::MathConstants<double>::pi * safeF / fs);
    const double Q = std::max(0.05, q);

    const double b0 = K * K + (A / Q) * K + 1.0;
    const double b1 = 2.0 * (K * K - 1.0);
    const double b2 = K * K - (A / Q) * K + 1.0;
    const double a0 = K * K + (1.0 / (A * Q)) * K + 1.0;
    const double a1 = 2.0 * (K * K - 1.0);
    const double a2 = K * K - (1.0 / (A * Q)) * K + 1.0;

    filter.updateCoefficients(b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
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
    previousInput = x;

    if (a <= 0.0f)
        return x;

    const float safeRate = static_cast<float>(std::max(8000.0, sampleRate));
    const float alpha = std::exp(-1.0f / (0.015f * safeRate));
    levelPower = alpha * levelPower + (1.0f - alpha) * (x * x);

    // RMS is used only to normalize the added harmonic generator. The dry
    // waveform is never gain-modulated, so this stage colours instead of compresses.
    const float level = std::max(
        0.03f, std::sqrt(std::max(levelPower * 2.0f, 1.0e-10f)));

    if (std::abs(x) <= 1.0e-6f && level < 0.031f)
        return x;

    const float amount = std::pow(a, 0.90f);
    const float z = juce::jlimit(-1.0f, 1.0f, x / level);

    const auto t2 = [](float v) noexcept { return 2.0f * v * v - 1.0f; };
    const auto t3 = [](float v) noexcept { return 4.0f * v * v * v - 3.0f * v; };
    const auto t4 = [](float v) noexcept
    {
        const float v2 = v * v;
        return 8.0f * v2 * v2 - 8.0f * v2 + 1.0f;
    };
    const auto t5 = [](float v) noexcept
    {
        const float v2 = v * v;
        return 16.0f * v2 * v2 * v - 20.0f * v2 * v + 5.0f * v;
    };
    const auto t7 = [](float v) noexcept
    {
        const float v2 = v * v;
        const float v3 = v2 * v;
        const float v5 = v3 * v2;
        const float v7 = v5 * v2;
        return 64.0f * v7 - 112.0f * v5 + 56.0f * v3 - 7.0f * v;
    };

    float harmonic = 0.0f;
    if (solidState)
    {
        harmonic =
            0.026f * t3(z)
            + 0.007f * t5(z)
            + 0.002f * t7(z);
    }
    else
    {
        const float raw =
            0.038f * t2(z)
            + 0.010f * t4(z)
            + 0.003f * t3(z);

        constexpr float dcAlpha = 0.99990f;
        evenDc = dcAlpha * evenDc + (1.0f - dcAlpha) * raw;
        harmonic = raw - evenDc;
    }

    return x + amount * 1.35f * level * harmonic;
}

void VVChainDSP::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    sr = std::max(8000.0, sampleRate);
    channels = juce::jlimit(1, 2, numChannels);
    dryBuffer.setSize(channels, juce::jmax(1, samplesPerBlock), false, true, true);
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

    for (auto& b : ottDynamics)
    {
        b.gateEnvDb = { 0.f, 0.f };
        b.lifterEnv = { 1.f, 1.f };
        b.compEnvDb = { 0.f, 0.f };
        b.upRmsPower = { 0.f, 0.f };
        b.downRmsPower = { 0.f, 0.f };
    }

    typeXover1.reset();
    typeXover2.reset();
    typeXover3.reset();

    masterBypassBlend = 0.f;
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
}

float VVChainDSP::rmsDetect(float input, float& power, float attackMs,
                                    float releaseMs, double sampleRate) noexcept
{
    const float target = input * input;
    const float alpha = target > power
        ? timeCoeff(sampleRate, attackMs)
        : timeCoeff(sampleRate, releaseMs);
    power = alpha * power + (1.f - alpha) * target;
    return std::sqrt(std::max(power, 1.0e-12f));
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
    juce::ignoreUnused(envDb, sampleRate);
    constexpr float ceiling = 0.9440608763f; // -0.5 dBFS
    const float magnitude = std::abs(input);
    if (magnitude <= ceiling)
        return input;
    const float excess = magnitude - ceiling;
    const float shaped = ceiling + excess / (1.0f + 20.0f * excess);
    return std::copysign(std::min(shaped, 0.99f), input);
}

void VVChainDSP::applyEq(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    for (size_t i = 0; i < eq.size(); ++i)
        updateAnalogPeak(eq[i], sr,
            juce::jlimit(20.f, static_cast<float>(sr * 0.45), p.freq[i]),
            juce::jlimit(-24.f, 24.f, p.gain[i]),
            juce::jlimit(0.1f, 18.f, p.q[i]));

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            float y = data[n];

            for (size_t band = 0; band < eq.size(); ++band)
            {
                y = eq[band].process(y, right);

                const float amount =
                    juce::jlimit(0.f, 100.f, p.eqColor[band]) / 100.f;

                if (!p.eqColorGlobalBypass && !p.eqColorBypass[band])
                    y = analogColor(
                    y, amount, p.eqColorSolidState[band],
                    analogPreviousInput[band][(size_t) ch],
                    analogEvenDc[band][(size_t) ch],
                    analogLevelPower[band][(size_t) ch],
                    sr);
            }

            data[n] = y;
        }
    }
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

    ottXover1.lp1 = makeLowPass(sr, x1, xoverQ);
    ottXover1.lp2 = makeLowPass(sr, x1, xoverQ);
    ottXover1.hp1 = makeHighPass(sr, x1, xoverQ);
    ottXover1.hp2 = makeHighPass(sr, x1, xoverQ);

    ottXover2.lp1 = makeLowPass(sr, x2, xoverQ);
    ottXover2.lp2 = makeLowPass(sr, x2, xoverQ);
    ottXover2.hp1 = makeHighPass(sr, x2, xoverQ);
    ottXover2.hp2 = makeHighPass(sr, x2, xoverQ);

    ottXover3.lp1 = makeLowPass(sr, x3, xoverQ);
    ottXover3.lp2 = makeLowPass(sr, x3, xoverQ);
    ottXover3.hp1 = makeHighPass(sr, x3, xoverQ);
    ottXover3.hp2 = makeHighPass(sr, x3, xoverQ);

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

            const float low = ottXover1.low(x, right);
            const float x1High = ottXover1.high(x, right);
            const float lowMid = ottXover2.low(x1High, right);
            const float x2High = ottXover2.high(x1High, right);
            const float midHigh = ottXover3.low(x2High, right);
            const float top = ottXover3.high(x2High, right);

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
                const float downRms = rmsDetect(
                    v, state.downRmsPower[(size_t) ch],
                    p.ottCompAttack[(size_t) band],
                    p.ottCompRelease[(size_t) band], sr);
                const float downDb = gainToDb(downRms);
                v = applyCompressorFromDetectorDb(
                    v, downDb, compEnv,
                    p.ottCompThreshold[(size_t) band],
                    p.ottCompAttack[(size_t) band],
                    p.ottCompRelease[(size_t) band],
                    compMix, sr, downRatio);

                const float upRms = rmsDetect(
                    v, state.upRmsPower[(size_t) ch],
                    p.ottLifterAttack[(size_t) band],
                    p.ottLifterRelease[(size_t) band], sr);
                const float upDb = gainToDb(upRms);
                const float liftThreshold =
                    juce::jmax(p.ottLifterThreshold[(size_t) band], -48.f);
                if (upDb > liftThreshold)
                {
                    v = applyLifterFromDetectorDb(
                        v, upDb, lifterEnv,
                        liftThreshold,
                        p.ottLifterAttack[(size_t) band],
                        p.ottLifterRelease[(size_t) band],
                        lifterMix, sr, upRatio);
                }

                v *= dbToGain(
                    juce::jlimit(-24.f, 12.f,
                        p.ottBandLevelDb[(size_t) band]));

                bands[band] = v;
            }

            float wet = bands[0] + bands[1] + bands[2] + bands[3];

            // Global safety only. It runs after the four independent band processors
            // and never feeds any result back into a band detector.
            wet = applyLimiter(
                wet, limiterEnvDb[(size_t) ch], sr);

            if (p.ottClipper)
                wet = std::tanh(wet * 1.7f);

            // OTT Depth/Mix is the final dry/wet blend. The input signal is
            // never used as a detector for another band.
            data[n] =
                (original + globalMix * (wet - original)) * outputGain;
        }
    }
}

void VVChainDSP::applyAType(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    // Type-A is a four-band harmonic exciter, not a multiband EQ.
    // Each band has its own sidechain detector + nonlinear generator.
    // The original band signal remains intact; only newly generated
    // harmonic content is mixed back at a controlled level.
    //
    // Band layout is controlled by the shared OTT X1/X2/X3 crossover,
    // so OTT and TAPE-A always use the same four frequency regions.
    const float tx1 = juce::jlimit(40.f, 1000.f, p.ottX1);
    const float tx2 = juce::jlimit(tx1 + 80.f, 5000.f, p.ottX2);
    const float tx3 = juce::jlimit(tx2 + 200.f, static_cast<float>(sr * 0.42), p.ottX3);
    const float typeQ = crossoverQFromOverlap(p.ottXoverOverlap);

    updateCrossover(typeXover1, sr, tx1, typeQ);
    updateCrossover(typeXover2, sr, tx2, typeQ);
    updateCrossover(typeXover3, sr, tx3, typeQ);

    const float inputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.atypeInputGainDb));
    const float outputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.atypeOutputGainDb));
    const float attackCoeff =
        timeCoeff(sr, juce::jlimit(1.f, 100.f, p.atypeAttackMs));
    const float releaseCoeff =
        timeCoeff(sr, juce::jlimit(20.f, 500.f, p.atypeReleaseMs));
    const float slowCoeff =
        timeCoeff(sr, juce::jlimit(10.f, 1000.f, p.atypeReleaseMs * 1.75f));
    const float dcCoeff = timeCoeff(sr, 20.f);

    // Fixed internal timbre profile. Lower bands favour even-order warmth;
    // higher bands progressively favour odd-order presence.
    constexpr std::array<float, 4> evenWeight
    {
        0.68f, 0.54f, 0.34f, 0.18f
    };

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float original = data[n];
            const float x = original * inputGain;

            const float b1 = typeXover1.low(x, right);
            const float x1High = typeXover1.high(x, right);
            const float b2 = typeXover2.low(x1High, right);
            const float x2High = typeXover2.high(x1High, right);
            const float b3 = typeXover3.low(x2High, right);
            const float b4 = typeXover3.high(x2High, right);
            const float bands[4] = { b1, b2, b3, b4 };

            float harmonicSum = 0.f;

            for (int band = 0; band < 4; ++band)
            {
                if (p.atypeBandBypass[(size_t) band])
                    continue;

                const float degree =
                    juce::jlimit(0.f, 100.f, p.atypeDegree[(size_t) band]);
                if (degree <= 0.0f)
                    continue;

                const float input = bands[band];
                const float magnitude = std::abs(input);

                float& fastEnv = typeFastEnv[(size_t) band][(size_t) ch];
                float& slowEnv = typeSlowEnv[(size_t) band][(size_t) ch];
                float& dc = typeDc[(size_t) band][(size_t) ch];

                const float fastAlpha = magnitude > fastEnv
                    ? attackCoeff
                    : releaseCoeff;
                fastEnv = fastAlpha * fastEnv
                    + (1.f - fastAlpha) * magnitude;

                slowEnv = slowCoeff * slowEnv
                    + (1.f - slowCoeff) * magnitude;

                // Aphex's transient-discriminate principle:
                // strong initial energy creates more harmonics; steady-state
                // material retains a smaller baseline harmonic contribution.
                const float transientRatio =
                    fastEnv / std::max(slowEnv, 1.0e-7f);
                const float transient =
                    juce::jlimit(0.f, 1.f, (transientRatio - 1.f) * 3.5f);

                // Harmonic generation is also level-dependent.
                // Very quiet material generates less; louder material generates
                // more, without directly changing the dry-band gain.
                const float levelDb = gainToDb(std::max(slowEnv, 1.0e-7f));
                const float levelFactor =
                    juce::jlimit(0.f, 1.25f, (levelDb + 48.f) / 36.f);

                // Transient discrimination is the main excitation envelope:
                // steady-state retains only a small floor, while a new transient
                // can open the harmonic generator strongly.
                const float amount = (degree / 100.f)
                    * (0.18f + 0.82f * transient);

                if (amount <= 1.0e-6f)
                    continue;

                // Normalize only the generator input. The final harmonic
                // amplitude is restored from the tracked envelope below.
                const float norm =
                    input / std::max(slowEnv, 1.0e-5f);

                // Saturating transfer creates the odd-order family.
                const float drive =
                    1.10f + 4.20f * (degree / 100.f)
                    * (0.35f + 0.65f * levelFactor);

                const float linearRef =
                    std::tanh(drive);
                const float oddShape =
                    linearRef > 1.0e-6f
                        ? std::tanh(norm * drive) / linearRef
                        : norm;

                // Add an asymmetric second-order component for even harmonics.
                // Its slow DC component is removed before summing.
                const float evenRaw = 0.5f * norm * norm;
                dc = dcCoeff * dc + (1.f - dcCoeff) * evenRaw;
                const float evenShape = evenRaw - dc;

                const float oddResidual = oddShape - norm;
                const float evenResidual = evenShape;

                const float ew = evenWeight[(size_t) band];
                const float ow = 1.f - ew;
                float harmonic = ow * oddResidual + ew * evenResidual;

                // Gentle harmonic-only containment prevents pathological peaks
                // while leaving the dry band untouched.
                harmonic = std::tanh(harmonic * 1.8f) / 1.8f;

                const float bandTrim =
                    dbToGain(juce::jlimit(-6.f, 6.f,
                                          p.atypeBandLevelDb[(size_t) band]));

                const float levelScaledAmount =
                    amount * (0.28f + 0.72f * levelFactor);

                harmonicSum += harmonic
                    * magnitude
                    * levelScaledAmount
                    * bandTrim
                    * 1.35f;
            }

            const float mix =
                juce::jlimit(0.f, 1.f, p.atypeMix / 100.f);

            // Crucial distinction from the previous implementation:
            // there is NO direct per-band gain boost and NO global saturation
            // of the full-band signal. Only newly generated harmonics are added.
            const float processed =
                x + harmonicSum * mix;

            data[n] = processed * outputGain;
        }
    }
}

void VVChainDSP::processDeEsser(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    const float referenceHz = juce::jlimit(6000.f, 18000.f, p.deessReferenceHz);
    const float detectionQ = 0.707f;

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

        updateHighPass(state.sidechainHP, sr, referenceHz, detectionQ);
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float input = data[n];
            const float sidechain =
                std::abs(state.sidechainHP.process(input, right));

            const float fastCoeff = sidechain > state.fastEnv
                ? fastAttack : fastRelease;
            state.fastEnv =
                fastCoeff * state.fastEnv
                + (1.f - fastCoeff) * sidechain;

            const float slowCoeff = sidechain > state.slowEnv
                ? slowAttack : slowRelease;
            state.slowEnv =
                slowCoeff * state.slowEnv
                + (1.f - slowCoeff) * sidechain;

            float targetReductionDb = 0.f;

            if (!p.deessBypass && p.deessIntensity > 0.f)
            {
                const float fastDb = gainToDb(state.fastEnv);
                const float slowDb = gainToDb(state.slowEnv);
                const float adaptiveExcess =
                    fastDb - slowDb - 2.0f - p.deessAverageOffset;

                const float trigger = juce::jlimit(
                    0.f, 1.f, adaptiveExcess / 6.f);

                targetReductionDb =
                    juce::jlimit(0.f, 8.f, p.deessIntensity) * trigger;
            }

            const float gainCoeff =
                targetReductionDb > state.gainDb
                    ? gainAttack : gainRelease;
            state.gainDb =
                gainCoeff * state.gainDb
                + (1.f - gainCoeff) * targetReductionDb;

            // Wide-band zero-latency gain control: one scalar gain on the full
            // sample preserves timing and avoids split-band phase rotation.
            data[n] = input * dbToGain(-state.gainDb);
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

