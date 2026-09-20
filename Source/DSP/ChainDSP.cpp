#include "ChainDSP.h"

static float dbToGain(float db) noexcept
{
    return juce::Decibels::decibelsToGain(db);
}

VVChainDSP::Biquad VVChainDSP::makePeak(double fs, double f0, double gainDb, double q)
{
    Biquad c;
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
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
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
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
    const float attack = 0.0025f, release = 0.080f;
    const float aA = std::exp(-1.0f / static_cast<float>(sr) / attack);
    const float aR = std::exp(-1.0f / static_cast<float>(sr) / release);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        float env = 0.f;
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float x = std::abs(d[n]);
            const float coeff = x > env ? (1.f - aA) : (1.f - aR);
            env += coeff * (x - env);
            const float threshold = 0.16f;
            const float down = juce::jlimit(0.f, 1.f, (env - threshold) * 3.5f);
            const float up = juce::jlimit(0.f, 1.f, (threshold - env) * 2.0f);
            const float downGain = std::pow(0.55f, down * depth);
            const float upGain = 1.0f + 1.4f * up * depth;
            const float wet = d[n] * downGain * upGain;
            d[n] = d[n] + mix * (wet - d[n]);
        }
    }
}

void VVChainDSP::applyAType(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    const float amount = juce::jlimit(0.f, 1.f, p.atypeAmount / 100.f);
    const float bias = juce::jlimit(-1.f, 1.f, p.atypeBias / 100.f);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float x = d[n] + bias * 0.003f;
            const float sat = std::tanh(x * (1.0f + amount * 2.2f));
            d[n] = x + amount * 0.35f * (sat - x);
        }
    }
}

void VVChainDSP::applyDeEsser(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    const float freq = juce::jlimit(2000.f, 12000.f, p.deessFreq);
    const float range = juce::jlimit(0.f, 24.f, p.deessRange);
    const float threshold = dbToGain(juce::jlimit(-80.f, 0.f, p.deessThreshold));
    const float alpha = std::exp(-2.0f * juce::MathConstants<float>::pi * freq / static_cast<float>(sr));
    const float gr = dbToGain(-range);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        float env = ch == 0 ? deessEnvL : deessEnvR;
        float hpv = 0.f;
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float x = d[n];
            hpv = alpha * hpv + (1.f - alpha) * x;
            env += 0.02f * (std::abs(hpv) - env);
            const float reduction = env > threshold ? gr : 1.f;
            d[n] = x * reduction + 0.35f * hpv * (1.f - reduction);
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
