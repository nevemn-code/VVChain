#pragma once

#include <JuceHeader.h>
#include <array>
#include <complex>

class VVChainDSP
{
public:
    struct Parameters
    {
        // Four-band analogue-coloured parametric EQ.
        std::array<float, 4> freq { 80.f, 350.f, 2500.f, 10000.f };
        std::array<float, 4> gain { 0.f, 0.f, 0.f, 0.f };
        std::array<float, 4> q { 0.707f, 0.707f, 0.707f, 0.707f };
        float eqColor = 35.f;
        float hfCornerHz = 70.f;

        // Four-band OTT / PunkOTT-MB style chain.
        std::array<float, 4> ottDegree { 100.f, 100.f, 100.f, 100.f };
        std::array<float, 4> ottLifterThreshold { -40.f, -40.f, -40.f, -40.f };
        std::array<float, 4> ottLifterAttack { 50.f, 50.f, 50.f, 50.f };
        std::array<float, 4> ottLifterRelease { 50.f, 50.f, 50.f, 50.f };
        std::array<float, 4> ottLifterMix { 100.f, 100.f, 100.f, 100.f };
        std::array<float, 4> ottCompThreshold { -12.f, -12.f, -12.f, -12.f };
        std::array<float, 4> ottCompAttack { 15.f, 15.f, 15.f, 15.f };
        std::array<float, 4> ottCompRelease { 60.f, 60.f, 60.f, 60.f };
        std::array<float, 4> ottCompMix { 100.f, 100.f, 100.f, 100.f };
        std::array<float, 4> ottBandLevelDb { 0.f, 0.f, 0.f, 0.f };
        float ottX1 = 350.f;
        float ottX2 = 1000.f;
        float ottX3 = 9000.f;
        float ottInputGainDb = 5.2f;
        float ottGateThresholdDb = -80.f;
        float ottMix = 100.f;
        bool ottClipper = true;
        float ottOutputGainDb = -6.f;

        // Four-band Type-A / Dolby-A-style dynamic enhancer.
        std::array<float, 4> atypeDegree { 0.f, 20.f, 70.f, 55.f };
        std::array<float, 4> atypeBandLevelDb { 0.f, 0.f, 1.f, 1.f };
        float atypeAttackMs = 10.f;
        float atypeReleaseMs = 120.f;
        float atypeMix = 100.f;
        float atypeInputGainDb = 0.f;
        float atypeOutputGainDb = 0.f;

        // DeEsser follows the public reference processing exactly.
        // Legacy parameter slots remain for preset compatibility but are ignored:
        // FFT 4096, threshold/reference 12.5 kHz, 2/3 overlap, middle 1/3 output.
        int deessVoice = 0;
        float deessIntensity = 10.f;
        float deessAverageOffset = 0.f;

        float dryWet = 100.f;
        float outputDb = 0.f;
    };

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const Parameters& p);

private:
    static constexpr int kDeessBlockSize = 4096;
    static constexpr int kDeessHopSize = 1365;
    static constexpr int kDeessDelay = kDeessBlockSize - kDeessHopSize;

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

    struct Crossover4th
    {
        Biquad lp1, lp2, hp1, hp2;

        void reset()
        {
            lp1.reset(); lp2.reset();
            hp1.reset(); hp2.reset();
        }

        inline float low(float x, bool right)
        {
            return lp2.process(lp1.process(x, right), right);
        }

        inline float high(float x, bool right)
        {
            return hp2.process(hp1.process(x, right), right);
        }
    };

    struct BandDynamics
    {
        std::array<float, 2> lifterEnv { 1.f, 1.f };
        std::array<float, 2> compEnvDb { 0.f, 0.f };
    };

    struct DeEssState
    {
        std::array<float, kDeessBlockSize> input {};
        std::array<float, kDeessBlockSize> output {};
        int inputCount = 0;
        int outputRead = 0;
        int outputReady = 0;
        std::array<float, kDeessBlockSize * 4> queue {};
        int queueRead = 0;
        int queueWrite = 0;
        int queueCount = 0;
    };

    static Biquad makeAnalogPeak(double fs, double f0, double gainDb, double q);
    static Biquad makeAnalogHighPass(double fs, double f0, double q);
    static Biquad makeLowPass(double fs, double f0, double q);
    static Biquad makeHighPass(double fs, double f0, double q);

    static float dbToGain(float db) noexcept;
    static float gainToDb(float gain) noexcept;
    static float timeCoeff(double sampleRate, float ms) noexcept;
    static float softColor(float x, float amount01) noexcept;

    static float applyLifter(float input, float& env, float thresholdDb,
                             float attackMs, float releaseMs, float mix,
                             double sampleRate);

    static float applyCompressor(float input, float& envDb, float thresholdDb,
                                 float attackMs, float releaseMs, float mix,
                                 double sampleRate, float ratio = 8.f);

    static float applyGate(float input, float& envDb, float thresholdDb,
                           double sampleRate);

    static float applyLimiter(float input, float& envDb, double sampleRate);

    static void fft(std::array<std::complex<double>, kDeessBlockSize>& data, bool inverse);

    void processDeEsserWindow(DeEssState& state, const Parameters& p);
    void processDeEsser(juce::AudioBuffer<float>& buffer, const Parameters& p);

    void applyEq(juce::AudioBuffer<float>&, const Parameters&);
    void applyOtt(juce::AudioBuffer<float>&, const Parameters&);
    void applyAType(juce::AudioBuffer<float>&, const Parameters&);

    std::array<Biquad, 4> eq {};
    Biquad hp {};

    Crossover4th ottXover1 {};
    Crossover4th ottXover2 {};
    Crossover4th ottXover3 {};
    std::array<BandDynamics, 4> ottDynamics {};

    Crossover4th typeXover1 {};
    Crossover4th typeXover2 {};
    Biquad typeHP9k {};
    std::array<std::array<float, 2>, 4> typeEnv {};

    std::array<DeEssState, 2> deess {};
    std::array<std::array<float, kDeessBlockSize>, 2> dryDelay {};
    int dryDelayWrite = 0;

    std::array<float, 2> gateEnvDb {};
    std::array<float, 2> limiterEnvDb {};

    std::array<std::complex<double>, kDeessBlockSize> deessFft {};

    double sr = 48000.0;
    int channels = 2;
};
