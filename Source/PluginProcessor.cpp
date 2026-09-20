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

    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            "EQ" + n + "_FREQ", "EQ " + n + " Freq",
            juce::NormalisableRange<float>(20.f, 20000.f, 0.01f, 0.25f),
            80.f * std::pow(3.0f, (float)i)));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            "EQ" + n + "_GAIN", "EQ " + n + " Gain",
            juce::NormalisableRange<float>(-24.f, 24.f, 0.01f), 0.f));

        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            "EQ" + n + "_Q", "EQ " + n + " Q",
            juce::NormalisableRange<float>(0.10f, 18.f, 0.001f, 0.35f), 0.707f));
    }

    auto f = [&p](const juce::String& id, const juce::String& name,
                  float lo, float hi, float def, float skew = 1.0f)
    {
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            id, name, juce::NormalisableRange<float>(lo, hi, 0.01f, skew), def));
    };

    f("EQ_COLOR", "Analog Color", 0.f, 100.f, 35.f);
    f("HF_CORNER", "HF / HPF", 40.f, 120.f, 70.f);

    for (int i = 0; i < 4; ++i)
        f("OTT_B" + juce::String(i + 1), "OTT Band " + juce::String(i + 1), 0.f, 100.f, 50.f);

    f("OTT_MIX", "OTT Mix", 0.f, 100.f, 50.f);
    f("OTT_THRESHOLD", "OTT Threshold", -60.f, 0.f, -24.f);
    f("OTT_UP_RATIO", "OTT Up Ratio", 1.f, 8.f, 4.f);
    f("OTT_DOWN_RATIO", "OTT Down Ratio", 1.f, 80.f, 20.f);
    f("OTT_ATTACK", "OTT Attack", 0.2f, 50.f, 2.5f, 0.35f);
    f("OTT_RELEASE", "OTT Release", 10.f, 500.f, 80.f, 0.35f);
    f("OTT_X1", "OTT Xover 1", 60.f, 900.f, 88.f, 0.35f);
    f("OTT_X2", "OTT Xover 2", 600.f, 5000.f, 2500.f, 0.35f);
    f("OTT_X3", "OTT Xover 3", 2500.f, 12000.f, 8500.f, 0.35f);
    f("OTT_INPUT", "OTT Input Gain", -12.f, 12.f, 5.2f);
    f("OTT_POST", "OTT Post Gain", -18.f, 18.f, 0.f);

    const float typeAmountDefaults[4] = { 0.f, 20.f, 70.f, 55.f };
    const float typeGainDefaults[4] = { 0.f, 0.f, 1.f, 1.f };

    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        f("ATYPE_B" + n, "Type-A Band " + n, 0.f, 100.f, typeAmountDefaults[i]);
        f("ATYPE_GAIN" + n, "Type-A Gain " + n, -6.f, 6.f, typeGainDefaults[i]);
    }

    f("ATYPE_ATTACK", "Type-A Attack", 1.f, 100.f, 10.f, 0.35f);
    f("ATYPE_RELEASE", "Type-A Release", 20.f, 500.f, 120.f, 0.35f);
    f("ATYPE_MIX", "Type-A Mix", 0.f, 100.f, 100.f);

    f("DEESS_LOW", "De-Esser Low Xover", 2500.f, 9000.f, 4500.f, 0.35f);
    f("DEESS_HIGH", "De-Esser High Xover", 6000.f, 15000.f, 10500.f, 0.35f);
    f("DEESS_RANGE", "De-Esser Range", 0.f, 24.f, 10.f);
    f("DEESS_STRENGTH", "De-Esser Strength", 0.f, 100.f, 75.f);
    f("DEESS_ATTACK", "De-Esser Attack", 0.1f, 20.f, 1.f, 0.35f);
    f("DEESS_RELEASE", "De-Esser Release", 10.f, 300.f, 80.f, 0.35f);
    p.push_back(std::make_unique<juce::AudioParameterBool>("DEESS_LISTEN", "De-Esser Listen", false));

    f("DRY_WET", "Dry / Wet", 0.f, 100.f, 100.f);
    f("OUTPUT_LEVEL", "Output Level", -24.f, 12.f, 0.f);

    return { p.begin(), p.end() };
}

void VVChainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

bool VVChainAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    return (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo())
        && mainOut == mainIn;
}

void VVChainAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midi);

    VVChainDSP::Parameters p;

    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        p.freq[(size_t)i] = apvts.getRawParameterValue("EQ" + n + "_FREQ")->load();
        p.gain[(size_t)i] = apvts.getRawParameterValue("EQ" + n + "_GAIN")->load();
        p.q[(size_t)i] = apvts.getRawParameterValue("EQ" + n + "_Q")->load();

        p.ottAmount[(size_t)i] = apvts.getRawParameterValue("OTT_B" + n)->load();
        p.atypeAmount[(size_t)i] = apvts.getRawParameterValue("ATYPE_B" + n)->load();
        p.atypeGainDb[(size_t)i] = apvts.getRawParameterValue("ATYPE_GAIN" + n)->load();
    }

    auto value = [this](const juce::String& id)
    {
        return apvts.getRawParameterValue(id)->load();
    };

    p.eqColor = value("EQ_COLOR");
    p.hfCornerHz = value("HF_CORNER");

    p.ottMix = value("OTT_MIX");
    p.ottThreshold = value("OTT_THRESHOLD");
    p.ottUpRatio = value("OTT_UP_RATIO");
    p.ottDownRatio = value("OTT_DOWN_RATIO");
    p.ottAttackMs = value("OTT_ATTACK");
    p.ottReleaseMs = value("OTT_RELEASE");
    p.ottX1 = value("OTT_X1");
    p.ottX2 = value("OTT_X2");
    p.ottX3 = value("OTT_X3");
    p.ottInputGainDb = value("OTT_INPUT");
    p.ottPostGainDb = value("OTT_POST");

    p.atypeAttackMs = value("ATYPE_ATTACK");
    p.atypeReleaseMs = value("ATYPE_RELEASE");
    p.atypeMix = value("ATYPE_MIX");

    p.deessLowHz = value("DEESS_LOW");
    p.deessHighHz = value("DEESS_HIGH");
    p.deessRangeDb = value("DEESS_RANGE");
    p.deessStrength = value("DEESS_STRENGTH");
    p.deessAttackMs = value("DEESS_ATTACK");
    p.deessReleaseMs = value("DEESS_RELEASE");
    p.deessListen = apvts.getRawParameterValue("DEESS_LISTEN")->load() > 0.5f;

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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VVChainAudioProcessor();
}
