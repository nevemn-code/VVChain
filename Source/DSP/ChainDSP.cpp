#include "ChainDSP.h"

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

    c.b0 = bb0 / aa0;
    c.b1 = bb1 / aa0;
    c.b2 = bb2 / aa0;
    c.a1 = aa1 / aa0;
    c.a2 = aa2 / aa0;
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

    c.b0 = bb0 / aa0;
    c.b1 = bb1 / aa0;
    c.b2 = bb2 / aa0;
    c.a1 = aa1 / aa0;
    c.a2 = aa2 / aa0;
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

    c.b0 = bb0 / aa0;
    c.b1 = bb1 / aa0;
    c.b2 = bb2 / aa0;
    c.a1 = aa1 / aa0;
    c.a2 = aa2 / aa0;
    return c;
}

float VVChainDSP::dbToGain(float db) noexcept
{
    return juce::Decibels::decibelsToGain(db);
}

float VVChainDSP::gainToDb(float gain) noexcept
{
    return juce::Decibels::gainToDecibels(std::max(gain, 1.0e-9f));
}

float VVChainDSP::analogColor(float x, float amount01) noexcept
{
    const float a = juce::jlimit(0.f, 1.f, amount01);
    const float drive = 1.0f + 0.95f * a;
    const float asymmetric = x + 0.018f * a * x * x;
    const float shaped = std::tanh(asymmetric * drive);
    const float ref = std::tanh(drive);
    return ref > 0.f ? shaped / ref : x;
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

    ottLP1.reset();
    ottHP1.reset();
    ottLP2.reset();
    ottHP2.reset();
    ottLP3.reset();
    ottHP3.reset();
    ottEnvL.fill(0.f);
    ottEnvR.fill(0.f);

    typeLP80.reset();
    typeHP80.reset();
    typeLP3k.reset();
    typeHP3k.reset();
    typeHP9k.reset();
    typeEnvL.fill(0.f);
    typeEnvR.fill(0.f);

    deessHP.reset();
    deessLP.reset();
    deessEnvL = deessEnvR = 0.f;
}

void VVChainDSP::applyEq(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    hp = makeHighPass(sr, juce::jlimit(40.0f, 120.0f, p.hfCornerHz), 0.707);

    for (size_t i = 0; i < eq.size(); ++i)
        eq[i] = makePeak(sr, p.freq[i], p.gain[i], p.q[i]);

    const float color = juce::jlimit(0.f, 100.f, p.eqColor) / 100.f;

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            float y = hp.process(d[n], right);

            for (size_t band = 0; band < eq.size(); ++band)
            {
                y = eq[band].process(y, right);
                y = analogColor(y, 0.16f + 0.84f * color);
            }

            d[n] = y;
        }
    }
}

void VVChainDSP::applyOtt(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    const float x1 = juce::jlimit(60.f, 900.f, p.ottX1);
    const float x2 = juce::jlimit(x1 + 80.f, 5000.f, p.ottX2);
    const float x3 = juce::jlimit(x2 + 150.f, static_cast<float>(sr * 0.42), p.ottX3);

    ottLP1 = makeLowPass(sr, x1, 0.707);
    ottHP1 = makeHighPass(sr, x1, 0.707);
    ottLP2 = makeLowPass(sr, x2, 0.707);
    ottHP2 = makeHighPass(sr, x2, 0.707);
    ottLP3 = makeLowPass(sr, x3, 0.707);
    ottHP3 = makeHighPass(sr, x3, 0.707);

    const float threshold = juce::jlimit(-60.f, 0.f, p.ottThreshold);
    const float upRatio = juce::jlimit(1.f, 8.f, p.ottUpRatio);
    const float downRatio = juce::jlimit(1.f, 80.f, p.ottDownRatio);
    const float attack = juce::jlimit(0.0002f, 0.100f, p.ottAttackMs / 1000.f);
    const float release = juce::jlimit(0.005f, 1.000f, p.ottReleaseMs / 1000.f);
    const float aA = std::exp(-1.f / (static_cast<float>(sr) * attack));
    const float aR = std::exp(-1.f / (static_cast<float>(sr) * release));
    const float inputGain = dbToGain(juce::jlimit(-12.f, 12.f, p.ottInputGainDb));
    const float mix = juce::jlimit(0.f, 1.f, p.ottMix / 100.f);
    const float postGain = dbToGain(juce::jlimit(-18.f, 18.f, p.ottPostGainDb));

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        auto& env = (ch == 0 ? ottEnvL : ottEnvR);
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float input = d[n];
            const float driven = input * inputGain;

            const float low = ottLP1.process(driven, right);
            const float high1 = ottHP1.process(driven, right);
            const float lowMid = ottLP2.process(high1, right);
            const float high2 = ottHP2.process(high1, right);
            const float midHigh = ottLP3.process(high2, right);
            const float top = ottHP3.process(high2, right);
            const float bands[4] = { low, lowMid, midHigh, top };

            float wet = 0.f;

            for (int band = 0; band < 4; ++band)
            {
                const float amount = juce::jlimit(0.f, 1.f, p.ottAmount[(size_t)band] / 100.f);
                const float level = std::abs(bands[band]);
                const float coeff = level > env[(size_t)band] ? (1.f - aA) : (1.f - aR);
                env[(size_t)band] += coeff * (level - env[(size_t)band]);

                const float levelDb = gainToDb(env[(size_t)band]);
                float deltaDown = 0.f;
                float deltaUp = 0.f;

                if (levelDb > threshold)
                    deltaDown = (threshold - levelDb) * (1.f - 1.f / downRatio);

                const float afterDownDb = levelDb + deltaDown;

                if (afterDownDb < threshold)
                    deltaUp = (threshold - afterDownDb) * (1.f - 1.f / upRatio);

                const float totalDeltaDb = juce::jlimit(-36.f, 24.f,
                    (deltaDown + deltaUp) * amount);

                wet += bands[band] * dbToGain(totalDeltaDb);
            }

            d[n] = input + mix * ((wet * postGain) - input);
        }
    }
}

void VVChainDSP::applyAType(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    typeLP80 = makeLowPass(sr, 80.f, 0.707);
    typeHP80 = makeHighPass(sr, 80.f, 0.707);
    typeLP3k = makeLowPass(sr, 3000.f, 0.707);
    typeHP3k = makeHighPass(sr, 3000.f, 0.707);
    typeHP9k = makeHighPass(sr, 9000.f, 0.707);

    const float attack = juce::jlimit(0.0005f, 0.250f, p.atypeAttackMs / 1000.f);
    const float release = juce::jlimit(0.005f, 1.000f, p.atypeReleaseMs / 1000.f);
    const float aA = std::exp(-1.f / (static_cast<float>(sr) * attack));
    const float aR = std::exp(-1.f / (static_cast<float>(sr) * release));
    const float mix = juce::jlimit(0.f, 1.f, p.atypeMix / 100.f);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        auto& env = (ch == 0 ? typeEnvL : typeEnvR);
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float x = d[n];

            const float b1 = typeLP80.process(x, right);
            const float hp80 = typeHP80.process(x, right);
            const float b2 = typeLP3k.process(hp80, right);
            const float b3 = typeHP3k.process(hp80, right);
            const float b4 = typeHP9k.process(b3, right);
            const float bands[4] = { b1, b2, b3, b4 };

            float enhanced = 0.f;

            for (int band = 0; band < 4; ++band)
            {
                const float amount = juce::jlimit(0.f, 1.f, p.atypeAmount[(size_t)band] / 100.f);
                const float level = std::abs(bands[band]);
                const float coeff = level > env[(size_t)band] ? (1.f - aA) : (1.f - aR);
                env[(size_t)band] += coeff * (level - env[(size_t)band]);

                const float levelDb = gainToDb(env[(size_t)band]);
                const float thresholdDb = -30.f;
                float boostDb = 0.f;

                if (levelDb < thresholdDb)
                    boostDb = juce::jlimit(0.f, 18.f,
                        (thresholdDb - levelDb) * (1.f - 1.f / 4.f) * amount);

                const float gainDb = boostDb + p.atypeGainDb[(size_t)band];
                enhanced += bands[band] * dbToGain(gainDb);
            }

            d[n] = x + mix * enhanced;
        }
    }
}

void VVChainDSP::applyDeEsser(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    float low = juce::jlimit(2000.f, 12000.f, p.deessLowHz);
    float high = juce::jlimit(low + 100.f, 18000.f, p.deessHighHz);
    high = std::min(high, static_cast<float>(sr * 0.42));

    deessHP = makeHighPass(sr, low, 0.707);
    deessLP = makeLowPass(sr, high, 0.707);

    const float thresholdDb = -36.f;
    const float range = juce::jlimit(0.f, 24.f, p.deessRangeDb);
    const float strength = juce::jlimit(0.f, 1.f, p.deessStrength / 100.f);
    const float attack = juce::jlimit(0.0002f, 0.050f, p.deessAttackMs / 1000.f);
    const float release = juce::jlimit(0.005f, 0.500f, p.deessReleaseMs / 1000.f);
    const float aA = std::exp(-1.f / (static_cast<float>(sr) * attack));
    const float aR = std::exp(-1.f / (static_cast<float>(sr) * release));

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        float& env = ch == 0 ? deessEnvL : deessEnvR;
        const bool right = ch == 1;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float x = d[n];
            const float band = deessLP.process(deessHP.process(x, right), right);
            const float level = std::abs(band);
            const float coeff = level > env ? (1.f - aA) : (1.f - aR);
            env += coeff * (level - env);

            const float levelDb = gainToDb(env);
            const float reductionDb = levelDb > thresholdDb
                ? -juce::jlimit(0.f, range, (levelDb - thresholdDb) * strength)
                : 0.f;
            const float reductionGain = dbToGain(reductionDb);

            if (p.deessListen)
                d[n] = band * reductionGain;
            else
                d[n] = x + band * (reductionGain - 1.f);
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
