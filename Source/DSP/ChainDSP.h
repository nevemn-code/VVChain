#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <complex>
#include <atomic>
#include "VVChain_AnalogADAA_v2.h"

class VVChainDSP
{
public:
    struct Parameters
    {
        bool eqBypass = false;
        bool masterBypass = false;
        bool udmbcBypass = false;
        bool tapeBypass = false;
        bool deessBypass = false;
        bool deltaMonitor = false;
        bool mixBypass = false;
        bool eqColorGlobalBypass = false;

        // Four-band analogue-coloured parametric EQ.
        std::array<float, 4> freq { 80.f, 350.f, 2500.f, 10000.f };
        std::array<float, 4> gain { 0.f, 0.f, 0.f, 0.f };
        std::array<float, 4> q { 0.707f, 0.707f, 0.707f, 0.707f };
        // 0 Parametric Bell, 1 Matched Bell, 2/3 Plateau A/B,
        // 4/5 Shelf, 6/7 resonant Shelf, 8/9 Contour,
        // 10 Focus Pass, 11 Deep Reject, 12/13 72 dB/oct roll-off.
        std::array<int, 4> eqType { 0, 0, 0, 0 };
        // Roll-off slope index: 0..5 = 12..72 dB/oct.
        std::array<int, 4> eqSlope { 5, 5, 5, 5 };

        // Four independent Dynamic EQ bands. Each detector is frequency-selective
        // and stereo-linked so L/R dynamics cannot wander independently.
        // Sonnox-style Dynamic EQ model:
        // Offset = resting EQ gain; Target defines the maximum dynamic span.
        // DYNAMICS is signed: -100..0% = downward compression,
        // 0..+100% = upward expansion. 0% is fully static.
        std::array<float, 4> dynTarget { 3.f, 3.f, 2.f, 2.f };
        std::array<float, 4> dynDynamics { 0.f, 0.f, 0.f, 0.f };
        std::array<float, 4> dynAttack { 8.f, 8.f, 5.f, 3.f };
        std::array<float, 4> dynRelease { 120.f, 120.f, 100.f, 80.f };
        std::array<float, 4> dynDetectOnsets { 50.f, 50.f, 50.f, 50.f };
        std::array<bool, 4> dynTriggerBelow { false, false, false, false };
        // 0 = Side only, 50 = equal Mid/Side, 100 = Mid only.
        std::array<float, 4> dynMSBalance { 50.f, 50.f, 50.f, 50.f };

        std::array<float, 4> eqColor { 35.f, 35.f, 35.f, 35.f };
        std::array<bool, 4> eqColorBypass { false, false, false, false };
        std::array<bool, 4> eqColorX2 { false, false, false, false };
        // false = TT (Tube Saturation), true = SS (Solid-State Saturation)
        std::array<bool, 4> eqColorSolidState { false, false, false, false };

        // Four-band UDMBC / UDMBC style chain.
        std::array<bool, 4> udmbcBandBypass { false, false, false, false };
        std::array<float, 4> udmbcDegree { 35.f, 35.f, 30.f, 25.f };
        std::array<float, 4> udmbcLifterThreshold { -45.f, -45.f, -45.f, -45.f };
        std::array<float, 4> udmbcLifterAttack { 1.f, 1.f, 1.f, 1.f };
        std::array<float, 4> udmbcLifterRelease { 50.f, 50.f, 50.f, 50.f };
        std::array<float, 4> udmbcLifterMix { 100.f, 100.f, 100.f, 100.f };
        std::array<float, 4> udmbcCompThreshold { -18.f, -18.f, -18.f, -18.f };
        std::array<float, 4> udmbcCompAttack { 15.f, 8.f, 3.f, 1.f };
        std::array<float, 4> udmbcCompRelease { 50.f, 50.f, 50.f, 50.f };
        std::array<float, 4> udmbcCompMix { 100.f, 100.f, 100.f, 100.f };
        std::array<float, 4> udmbcBandLevelDb { 0.f, 0.f, 0.f, 0.f };
        float udmbcX1 = 120.f;
        float udmbcX2 = 1000.f;
        float udmbcX3 = 7000.f;
        float udmbcXoverOverlap = 50.f;
        float udmbcInputGainDb = 0.f;
        float udmbcGateThresholdDb = -80.f;
        float udmbcMix = 25.f;
        bool udmbcClipper = false;
        float udmbcOutputGainDb = 0.f;

        // Four-band TAPE / multi-band enhancer dynamic enhancer.
        std::array<bool, 4> tapeBandBypass { false, false, false, false };
        std::array<float, 4> tapeDegree { 0.f, 20.f, 70.f, 55.f };
        std::array<float, 4> tapeBandLevelDb { 0.f, 0.f, 1.f, 1.f };
        float tapeAttackMs = 10.f;
        float tapeReleaseMs = 120.f;
        float tapeMix = 100.f;
        float tapeInputGainDb = 0.f;
        float tapeOutputGainDb = 0.f;

        // Hybrid mastering DeEsser controls.
        // Frequency selects the LR4 split point; threshold is the relative-HF
        // trigger value in dB. Lower threshold = more sensitive detection.
        float deessReferenceHz = 7500.f;
        float deessThresholdDb = -6.f;
        float deessAverageOffset = 0.f;
        float deessMode = 2.f;

        int soloBand = -1;
        bool soloPost = false;
        bool graphSoloActive = false;
        float graphSoloFreq = 1000.f;
        float graphSoloQ = 0.707f;

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

    struct TPTBell
    {
        double g = 0.0;
        double k = 1.0;
        double a1 = 1.0;
        double a2 = 0.0;
        double a3 = 0.0;
        double m1 = 0.0;
        double ic1eq = 0.0;
        double ic2eq = 0.0;

        void reset() noexcept
        {
            ic1eq = 0.0;
            ic2eq = 0.0;
        }

        inline float process(float input) noexcept
        {
            const double x = static_cast<double>(input);
            const double v3 = x - ic2eq;
            const double v1 = a1 * ic1eq + a2 * v3;
            const double v2 = ic2eq + a2 * ic1eq + a3 * v3;

            ic1eq = 2.0 * v1 - ic1eq;
            ic2eq = 2.0 * v2 - ic2eq;

            // Bell: y = x + m1 * band-pass.  At 0 dB, m1 is exactly 0,
            // making the filter structurally bit-transparent.
            return static_cast<float>(x + m1 * v1);
        }
    };

    struct EqFilter
    {
        TPTBell peak {};
        std::array<Biquad, 12> stages {};
        int stageCount = 0;
        int configuredType = -1;
        bool useTptPeak = true;
        bool parallelBandShelf = false;
        double parallelMix = 0.0;
        double outputGain = 1.0;
        float lastOutput = 0.0f;
        float transitionStart = 0.0f;
        float transition = 1.0f;

        void reset() noexcept
        {
            peak.reset();
            for (auto& s : stages) s.reset();
            stageCount = 0;
            configuredType = -1;
            useTptPeak = true;
            parallelBandShelf = false;
            parallelMix = 0.0;
            outputGain = 1.0;
            lastOutput = 0.0f;
            transitionStart = 0.0f;
            transition = 1.0f;
        }

        void beginType(int type) noexcept
        {
            if (configuredType == type)
                return;

            transitionStart = lastOutput;
            transition = 0.0f;
            peak.reset();
            for (auto& s : stages) s.reset();
            configuredType = type;
        }

        inline float process(float input) noexcept
        {
            float filtered = input;

            if (useTptPeak)
            {
                filtered = peak.process(input);
            }
            else
            {
                for (int i = 0; i < stageCount; ++i)
                    filtered = stages[(size_t)i].process(filtered, false);

                if (parallelBandShelf)
                    filtered = static_cast<float>(
                        static_cast<double>(input)
                        + parallelMix * static_cast<double>(filtered));
                else
                    filtered = static_cast<float>(
                        static_cast<double>(filtered) * outputGain);
            }

            if (transition < 1.0f)
            {
                // Click-safe parameter-type transition. This does not delay audio:
                // it is only a short value crossfade, not a lookahead/buffer.
                transition = juce::jmin(1.0f, transition + 1.0f / 64.0f);
                filtered = transitionStart * (1.0f - transition)
                         + filtered * transition;
            }

            lastOutput = filtered;
            return filtered;
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
        // Independent state for each UDMBC band / channel.
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
        // Hybrid De-Esser detector state:
        // broadband envelope + two high-frequency envelopes.
        float broadbandEnv = 0.f;
        float hfFastEnv = 0.f;
        float hfSlowEnv = 0.f;
        float gainDb = 0.f;
    };

    static void updateAnalogPeak(Biquad& filter, double fs, double f0,
                                 double gainDb, double q);
    static void updateDynamicPeak(TPTBell& filter, double fs, double f0,
                                  double gainDb, double q);
    static void updateEqFilter(EqFilter& filter, int type,
                               double fs, double f0,
                               double gainDb, double q,
                               int slopeIndex);
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
    std::array<EqFilter, 4> dynMidEq {};
    std::array<EqFilter, 4> dynSideEq {};
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

    // One nonlinear ADAA state per Analog band/channel.
    std::array<std::array<VVChain_AnalogADAA_v2, 2>, 4> analogADAA {};
    // Shared smoothed alpha trajectory per band; L/R consume the same value.
    std::array<double, 4> analogAlpha {};
    std::array<bool, 4> analogAlphaInitialized { false, false, false, false };

    // One shared 30 Hz / 12 dB/oct Butterworth high-pass before the
    // ANALOG -> UDMBC -> TAPE band-processing chain. This is intentionally
    // a single filter, not one HPF per module, so no inter-module phase mismatch
    // or additional sample latency is introduced.
    Biquad bandProcessingHighPass {};

    // Reusable scratch for allocation-free four-band ANALOG.
    Crossover4th analogXover1 {};
    Crossover4th analogXover2 {};
    Crossover4th analogXover3 {};
    Crossover4th udmbcXover1 {};
    Crossover4th udmbcXover2 {};
    Crossover4th udmbcXover3 {};

    // Phase-alignment dummy crossovers for the unequal-depth UDMBC branches:
    // Band 1 skips X2/X3; Band 2 skips X3.
    Crossover4th udmbcPhase2_B1 {};
    Crossover4th udmbcPhase3_B1 {};
    Crossover4th udmbcPhase3_B2 {};

    std::array<BandDynamics, 4> udmbcDynamics {};

    // Four independent TAPE exciter bands.
    // TAPE shares the exact same X1/X2/X3 crossover positions and
    // OVERLAP/Q control as UDMBC so the four upper graph bands are the
    // single source of truth for both processors.
    Crossover4th typeXover1 {};
    Crossover4th typeXover2 {};
    Crossover4th typeXover3 {};
    std::array<std::array<float, 2>, 4> typeFastEnv {};
    std::array<std::array<float, 2>, 4> typeSlowEnv {};
    std::array<std::array<float, 2>, 4> typeDc {};

    std::array<DeEssState, 2> deess {};
    Crossover4th deessSplit {};
    float deessLinkedGainDb = 0.f;
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
    Biquad graphSoloPre {};
    Biquad graphSoloPost {};
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
