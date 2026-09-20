#include "ChainDSP.h"

static float dbToGain(float db) noexcept
{
    return juce::Decibels::decibelsToGain(db);
}

static float gainToDb(float gain) noexcept
{
    return juce::Decibels::gainToDecibels(std::max(gain, 1.0e-9f));
}

VVChainDSP::Biquad VVChainDSP::makePeak(double fs, double f0, double gainDb, double q)
{
    Biquad c;
    const double safeF = juce::jlimit(20.0, fs * 0.45, f0);
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * safeF / fs;
    const double alpha = std::sin(w0) / (2.0 * std::max(0.05, q));
    const double cw = std::cos(w0);

    const double bb0 = 1.0 + alpha * A;
    const double bb1 = -2.0 * cw;
    const double bb2 = 1.0 - alpha * A;
    const double aa0 = 1.0 + alpha / A;
    const double aa1 = -2.0 * cw;
    const double aa2 = 1.0 - alpha / A;

    c.b0 = bb0 / aa0; c.b1 = bb1 / aa0; c.b2 = bb2 / aa0;
    c.a1 = aa1 / aa0; c.a2 = aa2 / aa0;
    return c;
}

VVChainDSP::Biquad VVChainDSP::makeHighPass(double fs, double f0, double q)
{
    Biquad c;
    const double safeF = juce::jlimit(10.0, fs * 0.45, f0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * safeF / fs;
    const double alpha = std::sin(w0) / (2.0 * std::max(0.05, q));
    const double cw = std::cos(w0);

    const double bb0 = (1.0 + cw) * 0.5;
    const double bb1 = -(1.0 + cw);
    const double bb2 = bb0;
    const double aa0 = 1.0 + alpha;
    const double aa1 = -2.0 * cw;
    const double aa2 = 1.0 - alpha;

    c.b0 = bb0 / aa0; c.b1 = bb1 / aa0; c.b2 = bb2 / aa0;
    c.a1 = aa1 / aa0; c.a2 = aa2 / aa0;
    return c;
}

VVChainDSP::Biquad VVChainDSP::makeLowPass(double fs, double f0, double q)
{
    Biquad c;
    const double safeF = juce::jlimit(10.0, fs * 0.45, f0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * safeF / fs;
    const double alpha = std::sin(w0) / (2.0 * std::max(0.05, q));
    const double cw = std::cos(w0);

    const double bb0 = (1.0 - cw) * 0.5;
    const double bb1 = 1.0 - cw;
    const double bb2 = bb0;
    const double aa0 = 1.0 + alpha;
    const double aa1 = -2.0 * cw;
    const double aa2 = 1.0 - alpha;

    c.b0 = bb0 / aa0; c.b1 = bb1 / aa0; c.b2 = bb2 / aa0;
    c.a1 = aa1 / aa0; c.a2 = aa2 / aa0;
    return c;
}

VVChainDSP::Biquad VVChainDSP::makeBandPass(double fs, double f0, double q)
{
    Biquad c;
    const double safeF = juce::jlimit(200.0, fs * 0.40, f0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * safeF / fs;
    const double alpha = std::sin(w0) / (2.0 * std::max(0.05, q));
    const double cw = std::cos(w0);

    const double bb0 = alpha;
    const double bb1 = 0.0;
    const double bb2 = -alpha;
    const double aa0 = 1.0 + alpha;
    const double aa1 = -2.0 * cw;
    const double aa2 = 1.0 - alpha;

    c.b0 = bb0 / aa0; c.b1 = bb1 / aa0; c.b2 = bb2 / aa0;
    c.a1 = aa1 / aa0; c.a2 = aa2 / aa0;
    return c;
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
    for (auto& b : ottFilters) b.reset();
    ottEnvL.fill(0.f);
    ottEnvR.fill(0.f);
    atypeHP.reset();
    atypeTone.reset();
    deessBand.reset();
    deessEnvL = deessEnvR = 0.f;
}

void VVChainDSP::applyEq(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    hp = makeHighPass(sr, juce::jlimit(40.0f, 120.0f, p.hfCornerHz), 0.707);

    for (size_t i = 0; i < eq.size(); ++i)
        eq[i] = makePeak(sr,
            juce::jlimit(20.0f, static_cast<float>(sr * 0.45), p.freq[i]),
            juce::jlimit(-24.0f, 24.0f, p.gain[i]),
            juce::jlimit(0.10f, 18.0f, p.q[i]));

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            float y = hp.process(d[n], ch == 1);
            for (auto& b : eq) y = b.process(y, ch == 1);
            d[n] = y;
        }
    }
}

void VVChainDSP::applyOtt(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    const float depth = juce::jlimit(0.f, 1.f, p.ottDepth / 100.f);
    const float mix = juce::jlimit(0.f, 1.f, p.ottMix / 100.f);
    const float thresholdDb = juce::jlimit(-60.f, 0.f, p.ottThresholdDb);
    const float upRatio = juce::jlimit(1.f, 8.f, p.ottUpRatio);
    const float downRatio = juce::jlimit(1.f, 20.f, p.ottDownRatio);
    const float attack = juce::jlimit(0.0001f, 0.100f, p.ottAttackMs * 0.001f);
    const float release = juce::jlimit(0.005f, 1.0f, p.ottReleaseMs * 0.001f);

    const float aA = std::exp(-1.0f / (static_cast<float>(sr) * attack));
    const float aR = std::exp(-1.0f / (static_cast<float>(sr) * release));

    const float c1 = juce::jlimit(60.f, 900.f, p.ottLowMidHz);
    const float c2 = juce::jlimit(c1 + 80.f, 3500.f, p.ottMidHighHz);
    const float c3 = juce::jlimit(c2 + 150.f, static_cast<float>(sr * 0.42), p.ottHighMidHz);

    ottFilters[0] = makeLowPass(sr, c1, 0.707);
    ottFilters[1] = makeHighPass(sr, c1, 0.707);
    ottFilters[2] = makeLowPass(sr, c2, 0.707);
    ottFilters[3] = makeHighPass(sr, c2, 0.707);
    ottFilters[4] = makeLowPass(sr, c3, 0.707);
    ottFilters[5] = makeHighPass(sr, c3, 0.707);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        auto& env = (ch == 0 ? ottEnvL : ottEnvR);

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float x = d[n];

            const float low = ottFilters[0].process(x, ch == 1);
            const float high1 = ottFilters[1].process(x, ch == 1);
            const float lowMid = ottFilters[2].process(high1, ch == 1);
            const float high2 = ottFilters[3].process(high1, ch == 1);
            const float midHigh = ottFilters[4].process(high2, ch == 1);
            const float top = ottFilters[5].process(high2, ch == 1);

            const float bands[4] = { low, lowMid, midHigh, top };
            float wet = 0.f;

            for (int b = 0; b < 4; ++b)
            {
                const float level = std::abs(bands[b]);
                const float coeff = level > env[(size_t)b] ? (1.f - aA) : (1.f - aR);
                env[(size_t)b] += coeff * (level - env[(size_t)b]);

                const float levelDb = gainToDb(env[(size_t)b]);
                float deltaDb = 0.f;

                if (levelDb > thresholdDb)
                    deltaDb = (thresholdDb - levelDb) * (1.f - 1.f / downRatio);
                else
                    deltaDb = (thresholdDb - levelDb) * (1.f - 1.f / upRatio);

                deltaDb = juce::jlimit(-24.f, 18.f, deltaDb * depth);
                const float bandGain = dbToGain(deltaDb);
                wet += bands[b] * bandGain;
            }

            d[n] = x + mix * (wet - x);
        }
    }

    const float postGain = dbToGain(juce::jlimit(-18.f, 18.f, p.ottPostGainDb));
    buffer.applyGain(postGain);
}

void VVChainDSP::applyAType(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    const float amount = juce::jlimit(0.f, 1.f, p.atypeAmount / 100.f);
    const float drive = dbToGain(juce::jlimit(0.f, 24.f, p.atypeDriveDb));
    const float bias = juce::jlimit(-1.f, 1.f, p.atypeBias / 100.f);
    const float mix = juce::jlimit(0.f, 1.f, p.atypeMix / 100.f);
    const float tone = juce::jlimit(0.f, 1.f, p.atypeTone / 100.f);
    const float hpf = juce::jlimit(20.f, 1000.f, p.atypeHpfHz);
    const float toneHz = juce::jmap(tone, 1200.f, static_cast<float>(juce::jmin(18000.0, sr * 0.4)));

    atypeHP = makeHighPass(sr, hpf, 0.707);
    atypeTone = makeLowPass(sr, toneHz, 0.707);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float x = d[n];
            const float pre = atypeHP.process(x, ch == 1);
            const float driven = pre * drive;
            const float asymmetric = driven + bias * 0.01f;
            const float shaped = std::tanh(asymmetric * (1.0f + amount * 2.8f));
            const float toneShaped = atypeTone.process(shaped, ch == 1);
            const float enhanced = x + amount * 1.25f * toneShaped;

            d[n] = x + mix * (enhanced - x);
        }
    }
}

void VVChainDSP::applyDeEsser(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    const float freq = juce::jlimit(2000.f, 12000.f, p.deessFreq);
    const float q = juce::jlimit(0.20f, 10.f, p.deessQ);
    const float range = juce::jlimit(0.f, 24.f, p.deessRange);
    const float thresholdDb = juce::jlimit(-80.f, 0.f, p.deessThreshold);
    const float attack = juce::jlimit(0.0001f, 0.050f, p.deessAttackMs * 0.001f);
    const float release = juce::jlimit(0.005f, 0.500f, p.deessReleaseMs * 0.001f);
    const float aA = std::exp(-1.0f / (static_cast<float>(sr) * attack));
    const float aR = std::exp(-1.0f / (static_cast<float>(sr) * release));

    deessBand = makeBandPass(sr, freq, q);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        float env = (ch == 0 ? deessEnvL : deessEnvR);

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float x = d[n];
            const float band = deessBand.process(x, ch == 1);
            const float level = std::abs(band);
            const float coeff = level > env ? (1.f - aA) : (1.f - aR);
            env += coeff * (level - env);

            const float levelDb = gainToDb(env);
            const float reductionDb = levelDb > thresholdDb
                ? -juce::jlimit(0.f, range, (levelDb - thresholdDb))
                : 0.f;
            const float reductionGain = dbToGain(reductionDb);

            if (p.deessListen)
                d[n] = band * reductionGain;
            else
                d[n] = x + band * (reductionGain - 1.f);
        }

        if (ch == 0) deessEnvL = env; else deessEnvR = env;
    }
}

void VVChainDSP::process(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0) return;

    const int nCh = std::min(buffer.getNumChannels(), channels);
    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer, true);

    applyEq(buffer, p);
    applyOtt(buffer, p);
    applyAType(buffer, p);
    applyDeEsser(buffer, p);

    const float mix = juce::jlimit(0.f, 1.f, p.dryWet / 100.f);
    const float out = dbToGain(juce::jlimit(-24.f, 12.f, p.outputDb));

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* w = buffer.getWritePointer(ch);
        const auto* d = dry.getReadPointer(ch);

        for (int n = 0; n < buffer.getNumSamples(); ++n)
            w[n] = (d[n] + mix * (w[n] - d[n])) * out;
    }

    for (int ch = nCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());
}
