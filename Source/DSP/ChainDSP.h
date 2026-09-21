#pragma once

#include <JuceHeader.h>
#include <array>
#include <complex>

class VVChainDSP
{
public:
    struct Parameters
    {
        bool eqBypass = false;
        bool masterBypass = false;
        bool ottBypass = false;
        bool atypeBypass = false;
        bool deessBypass = false;
        bool mixBypass = false;

        // Four-band analogue-coloured parametric EQ.
        std::array<float, 4> freq { 80.f, 350.f, 2500.f, 10000.f };
        std::array<float, 4> gain { 0.f, 0.f, 0.f, 0.f };
        std::array<float, 4> q { 0.707f, 0.707f, 0.707f, 0.707f };
        float eqColor = 35.f;
        float hfCornerHz = 70.f;

        // Four-band OTT / PunkOTT-MB style chain.
        std::array<bool, 4> ottBandBypass { false, false, false, false };
        std::array<float, 4> ottDegree { 35.f, 35.f, 30.f, 25.f };
        std::array<float, 4> ottLifterThreshold { -45.f, -45.f, -45.f, -45.f };
        std::array<float, 4> ottLifterAttack { 1.f, 1.f, 1.f, 1.f };
        std::array<float, 4> ottLifterRelease { 50.f, 50.f, 50.f, 50.f };
        std::array<float, 4> ottLifterMix { 100.f, 100.f, 100.f, 100.f };
        std::array<float, 4> ottCompThreshold { -18.f, -18.f, -18.f, -18.f };
        std::array<float, 4> ottCompAttack { 1.f, 1.f, 1.f, 1.f };
        std::array<float, 4> ottCompRelease { 50.f, 50.f, 50.f, 50.f };
        std::array<float, 4> ottCompMix { 100.f, 100.f, 100.f, 100.f };
        std::array<float, 4> ottBandLevelDb { 0.f, 0.f, 0.f, 0.f };
        float ottX1 = 120.f;
        float ottX2 = 1000.f;
        float ottX3 = 7000.f;
        float ottXoverOverlap = 50.f;
        float ottInputGainDb = 0.f;
        float ottGateThresholdDb = -80.f;
        float ottMix = 25.f;
        bool ottClipper = false;
        float ottOutputGainDb = 0.f;

        // Four-band Type-A / Dolby-A-style dynamic enhancer.
        std::array<bool, 4> atypeBandBypass { false, false, false, false };
        std::array<float, 4> atypeDegree { 0.f, 20.f, 70.f, 55.f };
        std::array<float, 4> atypeBandLevelDb { 0.f, 0.f, 1.f, 1.f };
        float atypeAttackMs = 10.f;
        float atypeReleaseMs = 120.f;
        float atypeMix = 100.f;
        float atypeInputGainDb = 0.f;
        float atypeOutputGainDb = 0.f;

        // Reference-based DeEsser controls. Defaults preserve the reference behaviour.
        float deessReferenceHz = 12500.f;
        float deessIntensity = 10.f;
        float deessAverageOffset = 0.f;

        float dryWet = 100.f;
        float outputDb = 0.f;
    };

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const Parameters& p);

private:
    static constexpr int kDeessBlockSize = 8192;

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
        // Independent state for each OTT band / channel.
        // Gate, upward and downward envelopes never share detector state.
        std::array<float, 2> gateEnvDb { 0.f, 0.f };
        std::array<float, 2> lifterEnv { 1.f, 1.f };
        std::array<float, 2> compEnvDb { 0.f, 0.f };
        std::array<float, 2> upRmsPower { 0.f, 0.f };
        std::array<float, 2> downRmsPower { 0.f, 0.f };
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

    static float rmsDetect(float input, float& power, float attackMs, float releaseMs,
                           double sampleRate) noexcept;

    static float applyLifterFromDetectorDb(float input, float detectorDb,
                                            float& env, float thresholdDb,
                                            float attackMs, float releaseMs,
                                            float mix, double sampleRate,
                                            float ratio = 4.f);

    static float applyCompressorFromDetectorDb(float input, float detectorDb,
                                                float& envDb, float thresholdDb,
                                                float attackMs, float releaseMs,
                                                float mix, double sampleRate,
                                                float ratio = 66.7f);


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

    // Four independent Type-A exciter bands.
    // Fixed crossovers follow a practical 4-band exciter layout:
    // ~20-200 Hz / 200-2 kHz / 2-7.8 kHz / 7.8-20 kHz.
    Crossover4th typeXover1 {};
    Crossover4th typeXover2 {};
    Crossover4th typeXover3 {};
    std::array<std::array<float, 2>, 4> typeFastEnv {};
    std::array<std::array<float, 2>, 4> typeSlowEnv {};
    std::array<std::array<float, 2>, 4> typeDc {};

    std::array<DeEssState, 2> deess {};
    std::array<std::array<float, kDeessBlockSize>, 2> dryDelay {};
    int dryDelayWrite = 0;

    std::array<float, 2> gateEnvDb {};
    std::array<float, 2> limiterEnvDb {};

    std::array<std::complex<double>, kDeessBlockSize> deessFft {};

    float masterBypassBlend = 0.f;

    double deessAvgSum = 0.0;
    uint64_t deessAvgCount = 0;
    uint64_t deessSampleCounter = 0;
    float deessPendingSample = 0.0f;

    double sr = 48000.0;
    int channels = 2;
};
