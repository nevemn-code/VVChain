#include "ChainDSP.h"

namespace
{
static constexpr float kLifterRatio = 6.0f;
static constexpr float kCompressorRatio = 8.0f;
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
    const float drive = 1.0f + 1.35f * a;
    const float asym = x + 0.012f * a * x * x;
    const float shaped = std::tanh(asym * drive);
    const float ref = std::tanh(drive);
    return ref > 0.0f ? shaped / ref : x;
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
        b.lifterEnv = { 1.f, 1.f };
        b.compEnvDb = { 0.f, 0.f };
    }

    typeXover1.reset();
    typeXover2.reset();
    typeHP9k.reset();
    for (auto& e : typeEnv)
        e = { 0.f, 0.f };

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
                              double sampleRate)
{
    const float slope = 1.0f - (1.0f / kLifterRatio);
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

    float targetGainDb = 0.f;
    if (inputDb < kneeStart)
        targetGainDb = (inputDb - thresholdDb) * ratioSlope;
    else if (inputDb < kneeEnd)
    {
        const float x = inputDb - kneeEnd;
        targetGainDb = ratioSlope / (2.f * knee) * x * x;
    }

    const float attack = timeCoeff(sampleRate, 100.f);
    const float release = timeCoeff(sampleRate, 30.f);
    const float alpha = targetGainDb < envDb ? attack : release;
    envDb = alpha * envDb + (1.f - alpha) * targetGainDb;

    return input * (0.90f * dbToGain(envDb) + 0.10f);
}

float VVChainDSP::applyLimiter(float input, float& envDb, double sampleRate)
{
    return applyCompressor(input, envDb, -3.f, 30.f, 100.f, 100.f, sampleRate, 20.f);
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
            {
                y = band.process(y, right);
                y = softColor(y, 0.20f + 0.80f * colorAmount);
            }
            data[n] = y;
        }
    }
}

void VVChainDSP::applyOtt(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
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

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float original = data[n];
            const float x = applyGate(original, gateEnvDb[(size_t)ch], p.ottGateThresholdDb, sr)
                * dbToGain(juce::jlimit(-24.f, 24.f, p.ottInputGainDb));

            const float low = ottXover1.low(x, right);
            const float x1High = ottXover1.high(x, right);
            const float lowMid = ottXover2.low(x1High, right);
            const float x2High = ottXover2.high(x1High, right);
            const float midHigh = ottXover3.low(x2High, right);
            const float top = ottXover3.high(x2High, right);

            float bands[4] = { low, lowMid, midHigh, top };

            for (int band = 0; band < 4; ++band)
            {
                const float degree = juce::jlimit(0.f, 100.f, p.ottDegree[(size_t)band]);
                const float lifterMix = juce::jlimit(0.f, 100.f,
                    p.ottLifterMix[(size_t)band] * degree / 100.f);
                const float compMix = juce::jlimit(0.f, 100.f,
                    p.ottCompMix[(size_t)band] * degree / 100.f);

                float& lifterEnv = ottDynamics[(size_t)band].lifterEnv[(size_t)ch];
                float& compEnv = ottDynamics[(size_t)band].compEnvDb[(size_t)ch];

                bands[band] = applyLifter(
                    bands[band], lifterEnv,
                    p.ottLifterThreshold[(size_t)band],
                    p.ottLifterAttack[(size_t)band],
                    p.ottLifterRelease[(size_t)band],
                    lifterMix, sr);

                bands[band] = applyCompressor(
                    bands[band], compEnv,
                    p.ottCompThreshold[(size_t)band],
                    p.ottCompAttack[(size_t)band],
                    p.ottCompRelease[(size_t)band],
                    compMix, sr, kCompressorRatio);

                bands[band] *= dbToGain(
                    juce::jlimit(-24.f, 12.f, p.ottBandLevelDb[(size_t)band]));
            }

            float y = bands[0] + bands[1] + bands[2] + bands[3];
            y = applyLimiter(y, limiterEnvDb[(size_t)ch], sr);

            if (p.ottClipper)
                y = std::tanh(y * 1.7f);

            const float mix = juce::jlimit(0.f, 1.f, p.ottMix / 100.f);
            data[n] = original + mix * (y - original);
            data[n] *= dbToGain(juce::jlimit(-24.f, 24.f, p.ottOutputGainDb));
        }
    }
}

void VVChainDSP::applyAType(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    // Type-A style four overlapping channels.
    typeXover1.lp1 = makeLowPass(sr, 80.f, 0.707);
    typeXover1.lp2 = makeLowPass(sr, 80.f, 0.707);
    typeXover1.hp1 = makeHighPass(sr, 80.f, 0.707);
    typeXover1.hp2 = makeHighPass(sr, 80.f, 0.707);

    typeXover2.lp1 = makeLowPass(sr, 3000.f, 0.707);
    typeXover2.lp2 = makeLowPass(sr, 3000.f, 0.707);
    typeXover2.hp1 = makeHighPass(sr, 3000.f, 0.707);
    typeXover2.hp2 = makeHighPass(sr, 3000.f, 0.707);
    typeHP9k = makeHighPass(sr, 9000.f, 0.707);

    const float inputGain = dbToGain(juce::jlimit(-24.f, 24.f, p.atypeInputGainDb));
    const float outputGain = dbToGain(juce::jlimit(-24.f, 24.f, p.atypeOutputGainDb));
    const float attack = timeCoeff(sr, p.atypeAttackMs);
    const float release = timeCoeff(sr, p.atypeReleaseMs);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float x = data[n] * inputGain;
            const float b1 = typeXover1.low(x, right);
            const float x1High = typeXover1.high(x, right);
            const float b2 = typeXover2.low(x1High, right);
            const float b3 = typeXover2.high(x1High, right);
            const float b4 = typeHP9k.process(b3, right);
            const float bands[4] = { b1, b2, b3, b4 };

            float enhanced = 0.f;
            for (int band = 0; band < 4; ++band)
            {
                const float degree = juce::jlimit(0.f, 100.f, p.atypeDegree[(size_t)band]);
                const float trim = dbToGain(juce::jlimit(-6.f, 6.f,
                                                          p.atypeBandLevelDb[(size_t)band]));
                float& env = typeEnv[(size_t)band][(size_t)ch];

                env += (std::abs(bands[band]) > env ? (1.f - attack) : (1.f - release))
                       * (std::abs(bands[band]) - env);

                const float levelDb = gainToDb(env);
                float boostDb = 0.f;
                if (levelDb < -40.f)
                    boostDb = juce::jlimit(0.f, 10.f,
                                           (-40.f - levelDb) * 0.5f * degree / 100.f);

                enhanced += bands[band] * (dbToGain(boostDb) * trim - 1.0f);
            }

            const float mix = juce::jlimit(0.f, 1.f, p.atypeMix / 100.f);
            data[n] = x / inputGain + enhanced * mix;
            data[n] *= outputGain;
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

    const int trigger = juce::jmax(1, p.deessTriggerCount);

    if (!p.deessBypass && countMore > trigger)
    {
        fft(deessFft, false);

        const double referenceHz = (p.deessReferenceHz >= 13000.f) ? 13500.0 : 12500.0;
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

    const float mix = juce::jlimit(0.f, 1.f, p.deessMix / 100.f);

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
