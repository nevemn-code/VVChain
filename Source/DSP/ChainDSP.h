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
        float eqColor = 35.f;
        float hfCornerHz = 70.f;

        std::array<float, 4> ottAmount { 50.f, 50.f, 50.f, 50.f };
        float ottMix = 50.f;
        float ottThreshold = -24.f;
        float ottUpRatio = 4.f;
        float ottDownRatio = 20.f;
        float ottAttackMs = 2.5f;
        float ottReleaseMs = 80.f;
        float ottX1 = 88.f;
        float ottX2 = 2500.f;
        float ottX3 = 8500.f;
        float ottInputGainDb = 5.2f;
        float ottPostGainDb = 0.f;

        std::array<float, 4> atypeAmount { 0.f, 20.f, 70.f, 55.f };
        std::array<float, 4> atypeGainDb { 0.f, 0.f, 1.f, 1.f };
        float atypeAttackMs = 10.f;
        float atypeReleaseMs = 120.f;
        float atypeMix = 100.f;

        float deessLowHz = 4500.f;
        float deessHighHz = 10500.f;
        float deessRangeDb = 10.f;
        float deessStrength = 75.f;
        float deessAttackMs = 1.0f;
        float deessReleaseMs = 80.f;
        bool deessListen = false;

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
    static Biquad makeLowPass(double fs, double f0, double q);
    static Biquad makeHighPass(double fs, double f0, double q);
    static float analogColor(float x, float amount01) noexcept;
    static float dbToGain(float db) noexcept;
    static float gainToDb(float gain) noexcept;

    std::array<Biquad, 4> eq {};
    Biquad hp {};

    Biquad ottLP1 {}, ottHP1 {}, ottLP2 {}, ottHP2 {}, ottLP3 {}, ottHP3 {};
    std::array<float, 4> ottEnvL {};
    std::array<float, 4> ottEnvR {};

    Biquad typeLP80 {}, typeHP80 {}, typeLP3k {}, typeHP3k {}, typeHP9k {};
    std::array<float, 4> typeEnvL {};
    std::array<float, 4> typeEnvR {};

    Biquad deessHP {};
    Biquad deessLP {};
    float deessEnvL = 0.f;
    float deessEnvR = 0.f;

    double sr = 48000.0;
    int channels = 2;

    void applyEq(juce::AudioBuffer<float>&, const Parameters&);
    void applyOtt(juce::AudioBuffer<float>&, const Parameters&);
    void applyAType(juce::AudioBuffer<float>&, const Parameters&);
    void applyDeEsser(juce::AudioBuffer<float>&, const Parameters&);
};
