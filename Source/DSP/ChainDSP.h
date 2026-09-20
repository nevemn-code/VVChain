#pragma once

#include <JuceHeader.h>
#include <array>

class VVChainDSP
{
public:
    struct Parameters
    {
        std::array<float, 4> freq { 80.f, 350.f, 2500.f, 10000.f };
        std::array<float, 4> gain { 0.f, 0.f, 0.f, 0.f };
        std::array<float, 4> q { 0.707f, 0.707f, 0.707f, 0.707f };
        float hfCornerHz = 70.f;
        float ottDepth = 50.f;
        float ottMix = 50.f;
        float atypeAmount = 20.f;
        float atypeBias = 0.f;
        float deessFreq = 6500.f;
        float deessThreshold = -30.f;
        float deessRange = 8.f;
        float deessAttackMs = 1.0f;
        float deessReleaseMs = 80.f;
        float dryWet = 100.f;
        float outputDb = 0.f;
    };

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const Parameters& p);

private:
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double z1L = 0.0, z2L = 0.0, z1R = 0.0, z2R = 0.0;

        void reset() { z1L = z2L = z1R = z2R = 0.0; }
        inline float process(float x, bool right)
        {
            double& z1 = right ? z1R : z1L;
            double& z2 = right ? z2R : z2L;
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return static_cast<float>(y);
        }
    };

    static Biquad makePeak(double fs, double f0, double gainDb, double q);
    static Biquad makeHighPass(double fs, double f0, double q);

    std::array<Biquad, 4> eq {};
    Biquad hp {};
    double sr = 48000.0;
    int channels = 2;
    float deessEnvL = 0.f;
    float deessEnvR = 0.f;

    void applyEq(juce::AudioBuffer<float>&, const Parameters&);
    void applyOtt(juce::AudioBuffer<float>&, const Parameters&);
    void applyAType(juce::AudioBuffer<float>&, const Parameters&);
    void applyDeEsser(juce::AudioBuffer<float>&, const Parameters&);
};
