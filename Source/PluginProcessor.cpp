#include "PluginProcessor.h"
#include "PluginEditor.h"

VVChainAudioProcessor::VVChainAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createParameterLayout())
{
    for (auto& value : analyzerSpectrumDb)
        value.store(-120.0f, std::memory_order_relaxed);
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

    // Four-band analogue-coloured EQ.
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        const float defaults[4] = { 80.f, 350.f, 2500.f, 10000.f };
        f("EQ" + n + "_FREQ", "EQ " + n + " Frequency", 20.f, 20000.f, defaults[i], 0.25f);
        f("EQ" + n + "_GAIN", "EQ " + n + " Gain", -24.f, 24.f, 0.f);
        f("EQ" + n + "_Q", "EQ " + n + " Q", 0.10f, 18.f, 0.707f, 0.35f);
    }
    f("EQ_COLOR", "EQ Analog Color", 0.f, 100.f, 35.f);
    f("HF_CORNER", "EQ High-pass Corner", 40.f, 120.f, 70.f);

    // Four-band OTT / PunkOTT-MB style controls.
    f("OTT_INPUT", "OTT Input Gain", -24.f, 24.f, 5.2f);
    f("OTT_GATE", "OTT Gate", -90.f, 0.f, -80.f);
    f("OTT_MIX", "OTT Mix", 0.f, 100.f, 100.f);
    p.push_back(std::make_unique<juce::AudioParameterBool>("OTT_CLIPPER", "OTT Clipper", true));
    f("OTT_OUTPUT", "OTT Output Gain", -24.f, 24.f, -6.f);
    f("OTT_X1", "OTT Crossover 1", 80.f, 600.f, 350.f, 1.5f);
    f("OTT_X2", "OTT Crossover 2", 750.f, 3000.f, 1000.f, 0.8f);
    f("OTT_X3", "OTT Crossover 3", 6000.f, 12000.f, 9000.f, 0.65f);

    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        f("OTT_DEGREE" + n, "OTT Band " + n + " Degree", 0.f, 100.f, 100.f);
        f("OTT_LIFT_T" + n, "OTT Band " + n + " Lifter Threshold", -80.f, 0.f, -40.f);
        f("OTT_LIFT_A" + n, "OTT Band " + n + " Lifter Attack", 1.f, 500.f, 50.f, 0.35f);
        f("OTT_LIFT_R" + n, "OTT Band " + n + " Lifter Release", 10.f, 2500.f, 50.f, 0.35f);
        f("OTT_LIFT_M" + n, "OTT Band " + n + " Lifter Mix", 0.f, 100.f, 100.f);
        f("OTT_COMP_T" + n, "OTT Band " + n + " Compressor Threshold", -24.f, 0.f, -12.f);
        f("OTT_COMP_A" + n, "OTT Band " + n + " Compressor Attack", 0.1f, 250.f, 15.f, 0.35f);
        f("OTT_COMP_R" + n, "OTT Band " + n + " Compressor Release", 10.f, 2500.f, 60.f, 0.35f);
        f("OTT_COMP_M" + n, "OTT Band " + n + " Compressor Mix", 0.f, 100.f, 100.f);
        f("OTT_LEVEL" + n, "OTT Band " + n + " Level", -24.f, 12.f, 0.f);
    }

    // Four-band Type-A style dynamic enhancer.
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String(i + 1);
        const float defaults[4] = { 0.f, 20.f, 70.f, 55.f };
        const float levels[4] = { 0.f, 0.f, 1.f, 1.f };
        f("ATYPE_DEGREE" + n, "Type-A Band " + n + " Degree", 0.f, 100.f, defaults[i]);
        f("ATYPE_LEVEL" + n, "Type-A Band " + n + " Level", -6.f, 6.f, levels[i]);
    }
    f("ATYPE_ATTACK", "Type-A Attack", 1.f, 100.f, 10.f, 0.35f);
    f("ATYPE_RELEASE", "Type-A Release", 20.f, 500.f, 120.f, 0.35f);
    f("ATYPE_INPUT", "Type-A Input Gain", -24.f, 24.f, 0.f);
    f("ATYPE_MIX", "Type-A Mix", 0.f, 100.f, 100.f);
    f("ATYPE_OUTPUT", "Type-A Output Gain", -24.f, 24.f, 0.f);

    // DeEsser reference controls. These intentionally mirror the public reference:
    // male/female target choice, intensity and average-threshold offset.
    p.push_back(std::make_unique<juce::AudioParameterChoice>(
        "DEESS_VOICE", "DeEsser Voice",
        juce::StringArray { "Male Vocal", "Female Vocal" }, 0));
    f("DEESS_INTENSITY", "DeEsser Intensity", 2.f, 10.f, 10.f);
    f("DEESS_OFFSET", "DeEsser Threshold Offset", -0.1f, 0.1f, 0.f);

    f("DRY_WET", "Dry / Wet", 0.f, 100.f, 100.f);
    f("OUTPUT_LEVEL", "Output Level", -24.f, 12.f, 0.f);

    return { p.begin(), p.end() };
}

void VVChainAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    analyzerSampleRate = sampleRate;
    analyzerFifoIndex = 0;
    analyzerFftData.fill(0.0f);
    for (auto& value : analyzerSpectrumDb)
        value.store(-120.0f, std::memory_order_relaxed);

    dsp.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    setLatencySamples(8192);
}

bool VVChainAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    return (mainIn == juce::AudioChannelSet::mono()
        || mainIn == juce::AudioChannelSet::stereo())
        && mainOut == mainIn;
}

void VVChainAudioProcessor::pushAnalyzerSamples(const juce::AudioBuffer<float>& buffer) noexcept
{
    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();
    if (channels <= 0 || samples <= 0)
        return;

    const auto* left = buffer.getReadPointer(0);
    const auto* right = channels > 1 ? buffer.getReadPointer(1) : left;

    for (int n = 0; n < samples; ++n)
    {
        analyzerFftData[(size_t)analyzerFifoIndex] = 0.5f * (left[n] + right[n]);
        ++analyzerFifoIndex;

        if (analyzerFifoIndex == kFFTSize)
        {
            for (int i = 0; i < kFFTSize; ++i)
                analyzerFftData[(size_t)i] *= 1.0f;

            analyzerWindow.multiplyWithWindowingTable(analyzerFftData.data(), kFFTSize);
            std::fill(analyzerFftData.begin() + kFFTSize, analyzerFftData.end(), 0.0f);
            analyzerFFT.performRealOnlyForwardTransform(analyzerFftData.data());

            for (int bin = 1; bin < kSpectrumBins; ++bin)
            {
                const float re = analyzerFftData[(size_t)(2 * bin)];
                const float im = analyzerFftData[(size_t)(2 * bin + 1)];
                const float mag = std::sqrt(re * re + im * im) / static_cast<float>(kFFTSize);
                const float db = juce::jlimit(-120.0f, 12.0f,
                    juce::Decibels::gainToDecibels(std::max(mag, 1.0e-9f)));
                analyzerSpectrumDb[(size_t)bin].store(db, std::memory_order_relaxed);
            }
            analyzerSpectrumDb[0].store(-120.0f, std::memory_order_relaxed);
            analyzerFifoIndex = 0;
        }

        // Keep the FFT overlap-free and deterministic.
        if (analyzerFifoIndex >= kFFTSize)
            analyzerFifoIndex = 0;
    }
}

void VVChainAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midi)
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

        p.ottDegree[(size_t)i] = apvts.getRawParameterValue("OTT_DEGREE" + n)->load();
        p.ottLifterThreshold[(size_t)i] = apvts.getRawParameterValue("OTT_LIFT_T" + n)->load();
        p.ottLifterAttack[(size_t)i] = apvts.getRawParameterValue("OTT_LIFT_A" + n)->load();
        p.ottLifterRelease[(size_t)i] = apvts.getRawParameterValue("OTT_LIFT_R" + n)->load();
        p.ottLifterMix[(size_t)i] = apvts.getRawParameterValue("OTT_LIFT_M" + n)->load();

        p.ottCompThreshold[(size_t)i] = apvts.getRawParameterValue("OTT_COMP_T" + n)->load();
        p.ottCompAttack[(size_t)i] = apvts.getRawParameterValue("OTT_COMP_A" + n)->load();
        p.ottCompRelease[(size_t)i] = apvts.getRawParameterValue("OTT_COMP_R" + n)->load();
        p.ottCompMix[(size_t)i] = apvts.getRawParameterValue("OTT_COMP_M" + n)->load();
        p.ottBandLevelDb[(size_t)i] = apvts.getRawParameterValue("OTT_LEVEL" + n)->load();

        p.atypeDegree[(size_t)i] = apvts.getRawParameterValue("ATYPE_DEGREE" + n)->load();
        p.atypeBandLevelDb[(size_t)i] = apvts.getRawParameterValue("ATYPE_LEVEL" + n)->load();
    }

    auto value = [this](const juce::String& id)
    {
        return apvts.getRawParameterValue(id)->load();
    };

    p.eqColor = value("EQ_COLOR");
    p.hfCornerHz = value("HF_CORNER");

    p.ottInputGainDb = value("OTT_INPUT");
    p.ottGateThresholdDb = value("OTT_GATE");
    p.ottMix = value("OTT_MIX");
    p.ottX1 = value("OTT_X1");
    p.ottX2 = value("OTT_X2");
    p.ottX3 = value("OTT_X3");
    p.ottOutputGainDb = value("OTT_OUTPUT");
    p.ottClipper = apvts.getRawParameterValue("OTT_CLIPPER")->load() > 0.5f;

    p.atypeAttackMs = value("ATYPE_ATTACK");
    p.atypeReleaseMs = value("ATYPE_RELEASE");
    p.atypeInputGainDb = value("ATYPE_INPUT");
    p.atypeMix = value("ATYPE_MIX");
    p.atypeOutputGainDb = value("ATYPE_OUTPUT");

    p.deessVoice = static_cast<int>(std::round(value("DEESS_VOICE")));
    p.deessIntensity = value("DEESS_INTENSITY");
    p.deessAverageOffset = value("DEESS_OFFSET");

    p.dryWet = value("DRY_WET");
    p.outputDb = value("OUTPUT_LEVEL");

    dsp.process(buffer, p);
    pushAnalyzerSamples(buffer);
}

void VVChainAudioProcessor::copySpectrumTo(float* destination, int numberOfBins) const noexcept
{
    if (destination == nullptr || numberOfBins <= 0)
        return;

    const int count = std::min(numberOfBins, kSpectrumBins);
    for (int i = 0; i < count; ++i)
        destination[i] = analyzerSpectrumDb[(size_t)i].load(std::memory_order_relaxed);
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
