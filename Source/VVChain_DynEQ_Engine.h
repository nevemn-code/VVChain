#pragma once
#include <algorithm>
#include <cmath>
#include <JuceHeader.h>
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
        const float safeStatic = juce::jlimit(-18.0f, 18.0f, staticGainDB);
        const float safeRange = juce::jlimit(-18.0f, 18.0f, dynRangeDB);
        const float safeRatio = juce::jlimit(0.0f, 1.0f, envRatio);

        // Dynamic EQ is centered on the current static EQ gain.
        // The dynamic span is applied from that exact point, not as a separate absolute range.
        return juce::jlimit(
            -18.0f,
            18.0f,
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
        // Graph-only transfer calculation for the same Cytomic / Simper TPT
        // Bell topology used by the current Dynamic EQ DSP. This function does
        // not process audio; it only evaluates the frequency response for UI.
        const double sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        const double f = juce::jlimit(20.0, sr * 0.45,
                                      static_cast<double>(centreFrequency));
        const double safeQ = juce::jlimit(0.1, 18.0,
                                          static_cast<double>(q));
        const double gDB = juce::jlimit(-18.0, 18.0,
                                        static_cast<double>(gainDB));
        const double hz = juce::jlimit(10.0, sr * 0.45,
                                       static_cast<double>(frequency));

        const double A = std::pow(10.0, gDB / 40.0);
        const double g = std::tan(juce::MathConstants<double>::pi * f / sr);
        const double k = 1.0 / (safeQ * A);
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;
        const double m1 = k * (A * A - 1.0);

        const double A11 = 2.0 * a1 - 1.0;
        const double A12 = -2.0 * a2;
        const double A21 = 2.0 * a2;
        const double A22 = 1.0 - 2.0 * a3;
        const double B1 = 2.0 * a2;
        const double B2 = 2.0 * a3;
        const double C1 = m1 * a1;
        const double C2 = -m1 * a2;
        const double D = 1.0 + m1 * a2;

        const double w = juce::MathConstants<double>::twoPi * hz / sr;
        const double zr = std::cos(w);
        const double zi = std::sin(w);

        const double d11 = zr - A11;
        const double d22 = zr - A22;
        const double detR = d11 * d22 - A12 * A21 - zi * zi;
        const double detI = zi * (d11 + d22);
        const double det2 = std::max(1.0e-30, detR * detR + detI * detI);

        const double n1R = d22 * B1 + A12 * B2;
        const double n1I = zi * B1;
        const double n2R = A21 * B1 + d11 * B2;
        const double n2I = zi * B2;

        const double h1R = (n1R * detR + n1I * detI) / det2;
        const double h1I = (n1I * detR - n1R * detI) / det2;
        const double h2R = (n2R * detR + n2I * detI) / det2;
        const double h2I = (n2I * detR - n2R * detI) / det2;

        const double outR = D + C1 * h1R + C2 * h2R;
        const double outI = C1 * h1I + C2 * h2I;
        const double magnitude = std::max(1.0e-12,
                                          std::hypot(outR, outI));
        return static_cast<float>(20.0 * std::log10(magnitude));
    }
};
