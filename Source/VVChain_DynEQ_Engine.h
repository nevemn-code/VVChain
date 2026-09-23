#pragma once
#include <JuceHeader.h>
#include <cmath>

// VVChain Dynamic EQ shared math.
// Keeps DSP target math and graph response math explicit and deterministic.
class VVChain_DynEQ_Engine
{
public:
    // Static gain and dynamic contribution are combined in dB.
    static float getTargetGainDB(float staticGainDB,
                                 float dynRangeDB,
                                 float envRatio) noexcept
    {
        const float safeStatic = juce::jlimit(-36.0f, 36.0f, staticGainDB);
        const float safeRange = juce::jlimit(-36.0f, 36.0f, dynRangeDB);
        const float safeRatio = juce::jlimit(0.0f, 1.0f, envRatio);

        return juce::jlimit(
            -36.0f,
            36.0f,
            safeStatic + safeRange * safeRatio);
    }

    // Exact RBJ peaking-EQ magnitude used by VVChain's dynamic EQ filter.
    // This avoids the old Gaussian visual approximation in the dynamic target
    // curve and keeps the graph tied to the same coefficient model as DSP.
    static float peakMagnitudeDBAtFrequency(double sampleRate,
                                             float centreFrequency,
                                             float q,
                                             float gainDB,
                                             float frequency) noexcept
    {
        const double sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        const double f = juce::jlimit(
            10.0,
            sr * 0.45,
            static_cast<double>(centreFrequency));
        const double safeQ = juce::jlimit(
            0.05,
            30.0,
            static_cast<double>(q));
        const double g = juce::jlimit(
            -36.0,
            36.0,
            static_cast<double>(gainDB));
        const double hz = juce::jlimit(
            10.0,
            sr * 0.45,
            static_cast<double>(frequency));

        const double A = std::pow(10.0, g / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * f / sr;
        const double w = juce::MathConstants<double>::twoPi * hz / sr;
        const double alpha =
            std::sin(w0) / (2.0 * safeQ);
        const double c0 = std::cos(w);
        const double c2 = std::cos(2.0 * w);
        const double s0 = std::sin(w);
        const double s2 = std::sin(2.0 * w);

        const double b0 = 1.0 + alpha * A;
        const double b1 = -2.0 * std::cos(w0);
        const double b2 = 1.0 - alpha * A;
        const double a0 = 1.0 + alpha / A;
        const double a1 = -2.0 * std::cos(w0);
        const double a2 = 1.0 - alpha / A;

        const double numReal =
            b0 + b1 * c0 + b2 * c2;
        const double numImag =
            b1 * (-s0) + b2 * (-s2);
        const double denReal =
            a0 + a1 * c0 + a2 * c2;
        const double denImag =
            a1 * (-s0) + a2 * (-s2);

        const double num2 =
            numReal * numReal + numImag * numImag;
        const double den2 =
            std::max(1.0e-24,
                     denReal * denReal + denImag * denImag);

        const double magnitude =
            std::sqrt(std::max(1.0e-24, num2 / den2));

        return juce::Decibels::gainToDecibels(
            static_cast<float>(magnitude));
    }
};
