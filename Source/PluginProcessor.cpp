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

    f("HF_CORNER", "HF Corner", 40.f, 120.f, 70.f);

    f("OTT_DEPTH", "OTT Depth", 0.f, 100.f, 50.f);
    f("OTT_MIX", "OTT Mix", 0.f, 100.f, 50.f);
    f("OTT_THRESHOLD", "OTT Threshold", -60.f, 0.f, -16.f);
    f("OTT_UP_RATIO", "OTT Up Ratio", 1.f, 8.f, 2.f);
    f("OTT_DOWN_RATIO", "OTT Down Ratio", 1.f, 20.f, 4.f);
    f("OTT_ATTACK", "OTT Attack", 0.1f, 30.f, 2.5f, 0.35f);
    f("OTT_RELEASE", "OTT Release", 10.f, 500.f, 80.f, 0.35f);
    f("OTT_X1", "OTT Low/Mid", 60.f, 900.f, 180.f, 0.35f);
    f("OTT_X2", "OTT Mid/High", 600.f, 3500.f, 1400.f, 0.35f);
    f("OTT_X3", "OTT High/Air", 2500.f, 12000.f, 6500.f, 0.35f);
    f("OTT_POST", "OTT Post Gain", -18.f, 18.f, 0.f);

    f("ATYPE_AMOUNT", "A-Type Amount", 0.f, 100.f, 20.f);
    f("ATYPE_DRIVE", "A-Type Drive", 0.f, 24.f, 6.f);
    f("ATYPE_BIAS", "A-Type Bias", -100.f, 100.f, 0.f);
    f("ATYPE_MIX", "A-Type Mix", 0.f, 100.f, 100.f);
    f("ATYPE_TONE", "A-Type Tone", 0.f, 100.f, 70.f);
    f("ATYPE_HPF", "A-Type HPF", 20.f, 1000.f, 60.f, 0.35f);

    f("DEESS_FREQ", "De-Esser Freq", 2000.f, 12000.f, 6500.f, 0.35f);
    f("DEESS_Q", "De-Esser Q", 0.20f, 10.f, 1.2f, 0.35f);
    f("DEESS_THRESHOLD", "De-Esser Threshold", -80.f, 0.f, -30.f);
    f("DEESS_RANGE", "De-Esser Range", 0.f, 24.f, 8.f);
    f("DEESS_ATTACK", "De-Esser Attack", 0.1f, 20.f, 1.f, 0.35f);
    f("DEESS_RELEASE", "De-Esser Release", 10.f, 300.f, 80.f, 0.35f);

    p.push_back(std::make_unique<juce::AudioParameterBool>("DEESS_LISTEN", "De-Esser Listen", false));

    f("DRY_WET", "Dry/Wet", 0.f, 100.f, 100.f);
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
    }

    auto v = [this](const juce::String& id) { return apvts.getRawParameterValue(id)->load(); };

    p.hfCornerHz = v("HF_CORNER");

    p.ottDepth = v("OTT_DEPTH");
    p.ottMix = v("OTT_MIX");
    p.ottThresholdDb = v("OTT_THRESHOLD");
    p.ottUpRatio = v("OTT_UP_RATIO");
    p.ottDownRatio = v("OTT_DOWN_RATIO");
    p.ottAttackMs = v("OTT_ATTACK");
    p.ottReleaseMs = v("OTT_RELEASE");
    p.ottLowMidHz = v("OTT_X1");
    p.ottMidHighHz = v("OTT_X2");
    p.ottHighMidHz = v("OTT_X3");
    p.ottPostGainDb = v("OTT_POST");

    p.atypeAmount = v("ATYPE_AMOUNT");
    p.atypeDriveDb = v("ATYPE_DRIVE");
    p.atypeBias = v("ATYPE_BIAS");
    p.atypeMix = v("ATYPE_MIX");
    p.atypeTone = v("ATYPE_TONE");
    p.atypeHpfHz = v("ATYPE_HPF");

    p.deessFreq = v("DEESS_FREQ");
    p.deessQ = v("DEESS_Q");
    p.deessThreshold = v("DEESS_THRESHOLD");
    p.deessRange = v("DEESS_RANGE");
    p.deessAttackMs = v("DEESS_ATTACK");
    p.deessReleaseMs = v("DEESS_RELEASE");
    p.deessListen = apvts.getRawParameterValue("DEESS_LISTEN")->load() > 0.5f;

    p.dryWet = v("DRY_WET");
    p.outputDb = v("OUTPUT_LEVEL");

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
