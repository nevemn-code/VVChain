#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <complex>
#include <atomic>

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
        bool eqColorGlobalBypass = false;

        // Four-band analogue-coloured parametric EQ.
        std::array<float, 4> freq { 80.f, 350.f, 2500.f, 10000.f };
        std::array<float, 4> gain { 0.f, 0.f, 0.f, 0.f };
        std::array<float, 4> q { 0.707f, 0.707f, 0.707f, 0.707f };

        // Four independent Dynamic EQ bands. Each detector is frequency-selective
        // and stereo-linked so L/R dynamics cannot wander independently.
        // Sonnox-style Dynamic EQ model:
        // Offset = resting EQ gain; Target defines the maximum dynamic span.
        // DYNAMICS is signed: -100..0% = downward compression,
        // 0..+100% = upward expansion. 0% is fully static.
        std::array<float, 4> dynThreshold { -24.f, -24.f, -24.f, -24.f };
        std::array<float, 4> dynTarget { -3.f, -3.f, -2.f, -2.f };
        std::array<float, 4> dynDynamics { 0.f, 0.f, 0.f, 0.f };
        std::array<float, 4> dynAttack { 8.f, 8.f, 5.f, 3.f };
        std::array<float, 4> dynRelease { 120.f, 120.f, 100.f, 80.f };
        std::array<bool, 4> dynDetectOnsets { false, false, false, false };
        std::array<bool, 4> dynTriggerBelow { false, false, false, false };
        // 0 = Side only, 50 = equal Mid/Side, 100 = Mid only.
        std::array<float, 4> dynMSBalance { 50.f, 50.f, 50.f, 50.f };

        std::array<float, 4> eqColor { 35.f, 35.f, 35.f, 35.f };
        std::array<bool, 4> eqColorBypass { false, false, false, false };
        // false = TT (Tube Saturation), true = SS (Solid-State Saturation)
        std::array<bool, 4> eqColorSolidState { false, false, false, false };

        // Four-band OTT / PunkOTT-MB style chain.
        std::array<bool, 4> ottBandBypass { false, false, false, false };
        std::array<float, 4> ottDegree { 35.f, 35.f, 30.f, 25.f };
        std::array<float, 4> ottLifterThreshold { -45.f, -45.f, -45.f, -45.f };
        std::array<float, 4> ottLifterAttack { 1.f, 1.f, 1.f, 1.f };
        std::array<float, 4> ottLifterRelease { 50.f, 50.f, 50.f, 50.f };
        std::array<float, 4> ottLifterMix { 100.f, 100.f, 100.f, 100.f };
        std::array<float, 4> ottCompThreshold { -18.f, -18.f, -18.f, -18.f };
        std::array<float, 4> ottCompAttack { 15.f, 8.f, 3.f, 1.f };
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
        float deessIntensity = 0.f;
        float deessAverageOffset = 0.f;

        int soloBand = -1;
        bool soloPost = false;

        float dryWet = 100.f;
        float outputDb = 0.f;
    };

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, const Parameters& p);

    // Live Dynamic EQ metering for the editor graph.
    float dynamicMidGainChangeDb(int band) const noexcept;
    float dynamicSideGainChangeDb(int band) const noexcept;
    float dynamicAverageGainChangeDb(int band) const noexcept;

    int getLatencySamples() const noexcept { return totalLatencySamples; }

private:

    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double z1L = 0.0, z2L = 0.0, z1R = 0.0, z2R = 0.0;

        void reset() { z1L = z2L = z1R = z2R = 0.0; }

        inline void updateCoefficients(double nb0, double nb1, double nb2,
                                       double na1, double na2) noexcept
        {
            b0 = nb0;
            b1 = nb1;
            b2 = nb2;
            a1 = na1;
            a2 = na2;
        }

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

        // LP4 + HP4 of the same crossover forms the phase-only all-pass
        // compensation needed when a band skipped this crossover.
        inline float allPass(float x, bool right)
        {
            return low(x, right) + high(x, right);
        }
    };

    struct Crossover2nd
    {
        Biquad lp, hp;

        void reset()
        {
            lp.reset();
            hp.reset();
        }

        inline float low(float x, bool right)
        {
            return lp.process(x, right);
        }

        inline float high(float x, bool right)
        {
            return hp.process(x, right);
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
        std::array<float, 2> upSlowRmsPower { 0.f, 0.f };
        std::array<float, 2> downRmsPower { 0.f, 0.f };
        std::array<float, 2> downSlowRmsPower { 0.f, 0.f };
    };

    struct DeEssState
    {
        Biquad sidechainHP {};
        float fastEnv = 0.f;
        float slowEnv = 0.f;
        float gainDb = 0.f;
    };

    static void updateAnalogPeak(Biquad& filter, double fs, double f0,
                                 double gainDb, double q);
    static void updateDynamicPeak(Biquad& filter, double fs, double f0,
                                  double gainDb, double q);
    static void updateDynamicDetector(Biquad& filter, double fs, double f0,
                                      double q);
    static void updateAnalogHighPass(Biquad& filter, double fs, double f0, double q);
    static void updateLowPass(Biquad& filter, double fs, double f0, double q);
    static void updateHighPass(Biquad& filter, double fs, double f0, double q);
    static void updateCrossover(Crossover4th& xover, double fs, double f0, double q);
    static void updateCrossover2nd(Crossover2nd& xover, double fs, double f0, double q);

    static float dbToGain(float db) noexcept;
    static float gainToDb(float gain) noexcept;
    static float timeCoeff(double sampleRate, float ms) noexcept;
    void processChebyshevAnalog(juce::dsp::AudioBlock<float>& block,
                                float drive, float amount);

    static float rmsDetectPDR(float input,
                               float& fastPower,
                               float& slowPower,
                               float attackMs,
                               float releaseMs,
                               double sampleRate,
                               float& programReleaseMs,
                               float attackCoeffOverride = -1.0f,
                               float releaseCoeffOverride = -1.0f) noexcept;

    static float applyLifterFromDetectorDb(float input, float detectorDb,
                                            float& env, float thresholdDb,
                                            float attackMs, float releaseMs,
                                            float mix, double sampleRate,
                                            float ratio = 4.f);

    static float applyCompressorFromDetectorDb(float input, float detectorDb,
                                                float& envDb, float thresholdDb,
                                                float attackMs, float releaseMs,
                                                float mix, double sampleRate,
                                                float ratio = 66.7f,
                                                float attackCoeffOverride = -1.0f,
                                                float releaseCoeffOverride = -1.0f);


    static float applyGate(float input, float& envDb, float thresholdDb,
                           double sampleRate);

    static float applyLimiter(float input, float& envDb, double sampleRate);

    void processDeEsser(juce::AudioBuffer<float>& buffer, const Parameters& p);

    void applyEq(juce::AudioBuffer<float>&, const Parameters&);
    void applyOtt(juce::AudioBuffer<float>&, const Parameters&);
    void applyAType(juce::AudioBuffer<float>&, const Parameters&);

    void processMasterLimiter(juce::AudioBuffer<float>& buffer, bool active);
    void alignDryBuffer(int numSamples);

    std::array<Biquad, 4> eq {};
    std::array<Biquad, 4> dynMidEq {};
    std::array<Biquad, 4> dynSideEq {};
    std::array<Biquad, 4> dynMidDetectors {};
    std::array<Biquad, 4> dynSideDetectors {};

    std::array<float, 4> dynMidEnvelopeDb
    {
        -120.f, -120.f, -120.f, -120.f
    };
    std::array<float, 4> dynSideEnvelopeDb
    {
        -120.f, -120.f, -120.f, -120.f
    };

    std::array<std::atomic<float>, 4> dynMidGainChangeDb
    {
        0.f, 0.f, 0.f, 0.f
    };
    std::array<std::atomic<float>, 4> dynSideGainChangeDb
    {
        0.f, 0.f, 0.f, 0.f
    };

    std::array<float, 4> dynMidSlowDb { -120.f, -120.f, -120.f, -120.f };
    std::array<float, 4> dynSideSlowDb { -120.f, -120.f, -120.f, -120.f };
    std::array<float, 4> dynMidActivation { 0.f, 0.f, 0.f, 0.f };
    std::array<float, 4> dynSideActivation { 0.f, 0.f, 0.f, 0.f };

    // Feed-forward detector source shared by all four Dynamic EQ bands.
    juce::AudioBuffer<float> dynamicDetectorInput;

    // Reusable one-channel scratch for allocation-free high-density ANALOG.
    juce::AudioBuffer<float> analogTempBuffer;
    Crossover4th ottXover1 {};
    Crossover4th ottXover2 {};
    Crossover4th ottXover3 {};

    // Phase-alignment dummy crossovers for the unequal-depth OTT branches:
    // Band 1 skips X2/X3; Band 2 skips X3.
    Crossover4th ottPhase2_B1 {};
    Crossover4th ottPhase3_B1 {};
    Crossover4th ottPhase3_B2 {};

    std::array<BandDynamics, 4> ottDynamics {};

    // Four independent Type-A exciter bands.
    // Fixed crossovers follow a practical 4-band exciter layout:
    // ~20-200 Hz / 200-2 kHz / 2-7.8 kHz / 7.8-20 kHz.
    Crossover2nd typeXover1 {};
    Crossover2nd typeXover2 {};
    Crossover2nd typeXover3 {};
    std::array<std::array<float, 2>, 4> typeFastEnv {};
    std::array<std::array<float, 2>, 4> typeSlowEnv {};
    std::array<std::array<float, 2>, 4> typeDc {};

    std::array<DeEssState, 2> deess {};
    Crossover4th deessSplit {};
    juce::dsp::Oversampling<float> eqOversampler
    {
        2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
        true, true
    };
    juce::dsp::Oversampling<float> limiterOversampler
    {
        2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true, true
    };
    juce::dsp::DelayLine<float> eqDryDelay { 4096 };
    juce::dsp::DelayLine<float> limiterLookahead { 8192 };
    juce::dsp::DelayLine<float> masterDryDelay { 8192 };
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> alignedDryBuffer;

    Crossover4th soloPreXover1 {}, soloPreXover2 {}, soloPreXover3 {};
    Crossover4th soloPostXover1 {}, soloPostXover2 {}, soloPostXover3 {};
    float soloBlend = 0.f;
    int lastSoloBand = -2;
    bool lastSoloPost = false;

    std::array<float, 2> gateEnvDb {};
    std::array<float, 2> limiterEnvDb {};

    float limiterGain = 1.f;
    float masterBypassBlend = 0.f;
    int eqLatencySamples = 0;
    int limiterOversamplingLatencySamples = 0;
    int limiterLookaheadSamples = 0;
    int totalLatencySamples = 0;

    double sr = 48000.0;
    int channels = 2;
};
