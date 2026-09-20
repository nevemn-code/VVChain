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
        p.push_back(std::make_unique<juce::AudioParameterFloat>("EQ" + n + "_FREQ", "EQ " + n + " Freq",
            juce::NormalisableRange<float>(20.f, 20000.f, 0.01f, 0.25f), 80.f * std::pow(3.0f, (float)i)));
        p.push_back(std::make_unique<juce::AudioParameterFloat>("EQ" + n + "_GAIN", "EQ " + n + " Gain",
            juce::NormalisableRange<float>(-24.f, 24.f, 0.01f), 0.f));
        p.push_back(std::make_unique<juce::AudioParameterFloat>("EQ" + n + "_Q", "EQ " + n + " Q",
            juce::NormalisableRange<float>(0.10f, 18.f, 0.001f, 0.35f), 0.707f));
    }

    p.push_back(std::make_unique<juce::AudioParameterFloat>("HF_CORNER", "HF Corner",
        juce::NormalisableRange<float>(40.f, 120.f, 0.01f), 70.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("OTT_DEPTH", "OTT Depth",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f), 50.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("OTT_MIX", "OTT Mix",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f), 50.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ATYPE_AMOUNT", "A-Type Amount",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f), 20.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("ATYPE_BIAS", "A-Type Bias",
        juce::NormalisableRange<float>(-100.f, 100.f, 0.01f), 0.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("DEESS_FREQ", "De-Esser Freq",
        juce::NormalisableRange<float>(2000.f, 12000.f, 0.01f, 0.35f), 6500.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("DEESS_THRESHOLD", "De-Esser Threshold",
        juce::NormalisableRange<float>(-80.f, 0.f, 0.01f), -30.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("DEESS_RANGE", "De-Esser Range",
        juce::NormalisableRange<float>(0.f, 24.f, 0.01f), 8.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("DRY_WET", "Dry/Wet",
        juce::NormalisableRange<float>(0.f, 100.f, 0.01f), 100.f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>("OUTPUT_LEVEL", "Output Level",
        juce::NormalisableRange<float>(-24.f, 12.f, 0.01f), 0.f));

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
    }

    p.hfCornerHz = apvts.getRawParameterValue("HF_CORNER")->load();
    p.ottDepth = apvts.getRawParameterValue("OTT_DEPTH")->load();
    p.ottMix = apvts.getRawParameterValue("OTT_MIX")->load();
    p.atypeAmount = apvts.getRawParameterValue("ATYPE_AMOUNT")->load();
    p.atypeBias = apvts.getRawParameterValue("ATYPE_BIAS")->load();
    p.deessFreq = apvts.getRawParameterValue("DEESS_FREQ")->load();
    p.deessThreshold = apvts.getRawParameterValue("DEESS_THRESHOLD")->load();
    p.deessRange = apvts.getRawParameterValue("DEESS_RANGE")->load();
    p.dryWet = apvts.getRawParameterValue("DRY_WET")->load();
    p.outputDb = apvts.getRawParameterValue("OUTPUT_LEVEL")->load();

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
