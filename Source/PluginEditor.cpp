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

    const std::array<juce::String, 5> names
    {
        "EQ / ANALOG", "OTT", "TYPE-A", "DE-ESSER", "MIX / OUT"
    };

    const std::array<juce::String, 5> bypassIds
    {
        "EQ_BYPASS", "OTT_BYPASS", "ATYPE_BYPASS", "DEESS_BYPASS", "MIX_BYPASS"
    };

    for (int i = 0; i < 5; ++i)
    {
        moduleButtons[(size_t)i].setButtonText(names[(size_t)i]);
        moduleButtons[(size_t)i].onClick = [this, i] { selectModule(i); };
        addAndMakeVisible(moduleButtons[(size_t)i]);

        bypassButtons[(size_t)i].setButtonText("BYPASS");
        bypassButtons[(size_t)i].setClickingTogglesState(true);
        bypassButtons[(size_t)i].setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff0ece0));
        bypassAttachments[(size_t)i] = std::make_unique<BoolAttachment>(
            audioProcessor.apvts, bypassIds[(size_t)i], bypassButtons[(size_t)i]);
        addAndMakeVisible(bypassButtons[(size_t)i]);
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

    addAndMakeVisible(ottClipper);

    deEssVoice.addItem("Male Vocal", 1);
    deEssVoice.addItem("Female Vocal", 2);
    deEssVoiceAttachment = std::make_unique<ComboAttachment>(
        audioProcessor.apvts, "DEESS_VOICE", deEssVoice);
    addAndMakeVisible(deEssVoice);

    selectModule(0);
    selectBand(0);
}

void VVChainAudioProcessorEditor::selectModule(int index)
{
    moduleIndex = juce::jlimit(0, 4, index);

    for (int i = 0; i < 5; ++i)
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
    if (index < 0 || index >= static_cast<int>(controls.size()))
        return;

    attachments[(size_t)index].reset();
    controls[(size_t)index].setVisible(false);
    controlLabels[(size_t)index].setVisible(false);
}

void VVChainAudioProcessorEditor::setControl(int index, const juce::String& parameterId,
                                             const juce::String& title)
{
    if (index < 0 || index >= static_cast<int>(controls.size()))
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
    if (index < 0 || index >= static_cast<int>(controls.size()))
        return;

    controls[(size_t)index].setRange(minimum, maximum, step);
    controls[(size_t)index].setTextValueSuffix(suffix);
}

void VVChainAudioProcessorEditor::rebuildControls()
{
    for (int i = 0; i < static_cast<int>(controls.size()); ++i)
        hideControl(i);

    for (auto& b : bypassButtons)
        b.setVisible(false);

    bypassButtons[(size_t)moduleIndex].setVisible(true);
    ottClipper.setVisible(moduleIndex == 1);

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
        setControl(0, "DEESS_INTENSITY", "INTENSITY");
        setControl(1, "DEESS_OFFSET", "OFFSET");

        setControlRangeForDisplay(0, 2.0, 10.0, 0.01, "");
        setControlRangeForDisplay(1, -0.1, 0.1, 0.0001, "");
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
            const float gain = parameterValue("EQ" + n + "_GAIN");
            const juce::Point<float> p(graphFrequencyToX(graph, f),
                                       graph.getCentreY() - gain / 36.f * graph.getHeight());
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
            const juce::Point<float> p(graphFrequencyToX(graph, centers[i]),
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
            const juce::Point<float> p(graphFrequencyToX(graph, centers[i]),
                                       graphPercentToY(graph, degree));
            if (p.getDistanceFrom(e.position) < nearest)
            {
                nearest = p.getDistanceFrom(e.position);
                dragTarget = static_cast<DragTarget>(
                    static_cast<int>(DragTarget::TypeDegree1) + i);
            }
        }
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
        setParameter("EQ" + n + "_FREQ", graphXToFrequency(graph, x));
        setParameter("EQ" + n + "_GAIN",
                     juce::jlimit(-24.f, 24.f, graphYToPercent(graph, y) * 0.48f - 24.f));
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
        return graph.getBottom() - graph.getHeight() *
            juce::jlimit(0.f, 1.f, (db + 18.f) / 36.f);
    };

    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawHorizontalLine((int)dbToY(0.f), graph.getX(), graph.getRight());

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
            graphFrequencyToX(graph, hz), dbToY(db));

        if (sample == 0) response.startNewSubPath(point);
        else response.lineTo(point);
    }

    g.setColour(juce::Colour(0xffded7c7));
    g.strokePath(response, juce::PathStrokeType(2.1f));

    for (int band = 0; band < 4; ++band)
    {
        const auto n = juce::String(band + 1);
        drawHandle(g,
                   { graphFrequencyToX(graph, parameterValue("EQ" + n + "_FREQ")),
                     dbToY(parameterValue("EQ" + n + "_GAIN")) },
                   bandColours[(size_t)band], bandIndex == band);
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

        const float y = graphPercentToY(graph, degree);
        g.setColour(bandColours[(size_t)band].withAlpha(0.88f));
        g.drawLine(graphFrequencyToX(graph, left), y,
                   graphFrequencyToX(graph, right), y, 3.f);
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
    for (int i = 0; i < 4; ++i)
    {
        const float degree = parameterValue("ATYPE_DEGREE" + juce::String(i + 1));
        const float x = graphFrequencyToX(graph, centers[i]);
        const float y = graphPercentToY(graph, degree);

        g.setColour(bandColours[(size_t)i].withAlpha(0.65f));
        g.drawLine(x, graph.getBottom(), x, y, 3.f);
        drawHandle(g, { x, y }, bandColours[(size_t)i], false);
    }

    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawText("TYPE-A: 80 Hz LP  /  80 Hz–3 kHz  /  >3 kHz  /  >9 kHz",
               (int)graph.getX() + 12, (int)graph.getBottom() - 25,
               (int)graph.getWidth() - 24, 18, juce::Justification::centredLeft);
}

void VVChainAudioProcessorEditor::drawDeEsserGraph(
    juce::Graphics& g, juce::Rectangle<float> graph)
{
    g.setColour(juce::Colour(0xff0b0c0b));
    g.fillRoundedRectangle(graph, 8.f);

    const float sideW = juce::jlimit(230.f, 285.f, graph.getWidth() * 0.25f);
    const auto side = graph.removeFromLeft(sideW).reduced(12.f);

    g.setColour(juce::Colour(0xff181a17));
    g.fillRoundedRectangle(side, 6.f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(14.f));
    g.drawText("DE-ESSER", (int)side.getX() + 14, (int)side.getY() + 12,
               (int)side.getWidth() - 28, 22, juce::Justification::left);

    g.setColour(juce::Colour(0xffb7b6aa));
    g.setFont(juce::FontOptions(10.f));

    const int voice = parameterValue("DEESS_VOICE") > 0.5f ? 1 : 0;
    const float intensity = parameterValue("DEESS_INTENSITY");
    const float offset = parameterValue("DEESS_OFFSET");
    const float ref = voice == 0 ? 12500.f : 13500.f;

    const std::array<juce::String, 8> info
    {
        "FFT SIZE       8192",
        "REFERENCE      " + juce::String(ref, 0) + " Hz",
        "BLOCK          8192",
        "OVERLAP        NONE",
        "PROCESS        FFT -> FILTER -> IFFT",
        "DETECTOR       PAIRED SAMPLE DIFFERENCE",
        "TRIGGER        COUNT > 10",
        "INTENSITY      " + juce::String(intensity, 2)
    };

    for (int i = 0; i < static_cast<int>(info.size()); ++i)
        g.drawText(info[(size_t)i], (int)side.getX() + 14,
                   (int)side.getY() + 48 + i * 18,
                   (int)side.getWidth() - 28, 16, juce::Justification::left);

    g.drawText("OFFSET         " + juce::String(offset, 4),
               (int)side.getX() + 14, (int)side.getY() + 48 + 8 * 18,
               (int)side.getWidth() - 28, 16, juce::Justification::left);

    auto plot = graph.reduced(8.f);
    drawGrid(g, plot, -12.f, 1.f);

    const float bandLeft = graphFrequencyToX(plot, 4000.f);
    const float bandRight = graphFrequencyToX(plot, 16000.f);
    g.setColour(juce::Colour(0xff62c8a2).withAlpha(0.10f));
    g.fillRect(bandLeft, plot.getY(), bandRight - bandLeft, plot.getHeight());

    const float refX = graphFrequencyToX(plot, ref);
    g.setColour(juce::Colour(0xffffc75a).withAlpha(0.9f));
    g.drawVerticalLine((int)refX, plot.getY(), plot.getBottom());

    juce::Path curve;
    for (int i = 0; i <= 280; ++i)
    {
        const float t = i / 280.f;
        const float hz = invLogMap(t, 20.f, 20000.f);
        float reductionDb = 0.f;

        if (hz >= ref)
        {
            const float coeff = intensity * ref / std::max(hz, 1.f);
            reductionDb = -12.f * juce::jlimit(0.f, 1.f, 1.f - 1.f / coeff);
        }
        else if (hz >= ref / 10.f)
        {
            const float coeff = 1.f + (intensity - 1.f)
                * std::pow(hz / ref, 3.f);
            reductionDb = -12.f * juce::jlimit(0.f, 1.f, 1.f - 1.f / coeff);
        }

        const float y = plot.getBottom()
            - plot.getHeight() * juce::jmap(reductionDb, -12.f, 1.f, 0.f, 1.f);

        if (i == 0) curve.startNewSubPath(graphFrequencyToX(plot, hz), y);
        else curve.lineTo(graphFrequencyToX(plot, hz), y);
    }

    g.setColour(juce::Colour(0xff67d3aa));
    g.strokePath(curve, juce::PathStrokeType(2.f));

    g.setColour(juce::Colour(0xffddd8c8));
    g.setFont(juce::FontOptions(10.f));
    g.drawText(voice == 0 ? "MALE VOCAL  12.5 kHz" : "FEMALE VOCAL  13.5 kHz",
               (int)plot.getX() + 8, (int)plot.getY() + 8,
               180, 16, juce::Justification::left);
}

void VVChainAudioProcessorEditor::drawMixGraph(juce::Graphics& g,
                                                juce::Rectangle<float> graph)
{
    drawGrid(g, graph, 0.f, 100.f);

    const float dry = parameterValue("DRY_WET");
    const float output = juce::jmap(parameterValue("OUTPUT_LEVEL"),
                                    -24.f, 12.f, 0.f, 100.f);
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
    g.drawText("DRY / WET", (int)x1 - 55, (int)graph.getY() + 14, 110, 16,
               juce::Justification::centred);
    g.drawText("OUTPUT", (int)x2 - 55, (int)graph.getY() + 14, 110, 16,
               juce::Justification::centred);
}

void VVChainAudioProcessorEditor::drawGraph(juce::Graphics& g,
                                            juce::Rectangle<float> graph)
{
    g.setColour(juce::Colour(0xff12120f));
    g.fillRoundedRectangle(graph, 10.f);

    if (moduleIndex == 0) drawEqGraph(g, graph);
    else if (moduleIndex == 1) drawOttGraph(g, graph);
    else if (moduleIndex == 2) drawTypeAGraph(g, graph);
    else if (moduleIndex == 3) drawDeEsserGraph(g, graph);
    else drawMixGraph(g, graph);
}

void VVChainAudioProcessorEditor::drawTopBar(juce::Graphics& g,
                                             juce::Rectangle<float> area)
{
    g.setColour(juce::Colour(0xff27251f));
    g.fillRect(area);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.f));
    g.drawText("VVChain", 16, (int)area.getY(), 140, (int)area.getHeight(),
               juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffaaa18f));
    g.setFont(juce::FontOptions(10.f));
    g.drawText("4-BAND ANALOG EQ  •  OTT  •  TYPE-A  •  REFERENCE DE-ESSER",
               16, (int)area.getY() + 35, (int)area.getWidth() - 32, 14,
               juce::Justification::centredLeft);
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

    deEssVoice.setVisible(moduleIndex == 3);
    if (moduleIndex == 3)
    {
        g.setColour(juce::Colour(0xff8f8878));
        g.setFont(juce::FontOptions(10.f));
        g.drawText("REFERENCE-BASED DE-ESSER CONTROLS",
                   16, (int)area.getY() + 8, 300, 16, juce::Justification::left);
    }
}

void VVChainAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10100d));

    drawTopBar(g, { 0.f, 0.f, (float)getWidth(), 64.f });

    g.setColour(juce::Colour(0xff171611));
    g.fillRect(0, 64, getWidth(), 44);

    const auto graph = juce::Rectangle<float>(24.f, 108.f,
                                               (float)getWidth() - 48.f,
                                               (float)getHeight() - 350.f);
    drawGraph(g, graph);

    drawParameterPanel(g, { 0.f, (float)getHeight() - 350.f,
                            (float)getWidth(), 350.f });
}

void VVChainAudioProcessorEditor::resized()
{
    const int width = getWidth();
    const int h = getHeight();

    const int tabW = (width - 36) / 5;
    for (int i = 0; i < 5; ++i)
        moduleButtons[(size_t)i].setBounds(12 + i * (tabW + 4), 68, tabW, 34);

    for (int i = 0; i < 5; ++i)
    {
        const bool visible = moduleIndex == i;
        bypassButtons[(size_t)i].setVisible(visible);
        bypassButtons[(size_t)i].setBounds(width - 110, h - 335, 92, 26);
    }

    for (int i = 0; i < 4; ++i)
    {
        const bool showBands = moduleIndex == 0 || moduleIndex == 1;
        bandButtons[(size_t)i].setVisible(showBands);
        bandButtons[(size_t)i].setBounds(12 + i * 92, h - 318, 84, 28);
    }

    const int startX = 22;
    const int y = h - 280;
    const int knobW = 116;
    const int gap = 10;

    for (int i = 0; i < static_cast<int>(controls.size()); ++i)
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

    ottClipper.setVisible(moduleIndex == 1);
    ottClipper.setBounds(width - 205, h - 335, 82, 26);

    deEssVoice.setVisible(moduleIndex == 3);
    deEssVoice.setBounds(24, h - 314, 190, 28);
    if (auto* clipParam = audioProcessor.apvts.getParameter("OTT_CLIPPER"))
        ottClipper.setToggleState(clipParam->getValue() > 0.5f,
                                  juce::dontSendNotification);
    ottClipper.onClick = [this]
    {
        if (auto* clipParam = audioProcessor.apvts.getParameter("OTT_CLIPPER"))
            clipParam->setValueNotifyingHost(ottClipper.getToggleState() ? 1.f : 0.f);
    };
}
