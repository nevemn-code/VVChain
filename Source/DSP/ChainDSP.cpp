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

void VVChainDSP::updateDynamicPeak(TPTBell& filter, double fs, double f0,
                                      double gainDb, double q)
{
    // Cytomic / Simper TPT Bell EQ. Gain-dependent damping is intrinsic:
    // k = 1 / (Q * A), A = 10^(gain / 40).
    // Coefficients and state are double precision; audio I/O remains float.
    const double safeF = juce::jlimit(20.0, fs * 0.45, f0);
    const double safeQ = juce::jlimit(0.1, 18.0, q);
    const double safeGain = juce::jlimit(-18.0, 18.0, gainDb);
    const double A = std::pow(10.0, safeGain / 40.0);
    const double g = std::tan(juce::MathConstants<double>::pi * safeF / fs);
    const double k = 1.0 / (safeQ * A);

    filter.g = g;
    filter.k = k;
    filter.a1 = 1.0 / (1.0 + g * (g + k));
    filter.a2 = g * filter.a1;
    filter.a3 = g * filter.a2;
    filter.m1 = k * (A * A - 1.0);
}

void VVChainDSP::updateEqFilter(EqFilter& filter, int type,
                                double fs, double f0,
                                double gainDb, double q,
                                int slopeIndex)
{
    type = juce::jlimit(0, 13, type);
    if (type == 3)
        type = 2;
    else if (type == 1 || type == 10 || type == 11)
        type = 0;
    filter.beginType(type);

    const double safeF = juce::jlimit(20.0, fs * 0.45, f0);
    const double safeQ = juce::jlimit(0.10, 18.0, q);
    const double safeGain = juce::jlimit(-18.0, 18.0, gainDb);
    const double linearGain = std::pow(10.0, safeGain / 20.0);

    filter.useTptPeak = false;
    filter.parallelBandShelf = false;
    filter.parallelMix = 0.0;
    filter.outputGain = 1.0;
    filter.stageCount = 1;

    auto identity = [](Biquad& b)
    {
        b.updateCoefficients(1.0, 0.0, 0.0, 0.0, 0.0);
    };

    auto rbjPeak = [fs](Biquad& b, double f, double gain, double qv)
    {
        const double sf = juce::jlimit(20.0, fs * 0.45, f);
        const double qq = juce::jlimit(0.10, 18.0, qv);
        const double A = std::pow(10.0, gain / 40.0);
        const double w = juce::MathConstants<double>::twoPi * sf / fs;
        const double sn = std::sin(w);
        const double cs = std::cos(w);
        const double alpha = sn / (2.0 * qq);
        const double a0 = 1.0 + alpha / A;
        b.updateCoefficients(
            (1.0 + alpha * A) / a0,
            (-2.0 * cs) / a0,
            (1.0 - alpha * A) / a0,
            (-2.0 * cs) / a0,
            (1.0 - alpha / A) / a0);
    };

    auto rbjShelf = [fs](Biquad& b, bool high, double f,
                         double gain, double slope)
    {
        const double sf = juce::jlimit(20.0, fs * 0.45, f);
        const double A = std::pow(10.0, gain / 40.0);
        const double w = juce::MathConstants<double>::twoPi * sf / fs;
        const double sn = std::sin(w);
        const double cs = std::cos(w);
        const double S = juce::jlimit(0.10, 1.0, slope);
        const double rootA = std::sqrt(A);
        const double term =
            juce::jmax(0.0, (A + 1.0 / A) * (1.0 / S - 1.0) + 2.0);
        const double alpha = 0.5 * sn * std::sqrt(term);
        const double beta = 2.0 * rootA * alpha;

        double b0, b1, b2, a0, a1, a2;
        if (!high)
        {
            b0 = A * ((A + 1.0) - (A - 1.0) * cs + beta);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cs);
            b2 = A * ((A + 1.0) - (A - 1.0) * cs - beta);
            a0 = (A + 1.0) + (A - 1.0) * cs + beta;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cs);
            a2 = (A + 1.0) + (A - 1.0) * cs - beta;
        }
        else
        {
            b0 = A * ((A + 1.0) + (A - 1.0) * cs + beta);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cs);
            b2 = A * ((A + 1.0) + (A - 1.0) * cs - beta);
            a0 = (A + 1.0) - (A - 1.0) * cs + beta;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cs);
            a2 = (A + 1.0) - (A - 1.0) * cs - beta;
        }

        const double inv = 1.0 / juce::jmax(1.0e-12, a0);
        b.updateCoefficients(
            b0 * inv, b1 * inv, b2 * inv,
            a1 * inv, a2 * inv);
    };

    auto bandPass = [fs](Biquad& b, double f, double qv)
    {
        const double sf = juce::jlimit(20.0, fs * 0.45, f);
        const double qq = juce::jlimit(0.10, 18.0, qv);
        const double w = juce::MathConstants<double>::twoPi * sf / fs;
        const double sn = std::sin(w);
        const double cs = std::cos(w);
        const double alpha = sn / (2.0 * qq);
        const double a0 = 1.0 + alpha;
        b.updateCoefficients(
            alpha / a0, 0.0, -alpha / a0,
            (-2.0 * cs) / a0, (1.0 - alpha) / a0);
    };

    auto notch = [fs](Biquad& b, double f, double qv)
    {
        const double sf = juce::jlimit(20.0, fs * 0.45, f);
        const double qq = juce::jlimit(0.10, 18.0, qv);
        const double w = juce::MathConstants<double>::twoPi * sf / fs;
        const double sn = std::sin(w);
        const double cs = std::cos(w);
        const double alpha = sn / (2.0 * qq);
        const double a0 = 1.0 + alpha;
        b.updateCoefficients(
            1.0 / a0, (-2.0 * cs) / a0, 1.0 / a0,
            (-2.0 * cs) / a0, (1.0 - alpha) / a0);
    };

    auto lowPass = [fs](Biquad& b, double f, double qv)
    {
        const double sf = juce::jlimit(20.0, fs * 0.45, f);
        const double qq = juce::jlimit(0.10, 18.0, qv);
        const double K = std::tan(juce::MathConstants<double>::pi * sf / fs);
        const double K2 = K * K;
        const double a0 = 1.0 + K / qq + K2;
        b.updateCoefficients(
            K2 / a0, 2.0 * K2 / a0, K2 / a0,
            2.0 * (K2 - 1.0) / a0,
            (1.0 - K / qq + K2) / a0);
    };

    auto highPass = [fs](Biquad& b, double f, double qv)
    {
        const double sf = juce::jlimit(20.0, fs * 0.45, f);
        const double qq = juce::jlimit(0.10, 18.0, qv);
        const double K = std::tan(juce::MathConstants<double>::pi * sf / fs);
        const double K2 = K * K;
        const double a0 = 1.0 + K / qq + K2;
        b.updateCoefficients(
            1.0 / a0, -2.0 / a0, 1.0 / a0,
            2.0 * (K2 - 1.0) / a0,
            (1.0 - K / qq + K2) / a0);
    };

    auto firstOrderLowPass = [fs](Biquad& b, double f)
    {
        const double sf = juce::jlimit(20.0, fs * 0.45, f);
        const double K = std::tan(
            juce::MathConstants<double>::pi * sf / fs);
        const double inv = 1.0 / (1.0 + K);
        b.updateCoefficients(
            K * inv, K * inv, 0.0,
            (K - 1.0) * inv, 0.0);
    };

    auto firstOrderHighPass = [fs](Biquad& b, double f)
    {
        const double sf = juce::jlimit(20.0, fs * 0.45, f);
        const double K = std::tan(
            juce::MathConstants<double>::pi * sf / fs);
        const double inv = 1.0 / (1.0 + K);
        b.updateCoefficients(
            inv, -inv, 0.0,
            (K - 1.0) * inv, 0.0);
    };

    // Peak keeps the current TPT/Simper path exactly.
    if (type == 0)
    {
        filter.useTptPeak = true;
        filter.stageCount = 0;
        updateDynamicPeak(filter.peak, fs, safeF, safeGain, safeQ);
        return;
    }

    if (type == 1) // Matched Bell
    {
        updateAnalogPeak(filter.stages[0], fs, safeF, safeGain, safeQ);
        return;
    }

    if (type == 2 || type == 3) // Wide / Steep Plateau
    {
        const double bandwidthOct =
            juce::jlimit(0.20, 4.0, 1.40 / std::sqrt(safeQ));
        const double ratio = std::pow(2.0, bandwidthOct * 0.5);
        const double lo = juce::jlimit(20.0, fs * 0.44, safeF / ratio);
        const double hi = juce::jlimit(lo * 1.02, fs * 0.45, safeF * ratio);
        filter.parallelBandShelf = true;
        filter.parallelMix = linearGain - 1.0;

        if (type == 2)
        {
            filter.stageCount = 2;
            const double edgeQ = juce::jlimit(0.45, 2.5, safeQ);
            highPass(filter.stages[0], lo, edgeQ);
            lowPass(filter.stages[1], hi, edgeQ);
        }
        else
        {
            // 12th-order HP + 12th-order LP = 72 dB/oct on both sides.
            constexpr double butterQ[6] =
            {
                0.5043144803, 0.5411961001, 0.6302362070,
                0.8213398159, 1.3065629649, 3.8306487878
            };
            filter.stageCount = 12;
            const double resonanceScale =
                juce::jlimit(0.35, 2.5, safeQ / 0.7071067811865476);
            for (int i = 0; i < 6; ++i)
            {
                const double rq = juce::jlimit(
                    0.25, 12.0, butterQ[i] * resonanceScale);
                highPass(filter.stages[(size_t)i], lo, rq);
                lowPass(filter.stages[(size_t)(6 + i)], hi, rq);
            }
        }
        return;
    }

    if (type == 4 || type == 5) // Low / High shelf
    {
        rbjShelf(filter.stages[0], type == 5, safeF, safeGain, 1.0);
        return;
    }

    if (type == 6 || type == 7) // Resonant shelf
    {
        filter.stageCount = 2;
        rbjShelf(filter.stages[0], type == 7, safeF, safeGain, 1.0);
        const double resonanceGain =
            (safeGain >= 0.0 ? 1.0 : -1.0)
            * juce::jmin(6.0, std::abs(safeGain) * 0.35);
        rbjPeak(filter.stages[1], safeF, resonanceGain, safeQ);
        return;
    }

    if (type == 8 || type == 9) // Gentle low/high contour
    {
        rbjShelf(filter.stages[0], type == 9, safeF, safeGain, 0.28);
        return;
    }

    if (type == 10) // Focus Pass
    {
        bandPass(filter.stages[0], safeF, safeQ);
        filter.outputGain = linearGain;
        return;
    }

    if (type == 11) // Deep Reject
    {
        notch(filter.stages[0], safeF, safeQ);
        return;
    }

    // 6 dB/oct is a genuine first-order IIR.
    // 12..72 dB/oct use 1..6 second-order Butterworth sections.
    slopeIndex = juce::jlimit(0, 6, slopeIndex);

    if (slopeIndex == 0)
    {
        filter.stageCount = 1;
        if (type == 12)
            firstOrderLowPass(filter.stages[0], safeF);
        else
            firstOrderHighPass(filter.stages[0], safeF);
    }
    else
    {
        filter.stageCount = juce::jlimit(1, 6, slopeIndex);
        const int order = filter.stageCount * 2;

        for (int i = 0; i < filter.stageCount; ++i)
        {
            const double angle =
                (2.0 * static_cast<double>(i) + 1.0)
                * juce::MathConstants<double>::pi
                / (2.0 * static_cast<double>(order));
            const double rq =
                1.0 / (2.0 * std::cos(angle));

            if (type == 12)
                lowPass(filter.stages[(size_t)i], safeF, rq);
            else
                highPass(filter.stages[(size_t)i], safeF, rq);
        }
    }

    // Clear unused coefficients defensively so old states cannot leak if the
    // stage count later increases after an automation jump.
    for (int i = filter.stageCount; i < 12; ++i)
    {
        filter.stages[(size_t)i].reset();
        identity(filter.stages[(size_t)i]);
    }
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

void VVChainDSP::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    sr = std::max(8000.0, sampleRate);
    channels = juce::jlimit(1, 2, numChannels);

    const int maxBlock = juce::jmax(1, samplesPerBlock);
    dryBuffer.setSize(channels, maxBlock, false, true, true);
    alignedDryBuffer.setSize(channels, maxBlock, false, true, true);
    dynamicDetectorInput.setSize(channels, maxBlock * 4, false, true, true);

    for (auto& stream : contributionStreams)
        stream.setSize(1, maxBlock, false, true, true);
    contributionPreAnalogBase.setSize(
        channels, maxBlock, false, true, true);

    eqOversampler.reset();
    contributionEqDownsampler.reset();
    limiterOversampler.reset();
    eqOversampler.initProcessing(static_cast<size_t>(maxBlock));
    contributionEqDownsampler.initProcessing(static_cast<size_t>(maxBlock));
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
    graphSoloPre.reset();
    graphSoloPost.reset();
    soloBlend = 0.f;
    lastSoloBand = -2;
    lastSoloPost = false;

    udmbcXover1.reset();
    udmbcXover2.reset();
    udmbcXover3.reset();
    udmbcPhase2_B1.reset();
    udmbcPhase3_B1.reset();
    udmbcPhase3_B2.reset();

    for (auto& b : udmbcDynamics)
    {
        b.gateEnvDb = { 0.f, 0.f };
        b.lifterEnv = { 1.f, 1.f };
        b.compEnvDb = { 0.f, 0.f };
        b.upRmsPower = { 0.f, 0.f };
        b.upSlowRmsPower = { 0.f, 0.f };
        b.downRmsPower = { 0.f, 0.f };
        b.downSlowRmsPower = { 0.f, 0.f };
    }

    for (auto& bandState : analogADAA)
        for (auto& channelState : bandState)
            channelState.resetState();
    analogAlpha = { 0.0, 0.0, 0.0, 0.0 };
    analogAlphaInitialized = { false, false, false, false };

    bandProcessingHighPass.reset();

    analogXover1.reset();
    analogXover2.reset();
    analogXover3.reset();

    typeXover1.reset();
    typeXover2.reset();
    typeXover3.reset();

    deessSplit.reset();

    eqOversampler.reset();
    contributionEqDownsampler.reset();
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
        state.broadbandEnv = 0.f;
        state.hfFastEnv = 0.f;
        state.hfSlowEnv = 0.f;
        state.gainDb = 0.f;
    }
    deessLinkedGainDb = 0.f;

    gateEnvDb = { 0.f, 0.f };
    limiterEnvDb = { 0.f, 0.f };
    dryBuffer.clear();
    alignedDryBuffer.clear();
    dynamicDetectorInput.clear();
    contributionPreAnalogBase.clear();
    for (auto& stream : contributionStreams)
        stream.clear();
    contributionPreAnalogReady = false;
}

void VVChainDSP::captureContributionMono(
    int stream,
    const juce::AudioBuffer<float>& source,
    int numSamples) noexcept
{
    if (!contributionAnalysisEnabled
        || stream < 0 || stream >= 6
        || numSamples <= 0)
        return;

    auto& destination = contributionStreams[(size_t)stream];
    const int count = juce::jmin(numSamples, destination.getNumSamples());
    const int nCh = juce::jmax(1, source.getNumChannels());
    auto* out = destination.getWritePointer(0);

    for (int n = 0; n < count; ++n)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            mono += source.getReadPointer(ch)[n];
        out[n] = mono / static_cast<float>(nCh);
    }
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
    // threshold, which could make UDMBC jump by many dB on quiet material.
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

                const float onsetMix =
                    juce::jlimit(0.f, 1.f, p.dynDetectOnsets[band] / 100.f);
                if (onsetMix > 0.f)
                {
                    const float midRise =
                        juce::jmax(0.f, midDb - midSlow);
                    const float sideRise =
                        juce::jmax(0.f, sideDb - sideSlow);

                    midTriggerDb += onsetMix
                        * juce::jlimit(0.f, 12.f, midRise * 1.5f);
                    sideTriggerDb += onsetMix
                        * juce::jlimit(0.f, 12.f, sideRise * 1.5f);
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
                // The TPT Bell handles gain-dependent pole damping via A.
                const double midQ = baseQ;
                const double sideQ = baseQ;

                // Update every sample. The detector gain movement is already
                // attack/release smoothed, eliminating the old 4-sample zipper.
                int eqType = juce::jlimit(0, 13, p.eqType[band]);
                if (eqType == 3)
                    eqType = 2;
                else if (eqType == 1 || eqType == 10 || eqType == 11)
                    eqType = 0;
                if ((band == 1 || band == 2) && eqType >= 12)
                    eqType = 0;

                const int slopeIndex =
                    juce::jlimit(0, 6, p.eqSlope[band]);

                updateEqFilter(
                    dynMidEq[band], eqType,
                    osSr, frequency, safeMidTotalGain, midQ,
                    slopeIndex);
                updateEqFilter(
                    dynSideEq[band], eqType,
                    osSr, frequency, safeSideTotalGain, sideQ,
                    slopeIndex);

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
                        dynMidEq[band].process(currentMid);
                    currentSide =
                        dynSideEq[band].process(currentSide);

                    left[sample] =
                        (currentMid + currentSide) * invSqrt2;
                    right[sample] =
                        (currentMid - currentSide) * invSqrt2;
                }
                else
                {
                    left[sample] =
                        dynMidEq[band].process(leftIn);
                }
            }

            // Analog Color is routed after EQ/Dynamics gating below so EQ_BYPASS
            // does not silently bypass the independently selectable ANALOG module.
        }
    }

    contributionPreAnalogReady = false;

    // Shared BAND 1 floor: one zero-sample-latency 30 Hz / 12 dB/oct
    // Butterworth HPF for ANALOG -> UDMBC -> TAPE. EQ/Dynamics remain upstream.
    // Keeping this filter singular prevents the three processors from accumulating
    // different low-frequency phase rotations. Master BYPASS still crossfades to
    // the latency-aligned dry path, and this IIR adds no samples to reported PDC.
    constexpr double kBandProcessingLowCutHz = 30.0;
    constexpr double kButterworthQ = 0.7071067811865476;
    updateHighPass(
        bandProcessingHighPass,
        osSr,
        kBandProcessingLowCutHz,
        kButterworthQ);

    for (int ch = 0; ch < osChannels && ch < 2; ++ch)
    {
        auto* data =
            osBlock.getChannelPointer(static_cast<size_t>(ch));
        const bool right = ch == 1;

        for (int sample = 0; sample < osSamples; ++sample)
            data[sample] =
                bandProcessingHighPass.process(data[sample], right);
    }

    // Analyzer-only exact pre-Analog base-rate tap. The signal already passed
    // the audible EQ oversampler's up path, so only an identical down path is
    // required here. This block is never mixed back into the audio output.
    bool analogContributionActive = false;
    if (contributionAnalysisEnabled && !p.eqColorGlobalBypass)
    {
        for (size_t band = 0; band < 4; ++band)
        {
            if (!p.eqColorBypass[band]
                && p.eqColor[band] > 0.0001f)
            {
                analogContributionActive = true;
                break;
            }
        }
    }

    if (analogContributionActive)
    {
        const int baseSamples = buffer.getNumSamples();

        for (int ch = 0; ch < contributionPreAnalogBase.getNumChannels(); ++ch)
            contributionPreAnalogBase.clear(ch, 0, baseSamples);

        juce::dsp::AudioBlock<const float> analysisBaseInput(
            contributionPreAnalogBase);
        auto analysisBaseSub =
            analysisBaseInput.getSubBlock(0, static_cast<size_t>(baseSamples));
        auto analysisOsBlock =
            contributionEqDownsampler.processSamplesUp(analysisBaseSub);

        const int copyChannels = juce::jmin(
            osChannels, static_cast<int>(analysisOsBlock.getNumChannels()));
        const int copySamples = juce::jmin(
            osSamples, static_cast<int>(analysisOsBlock.getNumSamples()));

        for (int ch = 0; ch < copyChannels; ++ch)
            std::copy_n(
                osBlock.getChannelPointer(static_cast<size_t>(ch)),
                copySamples,
                analysisOsBlock.getChannelPointer(static_cast<size_t>(ch)));

        juce::dsp::AudioBlock<float> analysisBaseOutput(
            contributionPreAnalogBase);
        auto analysisBaseOutSub =
            analysisBaseOutput.getSubBlock(
                0, static_cast<size_t>(baseSamples));

        contributionEqDownsampler.processSamplesDown(
            analysisBaseOutSub);

        contributionPreAnalogReady = true;
    }

    // ANALOG COLOR v1.0.46: true four-band routing.
    // Shared X1/X2/X3 positions define four crossover bands. Each band has
    // independent COLOR/TT/SS/ADAA state, then all four bands are rebuilt.
    const float analogX1 = juce::jlimit(80.f, 900.f, p.udmbcX1);
    const float analogX2 =
        juce::jlimit(analogX1 + 80.f, 5000.f, p.udmbcX2);
    const float analogX3 =
        juce::jlimit(analogX2 + 200.f,
                     static_cast<float>(sr * 0.42),
                     p.udmbcX3);
    const float analogCrossoverQ =
        crossoverQFromOverlap(p.udmbcXoverOverlap);

    updateCrossover(analogXover1, osSr, analogX1, analogCrossoverQ);
    updateCrossover(analogXover2, osSr, analogX2, analogCrossoverQ);
    updateCrossover(analogXover3, osSr, analogX3, analogCrossoverQ);

    std::array<double, 4> analogTargetAlpha {};
    for (size_t band = 0; band < 4; ++band)
    {
        const bool bypass =
            p.eqColorGlobalBypass || p.eqColorBypass[band];
        const float amount = bypass
            ? 0.0f
            : juce::jlimit(0.0f, 60.0f, p.eqColor[band]) / 100.0f;
        const double modeAlpha =
            p.eqColorSolidState[band] ? 1.80 : 1.55;

        analogTargetAlpha[band] =
            static_cast<double>(amount) * modeAlpha;

        const auto mode = p.eqColorSolidState[band]
            ? VVChain_AnalogADAA_v2::Mode::SS
            : VVChain_AnalogADAA_v2::Mode::TT;

        for (size_t ch = 0;
             ch < static_cast<size_t>(osChannels) && ch < 2;
             ++ch)
        {
            analogADAA[band][ch].setMode(mode);
        }

        if (amount <= 0.000001f)
        {
            analogAlpha[band] = 0.0;
            analogAlphaInitialized[band] = true;
            for (size_t ch = 0;
                 ch < static_cast<size_t>(osChannels) && ch < 2;
                 ++ch)
                analogADAA[band][ch].resetState();
        }
        else if (!analogAlphaInitialized[band])
        {
            analogAlpha[band] = analogTargetAlpha[band];
            analogAlphaInitialized[band] = true;
        }
    }

    constexpr double kAnalogSmoothingMs = 0.25;
    const double analogSmoothingCoeff =
        std::exp(-1.0 / (0.001 * kAnalogSmoothingMs * osSr));

    for (int sample = 0; sample < osSamples; ++sample)
    {
        for (size_t band = 0; band < 4; ++band)
        {
            if (analogTargetAlpha[band] > 0.0)
            {
                analogAlpha[band] +=
                    (analogTargetAlpha[band] - analogAlpha[band])
                    * (1.0 - analogSmoothingCoeff);
            }
            else
                analogAlpha[band] = 0.0;
        }

        for (int ch = 0; ch < osChannels && ch < 2; ++ch)
        {
            auto* data =
                osBlock.getChannelPointer(static_cast<size_t>(ch));
            const bool right = ch == 1;
            const float input = data[sample];

            const float low = analogXover1.low(input, right);
            const float x1High = analogXover1.high(input, right);
            const float lowMid = analogXover2.low(x1High, right);
            const float x2High = analogXover2.high(x1High, right);
            const float midHigh = analogXover3.low(x2High, right);
            const float high = analogXover3.high(x2High, right);
            const float bands[4] = { low, lowMid, midHigh, high };

            float reconstructed = 0.0f;
            for (size_t band = 0; band < 4; ++band)
            {
                const bool bypass =
                    p.eqColorGlobalBypass || p.eqColorBypass[band];

                if (bypass || analogAlpha[band] <= 0.000001)
                {
                    reconstructed += bands[band];
                    continue;
                }

                const double shapingInput =
                    juce::jlimit(
                        -1.0, 1.0,
                        static_cast<double>(bands[band]));
                const double saturated =
                    analogADAA[band][static_cast<size_t>(ch)]
                        .processSample(
                            shapingInput,
                            analogAlpha[band]);
                const double delta = saturated - shapingInput;
                const double x2 =
                    p.eqColorX2[band] ? 2.0 : 1.0;
                const double bandOutput =
                    static_cast<double>(bands[band]) + delta * x2;

                reconstructed += static_cast<float>(
                    std::isfinite(bandOutput)
                        ? bandOutput
                        : static_cast<double>(bands[band]));
            }

            data[sample] = reconstructed;
        }
    }

    eqOversampler.processSamplesDown(outputBlock);
}
void VVChainDSP::applyOtt(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    // Four independent UDMBC bands. Each band has its own detector state and
    // runs downward compression first, then upward compression, followed by
    // per-band makeup. The gate is also applied after the crossover so it
    // cannot make one frequency band modulate another.
    const float x1 = juce::jlimit(80.f, 900.f, p.udmbcX1);
    const float x2 = juce::jlimit(x1 + 80.f, 5000.f, p.udmbcX2);
    const float x3 = juce::jlimit(x2 + 200.f, static_cast<float>(sr * 0.42), p.udmbcX3);

    const float xoverQ = crossoverQFromOverlap(p.udmbcXoverOverlap);

    updateCrossover(udmbcXover1, sr, x1, xoverQ);
    updateCrossover(udmbcXover2, sr, x2, xoverQ);
    updateCrossover(udmbcXover3, sr, x3, xoverQ);

    // Equalize the number of crossover sections traversed by each branch.
    // B1: X1 -> add all-pass X2 + X3. B2: X1+X2 -> add all-pass X3.
    updateCrossover(udmbcPhase2_B1, sr, x2, xoverQ);
    updateCrossover(udmbcPhase3_B1, sr, x3, xoverQ);
    updateCrossover(udmbcPhase3_B2, sr, x3, xoverQ);

    const float inputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.udmbcInputGainDb));
    const float globalMix =
        juce::jlimit(0.f, 1.f, p.udmbcMix / 100.f);
    const float outputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.udmbcOutputGainDb));

    float amountSum = 0.0f;
    int amountCount = 0;
    for (int band = 0; band < 4; ++band)
    {
        if (p.udmbcBandBypass[(size_t) band])
            continue;
        amountSum += juce::jlimit(
            0.0f, 100.0f, p.udmbcDegree[(size_t) band]) / 100.0f;
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

            float low = udmbcXover1.low(x, right);
            const float x1High = udmbcXover1.high(x, right);
            float lowMid = udmbcXover2.low(x1High, right);
            const float x2High = udmbcXover2.high(x1High, right);
            const float midHigh = udmbcXover3.low(x2High, right);
            const float top = udmbcXover3.high(x2High, right);

            // LP4 + HP4 compensation restores the phase path for skipped
            // crossovers without adding host/plugin latency.
            low = udmbcPhase2_B1.allPass(low, right);
            low = udmbcPhase3_B1.allPass(low, right);
            lowMid = udmbcPhase3_B2.allPass(lowMid, right);

            float bands[4] = { low, lowMid, midHigh, top };

            for (int band = 0; band < 4; ++band)
            {
                if (p.udmbcBandBypass[(size_t) band])
                    continue;

                const float degree =
                    juce::jlimit(0.f, 100.f, p.udmbcDegree[(size_t) band]);

                if (degree <= 0.0001f)
                    continue;

                auto& state = udmbcDynamics[(size_t) band];
                float& gateEnv = state.gateEnvDb[(size_t) ch];
                float& lifterEnv = state.lifterEnv[(size_t) ch];
                float& compEnv = state.compEnvDb[(size_t) ch];

                // The existing GATE control is now independent per frequency band.
                float v = applyGate(
                    bands[band], gateEnv,
                    p.udmbcGateThresholdDb, sr);

                // Degree=0 means true unity ratio. Degree=100 reaches the
                // UDMBC-style maximum ratios while preserving the user's
                // existing per-band controls.
                const float depth = degree / 100.f;

                // Classic UDMBC-style scaling: upward reaches 4:1.
                // Downward is intentionally much stronger, matching the
                // documented Ableton/Xfer family character. The top band
                // uses the slightly harder target.
                const float downMaxRatio = band == 3 ? 100.f : kCompressorRatio;
                const float downRatio =
                    1.f + depth * (downMaxRatio - 1.f);
                const float upRatio =
                    1.f + depth * (kLifterRatio - 1.f);

                const float compMix =
                    juce::jlimit(0.f, 100.f, p.udmbcCompMix[(size_t) band]);
                const float lifterMix =
                    juce::jlimit(0.f, 100.f, p.udmbcLifterMix[(size_t) band]);

                // Standard UDMBC order: downward first, upward second.
                // The user's Attack is automatically lengthened as UDMBC Amount
                // (degree) rises, reducing high-depth click / transient tearing.
                const float amount = depth;

                // The Compressor Attack parameter is the actual user base time.
                // Defaults are Low=15ms, LowMid=8ms, HighMid=3ms, High=1ms.
                const float baseAttackMs =
                    juce::jlimit(0.1f, 120.0f,
                                 p.udmbcCompAttack[(size_t) band]);

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
                                 p.udmbcCompRelease[(size_t) band]);
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
                float downReleaseMs = p.udmbcCompRelease[(size_t) band];
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
                    p.udmbcCompThreshold[(size_t) band],
                    finalAttackMs,
                    finalReleaseMs,
                    compMix, sr, downRatio,
                    attackCoef,
                    releaseCoef);

                float upReleaseMs = p.udmbcLifterRelease[(size_t) band];
                const float upDb = rmsDetectPDR(
                    v,
                    state.upRmsPower[(size_t) ch],
                    state.upSlowRmsPower[(size_t) ch],
                    p.udmbcLifterAttack[(size_t) band],
                    p.udmbcLifterRelease[(size_t) band],
                    sr,
                    upReleaseMs);

                const float liftThreshold =
                    juce::jmax(p.udmbcLifterThreshold[(size_t) band], -48.f);

                v = applyLifterFromDetectorDb(
                    v, upDb, lifterEnv,
                    liftThreshold,
                    p.udmbcLifterAttack[(size_t) band],
                    upReleaseMs,
                    lifterMix, sr, upRatio);

                v *= dbToGain(
                    juce::jlimit(-24.f, 12.f,
                        p.udmbcBandLevelDb[(size_t) band]));

                bands[band] = v;
            }

            float wet = bands[0] + bands[1] + bands[2] + bands[3];

            if (p.udmbcClipper)
                wet = std::tanh(wet * 1.7f);

            wet *= outputGain;

            // Do not clip the UDMBC reconstruction here. The final true-peak
            // lookahead limiter operates on the complete mixed programme.
            data[n] =
                (original + globalMix * (wet - original))
                * autoTrimGain;
        }
    }
}

void VVChainDSP::applyAType(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    // TAPE COLOR startup fix: stateless normalized tanh. Attack / Release and
    // envelope states are intentionally removed from the gain path.
    const float inputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.tapeInputGainDb));
    const float outputGain =
        dbToGain(juce::jlimit(-24.f, 24.f, p.tapeOutputGainDb));
    const float mix =
        juce::jlimit(0.f, 1.f, p.tapeMix / 100.f);

    // TAPE uses the same shared graph crossovers as UDMBC.
    const float x1 = juce::jlimit(40.f, 1000.f, p.udmbcX1);
    const float x2 = juce::jlimit(x1 + 80.f, 5000.f, p.udmbcX2);
    const float x3 = juce::jlimit(
        x2 + 200.f, static_cast<float>(sr * 0.42), p.udmbcX3);
    const float crossoverQ = crossoverQFromOverlap(p.udmbcXoverOverlap);

    updateCrossover(typeXover1, sr, x1, crossoverQ);
    updateCrossover(typeXover2, sr, x2, crossoverQ);
    updateCrossover(typeXover3, sr, x3, crossoverQ);

    std::array<float, 4> driveParam {};
    std::array<float, 4> staticMakeupMultiplier {};
    std::array<float, 4> bandTrim {};
    constexpr float kTypeAMaxDegree[4] = { 50.f, 60.f, 70.f, 90.f };
    // Keep legacy parameter ranges for preset/automation compatibility.
    // Full travel in every band now maps to the old 50-degree reference
    // (depth 0.5), the requested ~3 dB maximum colour range.

    // Parameter/update section: calculate each band's fixed drive and makeup
    // once per audio block, avoiding per-sample division.
    for (size_t band = 0; band < 4; ++band)
    {
        const float limitedDegree =
            juce::jlimit(0.f, kTypeAMaxDegree[band], p.tapeDegree[band]);
        const float controlNorm =
            limitedDegree / juce::jmax(1.0f, kTypeAMaxDegree[band]);
        const float depth =
            juce::jlimit(0.f, 0.5f, controlNorm * 0.5f);
        const float rawDriveParam = 1.0f + 1.5f * depth;
        driveParam[band] = juce::jmax(1.0f, rawDriveParam);

        float makeupDenominator = std::tanh(driveParam[band]);
        makeupDenominator =
            juce::jmax(makeupDenominator, 1.0e-6f);
        staticMakeupMultiplier[band] = 1.0f / makeupDenominator;

        bandTrim[band] = dbToGain(juce::jlimit(
            -6.f, 6.f, p.tapeBandLevelDb[band]));
    }

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer(ch);

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float original = data[n];
            const float x = original * inputGain;
            const bool right = ch == 1;

            // Exactly the same 4-band reconstruction used by UDMBC:
            // LP(X1), BP(X1..X2), BP(X2..X3), HP(X3).
            const float low =
                typeXover1.low(x, right);
            const float x1High =
                typeXover1.high(x, right);
            const float lowMid =
                typeXover2.low(x1High, right);
            const float x2High =
                typeXover2.high(x1High, right);
            const float midHigh =
                typeXover3.low(x2High, right);
            const float top =
                typeXover3.high(x2High, right);

            const float bands[4] = {
                low, lowMid, midHigh, top
            };

            float enhancement = 0.f;

            for (size_t band = 0; band < 4; ++band)
            {
                if (p.tapeBandBypass[band])
                    continue;

                const float limitedDegree =
                    juce::jlimit(0.f, kTypeAMaxDegree[band], p.tapeDegree[band]);
                const float controlNorm =
                    limitedDegree / juce::jmax(1.0f, kTypeAMaxDegree[band]);
                const float depth =
                    juce::jlimit(0.f, 0.5f, controlNorm * 0.5f);
                if (depth <= 0.f)
                    continue;

                // Requested core:
                // tanh(input * drive) / tanh(drive).
                // For |input| <= 1, the 0 dBFS reference maps to unity.
                const float driven =
                    std::tanh(bands[band] * driveParam[band])
                    * staticMakeupMultiplier[band];
                const float processed = driven * bandTrim[band];

                enhancement += (processed - bands[band]) * depth;
            }

            data[n] =
                (x + enhancement * mix) * outputGain;
        }
    }
}

void VVChainDSP::processDeEsser(juce::AudioBuffer<float>& buffer, const Parameters& p)
{
    // HYBRID MASTERING DE-ESSER
    // ------------------------------------------------------------
    // Detector:
    //   1) frequency-selective LR4 high-band
    //   2) slower broadband envelope for relative-HF detection
    //   3) fast + slow HF envelopes for stable sibilance detection
    //   4) L/R energy-linked detector so gain reduction cannot wander by side
    //
    // Audio path:
    //   lowBand + highBand * smoothedGain
    //
    // Design goals:
    //   - No detector signal is ever mixed back into the audio path.
    //   - No filter coefficients are recalculated per sample.
    //   - At 0 dB GR the original input is returned bit-for-bit.
    //   - The same LR4 crossover is used for detector and recombination.
    //   - Gain is smoothed in dB and is shared across the stereo pair.
    if (p.deessBypass)
    {
        deessLinkedGainDb = 0.0f;
        for (auto& state : deess)
            state.gainDb = 0.0f;
        return;
    }

    const float referenceHz =
        juce::jlimit(6000.0f, 18000.0f, p.deessReferenceHz);

    // Linkwitz-Riley 4th-order split: two Butterworth Q=0.707 sections.
    constexpr float crossoverQ = 0.70710678f;
    updateCrossover(deessSplit, sr, referenceHz, crossoverQ);

    struct DeEssPreset
    {
        float attackMs;
        float releaseMs;
        float knee;
    };

    // Response profiles. THRESHOLD is the user's trigger control.
    // Maximum GR is a fixed internal 8 dB mastering ceiling.
    static constexpr DeEssPreset presets[4]
    {
        { 2.50f, 120.0f, 2.00f }, // I   SAFE / SMOOTH
        { 1.50f,  70.0f, 1.75f }, // II  MASTER / BALANCED
        { 0.90f,  45.0f, 1.50f }, // III FAST / SILKY
        { 0.60f,  30.0f, 1.25f }  // IV  FIRM / CONTROLLED
    };

    const int modeIndex =
        juce::jlimit(1, 4, juce::roundToInt(p.deessMode)) - 1;
    const auto& preset = presets[modeIndex];

    constexpr float maxReductionDb = 8.0f;

    const float thresholdDb =
        juce::jlimit(
            -36.0f,
            0.0f,
            p.deessThresholdDb + p.deessAverageOffset);

    const float broadbandAttack =
        timeCoeff(sr, 12.0f);
    const float broadbandRelease =
        timeCoeff(sr, 180.0f);

    const float hfFastAttack =
        timeCoeff(sr, preset.attackMs);
    const float hfFastRelease =
        timeCoeff(sr, preset.releaseMs);
    const float hfSlowAttack =
        timeCoeff(sr, 8.0f);
    const float hfSlowRelease =
        timeCoeff(
            sr,
            juce::jmax(60.0f, preset.releaseMs * 1.5f));

    const float gainAttack =
        timeCoeff(sr, preset.attackMs);
    const float gainRelease =
        timeCoeff(sr, preset.releaseMs);
    const float gainReleaseSlow =
        timeCoeff(
            sr,
            juce::jmax(60.0f, preset.releaseMs * 1.8f));

    const float detectorFloor =
        juce::Decibels::decibelsToGain(-60.0f);

    const int activeChannels =
        juce::jlimit(
            1,
            2,
            std::min(buffer.getNumChannels(), channels));

    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        std::array<float, 2> lowBand { 0.0f, 0.0f };
        std::array<float, 2> highBand { 0.0f, 0.0f };
        std::array<float, 2> hfLevel { 0.0f, 0.0f };
        std::array<float, 2> broadLevel { 0.0f, 0.0f };

        for (int ch = 0; ch < activeChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            const bool right = ch == 1;

            const float input = data[n];

            // The crossover runs continuously so its internal states stay
            // warm even while gain reduction is at zero.
            lowBand[(size_t)ch] =
                deessSplit.low(input, right);
            highBand[(size_t)ch] =
                deessSplit.high(input, right);

            const float absInput =
                std::abs(input);
            const float absHigh =
                std::abs(highBand[(size_t)ch]);

            auto& state = deess[(size_t)ch];

            const float broadCoeff =
                absInput > state.broadbandEnv
                    ? broadbandAttack
                    : broadbandRelease;
            state.broadbandEnv =
                broadCoeff * state.broadbandEnv
                + (1.0f - broadCoeff) * absInput;

            const float fastCoeff =
                absHigh > state.hfFastEnv
                    ? hfFastAttack
                    : hfFastRelease;
            state.hfFastEnv =
                fastCoeff * state.hfFastEnv
                + (1.0f - fastCoeff) * absHigh;

            const float slowCoeff =
                absHigh > state.hfSlowEnv
                    ? hfSlowAttack
                    : hfSlowRelease;
            state.hfSlowEnv =
                slowCoeff * state.hfSlowEnv
                + (1.0f - slowCoeff) * absHigh;

            hfLevel[(size_t)ch] =
                0.72f * state.hfFastEnv
                + 0.28f * state.hfSlowEnv;
            broadLevel[(size_t)ch] =
                state.broadbandEnv;
        }

        // RMS-style energy link is calculated once for the stereo pair.
        // This yields one shared GR value for both channels.
        float hfPower = 0.0f;
        float broadPower = 0.0f;
        for (int ch = 0; ch < activeChannels; ++ch)
        {
            const float hf =
                hfLevel[(size_t)ch];
            const float broad =
                broadLevel[(size_t)ch];

            hfPower += hf * hf;
            broadPower += broad * broad;
        }

        const float invChannels =
            1.0f / static_cast<float>(activeChannels);
        hfPower *= invChannels;
        broadPower *= invChannels;

        float targetGR = 0.0f;

        const float detectorFloorPower =
            detectorFloor * detectorFloor;

        if (broadPower > detectorFloorPower
            && hfPower > 1.0e-10f)
        {
            // Power-ratio form of HF/Broadband detection. One sqrt() per
            // sample replaces the two square-roots previously needed for
            // separate linked amplitudes.
            const float relativeHf =
                std::sqrt(
                    hfPower
                    / juce::jmax(broadPower, 1.0e-12f));
            const float relativeHfDb =
                gainToDb(relativeHf);

            const float kneeWidthDb =
                juce::jmax(0.25f, preset.knee);
            const float kneeT =
                juce::jlimit(
                    0.0f,
                    1.0f,
                    (relativeHfDb - thresholdDb)
                    / kneeWidthDb);

            const float trigger =
                kneeT * kneeT * (3.0f - 2.0f * kneeT);

            targetGR =
                -maxReductionDb * trigger;
        }

        // Deep reduction gets a somewhat longer release. Both coefficients are
        // precomputed, so there is no per-sample exp()/pow()/tan() rebuild.
        const float depth =
            juce::jlimit(
                0.0f,
                1.0f,
                std::abs(deessLinkedGainDb)
                / juce::jmax(maxReductionDb, 1.0e-6f));

        const float releaseCoeff =
            gainRelease
            + (gainReleaseSlow - gainRelease) * depth;

        const float gainCoeff =
            targetGR < deessLinkedGainDb
                ? gainAttack
                : releaseCoeff;

        deessLinkedGainDb =
            gainCoeff * deessLinkedGainDb
            + (1.0f - gainCoeff) * targetGR;

        deessLinkedGainDb =
            juce::jlimit(
                -maxReductionDb,
                0.0f,
                deessLinkedGainDb);

        for (int ch = 0; ch < activeChannels; ++ch)
            deess[(size_t)ch].gainDb = deessLinkedGainDb;

        if (deessLinkedGainDb > -0.001f)
            continue;

        const float gainLinear =
            dbToGain(deessLinkedGainDb);

        for (int ch = 0; ch < activeChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            data[n] =
                lowBand[(size_t)ch]
                + highBand[(size_t)ch] * gainLinear;
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

    const bool captureContributions =
        contributionAnalysisEnabled && !p.masterBypass;

    if (captureContributions)
    {
        if (contributionPreAnalogReady)
            captureContributionMono(
                0, contributionPreAnalogBase, numSamples);
        else
            captureContributionMono(0, buffer, numSamples);

        captureContributionMono(1, buffer, numSamples);
        captureContributionMono(2, buffer, numSamples);
    }

    if (!p.udmbcBypass)
        applyOtt(buffer, p);

    if (captureContributions)
    {
        captureContributionMono(3, buffer, numSamples);
        captureContributionMono(4, buffer, numSamples);
    }

    if (!p.tapeBypass)
        applyAType(buffer, p);

    if (captureContributions)
        captureContributionMono(5, buffer, numSamples);

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
        juce::jlimit(40.f, 1000.f, p.udmbcX1);
    const float soloX2 =
        juce::jlimit(soloX1 + 80.f, 5000.f, p.udmbcX2);
    const float soloX3 =
        juce::jlimit(soloX2 + 200.f,
                     static_cast<float>(sr * 0.42),
                     p.udmbcX3);
    const float soloQ = crossoverQFromOverlap(
        p.udmbcXoverOverlap);

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

    updateDynamicDetector(
        graphSoloPre, sr,
        juce::jlimit(20.f, static_cast<float>(sr * 0.45), p.graphSoloFreq),
        juce::jlimit(0.1f, 18.f, p.graphSoloQ));
    updateDynamicDetector(
        graphSoloPost, sr,
        juce::jlimit(20.f, static_cast<float>(sr * 0.45), p.graphSoloFreq),
        juce::jlimit(0.1f, 18.f, p.graphSoloQ));

    const bool bandSoloEnabled =
        p.soloBand >= 0 && p.soloBand < 4;
    const bool soloEnabled =
        p.graphSoloActive || bandSoloEnabled;

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

            const float preSolo =
                p.graphSoloActive
                    ? graphSoloPre.process(
                        alignedDryBuffer.getSample(ch, n), right)
                    : splitBand(
                        alignedDryBuffer.getSample(ch, n),
                        soloPreXover1,
                        soloPreXover2,
                        soloPreXover3,
                        safeSoloBand,
                        right);

            const float postSolo =
                p.graphSoloActive
                    ? graphSoloPost.process(wet[n], right)
                    : splitBand(
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


