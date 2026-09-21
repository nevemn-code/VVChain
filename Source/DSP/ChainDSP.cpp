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
}

VVChainDSP::Biquad VVChainDSP::makeAnalogPeak(double fs, double f0, double gainDb, double q)
{
    Biquad c;
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

    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
    return c;
}

VVChainDSP::Biquad VVChainDSP::makeAnalogHighPass(double fs, double f0, double q)
{
    Biquad c;
    const double safeF = juce::jlimit(10.0, fs * 0.45, f0);
    const double K = std::tan(juce::MathConstants<double>::pi * safeF / fs);
    const double Q = std::max(0.05, q);

    const double b0 = 1.0;
    const double b1 = -2.0;
    const double b2 = 1.0;
    const double a0 = 1.0 + K / Q + K * K;
    const double a1 = 2.0 * (K * K - 1.0);
    const double a2 = 1.0 - K / Q + K * K;

    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
    return c;
}

VVChainDSP::Biquad VVChainDSP::makeLowPass(double fs, double f0, double q)
{
    Biquad c;
    const double safeF = juce::jlimit(10.0, fs * 0.45, f0);
    const double K = std::tan(juce::MathConstants<double>::pi * safeF / fs);
    const double Q = std::max(0.05, q);

    const double b0 = K * K;
    const double b1 = 2.0 * K * K;
    const double b2 = K * K;
    const double a0 = 1.0 + K / Q + K * K;
    const double a1 = 2.0 * (K * K - 1.0);
    const double a2 = 1.0 - K / Q + K * K;

    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
    return c;
}

VVChainDSP::Biquad VVChainDSP::makeHighPass(double fs, double f0, double q)
{
    return makeAnalogHighPass(fs, f0, q);
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

float VVChainDSP::softColor(float x, float amount01) noexcept
{
    const float a = juce::jlimit(0.f, 1.f, amount01);
    if (a <= 0.0f)
        return x;

    const float drive = 1.0f + 1.35f * a;
    const float asym = x + 0.012f * a * x * x;

    // Exact small-signal gain compensation:
    // tanh(drive * x) has a linear gain of "drive" around 0 dBFS.
    // Applying the reciprocal makeup keeps the analog color from becoming
    // an accidental level boost while retaining harmonic saturation.
    const float makeupGain = 1.0f / drive;
    return std::tanh(asym * drive) * makeupGain;
}

void VVChainDSP::prepare(double sampleRate, int, int numChannels)
{
    sr = std::max(8000.0, sampleRate);
    channels = juce::jlimit(1, 2, numChannels);
    reset();
}

void VVChainDSP::reset()
{
    for (auto& b : eq) b.reset();
    hp.reset();

    ottXover1.reset();
    ottXover2.reset();
    ottXover3.reset();

    for (auto& b : ottDynamics)
    {
        b.gateEnvDb = { 0.f, 0.f };
        b.lifterEnv = { 1.f, 1.f };
        b.compEnvDb = { 0.f, 0.f };
    }

    typeXover1.reset();
    typeXover2.reset();
    typeHP7k8.reset();
    for (size_t band = 0; band < 4; ++band)
    {
        typeFastEnv[band] = { 0.f, 0.f };
        typeSlowEnv[band] = { 0.f, 0.f };
        typeDc[band] = { 0.f, 0.f };
    }

    for (auto& state : deess)
    {
        state.input.fill(0.f);
        state.output.fill(0.f);
        state.inputCount = 0;
        state.queue.fill(0.f);
        state.queueRead = 0;
        state.queueWrite = 0;
        state.queueCount = 0;
    }

    deessAvgSum = 0.0;
    deessAvgCount = 0;
    deessSampleCounter = 0;
    deessPendingSample = 0.0f;

    for (auto& channel : dryDelay)
        channel.fill(0.f);
    dryDelayWrite = 0;

    gateEnvDb = { 0.f, 0.f };
    limiterEnvDb = { 0.f, 0.f };
}

float VVChainDSP::applyLifter(float input, float& env, float thresholdDb,
                              float attackMs, float releaseMs, float mix,
                              double sampleRate, float ratio)
{
    const float safeRatio = juce::jmax(1.0f, ratio);
    const float slope = 1.0f - (1.0f / safeRatio);
    const float kneeStart = thresholdDb - kLifterKneeDb * 0.5f;
    const float kneeEnd = thresholdDb + kLifterKneeDb * 0.5f;
    const float magnitude = std::max(std::abs(input), 0.0001f);
    const float inputDb = juce::Decibels::gainToDecibels(magnitude);

    float targetGainDb = 0.f;
    if (inputDb < kneeStart)
        targetGainDb = (thresholdDb - inputDb) * slope;
    else if (inputDb < kneeEnd)
    {
        const float x = kneeEnd - inputDb;
        targetGainDb = slope / (2.0f * kLifterKneeDb) * x * x;
    }

    const float targetLinear = dbToGain(juce::jlimit(0.f, 30.f, targetGainDb));
    const float attack = timeCoeff(sampleRate, attackMs);
    const float release = timeCoeff(sampleRate, releaseMs);
    const float alpha = targetLinear > env ? attack : release;
    env = alpha * env + (1.0f - alpha) * targetLinear;

    const float wet = input * env;
    const float m = juce::jlimit(0.f, 1.f, mix / 100.f);
    return wet * m + input * (1.0f - m);
}

float VVChainDSP::applyCompressor(float input, float& envDb, float thresholdDb,
                                  float attackMs, float releaseMs, float mix,
                                  double sampleRate, float ratio)
{
    const float slope = 1.0f - (1.0f / ratio);
    const float kneeStart = thresholdDb - kCompressorKneeDb * 0.5f;
    const float kneeEnd = thresholdDb + kCompressorKneeDb * 0.5f;
    const float magnitude = std::max(std::abs(input), 0.00001f);
    const float inputDb = juce::Decibels::gainToDecibels(magnitude);

    float targetReductionDb = 0.f;
    if (inputDb > kneeEnd)
        targetReductionDb = (inputDb - thresholdDb) * slope;
    else if (inputDb > kneeStart)
    {
        const float x = inputDb - kneeStart;
        targetReductionDb = slope / (2.0f * kCompressorKneeDb) * x * x;
    }

    const float currentReductionDb = -envDb;
    const float alpha = targetReductionDb > currentReductionDb
        ? timeCoeff(sampleRate, attackMs)
        : timeCoeff(sampleRate, releaseMs);
    const float smoothedReduction = alpha * currentReductionDb
        + (1.0f - alpha) * targetReductionDb;

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

    // Safety stage only: never let OTT's wet bus become an accidental
    // +10/+20 dB jump. Unity is untouched below the ceiling.
    constexpr float ceiling = 0.9440608763f; // -0.5 dBFS
    const float magnitude = std::abs(input);
    if (magnitude <= ceiling)
        return input;

    const float excess = magnitude - ceiling;
    const float shaped = ceiling + excess / (1.0f + 20.0f * excess);
    const float safe = std::min(shaped, 0.99f);
    return std::copysign(safe, input);
}

void VVChainDSP::applyEq(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    hp = makeAnalogHighPass(sr, juce::jlimit(40.f, 120.f, p.hfCornerHz), 0.707);

    for (size_t i = 0; i < eq.size(); ++i)
        eq[i] = makeAnalogPeak(
            sr,
            juce::jlimit(20.f, static_cast<float>(sr * 0.45), p.freq[i]),
            juce::jlimit(-24.f, 24.f, p.gain[i]),
            juce::jlimit(0.1f, 18.f, p.q[i]));

    const float colorAmount = juce::jlimit(0.f, 100.f, p.eqColor) / 100.f;

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            float y = hp.process(data[n], right);
            for (auto& band : eq)
                y = band.process(y, right);

            // Apply analogue coloration once after the four EQ bands.
            // This prevents four separate nonlinear gain stages from stacking.
            y = softColor(y, colorAmount);
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

    ottXover1.lp1 = makeLowPass(sr, x1, 0.707);
    ottXover1.lp2 = makeLowPass(sr, x1, 0.707);
    ottXover1.hp1 = makeHighPass(sr, x1, 0.707);
    ottXover1.hp2 = makeHighPass(sr, x1, 0.707);

    ottXover2.lp1 = makeLowPass(sr, x2, 0.707);
    ottXover2.lp2 = makeLowPass(sr, x2, 0.707);
    ottXover2.hp1 = makeHighPass(sr, x2, 0.707);
    ottXover2.hp2 = makeHighPass(sr, x2, 0.707);

    ottXover3.lp1 = makeLowPass(sr, x3, 0.707);
    ottXover3.lp2 = makeLowPass(sr, x3, 0.707);
    ottXover3.hp1 = makeHighPass(sr, x3, 0.707);
    ottXover3.hp2 = makeHighPass(sr, x3, 0.707);

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
                v = applyCompressor(
                    v, compEnv,
                    p.ottCompThreshold[(size_t) band],
                    p.ottCompAttack[(size_t) band],
                    p.ottCompRelease[(size_t) band],
                    compMix, sr, downRatio);

                v = applyLifter(
                    v, lifterEnv,
                    p.ottLifterThreshold[(size_t) band],
                    p.ottLifterAttack[(size_t) band],
                    p.ottLifterRelease[(size_t) band],
                    lifterMix, sr, upRatio);

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
    // Band layout:
    // B1 20-200 Hz
    // B2 200-2000 Hz
    // B3 2000-7800 Hz
    // B4 7800-20 kHz
    constexpr float x1 = 200.f;
    constexpr float x2 = 2000.f;
    constexpr float x3 = 7800.f;

    typeXover1.lp1 = makeLowPass(sr, x1, 0.707);
    typeXover1.lp2 = makeLowPass(sr, x1, 0.707);
    typeXover1.hp1 = makeHighPass(sr, x1, 0.707);
    typeXover1.hp2 = makeHighPass(sr, x1, 0.707);

    typeXover2.lp1 = makeLowPass(sr, x2, 0.707);
    typeXover2.lp2 = makeLowPass(sr, x2, 0.707);
    typeXover2.hp1 = makeHighPass(sr, x2, 0.707);
    typeXover2.hp2 = makeHighPass(sr, x2, 0.707);

    typeHP7k8 = makeHighPass(sr, x3, 0.707);

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
            const float b3Input = typeXover2.high(x1High, right);
            const float b3 = b3Input;
            const float b4 = typeHP7k8.process(b3Input, right);
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
                    * (0.10f + 0.90f * transient);

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
                harmonic = std::tanh(harmonic * 1.5f) / 1.5f;

                const float bandTrim =
                    dbToGain(juce::jlimit(-6.f, 6.f,
                                          p.atypeBandLevelDb[(size_t) band]));

                const float levelScaledAmount =
                    amount * (0.20f + 0.80f * levelFactor);

                harmonicSum += harmonic
                    * magnitude
                    * levelScaledAmount
                    * bandTrim;
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

void VVChainDSP::fft(std::array<std::complex<double>, kDeessBlockSize>& data, bool inverse)
{
    // Iterative radix-2 Cooley-Tukey FFT. No windowing: this intentionally
    // follows the reference processor's FFT -> filter -> IFFT structure.
    const size_t n = data.size();

    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(data[i], data[j]);
    }

    for (size_t length = 2; length <= n; length <<= 1)
    {
        const double angle = (inverse ? 1.0 : -1.0) * kTwoPi / static_cast<double>(length);
        const std::complex<double> wlen(std::cos(angle), std::sin(angle));

        for (size_t i = 0; i < n; i += length)
        {
            std::complex<double> w(1.0, 0.0);
            for (size_t j = 0; j < length / 2; ++j)
            {
                const auto u = data[i + j];
                const auto v = data[i + j + length / 2] * w;
                data[i + j] = u + v;
                data[i + j + length / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse)
    {
        const double scale = 1.0 / static_cast<double>(n);
        for (auto& v : data)
            v *= scale;
    }
}

void VVChainDSP::processDeEsserWindow(DeEssState& state, const Parameters& p)
{
    constexpr int count = kDeessBlockSize;
    const float* input = state.input.data();

    const float globalAverage = deessAvgCount > 0
        ? static_cast<float>(deessAvgSum / static_cast<double>(deessAvgCount))
        : 0.0f;
    const float average = globalAverage + p.deessAverageOffset;

    int countMore = 0;
    for (int i = 0; i < count - 2; i += 2)
        if (std::abs(input[i + 1] - input[i]) > average)
            ++countMore;

    for (int i = 0; i < count; ++i)
        deessFft[(size_t)i] = std::complex<double>(static_cast<double>(input[i]), 0.0);

    // Reference trigger condition: COUNT > 10.
    constexpr int trigger = 10;

    if (!p.deessBypass && countMore > trigger)
    {
        fft(deessFft, false);

        const double referenceHz = juce::jlimit(6000.0, 18000.0,
            static_cast<double>(p.deessReferenceHz));
        const double intensity =
            juce::jlimit(2.0, 10.0, static_cast<double>(p.deessIntensity));

        for (int i = 1; i < count / 2; ++i)
        {
            const double freq = kReferenceSampleRate * static_cast<double>(i)
                              / static_cast<double>(count);

            const double coeff = freq < referenceHz / 10.0
                ? 0.5
                : (freq >= referenceHz
                    ? intensity * referenceHz / std::max(freq, 1.0)
                    : 1.0 + (intensity - 1.0) * std::pow(freq / referenceHz, 3.0));

            deessFft[(size_t)i] /= coeff;
            deessFft[(size_t)(count - i)] /= coeff;
        }

        fft(deessFft, true);
    }

    constexpr float mix = 1.0f;

    for (int i = 0; i < count; ++i)
    {
        float y = (!p.deessBypass && countMore > trigger)
            ? static_cast<float>(deessFft[(size_t)i].real())
            : input[i];

        y = input[i] * (1.0f - mix) + y * mix;

        state.queue[(size_t)state.queueWrite] = y;
        state.queueWrite =
            (state.queueWrite + 1) % static_cast<int>(state.queue.size());
    }

    state.queueCount = std::min(
        state.queueCount + count, static_cast<int>(state.queue.size()));
    state.inputCount = 0;
}

void VVChainDSP::processDeEsser(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    const int samples = buffer.getNumSamples();

    for (int n = 0; n < samples; ++n)
    {
        const float detectorSample = buffer.getReadPointer(0)[n];

        if ((deessSampleCounter & 1ULL) == 0)
            deessPendingSample = detectorSample;
        else
        {
            deessAvgSum += std::abs(detectorSample - deessPendingSample);
            ++deessAvgCount;
        }

        ++deessSampleCounter;

        for (int ch = 0; ch < channels; ++ch)
            deess[(size_t)ch].input[(size_t)deess[(size_t)ch].inputCount++] =
                buffer.getReadPointer(ch)[n];

        if (deess[0].inputCount == kDeessBlockSize)
            for (int ch = 0; ch < channels; ++ch)
                processDeEsserWindow(deess[(size_t)ch], p);

        for (int ch = 0; ch < channels; ++ch)
        {
            auto& state = deess[(size_t)ch];
            float output = 0.0f;

            if (state.queueCount > 0)
            {
                output = state.queue[(size_t)state.queueRead];
                state.queueRead =
                    (state.queueRead + 1) % static_cast<int>(state.queue.size());
                --state.queueCount;
            }

            buffer.getWritePointer(ch)[n] = output;
        }
    }
}

void VVChainDSP::process(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;

    const int nCh = std::min(buffer.getNumChannels(), channels);
    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    // The supplied reference uses complete 8192-sample blocks. The dry path uses the same fixed delay so MIX remains sample-aligned.
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        for (int ch = 0; ch < nCh; ++ch)
        {
            dry.getWritePointer(ch)[n] = dryDelay[(size_t)ch][(size_t)dryDelayWrite];
            dryDelay[(size_t)ch][(size_t)dryDelayWrite] = buffer.getReadPointer(ch)[n];
        }
        dryDelayWrite = (dryDelayWrite + 1) % kDeessBlockSize;
    }

    if (!p.eqBypass)
        applyEq(buffer, p);

    if (!p.ottBypass)
        applyOtt(buffer, p);

    if (!p.atypeBypass)
        applyAType(buffer, p);

    // Always execute the streaming stage so bypass has the same reported
    // latency and remains sample-aligned with the rest of the chain.
    processDeEsser(buffer, p);

    if (!p.mixBypass)
    {
        const float mix = juce::jlimit(0.f, 1.f, p.dryWet / 100.f);
        const float out = dbToGain(juce::jlimit(-24.f, 12.f, p.outputDb));

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const auto* delayedDry = dry.getReadPointer(ch);

            for (int n = 0; n < buffer.getNumSamples(); ++n)
                wet[n] = (delayedDry[n] + mix * (wet[n] - delayedDry[n])) * out;
        }
    }

    for (int ch = nCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());
}
