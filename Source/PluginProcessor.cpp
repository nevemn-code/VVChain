#include "PluginProcessor.h"
#include "PluginEditor.h"

VVChainAudioProcessor::VVChainAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout VVChainAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto f = [&p](const juce::String& id, const juce::String& name,
                  float lo, float hi, float def, float skew = 1.0f)
    {
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            id, name, juce::NormalisableRange<float>(lo, hi, 0.01f, skew), def));
    };

    // Every chain section has a real DSP bypass parameter.
    p.push_back(std::make_unique<juce::AudioParameterBool>("EQ_BYPASS", "EQ Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("UDMBC_BYPASS", "UDMBC Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("TAPE_BYPASS", "TAPE Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("DEESS_BYPASS", "DeEsser Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("DELTA_MONITOR", "Delta Monitor", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("EQ_COLOR_GLOBAL_BYPASS", "Analog Color Global Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("MIX_BYPASS", "Mix / Out Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("MASTER_BYPASS", "Master Bypass", false));

    // Four-band analogue-coloured parametric EQ.
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        const float defaults[4] = { 80.f, 350.f, 2500.f, 10000.f };
        f("EQ" + n + "_FREQ", "EQ " + n + " Frequency", 20.f, 20000.f, defaults[i], 0.25f);
        f("EQ" + n + "_GAIN", "EQ " + n + " Gain", -18.f, 18.f, 0.f);
        f("EQ" + n + "_Q", "EQ " + n + " Q", 0.10f, 18.f, 0.707f, 0.35f);
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            "EQ" + n + "_TYPE", "EQ " + n + " Filter Type",
            juce::StringArray {
                "Parametric Bell",
                "Matched Bell",
                "Wide Plateau",
                "Steep Plateau 72",
                "Low Shelf",
                "High Shelf",
                "Low Shelf + Res",
                "High Shelf + Res",
                "Low Contour",
                "High Contour",
                "Focus Pass",
                "Deep Reject",
                "HF Roll-Off",
                "LF Roll-Off"
            }, 0));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            "EQ" + n + "_SLOPE", "EQ " + n + " Roll-Off Slope",
            juce::StringArray {
                "6 dB/oct", "12 dB/oct", "24 dB/oct",
                "36 dB/oct", "48 dB/oct", "60 dB/oct", "72 dB/oct"
            }, 1));

        const float dynTargetDefaults[4] = { 18.f, 18.f, 18.f, 18.f };
        const float dynDynamicsDefaults[4] = { 0.f, 0.f, 0.f, 0.f };
        const float dynAttackDefaults[4] = { 8.f, 8.f, 5.f, 3.f };
        const float dynReleaseDefaults[4] = { 120.f, 120.f, 100.f, 80.f };

        f("DYN_TARGET" + n, "Dynamic EQ " + n + " Target Gain",
          -18.f, 18.f, dynTargetDefaults[i]);
        f("DYN_DYNAMICS" + n, "Dynamic EQ " + n + " Dynamics",
          -100.f, 100.f, dynDynamicsDefaults[i]);
        f("DYN_ATTACK" + n, "Dynamic EQ " + n + " Attack",
          0.1f, 200.f, dynAttackDefaults[i], 0.35f);
        f("DYN_RELEASE" + n, "Dynamic EQ " + n + " Release",
          5.f, 2000.f, dynReleaseDefaults[i], 0.35f);
        f("DYN_DETECT_ONSETS" + n, "Dynamic EQ " + n + " Peak Onsets Blend",
          0.f, 100.f, 50.f);
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            "DYN_TRIGGER_BELOW" + n, "Dynamic EQ " + n + " Trigger Below", false));
        f("DYN_MS" + n, "Dynamic EQ " + n + " Mid Weight",
          0.f, 100.f, 50.f);
    }
    // Legacy EQ_COLOR remains for old presets; the active UI/DSP uses one color per band.
    f("EQ_COLOR", "Legacy EQ Analog Color", 0.f, 100.f, 35.f);
    for (int i = 0; i < 4; ++i)
    {
        const auto n = juce::String(i + 1);
        f("EQ_COLOR" + n, "EQ " + n + " Analog Color", 0.f, 60.f, 0.f);
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            "EQ_COLOR_BYPASS" + n, "EQ " + n + " Analog Color Bypass", false));
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            "EQ_COLOR_MODE" + n, "EQ " + n + " Analog Mode SS", false));
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            "EQ_COLOR_X2" + n, "EQ " + n + " Analog Color X2", false));
    }
    f("HF_CORNER", "EQ High-pass Corner", 40.f, 120.f, 70.f);

    // Four-band UDMBC / UDMBC style controls.
    f("UDMBC_INPUT", "UDMBC Input Gain", -24.f, 24.f, 0.f);
    f("UDMBC_GATE", "UDMBC Gate", -90.f, 0.f, -80.f);
    f("UDMBC_MIX", "UDMBC Mix", 0.f, 100.f, 25.f);
    p.push_back(std::make_unique<juce::AudioParameterBool>("UDMBC_CLIPPER", "UDMBC Clipper", false));
    f("UDMBC_OUTPUT", "UDMBC Output Gain", -24.f, 24.f, 0.f);
    f("UDMBC_X1", "UDMBC Crossover 1", 80.f, 600.f, 90.f, 1.5f);
    f("UDMBC_X2", "UDMBC Crossover 2", 750.f, 3000.f, 2500.f, 0.8f);
    f("UDMBC_X3", "UDMBC Crossover 3", 6000.f, 12000.f, 7000.f, 0.65f);
    f("XOVER_OVERLAP", "Shared Crossover Overlap", 0.f, 100.f, 50.f);

    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            "UDMBC_BAND_BYPASS" + n, "UDMBC Band " + n + " Bypass", false));
        const float udmbcDegreeDefaults[4] = { 0.f, 0.f, 0.f, 0.f };
        const float udmbcLifterMixDefaults[4] = { 70.f, 70.f, 60.f, 50.f };
        f("UDMBC_DEGREE" + n, "UDMBC Band " + n + " Degree", 0.f, 100.f, udmbcDegreeDefaults[i]);
        f("UDMBC_LIFT_T" + n, "UDMBC Band " + n + " Lifter Threshold", -80.f, 0.f, -35.f);
        f("UDMBC_LIFT_A" + n, "UDMBC Band " + n + " Lifter Attack", 1.f, 500.f, 1.f, 0.35f);
        f("UDMBC_LIFT_R" + n, "UDMBC Band " + n + " Lifter Release", 10.f, 2500.f, 80.f, 0.35f);
        f("UDMBC_LIFT_M" + n, "UDMBC Band " + n + " Lifter Mix", 0.f, 100.f, udmbcLifterMixDefaults[i]);
        f("UDMBC_COMP_T" + n, "UDMBC Band " + n + " Compressor Threshold", -40.f, 0.f, -24.f);
        const float udmbcAttackDefaults[4] = { 15.f, 8.f, 3.f, 1.f };
        f("UDMBC_COMP_A" + n, "UDMBC Band " + n + " Compressor Attack", 0.1f, 120.f, udmbcAttackDefaults[i], 0.35f);
        f("UDMBC_COMP_R" + n, "UDMBC Band " + n + " Compressor Release", 10.f, 2500.f, 60.f, 0.35f);
        f("UDMBC_COMP_M" + n, "UDMBC Band " + n + " Compressor Mix", 0.f, 100.f, 85.f);
        f("UDMBC_LEVEL" + n, "UDMBC Band " + n + " Level", -24.f, 12.f, 0.f);
    }

    // Four-band TAPE style dynamic enhancer.
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        const float defaults[4] = { 0.f, 0.f, 0.f, 0.f };
        const float levels[4] = { 0.f, 0.f, 1.f, 1.f };
        const float maxDegrees[4] = { 50.f, 60.f, 70.f, 90.f };
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            "TAPE_BAND_BYPASS" + n, "TAPE Band " + n + " Bypass", false));
        f("TAPE_DEGREE" + n, "TAPE Band " + n + " Degree",
          0.f, maxDegrees[i], defaults[i]);
        f("TAPE_LEVEL" + n, "TAPE Band " + n + " Level", -6.f, 6.f, levels[i]);
    }
    f("TAPE_ATTACK", "TAPE Attack", 1.f, 100.f, 1.f, 0.35f);
    f("TAPE_RELEASE", "TAPE Release", 20.f, 500.f, 20.f, 0.35f);
    f("TAPE_INPUT", "TAPE Input Gain", -24.f, 24.f, 0.f);
    f("TAPE_MIX", "TAPE Mix", 0.f, 100.f, 100.f);
    f("TAPE_OUTPUT", "TAPE Output Gain", -24.f, 24.f, 0.f);

    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        "SOLO_BAND", "Solo Band",
        juce::StringArray { "OFF", "BAND 1", "BAND 2", "BAND 3", "BAND 4" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        "SOLO_MODE", "Solo Routing",
        juce::StringArray { "PRE", "POST" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterBool>(
        "GRAPH_SOLO_ACTIVE", "Graph Frequency Solo Active", false));
    f("GRAPH_SOLO_FREQ", "Graph Frequency Solo Frequency",
      20.f, 20000.f, 1000.f, 0.25f);
    f("GRAPH_SOLO_Q", "Graph Frequency Solo Q",
      0.10f, 18.f, 0.707f, 0.35f);

    // Reference-based DeEsser controls. Frequency is now directly selectable.
    // Reference reference points remain documented at 12.5 kHz / 13.5 kHz;
    // the active processor accepts the full selectable frequency range.
    // DEESS_VOICE is retained for legacy preset compatibility but is no longer
    // used by the realtime processor.
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        "DEESS_VOICE", "Legacy DeEsser Voice",
        juce::StringArray { "Male Vocal", "Female Vocal" }, 0));
    f("DEESS_FREQ", "DeEsser Frequency", 6000.f, 18000.f, 7500.f);
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESS_THRESHOLD", "DeEsser Threshold",
        juce::NormalisableRange<float>(-36.f, 0.f, 0.1f), 0.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESS_MODE", "DeEsser Response",
        juce::NormalisableRange<float>(1.f, 4.f, 1.f), 2.f));
    f("DEESS_OFFSET", "DeEsser Average Offset", -0.1f, 0.1f, 0.f);

    f("DRY_WET", "Dry / Wet", 0.f, 100.f, 100.f);
    f("OUTPUT_LEVEL", "Output Level", -24.f, 12.f, 0.f);

    return { p.begin(), p.end() };
}

void VVChainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples(dsp.getLatencySamples());
}

bool VVChainAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    return (mainIn == juce::AudioChannelSet::mono()
        || mainIn == juce::AudioChannelSet::stereo())
        && mainOut == mainIn;
}

void VVChainAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midi);

    if (analyzerEnabled.load(std::memory_order_relaxed))
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmax(1, buffer.getNumChannels());
        const auto regions = analyzerFifo.write(numSamples);
        int written = 0;

        const auto writeRegion = [&](int start, int size)
        {
            for (int i = 0; i < size; ++i)
            {
                float mono = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    mono += buffer.getReadPointer(ch)[written + i];
                analyzerBuffer[(size_t)(start + i)] =
                    mono / static_cast<float>(numChannels);
            }
            written += size;
        };

        writeRegion(regions.startIndex1, regions.blockSize1);
        writeRegion(regions.startIndex2, regions.blockSize2);
    }

    VVChainDSP::Parameters p;

    auto value = [this](const juce::String& id)
    {
        return apvts.getRawParameterValue(id)->load();
    };

    p.eqBypass = value("EQ_BYPASS") > 0.5f;
    p.masterBypass = value("MASTER_BYPASS") > 0.5f;
    p.udmbcBypass = value("UDMBC_BYPASS") > 0.5f;
    p.tapeBypass = value("TAPE_BYPASS") > 0.5f;
    p.deessBypass = value("DEESS_BYPASS") > 0.5f;
    p.mixBypass = value("MIX_BYPASS") > 0.5f;
    p.eqColorGlobalBypass = value("EQ_COLOR_GLOBAL_BYPASS") > 0.5f;

    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        p.freq[(size_t)i] = value("EQ" + n + "_FREQ");
        p.gain[(size_t)i] = value("EQ" + n + "_GAIN");
        p.q[(size_t)i] = value("EQ" + n + "_Q");
        p.eqType[(size_t)i] = juce::jlimit(
            0, 13,
            juce::roundToInt(value("EQ" + n + "_TYPE")));
        // Removed user-facing legacy shapes keep their numeric slots so
        // existing host automation indices do not shift.
        if (p.eqType[(size_t)i] == 3)
            p.eqType[(size_t)i] = 2; // Steep Plateau -> Wide Plateau
        else if (p.eqType[(size_t)i] == 1
              || p.eqType[(size_t)i] == 10
              || p.eqType[(size_t)i] == 11)
            p.eqType[(size_t)i] = 0; // removed shapes -> Parametric Bell
        p.eqSlope[(size_t)i] = juce::jlimit(
            0, 6,
            juce::roundToInt(value("EQ" + n + "_SLOPE")));
        // Bands 2/3 intentionally do not expose LP/HP cut modes.
        if ((i == 1 || i == 2) && p.eqType[(size_t)i] >= 12)
            p.eqType[(size_t)i] = 0;
        p.dynTarget[(size_t)i] = value("DYN_TARGET" + n);
        p.dynDynamics[(size_t)i] = value("DYN_DYNAMICS" + n);
        p.dynAttack[(size_t)i] = value("DYN_ATTACK" + n);
        p.dynRelease[(size_t)i] = value("DYN_RELEASE" + n);
        p.dynDetectOnsets[(size_t)i] = value("DYN_DETECT_ONSETS" + n);
        p.dynTriggerBelow[(size_t)i] = value("DYN_TRIGGER_BELOW" + n) > 0.5f;
        p.dynMSBalance[(size_t)i] = value("DYN_MS" + n);
        p.eqColor[(size_t)i] = value("EQ_COLOR" + n);
        p.eqColorBypass[(size_t)i] = value("EQ_COLOR_BYPASS" + n) > 0.5f;
        p.eqColorSolidState[(size_t)i] =
            value("EQ_COLOR_MODE" + n) > 0.5f;
        p.eqColorX2[(size_t)i] =
            value("EQ_COLOR_X2" + n) > 0.5f;

        p.udmbcBandBypass[(size_t)i] = value("UDMBC_BAND_BYPASS" + n) > 0.5f;
        p.udmbcDegree[(size_t)i] = value("UDMBC_DEGREE" + n);
        p.udmbcLifterThreshold[(size_t)i] = value("UDMBC_LIFT_T" + n);
        p.udmbcLifterAttack[(size_t)i] = value("UDMBC_LIFT_A" + n);
        p.udmbcLifterRelease[(size_t)i] = value("UDMBC_LIFT_R" + n);
        p.udmbcLifterMix[(size_t)i] = value("UDMBC_LIFT_M" + n);

        p.udmbcCompThreshold[(size_t)i] = value("UDMBC_COMP_T" + n);
        p.udmbcCompAttack[(size_t)i] = value("UDMBC_COMP_A" + n);
        p.udmbcCompRelease[(size_t)i] = value("UDMBC_COMP_R" + n);
        p.udmbcCompMix[(size_t)i] = value("UDMBC_COMP_M" + n);
        p.udmbcBandLevelDb[(size_t)i] = value("UDMBC_LEVEL" + n);

        p.tapeBandBypass[(size_t)i] = value("TAPE_BAND_BYPASS" + n) > 0.5f;
        p.tapeDegree[(size_t)i] = value("TAPE_DEGREE" + n);
        p.tapeBandLevelDb[(size_t)i] = value("TAPE_LEVEL" + n);
    }


    p.udmbcInputGainDb = value("UDMBC_INPUT");
    p.udmbcGateThresholdDb = value("UDMBC_GATE");
    p.udmbcMix = value("UDMBC_MIX");
    p.udmbcX1 = value("UDMBC_X1");
    p.udmbcX2 = value("UDMBC_X2");
    p.udmbcX3 = value("UDMBC_X3");
    p.udmbcXoverOverlap = value("XOVER_OVERLAP");
    p.udmbcOutputGainDb = value("UDMBC_OUTPUT");
    p.udmbcClipper = value("UDMBC_CLIPPER") > 0.5f;

    p.tapeAttackMs = value("TAPE_ATTACK");
    p.tapeReleaseMs = value("TAPE_RELEASE");
    p.tapeInputGainDb = value("TAPE_INPUT");
    p.tapeMix = value("TAPE_MIX");
    p.tapeOutputGainDb = value("TAPE_OUTPUT");

    p.soloBand = static_cast<int>(juce::roundToInt(value("SOLO_BAND"))) - 1;
    p.soloPost = value("SOLO_MODE") > 0.5f;
    p.graphSoloActive = value("GRAPH_SOLO_ACTIVE") > 0.5f;
    p.graphSoloFreq = value("GRAPH_SOLO_FREQ");
    p.graphSoloQ = value("GRAPH_SOLO_Q");

    p.deessReferenceHz = value("DEESS_FREQ");
    p.deessThresholdDb = value("DEESS_THRESHOLD");
    p.deessMode = value("DEESS_MODE");
    p.deessAverageOffset = value("DEESS_OFFSET");
    p.deltaMonitor = value("DELTA_MONITOR") > 0.5f;

    p.dryWet = value("DRY_WET");
    p.outputDb = value("OUTPUT_LEVEL");

    const bool analyzerOn =
        analyzerEnabled.load(std::memory_order_relaxed);

    dsp.setContributionAnalysisEnabled(
        analyzerOn && !p.masterBypass);

    dsp.process(buffer, p);

    if (analyzerOn && !p.masterBypass)
    {
        const int numSamples = buffer.getNumSamples();
        const auto regions = contributionFifo.write(numSamples);
        int written = 0;

        const auto writeRegion = [&](int start, int size)
        {
            for (int stream = 0; stream < 6; ++stream)
            {
                const auto& source = dsp.contributionStream(stream);
                const auto* in = source.getReadPointer(0);

                std::copy_n(
                    in + written,
                    size,
                    contributionBuffers[(size_t)stream].data() + start);
            }

            written += size;
        };

        writeRegion(regions.startIndex1, regions.blockSize1);
        writeRegion(regions.startIndex2, regions.blockSize2);
    }

}

int VVChainAudioProcessor::popContributionSamples(
    const std::array<float*, 6>& destinations,
    int maxSamples) noexcept
{
    if (maxSamples <= 0)
        return 0;

    for (auto* destination : destinations)
        if (destination == nullptr)
            return 0;

    const int available =
        juce::jmin(maxSamples, contributionFifo.getNumReady());
    const auto regions = contributionFifo.read(available);
    int copied = 0;

    const auto copyRegion = [&](int start, int size)
    {
        if (size <= 0)
            return;

        for (int stream = 0; stream < 6; ++stream)
        {
            std::copy_n(
                contributionBuffers[(size_t)stream].data() + start,
                size,
                destinations[(size_t)stream] + copied);
        }

        copied += size;
    };

    copyRegion(regions.startIndex1, regions.blockSize1);
    copyRegion(regions.startIndex2, regions.blockSize2);
    return copied;
}

int VVChainAudioProcessor::popAnalyzerSamples(float* destination,
                                               int maxSamples) noexcept
{
    if (destination == nullptr || maxSamples <= 0)
        return 0;

    const int available = juce::jmin(maxSamples, analyzerFifo.getNumReady());
    const auto regions = analyzerFifo.read(available);
    int copied = 0;

    const auto copyRegion = [&](int start, int size)
    {
        if (size > 0)
        {
            std::copy_n(analyzerBuffer.data() + start, size,
                        destination + copied);
            copied += size;
        }
    };

    copyRegion(regions.startIndex1, regions.blockSize1);
    copyRegion(regions.startIndex2, regions.blockSize2);
    return copied;
}


void VVChainAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VVChainAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* VVChainAudioProcessor::createEditor()
{
    return new VVChainAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VVChainAudioProcessor();
}
