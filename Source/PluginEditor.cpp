#include "PluginEditor.h"

namespace
{
const std::array<juce::Colour, 4> bandColours
{
    juce::Colour(0xff39a9ff),
    juce::Colour(0xff35d0bf),
    juce::Colour(0xfff05ce1),
    juce::Colour(0xff9bde4d)
};

float logMap(float value, float min, float max)
{
    return std::log(std::max(value, min) / min) / std::log(max / min);
}

float invLogMap(float t, float min, float max)
{
    return min * std::pow(max / min, juce::jlimit(0.f, 1.f, t));
}

void drawHandle(juce::Graphics& g, juce::Point<float> p, juce::Colour c, bool selected)
{
    if (selected)
    {
        g.setColour(c.withAlpha(0.18f));
        g.fillEllipse(p.x - 15.f, p.y - 15.f, 30.f, 30.f);
    }

    g.setColour(c);
    g.fillEllipse(p.x - 6.5f, p.y - 6.5f, 13.f, 13.f);
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.drawEllipse(p.x - 6.5f, p.y - 6.5f, 13.f, 13.f, 1.f);
}
}

VVChainAudioProcessorEditor::VVChainAudioProcessorEditor(VVChainAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setResizable(true, true);
    setSize(1360, 900);

    const std::array<juce::String, 6> names
    {
        "EQ / ANALOG", "OTT", "TYPE-A", "DE-ESSER", "MIX / OUT", "ANALYZER"
    };

    for (int i = 0; i < 6; ++i)
    {
        moduleButtons[(size_t)i].setButtonText(names[(size_t)i]);
        moduleButtons[(size_t)i].onClick = [this, i] { selectModule(i); };
        addAndMakeVisible(moduleButtons[(size_t)i]);
    }

    const std::array<juce::String, 4> bands
    {
        "BAND 1", "BAND 2", "BAND 3", "BAND 4"
    };

    for (int i = 0; i < 4; ++i)
    {
        bandButtons[(size_t)i].setButtonText(bands[(size_t)i]);
        bandButtons[(size_t)i].onClick = [this, i] { selectBand(i); };
        bandButtons[(size_t)i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff292820));
        addAndMakeVisible(bandButtons[(size_t)i]);
    }

    for (size_t i = 0; i < controls.size(); ++i)
    {
        controls[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        controls[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 88, 20);
        controls[i].setRange(0.0, 1.0, 0.01);
        controls[i].setDoubleClickReturnValue(true, 0.0);
        controlLabels[i].setJustificationType(juce::Justification::centred);
        controlLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffddd8cb));
        addAndMakeVisible(controls[i]);
        addAndMakeVisible(controlLabels[i]);
    }

    deEssVoice.addItem("Male Vocal", 1);
    deEssVoice.addItem("Female Vocal", 2);
    deEssVoiceAttachment = std::make_unique<ComboAttachment>(
        audioProcessor.apvts, "DEESS_VOICE", deEssVoice);
    addAndMakeVisible(deEssVoice);

    addAndMakeVisible(ottClipper);
    addAndMakeVisible(analyzerAverage);
    addAndMakeVisible(analyzerPeak);
    addAndMakeVisible(analyzerPersistence);
    addAndMakeVisible(analyzerSmooth);

    analyzerAverage.setToggleState(true, juce::dontSendNotification);
    analyzerPeak.setToggleState(true, juce::dontSendNotification);
    analyzerSmooth.setToggleState(true, juce::dontSendNotification);

    selectModule(0);
    selectBand(0);
    analyzerSmoothed.fill(-120.0f);
    peakSpectrum.fill(-120.0f);
    for (auto& row : waterfall)
        row.fill(-120.0f);

    startTimerHz(30);
}

void VVChainAudioProcessorEditor::selectModule(int index)
{
    moduleIndex = juce::jlimit(0, 5, index);

    for (int i = 0; i < 6; ++i)
    {
        const bool active = i == moduleIndex;
        moduleButtons[(size_t)i].setColour(
            juce::TextButton::buttonColourId,
            active ? juce::Colour(0xff5f584b) : juce::Colour(0xff2b2a25));
        moduleButtons[(size_t)i].setColour(
            juce::TextButton::textColourOffId,
            active ? juce::Colours::white : juce::Colour(0xffbdb7a9));
    }

    rebuildControls();
    resized();
    repaint();
}

void VVChainAudioProcessorEditor::selectBand(int index)
{
    bandIndex = juce::jlimit(0, 3, index);

    for (int i = 0; i < 4; ++i)
    {
        const bool active = i == bandIndex;
        bandButtons[(size_t)i].setColour(
            juce::TextButton::buttonColourId,
            active ? bandColours[(size_t)i].withAlpha(0.28f) : juce::Colour(0xff292820));
    }

    rebuildControls();
    resized();
    repaint();
}

void VVChainAudioProcessorEditor::hideControl(int index)
{
    if (index < 0 || index >= (int)controls.size())
        return;

    attachments[(size_t)index].reset();
    controls[(size_t)index].setVisible(false);
    controlLabels[(size_t)index].setVisible(false);
}

void VVChainAudioProcessorEditor::setControl(int index, const juce::String& parameterId,
                                             const juce::String& title)
{
    if (index < 0 || index >= (int)controls.size())
        return;

    controls[(size_t)index].setVisible(true);
    controlLabels[(size_t)index].setVisible(true);
    controlLabels[(size_t)index].setText(title, juce::dontSendNotification);
    attachments[(size_t)index] = std::make_unique<Attachment>(
        audioProcessor.apvts, parameterId, controls[(size_t)index]);
}

void VVChainAudioProcessorEditor::setControlRangeForDisplay(int index, double minimum,
                                                            double maximum, double step,
                                                            const juce::String& suffix)
{
    if (index < 0 || index >= (int)controls.size())
        return;

    controls[(size_t)index].setRange(minimum, maximum, step);
    controls[(size_t)index].setTextValueSuffix(suffix);
}

void VVChainAudioProcessorEditor::rebuildControls()
{
    for (int i = 0; i < (int)controls.size(); ++i)
        hideControl(i);

    ottClipper.setVisible(moduleIndex == 1);
    deEssVoice.setVisible(false);

    analyzerAverage.setVisible(moduleIndex == 5);
    analyzerPeak.setVisible(moduleIndex == 5);
    analyzerPersistence.setVisible(moduleIndex == 5);
    analyzerSmooth.setVisible(moduleIndex == 5);

    if (moduleIndex == 0)
    {
        const auto n = juce::String(bandIndex + 1);
        setControl(0, "EQ" + n + "_FREQ", "FREQUENCY");
        setControl(1, "EQ" + n + "_GAIN", "GAIN");
        setControl(2, "EQ" + n + "_Q", "Q");
        setControl(3, "EQ_COLOR", "ANALOG COLOR");
        setControl(4, "HF_CORNER", "HP / CORNER");

        setControlRangeForDisplay(0, 20, 20000, 1, " Hz");
        setControlRangeForDisplay(1, -24, 24, 0.1, " dB");
        setControlRangeForDisplay(2, 0.10, 18, 0.01, "");
        setControlRangeForDisplay(3, 0, 100, 0.1, " %");
        setControlRangeForDisplay(4, 40, 120, 1, " Hz");
    }
    else if (moduleIndex == 1)
    {
        const auto n = juce::String(bandIndex + 1);

        setControl(0, "OTT_DEGREE" + n, "DEGREE");
        setControl(1, "OTT_LIFT_T" + n, "LIFTER THRESH");
        setControl(2, "OTT_LIFT_A" + n, "LIFTER ATT");
        setControl(3, "OTT_LIFT_R" + n, "LIFTER REL");
        setControl(4, "OTT_LIFT_M" + n, "LIFTER MIX");
        setControl(5, "OTT_COMP_T" + n, "COMP THRESH");
        setControl(6, "OTT_COMP_A" + n, "COMP ATT");
        setControl(7, "OTT_COMP_R" + n, "COMP REL");
        setControl(8, "OTT_COMP_M" + n, "COMP MIX");
        setControl(9, "OTT_LEVEL" + n, "BAND LEVEL");
        setControl(10, "OTT_X1", "XOVER 1");
        setControl(11, "OTT_X2", "XOVER 2");
        setControl(12, "OTT_X3", "XOVER 3");
        setControl(13, "OTT_INPUT", "INPUT");
        setControl(14, "OTT_GATE", "GATE");
        setControl(15, "OTT_MIX", "MASTER MIX");
        setControl(16, "OTT_OUTPUT", "OUTPUT");

        setControlRangeForDisplay(0, 0, 100, 0.1, " %");
        setControlRangeForDisplay(1, -80, 0, 0.1, " dB");
        setControlRangeForDisplay(2, 1, 500, 0.1, " ms");
        setControlRangeForDisplay(3, 10, 2500, 1, " ms");
        setControlRangeForDisplay(4, 0, 100, 0.1, " %");
        setControlRangeForDisplay(5, -24, 0, 0.1, " dB");
        setControlRangeForDisplay(6, 0.1, 250, 0.1, " ms");
        setControlRangeForDisplay(7, 10, 2500, 1, " ms");
        setControlRangeForDisplay(8, 0, 100, 0.1, " %");
        setControlRangeForDisplay(9, -24, 12, 0.1, " dB");
        setControlRangeForDisplay(10, 80, 600, 1, " Hz");
        setControlRangeForDisplay(11, 750, 3000, 1, " Hz");
        setControlRangeForDisplay(12, 6000, 12000, 1, " Hz");
        setControlRangeForDisplay(13, -24, 24, 0.1, " dB");
        setControlRangeForDisplay(14, -90, 0, 0.1, " dB");
        setControlRangeForDisplay(15, 0, 100, 0.1, " %");
        setControlRangeForDisplay(16, -24, 24, 0.1, " dB");
    }
    else if (moduleIndex == 2)
    {
        setControl(0, "ATYPE_DEGREE1", "TYPE-A B1");
        setControl(1, "ATYPE_DEGREE2", "TYPE-A B2");
        setControl(2, "ATYPE_DEGREE3", "TYPE-A B3");
        setControl(3, "ATYPE_DEGREE4", "TYPE-A B4");
        setControl(4, "ATYPE_ATTACK", "ATTACK");
        setControl(5, "ATYPE_RELEASE", "RELEASE");
        setControl(6, "ATYPE_INPUT", "INPUT");
        setControl(7, "ATYPE_MIX", "MIX");
        setControl(8, "ATYPE_OUTPUT", "OUTPUT");

        for (int i = 0; i < 4; ++i)
            setControlRangeForDisplay(i, 0, 100, 0.1, " %");
        setControlRangeForDisplay(4, 1, 100, 0.1, " ms");
        setControlRangeForDisplay(5, 20, 500, 1, " ms");
        setControlRangeForDisplay(6, -24, 24, 0.1, " dB");
        setControlRangeForDisplay(7, 0, 100, 0.1, " %");
        setControlRangeForDisplay(8, -24, 24, 0.1, " dB");

        for (int i = 0; i < 4; ++i)
            controlLabels[(size_t)i].setColour(
                juce::Label::textColourId, bandColours[(size_t)i]);
    }
    else if (moduleIndex == 3)
    {
        for (int i = 0; i < 18; ++i)
            hideControl(i);

        for (int i = 0; i < 8; ++i)
        {
            controlLabels[(size_t)i].setText(
                i == 0 ? "FFT SIZE" :
                i == 1 ? "REFERENCE FREQ" :
                i == 2 ? "BUFFER STEP" :
                i == 3 ? "OVERLAP" :
                i == 4 ? "OUTPUT" :
                i == 5 ? "DETECTOR" :
                i == 6 ? "TRIGGER" : "REFERENCE RATE",
                juce::dontSendNotification);
            controlLabels[(size_t)i].setVisible(true);
        }
    }
    else if (moduleIndex == 4)
    {
        setControl(0, "DRY_WET", "DRY / WET");
        setControl(1, "OUTPUT_LEVEL", "OUTPUT");
        setControlRangeForDisplay(0, 0, 100, 0.1, " %");
        setControlRangeForDisplay(1, -24, 12, 0.1, " dB");
    }
}

float VVChainAudioProcessorEditor::parameterValue(const juce::String& id) const
{
    if (auto* parameter = audioProcessor.apvts.getRawParameterValue(id))
        return parameter->load();

    return 0.0f;
}

void VVChainAudioProcessorEditor::setParameter(const juce::String& id, float value)
{
    if (auto* parameter = audioProcessor.apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float VVChainAudioProcessorEditor::graphFrequencyToX(
    const juce::Rectangle<float>& graph, float hz) const
{
    return graph.getX() + graph.getWidth() * logMap(hz, 20.f, 20000.f);
}

float VVChainAudioProcessorEditor::graphXToFrequency(
    const juce::Rectangle<float>& graph, float x) const
{
    const float t = juce::jlimit(0.f, 1.f, (x - graph.getX()) / graph.getWidth());
    return invLogMap(t, 20.f, 20000.f);
}

float VVChainAudioProcessorEditor::graphPercentToY(
    const juce::Rectangle<float>& graph, float percent) const
{
    return graph.getBottom() - graph.getHeight() * juce::jlimit(0.f, 1.f, percent / 100.f);
}

float VVChainAudioProcessorEditor::graphYToPercent(
    const juce::Rectangle<float>& graph, float y) const
{
    return juce::jlimit(
        0.f, 100.f, 100.f * (graph.getBottom() - y) / graph.getHeight());
}

void VVChainAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    const auto graph = juce::Rectangle<float>(24.f, 108.f,
                                               (float)getWidth() - 48.f,
                                               (float)getHeight() - 350.f);

    if (!graph.contains(e.position))
        return;

    dragTarget = DragTarget::None;

    if (moduleIndex == 0)
    {
        float nearest = 18.f;
        for (int i = 0; i < 4; ++i)
        {
            const auto n = juce::String(i + 1);
            const float f = parameterValue("EQ" + n + "_FREQ");
            const float g = parameterValue("EQ" + n + "_GAIN");
            const juce::Point<float> p(
                graphFrequencyToX(graph, f),
                graph.getCentreY() - g / 36.f * graph.getHeight());

            if (p.getDistanceFrom(e.position) < nearest)
            {
                nearest = p.getDistanceFrom(e.position);
                dragTarget = static_cast<DragTarget>(
                    static_cast<int>(DragTarget::EqBand1) + i);
            }
        }
    }
    else if (moduleIndex == 1)
    {
        const float xs[3] = {
            parameterValue("OTT_X1"), parameterValue("OTT_X2"), parameterValue("OTT_X3")
        };

        for (int i = 0; i < 3; ++i)
        {
            const float x = graphFrequencyToX(graph, xs[i]);
            if (std::abs(e.position.x - x) < 12.f)
            {
                dragTarget = static_cast<DragTarget>(
                    static_cast<int>(DragTarget::OttX1) + i);
                return;
            }
        }

        const float centers[4] = {
            std::sqrt(20.f * xs[0]),
            std::sqrt(xs[0] * xs[1]),
            std::sqrt(xs[1] * xs[2]),
            std::sqrt(xs[2] * 18000.f)
        };

        float nearest = 20.f;
        for (int i = 0; i < 4; ++i)
        {
            const float degree = parameterValue("OTT_DEGREE" + juce::String(i + 1));
            const juce::Point<float> p(
                graphFrequencyToX(graph, centers[i]),
                graphPercentToY(graph, degree));

            if (p.getDistanceFrom(e.position) < nearest)
            {
                nearest = p.getDistanceFrom(e.position);
                dragTarget = static_cast<DragTarget>(
                    static_cast<int>(DragTarget::OttDegree1) + i);
                bandIndex = i;
            }
        }
    }
    else if (moduleIndex == 2)
    {
        const float centers[4] = { 40.f, 800.f, 5200.f, 12000.f };
        float nearest = 22.f;

        for (int i = 0; i < 4; ++i)
        {
            const float degree = parameterValue("ATYPE_DEGREE" + juce::String(i + 1));
            const juce::Point<float> p(
                graphFrequencyToX(graph, centers[i]),
                graphPercentToY(graph, degree));

            if (p.getDistanceFrom(e.position) < nearest)
            {
                nearest = p.getDistanceFrom(e.position);
                dragTarget = static_cast<DragTarget>(
                    static_cast<int>(DragTarget::TypeDegree1) + i);
            }
        }
    }
    else if (moduleIndex == 3)
    {
        dragTarget = DragTarget::None;
    }
    else if (moduleIndex == 4)
    {
        const juce::Point<float> pDry(
            graph.getX() + graph.getWidth() * 0.35f,
            graphPercentToY(graph, parameterValue("DRY_WET")));

        const juce::Point<float> pOut(
            graph.getX() + graph.getWidth() * 0.65f,
            graphPercentToY(graph,
                            juce::jmap(parameterValue("OUTPUT_LEVEL"),
                                      -24.f, 12.f, 0.f, 100.f)));

        if (pDry.getDistanceFrom(e.position) < 24.f)
            dragTarget = DragTarget::MixDryWet;
        else if (pOut.getDistanceFrom(e.position) < 24.f)
            dragTarget = DragTarget::MixOutput;
    }
}

void VVChainAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (dragTarget == DragTarget::None)
        return;

    const auto graph = juce::Rectangle<float>(24.f, 108.f,
                                               (float)getWidth() - 48.f,
                                               (float)getHeight() - 350.f);

    const float x = e.position.x;
    const float y = e.position.y;

    const int dragId = static_cast<int>(dragTarget);

    if (dragTarget >= DragTarget::EqBand1 && dragTarget <= DragTarget::EqBand4)
    {
        const int band = dragId - static_cast<int>(DragTarget::EqBand1);
        const auto n = juce::String(band + 1);
        const float freq = graphXToFrequency(graph, x);
        const float gain = juce::jlimit(-24.f, 24.f,
            graphYToPercent(graph, y) * 0.48f - 24.f);
        setParameter("EQ" + n + "_FREQ", freq);
        setParameter("EQ" + n + "_GAIN", gain);
        return;
    }

    if (dragTarget >= DragTarget::OttDegree1 && dragTarget <= DragTarget::OttDegree4)
    {
        const int band = dragId - static_cast<int>(DragTarget::OttDegree1);
        setParameter("OTT_DEGREE" + juce::String(band + 1), graphYToPercent(graph, y));
        bandIndex = band;
        return;
    }

    if (dragTarget >= DragTarget::OttX1 && dragTarget <= DragTarget::OttX3)
    {
        const int cross = dragId - static_cast<int>(DragTarget::OttX1);
        const float f = graphXToFrequency(graph, x);

        if (cross == 0)
            setParameter("OTT_X1", juce::jlimit(80.f, 600.f, f));
        else if (cross == 1)
        {
            const float x1 = parameterValue("OTT_X1");
            setParameter("OTT_X2", juce::jlimit(std::max(750.f, x1 + 80.f), 3000.f, f));
        }
        else
        {
            const float x2 = parameterValue("OTT_X2");
            setParameter("OTT_X3", juce::jlimit(std::max(6000.f, x2 + 200.f), 12000.f, f));
        }
        return;
    }

    if (dragTarget >= DragTarget::TypeDegree1 && dragTarget <= DragTarget::TypeDegree4)
    {
        const int band = dragId - static_cast<int>(DragTarget::TypeDegree1);
        setParameter("ATYPE_DEGREE" + juce::String(band + 1), graphYToPercent(graph, y));
        return;
    }

    if (dragTarget == DragTarget::MixDryWet)
    {
        setParameter("DRY_WET", graphYToPercent(graph, y));
        return;
    }

    if (dragTarget == DragTarget::MixOutput)
    {
        setParameter("OUTPUT_LEVEL",
                     juce::jmap(graphYToPercent(graph, y), 0.f, 100.f, -24.f, 12.f));
    }
}

void VVChainAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    dragTarget = DragTarget::None;
}

void VVChainAudioProcessorEditor::drawGrid(juce::Graphics& g,
                                            juce::Rectangle<float> graph,
                                            float minDb,
                                            float maxDb)
{
    g.setColour(juce::Colour(0xff3b392f).withAlpha(0.70f));

    for (int i = 1; i < 10; ++i)
    {
        const float x = graph.getX() + graph.getWidth() * i / 10.f;
        g.drawVerticalLine((int)x, graph.getY(), graph.getBottom());
    }

    const int lines = 8;
    for (int i = 0; i <= lines; ++i)
    {
        const float y = graph.getY() + graph.getHeight() * i / (float)lines;
        g.drawHorizontalLine((int)y, graph.getX(), graph.getRight());
    }

    g.setColour(juce::Colours::white.withAlpha(0.38f));
    g.setFont(juce::FontOptions(10.f));

    for (int i = 0; i <= lines; ++i)
    {
        const float db = maxDb - (maxDb - minDb) * i / (float)lines;
        const float y = graph.getY() + graph.getHeight() * i / (float)lines;
        g.drawText(juce::String(db, 0) + " dB", 4, (int)y - 7, 40, 14, juce::Justification::right);
    }
}

void VVChainAudioProcessorEditor::drawEqGraph(juce::Graphics& g, juce::Rectangle<float> graph)
{
    drawGrid(g, graph, -18.f, 18.f);

    auto dbToY = [&graph](float db)
    {
        return graph.getBottom() - graph.getHeight() * juce::jlimit(0.f, 1.f, (db + 18.f) / 36.f);
    };

    g.setColour(juce::Colours::white.withAlpha(0.35f));
    const float zeroY = dbToY(0.f);
    g.drawHorizontalLine((int)zeroY, graph.getX(), graph.getRight());

    juce::Path response;

    for (int sample = 0; sample <= 320; ++sample)
    {
        const float t = sample / 320.f;
        const float hz = invLogMap(t, 20.f, 20000.f);
        float db = 0.f;

        for (int band = 0; band < 4; ++band)
        {
            const auto n = juce::String(band + 1);
            const float f0 = parameterValue("EQ" + n + "_FREQ");
            const float gain = parameterValue("EQ" + n + "_GAIN");
            const float q = std::max(0.1f, parameterValue("EQ" + n + "_Q"));
            const float x = std::log(std::max(hz, 20.f) / std::max(f0, 20.f));
            const float width = std::max(0.02f, 1.f / (q * 1.8f));
            db += gain * std::exp(-(x * x) / (2.f * width * width));
        }

        const auto point = juce::Point<float>(
            graphFrequencyToX(graph, hz),
            dbToY(db));

        if (sample == 0)
            response.startNewSubPath(point);
        else
            response.lineTo(point);
    }

    g.setColour(juce::Colour(0xffded7c7));
    g.strokePath(response, juce::PathStrokeType(2.1f));

    for (int band = 0; band < 4; ++band)
    {
        const auto n = juce::String(band + 1);
        const float f = parameterValue("EQ" + n + "_FREQ");
        const float gain = parameterValue("EQ" + n + "_GAIN");
        drawHandle(g,
                   { graphFrequencyToX(graph, f), dbToY(gain) },
                   bandColours[(size_t)band],
                   bandIndex == band);
    }
}

void VVChainAudioProcessorEditor::drawOttGraph(juce::Graphics& g, juce::Rectangle<float> graph)
{
    drawGrid(g, graph, 0.f, 100.f);

    const float xs[3] = {
        parameterValue("OTT_X1"), parameterValue("OTT_X2"), parameterValue("OTT_X3")
    };

    for (int i = 0; i < 3; ++i)
    {
        const float x = graphFrequencyToX(graph, xs[i]);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawVerticalLine((int)x, graph.getY(), graph.getBottom());
        g.drawText(juce::String(xs[i], 0) + " Hz",
                   (int)x - 38, (int)graph.getY() + 8, 76, 16,
                   juce::Justification::centred);
    }

    const float centers[4] = {
        std::sqrt(20.f * xs[0]),
        std::sqrt(xs[0] * xs[1]),
        std::sqrt(xs[1] * xs[2]),
        std::sqrt(xs[2] * 18000.f)
    };

    for (int band = 0; band < 4; ++band)
    {
        const float degree = parameterValue("OTT_DEGREE" + juce::String(band + 1));
        const float left = band == 0 ? 20.f : xs[band - 1];
        const float right = band == 3 ? 18000.f : xs[band];

        juce::Path bandPath;
        const float y = graphPercentToY(graph, degree);
        bandPath.startNewSubPath(graphFrequencyToX(graph, left), y);
        bandPath.lineTo(graphFrequencyToX(graph, right), y);

        g.setColour(bandColours[(size_t)band].withAlpha(0.88f));
        g.strokePath(bandPath, juce::PathStrokeType(3.f));

        drawHandle(g, { graphFrequencyToX(graph, centers[band]), y },
                   bandColours[(size_t)band], bandIndex == band);
    }

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(juce::FontOptions(12.f));
    g.drawText("PUNKOTT-MB STYLE: GATE 6:1  →  LIFTER 6:1  →  COMPRESSOR 8:1  →  LIMITER",
               (int)graph.getX() + 12, (int)graph.getBottom() - 25,
               (int)graph.getWidth() - 24, 18, juce::Justification::centredLeft);
}

void VVChainAudioProcessorEditor::drawTypeAGraph(juce::Graphics& g, juce::Rectangle<float> graph)
{
    drawGrid(g, graph, 0.f, 100.f);

    const float xover[] = { 80.f, 3000.f, 9000.f };
    for (float f : xover)
    {
        const float x = graphFrequencyToX(graph, f);
        g.setColour(juce::Colours::white.withAlpha(0.48f));
        g.drawVerticalLine((int)x, graph.getY(), graph.getBottom());
        g.drawText(juce::String(f, 0) + " Hz", (int)x - 35, (int)graph.getY() + 8,
                   70, 16, juce::Justification::centred);
    }

    const float centers[4] = { 40.f, 700.f, 5200.f, 12000.f };
    const std::array<juce::String, 4> labels { "B1", "B2", "B3", "B4" };

    for (int i = 0; i < 4; ++i)
    {
        const float degree = parameterValue("ATYPE_DEGREE" + juce::String(i + 1));
        const float x = graphFrequencyToX(graph, centers[i]);
        const float y = graphPercentToY(graph, degree);

        g.setColour(bandColours[(size_t)i].withAlpha(0.65f));
        g.drawLine(x, graph.getBottom(), x, y, 3.f);
        drawHandle(g, { x, y }, bandColours[(size_t)i], false);

        g.setColour(bandColours[(size_t)i]);
        g.setFont(juce::FontOptions(12.f));
        g.drawText(labels[(size_t)i] + "  " + juce::String(degree, 1) + "%",
                   (int)x - 40, (int)y - 25, 80, 18, juce::Justification::centred);
    }

    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawText("TYPE-A: 80 Hz LP  /  80 Hz–3 kHz  /  >3 kHz  /  >9 kHz",
               (int)graph.getX() + 12, (int)graph.getBottom() - 25,
               (int)graph.getWidth() - 24, 18, juce::Justification::centredLeft);
}

void VVChainAudioProcessorEditor::drawDeEsserGraph(juce::Graphics& g, juce::Rectangle<float> graph)
{
    g.setColour(juce::Colour(0xff0b0c0b));
    g.fillRoundedRectangle(graph, 8.f);

    const float panelW = juce::jlimit(185.f, 250.f, graph.getWidth() * 0.24f);
    const float gap = 10.f;
    const float wx = graph.getX() + panelW + 14.f;
    const float ww = std::max(120.f, graph.getRight() - wx - 12.f);
    const float wh = std::max(100.f, (graph.getHeight() - 20.f - gap) * 0.5f);

    auto drawPanel = [&g](juce::Rectangle<float> r, const juce::String& label,
                          juce::Colour colour, float phase, float scale)
    {
        g.setColour(juce::Colour(0xff121511));
        g.fillRect(r);
        g.setColour(juce::Colour(0xff383c34));
        g.drawRect(r, 1.f);

        juce::Path p;
        for (int i = 0; i <= 320; ++i)
        {
            const float t = i / 320.f;
            const float x = r.getX() + 6.f + t * (r.getWidth() - 12.f);
            const float env = 0.23f + 0.77f * (0.5f + 0.5f * std::sin(t * 13.5f + phase));
            const float y = r.getCentreY() -
                            std::sin(t * 40.f + phase) * r.getHeight() * 0.24f * env * scale;
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }

        g.setColour(colour);
        g.strokePath(p, juce::PathStrokeType(1.4f));
        g.setFont(juce::FontOptions(10.f));
        g.drawText(label, (int)r.getX() + 8, (int)r.getY() + 6, 120, 16,
                   juce::Justification::left);
    };

    g.setColour(juce::Colour(0xff181a17));
    g.fillRect(graph.getX() + 8.f, graph.getY() + 8.f, panelW, graph.getHeight() - 16.f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(11.f));
    g.drawText("DE-ESSER", (int)graph.getX() + 20, (int)graph.getY() + 18, 140, 18,
               juce::Justification::left);

    const std::array<juce::String, 8> info {
        "FFT SIZE       4096",
        "REFERENCE       12.5 kHz",
        "BUFFER STEP     1365",
        "OVERLAP         2 / 3",
        "OUTPUT          MIDDLE 1 / 3",
        "DETECTOR        SAMPLE DIFFERENCE",
        "TRIGGER         > 10",
        "REFERENCE RATE  44.1 kHz"
    };

    g.setColour(juce::Colour(0xffa9aa9f));
    g.setFont(juce::FontOptions(10.f));
    for (int i = 0; i < (int)info.size(); ++i)
        g.drawText(info[(size_t)i], (int)graph.getX() + 20,
                   (int)graph.getY() + 42 + i * 18, (int)panelW - 24, 16,
                   juce::Justification::left);

    const auto original = juce::Rectangle<float>(wx, graph.getY() + 10.f, ww, wh);
    const auto processed = juce::Rectangle<float>(wx, original.getBottom() + gap, ww, wh);

    drawPanel(original, "ORIGINAL", juce::Colour(0xffd8d2c3), 0.f, 1.0f);
    drawPanel(processed, "DE-ESSED", juce::Colour(0xff64c9a7), 0.7f, 0.72f);

    g.setColour(juce::Colour(0xff9c9e94));
    g.setFont(juce::FontOptions(10.f));
    g.drawText("REFERENCE: FFT -> FILTER -> IFFT",
               (int)wx, (int)graph.getBottom() - 16, (int)ww, 14,
               juce::Justification::left);
}

void VVChainAudioProcessorEditor::drawAnalyzerGraph(juce::Graphics& g, juce::Rectangle<float> graph)
{
    // QSpectrumAnalyzer-style layout: spectrum above, waterfall below.
    const auto spectrumRect = graph.withHeight(graph.getHeight() * 0.62f);
    auto waterfallRect = graph.withY(spectrumRect.getBottom() + 4.f)
                              .withHeight(graph.getHeight() - spectrumRect.getHeight() - 4.f);

    drawGrid(g, spectrumRect, -100.f, 6.f);

    auto xForBin = [this, spectrumRect](int bin)
    {
        const float hz = std::max(20.0f,
            bin * (float)audioProcessor.getAnalyzerSampleRate() /
            (2.0f * (float)(VVChainAudioProcessor::kSpectrumBins - 1)));

        return graphFrequencyToX(spectrumRect, hz);
    };

    std::array<float, 256> current {};
    for (int i = 0; i < 256; ++i)
    {
        const int bin = 1 + (i * (VVChainAudioProcessor::kSpectrumBins - 2) / 255);
        current[(size_t)i] = spectrum[(size_t)bin];
        if (analyzerSmooth.getToggleState() && i > 0 && i < 255)
            current[(size_t)i] = (spectrum[(size_t)(bin - 1)] +
                                   spectrum[(size_t)bin] +
                                   spectrum[(size_t)(bin + 1)]) / 3.f;

        if (analyzerAverage.getToggleState())
            analyzerSmoothed[(size_t)i] =
                analyzerSmoothed[(size_t)i] * 0.78f + current[(size_t)i] * 0.22f;
        else
            analyzerSmoothed[(size_t)i] = current[(size_t)i];

        if (analyzerPeak.getToggleState())
            peakSpectrum[(size_t)bin] =
                std::max(peakSpectrum[(size_t)bin] - 0.15f, spectrum[(size_t)bin]);

        current[(size_t)i] = analyzerSmoothed[(size_t)i];
    }

    juce::Path trace;
    for (int i = 0; i < 256; ++i)
    {
        const float hz = invLogMap(i / 255.f, 20.f, 20000.f);
        const float x = graphFrequencyToX(spectrumRect, hz);
        const float db = juce::jlimit(-100.f, 6.f, current[(size_t)i]);
        const float y = spectrumRect.getBottom() -
                        spectrumRect.getHeight() * (db + 100.f) / 106.f;

        if (i == 0)
            trace.startNewSubPath(x, y);
        else
            trace.lineTo(x, y);
    }

    g.setColour(juce::Colour(0xffe4d9b7));
    g.strokePath(trace, juce::PathStrokeType(1.8f));

    if (analyzerPeak.getToggleState())
    {
        juce::Path peak;
        for (int i = 0; i < 256; ++i)
        {
            const float hz = invLogMap(i / 255.f, 20.f, 20000.f);
            const int bin = juce::jlimit(1, VVChainAudioProcessor::kSpectrumBins - 1,
                                         (int)(hz / (float)audioProcessor.getAnalyzerSampleRate() *
                                               2.0f * (VVChainAudioProcessor::kSpectrumBins - 1)));
            const float db = juce::jlimit(-100.f, 6.f, peakSpectrum[(size_t)bin]);
            const float x = graphFrequencyToX(spectrumRect, hz);
            const float y = spectrumRect.getBottom() -
                            spectrumRect.getHeight() * (db + 100.f) / 106.f;
            if (i == 0) peak.startNewSubPath(x, y); else peak.lineTo(x, y);
        }

        g.setColour(juce::Colour(0xff8da1ff).withAlpha(0.6f));
        g.strokePath(peak, juce::PathStrokeType(1.f));
    }

    g.setColour(juce::Colour(0xff0d0d0d));
    g.fillRoundedRectangle(waterfallRect, 4.f);

    const int rows = (int)waterfall.size();
    for (int row = 0; row < rows; ++row)
    {
        const auto r = waterfallRect.removeFromTop(waterfallRect.getHeight() / (float)rows);
        for (int col = 0; col < 256; ++col)
        {
            const float v = waterfall[(size_t)row][(size_t)col];
            const float n = juce::jlimit(0.f, 1.f, (v + 100.f) / 100.f);
            g.setColour(juce::Colour::fromHSV(0.68f - 0.58f * n, 0.85f,
                                                0.10f + 0.90f * n, 1.0f));
            g.fillRect(r.getX() + r.getWidth() * col / 256.f,
                       r.getY(),
                       r.getWidth() / 256.f + 0.5f,
                       r.getHeight());
        }
    }

    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.drawText("SPECTRUM", (int)spectrumRect.getX() + 7, (int)spectrumRect.getY() + 5,
               100, 16, juce::Justification::left);
    g.drawText("WATERFALL", (int)waterfallRect.getX() + 7, (int)waterfallRect.getY() + 5,
               100, 16, juce::Justification::left);
}

void VVChainAudioProcessorEditor::drawMixGraph(juce::Graphics& g, juce::Rectangle<float> graph)
{
    drawGrid(g, graph, 0.f, 100.f);

    const float dry = parameterValue("DRY_WET");
    const float output = juce::jmap(parameterValue("OUTPUT_LEVEL"), -24.f, 12.f, 0.f, 100.f);

    const float x1 = graph.getX() + graph.getWidth() * 0.35f;
    const float x2 = graph.getX() + graph.getWidth() * 0.65f;

    g.setColour(juce::Colour(0xff39a9ff).withAlpha(0.75f));
    g.drawVerticalLine((int)x1, graph.getY(), graph.getBottom());
    g.setColour(juce::Colour(0xff9bde4d).withAlpha(0.75f));
    g.drawVerticalLine((int)x2, graph.getY(), graph.getBottom());

    drawHandle(g, { x1, graphPercentToY(graph, dry) }, juce::Colour(0xff39a9ff), true);
    drawHandle(g, { x2, graphPercentToY(graph, output) }, juce::Colour(0xff9bde4d), true);

    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(juce::FontOptions(12.f));
    g.drawText("DRY / WET", (int)x1 - 55, (int)graph.getY() + 14, 110, 16, juce::Justification::centred);
    g.drawText("OUTPUT", (int)x2 - 55, (int)graph.getY() + 14, 110, 16, juce::Justification::centred);
}

void VVChainAudioProcessorEditor::drawGraph(juce::Graphics& g, juce::Rectangle<float> graph)
{
    g.setColour(juce::Colour(0xff12120f));
    g.fillRoundedRectangle(graph, 10.f);

    if (moduleIndex == 0) drawEqGraph(g, graph);
    else if (moduleIndex == 1) drawOttGraph(g, graph);
    else if (moduleIndex == 2) drawTypeAGraph(g, graph);
    else if (moduleIndex == 3) drawDeEsserGraph(g, graph);
    else if (moduleIndex == 4) drawMixGraph(g, graph);
    else drawAnalyzerGraph(g, graph);
}

void VVChainAudioProcessorEditor::drawTopBar(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(juce::Colour(0xff27251f));
    g.fillRect(area);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.f));
    g.drawText("VVChain", 16, (int)area.getY(), 140, (int)area.getHeight(),
               juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffaaa18f));
    g.setFont(juce::FontOptions(10.f));
    g.drawText("4-BAND ANALOG EQ  •  OTT  •  TYPE-A  •  REFERENCE DE-ESSER  •  FFT ANALYZER",
               16, (int)area.getY() + 35, (int)area.getWidth() - 32, 14,
               juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff4a473e));
    g.fillRoundedRectangle(730.f, area.getY() + 13.f, 250.f, 34.f, 6.f);
    g.setColour(juce::Colours::white.withAlpha(0.78f));
    g.drawText("AUDIO RANGE  0:00.000 → HOST", 741, (int)area.getY() + 20, 228, 20,
               juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff4a473e));
    g.fillRoundedRectangle(995.f, area.getY() + 13.f, 82.f, 34.f, 6.f);
    g.setColour(juce::Colours::white);
    g.drawText("LOOP", 995, (int)area.getY() + 20, 82, 20, juce::Justification::centred);
}

void VVChainAudioProcessorEditor::drawParameterPanel(juce::Graphics& g,
                                                      juce::Rectangle<float> area)
{
    g.setColour(juce::Colour(0xff1c1b17));
    g.fillRect(area);

    if (moduleIndex == 0 || moduleIndex == 1 || moduleIndex == 2)
    {
        g.setColour(juce::Colour(0xff8f8878));
        g.setFont(juce::FontOptions(10.f));
        g.drawText(moduleIndex == 0 ? "EQ BAND SELECT" :
                   moduleIndex == 1 ? "OTT BAND SELECT" : "TYPE-A ALL FOUR BANDS",
                   16, (int)area.getY() + 8, 180, 16, juce::Justification::left);
    }

    for (int i = 0; i < 4; ++i)
    {
        bandButtons[(size_t)i].setVisible(moduleIndex == 0 || moduleIndex == 1);
    }
}

void VVChainAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10100d));

    drawTopBar(g, { 0.f, 0.f, (float)getWidth(), 64.f });

    g.setColour(juce::Colour(0xff171611));
    g.fillRect(0, 64, getWidth(), 44);

    for (int i = 0; i < 6; ++i)
        moduleButtons[(size_t)i].setBounds(
            12 + i * ((getWidth() - 24) / 6), 68,
            (getWidth() - 36) / 6, 34);

    const auto graph = juce::Rectangle<float>(24.f, 108.f,
                                               (float)getWidth() - 48.f,
                                               (float)getHeight() - 350.f);

    drawGraph(g, graph);

    if (moduleIndex == 3)
    {
        g.setColour(juce::Colour(0xffb6ae9d));
        g.setFont(juce::FontOptions(11.f));
        g.drawText("Voice target:",
                   18, getHeight() - 222, 75, 22, juce::Justification::left);
    }

    if (moduleIndex == 5)
    {
        g.setColour(juce::Colour(0xff1a1916));
        g.fillRoundedRectangle(18.f, (float)getHeight() - 226.f,
                               (float)getWidth() - 36.f, 46.f, 6.f);
    }
}

void VVChainAudioProcessorEditor::resized()
{
    const int width = getWidth();
    const int h = getHeight();

    for (int i = 0; i < 6; ++i)
        moduleButtons[(size_t)i].setBounds(
            12 + i * ((width - 24) / 6), 68,
            (width - 36) / 6, 34);

    for (int i = 0; i < 4; ++i)
    {
        bandButtons[(size_t)i].setVisible(moduleIndex == 0 || moduleIndex == 1);
        bandButtons[(size_t)i].setBounds(12 + i * 92, h - 318, 84, 28);
    }

    const int startX = 22;
    const int y = h - 280;
    const int knobW = 116;
    const int gap = 10;

    for (int i = 0; i < (int)controls.size(); ++i)
    {
        if (!controls[(size_t)i].isVisible())
            continue;

        const int col = i % 9;
        const int row = i / 9;
        const int x = startX + col * (knobW + gap);
        const int yy = y + row * 118;

        controls[(size_t)i].setBounds(x, yy + 18, knobW, 86);
        controlLabels[(size_t)i].setBounds(x, yy, knobW, 18);
    }

    deEssVoice.setVisible(moduleIndex == 3);
    deEssVoice.setBounds(width - 180, h - 252, 160, 28);

    ottClipper.setVisible(moduleIndex == 1);
    ottClipper.setBounds(width - 105, h - 318, 92, 28);
    if (auto* clipParam = audioProcessor.apvts.getParameter("OTT_CLIPPER"))
        ottClipper.setToggleState(clipParam->getValue() > 0.5f, juce::dontSendNotification);
    ottClipper.onClick = [this]
    {
        if (auto* clipParam = audioProcessor.apvts.getParameter("OTT_CLIPPER"))
            clipParam->setValueNotifyingHost(ottClipper.getToggleState() ? 1.f : 0.f);
    };

    analyzerAverage.setBounds(16, h - 210, 110, 28);
    analyzerPeak.setBounds(128, h - 210, 110, 28);
    analyzerPersistence.setBounds(240, h - 210, 120, 28);
    analyzerSmooth.setBounds(362, h - 210, 110, 28);

    const bool analyzer = moduleIndex == 5;
    analyzerAverage.setVisible(analyzer);
    analyzerPeak.setVisible(analyzer);
    analyzerPersistence.setVisible(analyzer);
    analyzerSmooth.setVisible(analyzer);

    for (int i = 0; i < 4; ++i)
        bandButtons[(size_t)i].setVisible(moduleIndex == 0 || moduleIndex == 1);
}

void VVChainAudioProcessorEditor::timerCallback()
{
    audioProcessor.copySpectrumTo(spectrum.data(), (int)spectrum.size());

    if (analyzerPersistence.getToggleState())
    {
        for (int row = (int)waterfall.size() - 1; row > 0; --row)
            waterfall[(size_t)row] = waterfall[(size_t)(row - 1)];
        for (int col = 0; col < 256; ++col)
        {
            const int bin = 1 + col * (VVChainAudioProcessor::kSpectrumBins - 2) / 255;
            waterfall[0][(size_t)col] = spectrum[(size_t)bin];
        }
    }
    else
    {
        for (auto& row : waterfall)
            row.fill(-120.f);
    }

    repaint();
}
