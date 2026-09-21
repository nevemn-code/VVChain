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
    p.push_back(std::make_unique<juce::AudioParameterBool>("EQ_BYPASS", "EQ / Analog Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("OTT_BYPASS", "OTT Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("ATYPE_BYPASS", "Type-A Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("DEESS_BYPASS", "DeEsser Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("EQ_COLOR_GLOBAL_BYPASS", "Analog Color Global Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("MIX_BYPASS", "Mix / Out Bypass", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>("MASTER_BYPASS", "Master Bypass", false));

    // Four-band analogue-coloured parametric EQ.
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        const float defaults[4] = { 80.f, 350.f, 2500.f, 10000.f };
        f("EQ" + n + "_FREQ", "EQ " + n + " Frequency", 20.f, 20000.f, defaults[i], 0.25f);
        f("EQ" + n + "_GAIN", "EQ " + n + " Gain", -24.f, 24.f, 0.f);
        f("EQ" + n + "_Q", "EQ " + n + " Q", 0.10f, 18.f, 0.707f, 0.35f);
    }
    // Legacy EQ_COLOR remains for old presets; the active UI/DSP uses one color per band.
    f("EQ_COLOR", "Legacy EQ Analog Color", 0.f, 100.f, 35.f);
    for (int i = 0; i < 4; ++i)
    {
        const auto n = juce::String(i + 1);
        f("EQ_COLOR" + n, "EQ " + n + " Analog Color", 0.f, 100.f, 35.f);
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            "EQ_COLOR_BYPASS" + n, "EQ " + n + " Analog Color Bypass", false));
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            "EQ_COLOR_MODE" + n, "EQ " + n + " Analog Mode SS", false));
    }
    f("HF_CORNER", "EQ High-pass Corner", 40.f, 120.f, 70.f);

    // Four-band OTT / PunkOTT-MB style controls.
    f("OTT_INPUT", "OTT Input Gain", -24.f, 24.f, 0.f);
    f("OTT_GATE", "OTT Gate", -90.f, 0.f, -80.f);
    f("OTT_MIX", "OTT Mix", 0.f, 100.f, 25.f);
    p.push_back(std::make_unique<juce::AudioParameterBool>("OTT_CLIPPER", "OTT Clipper", false));
    f("OTT_OUTPUT", "OTT Output Gain", -24.f, 24.f, 0.f);
    f("OTT_X1", "OTT Crossover 1", 80.f, 600.f, 90.f, 1.5f);
    f("OTT_X2", "OTT Crossover 2", 750.f, 3000.f, 2500.f, 0.8f);
    f("OTT_X3", "OTT Crossover 3", 6000.f, 12000.f, 7000.f, 0.65f);
    f("XOVER_OVERLAP", "Shared Crossover Overlap", 0.f, 100.f, 50.f);

    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            "OTT_BAND_BYPASS" + n, "OTT Band " + n + " Bypass", false));
        const float ottDegreeDefaults[4] = { 25.f, 25.f, 22.f, 18.f };
        const float ottLifterMixDefaults[4] = { 70.f, 70.f, 60.f, 50.f };
        f("OTT_DEGREE" + n, "OTT Band " + n + " Degree", 0.f, 100.f, ottDegreeDefaults[i]);
        f("OTT_LIFT_T" + n, "OTT Band " + n + " Lifter Threshold", -80.f, 0.f, -35.f);
        f("OTT_LIFT_A" + n, "OTT Band " + n + " Lifter Attack", 1.f, 500.f, 1.f, 0.35f);
        f("OTT_LIFT_R" + n, "OTT Band " + n + " Lifter Release", 10.f, 2500.f, 80.f, 0.35f);
        f("OTT_LIFT_M" + n, "OTT Band " + n + " Lifter Mix", 0.f, 100.f, ottLifterMixDefaults[i]);
        f("OTT_COMP_T" + n, "OTT Band " + n + " Compressor Threshold", -40.f, 0.f, -24.f);
        f("OTT_COMP_A" + n, "OTT Band " + n + " Compressor Attack", 0.1f, 250.f, 1.f, 0.35f);
        f("OTT_COMP_R" + n, "OTT Band " + n + " Compressor Release", 10.f, 2500.f, 60.f, 0.35f);
        f("OTT_COMP_M" + n, "OTT Band " + n + " Compressor Mix", 0.f, 100.f, 85.f);
        f("OTT_LEVEL" + n, "OTT Band " + n + " Level", -24.f, 12.f, 0.f);
    }

    // Four-band Type-A style dynamic enhancer.
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        const float defaults[4] = { 6.f, 12.f, 28.f, 22.f };
        const float levels[4] = { 0.f, 0.f, 1.f, 1.f };
        p.push_back(std::make_unique<juce::AudioParameterBool>(
            "ATYPE_BAND_BYPASS" + n, "Type-A Band " + n + " Bypass", false));
        f("ATYPE_DEGREE" + n, "Type-A Band " + n + " Degree", 0.f, 100.f, defaults[i]);
        f("ATYPE_LEVEL" + n, "Type-A Band " + n + " Level", -6.f, 6.f, levels[i]);
    }
    f("ATYPE_ATTACK", "Type-A Attack", 1.f, 100.f, 10.f, 0.35f);
    f("ATYPE_RELEASE", "Type-A Release", 20.f, 500.f, 80.f, 0.35f);
    f("ATYPE_INPUT", "Type-A Input Gain", -24.f, 24.f, 0.f);
    f("ATYPE_MIX", "Type-A Mix", 0.f, 100.f, 100.f);
    f("ATYPE_OUTPUT", "Type-A Output Gain", -24.f, 24.f, 0.f);

    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        "SOLO_BAND", "Solo Band",
        juce::StringArray { "OFF", "BAND 1", "BAND 2", "BAND 3", "BAND 4" }, 0));
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        "SOLO_MODE", "Solo Routing",
        juce::StringArray { "PRE", "POST" }, 0));

    // Reference-based DeEsser controls. Frequency is now directly selectable.
    // Reference reference points remain documented at 12.5 kHz / 13.5 kHz;
    // the active processor accepts the full selectable frequency range.
    // DEESS_VOICE is retained for legacy preset compatibility but is no longer
    // used by the realtime processor.
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        "DEESS_VOICE", "Legacy DeEsser Voice",
        juce::StringArray { "Male Vocal", "Female Vocal" }, 0));
    f("DEESS_FREQ", "DeEsser Frequency", 6000.f, 18000.f, 8000.f);
    p.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DEESS_INTENSITY", "DeEsser Maximum Reduction",
        juce::NormalisableRange<float>(0.f, 8.f, 0.1f), 3.f));
    f("DEESS_OFFSET", "DeEsser Average Offset", -0.1f, 0.1f, 0.f);

    f("DRY_WET", "Dry / Wet", 0.f, 100.f, 100.f);
    f("OUTPUT_LEVEL", "Output Level", -24.f, 12.f, 0.f);

    return { p.begin(), p.end() };
}

void VVChainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples(0);
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

    VVChainDSP::Parameters p;

    auto value = [this](const juce::String& id)
    {
        return apvts.getRawParameterValue(id)->load();
    };

    p.eqBypass = value("EQ_BYPASS") > 0.5f;
    p.masterBypass = value("MASTER_BYPASS") > 0.5f;
    p.ottBypass = value("OTT_BYPASS") > 0.5f;
    p.atypeBypass = value("ATYPE_BYPASS") > 0.5f;
    p.deessBypass = value("DEESS_BYPASS") > 0.5f;
    p.mixBypass = value("MIX_BYPASS") > 0.5f;
    p.eqColorGlobalBypass = value("EQ_COLOR_GLOBAL_BYPASS") > 0.5f;

    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        p.freq[(size_t)i] = value("EQ" + n + "_FREQ");
        p.gain[(size_t)i] = value("EQ" + n + "_GAIN");
        p.q[(size_t)i] = value("EQ" + n + "_Q");
        p.eqColor[(size_t)i] = value("EQ_COLOR" + n);
        p.eqColorBypass[(size_t)i] = value("EQ_COLOR_BYPASS" + n) > 0.5f;
        p.eqColorSolidState[(size_t)i] =
            value("EQ_COLOR_MODE" + n) > 0.5f;

        p.ottBandBypass[(size_t)i] = value("OTT_BAND_BYPASS" + n) > 0.5f;
        p.ottDegree[(size_t)i] = value("OTT_DEGREE" + n);
        p.ottLifterThreshold[(size_t)i] = value("OTT_LIFT_T" + n);
        p.ottLifterAttack[(size_t)i] = value("OTT_LIFT_A" + n);
        p.ottLifterRelease[(size_t)i] = value("OTT_LIFT_R" + n);
        p.ottLifterMix[(size_t)i] = value("OTT_LIFT_M" + n);

        p.ottCompThreshold[(size_t)i] = value("OTT_COMP_T" + n);
        p.ottCompAttack[(size_t)i] = value("OTT_COMP_A" + n);
        p.ottCompRelease[(size_t)i] = value("OTT_COMP_R" + n);
        p.ottCompMix[(size_t)i] = value("OTT_COMP_M" + n);
        p.ottBandLevelDb[(size_t)i] = value("OTT_LEVEL" + n);

        p.atypeBandBypass[(size_t)i] = value("ATYPE_BAND_BYPASS" + n) > 0.5f;
        p.atypeDegree[(size_t)i] = value("ATYPE_DEGREE" + n);
        p.atypeBandLevelDb[(size_t)i] = value("ATYPE_LEVEL" + n);
    }


    p.ottInputGainDb = value("OTT_INPUT");
    p.ottGateThresholdDb = value("OTT_GATE");
    p.ottMix = value("OTT_MIX");
    p.ottX1 = value("OTT_X1");
    p.ottX2 = value("OTT_X2");
    p.ottX3 = value("OTT_X3");
    p.ottXoverOverlap = value("XOVER_OVERLAP");
    p.ottOutputGainDb = value("OTT_OUTPUT");
    p.ottClipper = value("OTT_CLIPPER") > 0.5f;

    p.atypeAttackMs = value("ATYPE_ATTACK");
    p.atypeReleaseMs = value("ATYPE_RELEASE");
    p.atypeInputGainDb = value("ATYPE_INPUT");
    p.atypeMix = value("ATYPE_MIX");
    p.atypeOutputGainDb = value("ATYPE_OUTPUT");

    p.soloBand = static_cast<int>(juce::roundToInt(value("SOLO_BAND"))) - 1;
    p.soloPost = value("SOLO_MODE") > 0.5f;

    p.deessReferenceHz = value("DEESS_FREQ");
    p.deessIntensity = value("DEESS_INTENSITY");
    p.deessAverageOffset = value("DEESS_OFFSET");

    p.dryWet = value("DRY_WET");
    p.outputDb = value("OUTPUT_LEVEL");

    dsp.process(buffer, p);

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
