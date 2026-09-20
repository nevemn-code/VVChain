#include "PluginEditor.h"

namespace
{
struct ControlDef
{
    juce::String id;
    juce::String label;
};

juce::Rectangle<float> graphBounds(const juce::Component& c)
{
    return { 36.f, 86.f, (float)c.getWidth() - 72.f, (float)c.getHeight() - 330.f };
}

float logMap(float value, float min, float max)
{
    return std::log(value / min) / std::log(max / min);
}

float fromLogMap(float t, float min, float max)
{
    return min * std::pow(max / min, juce::jlimit(0.f, 1.f, t));
}

void drawHandle(juce::Graphics& g, juce::Point<float> p, juce::Colour colour, bool selected)
{
    g.setColour(colour.withAlpha(0.24f));
    if (selected)
        g.fillEllipse(p.x - 13.f, p.y - 13.f, 26.f, 26.f);

    g.setColour(colour);
    g.fillEllipse(p.x - 7.f, p.y - 7.f, 14.f, 14.f);
}
}

VVChainAudioProcessorEditor::VVChainAudioProcessorEditor(VVChainAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setResizable(true, true);
    setSize(1320, 850);

    const char* modules[] = { "EQ / ANALOG", "OTT", "TYPE-A", "DE-ESSER", "MIX / OUT", "ANALYZER" };
    for (int i = 0; i < (int)moduleButtons.size(); ++i)
    {
        moduleButtons[(size_t)i].setButtonText(modules[i]);
        moduleButtons[(size_t)i].onClick = [this, i] { selectModule(i); };
        addAndMakeVisible(moduleButtons[(size_t)i]);
    }

    const char* bands[] = { "BAND 1", "BAND 2", "BAND 3", "BAND 4" };
    for (int i = 0; i < (int)bandButtons.size(); ++i)
    {
        bandButtons[(size_t)i].setButtonText(bands[i]);
        bandButtons[(size_t)i].onClick = [this, i] { selectBand(i); };
        addAndMakeVisible(bandButtons[(size_t)i]);
    }

    for (size_t i = 0; i < sliders.size(); ++i)
    {
        sliders[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sliders[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 82, 18);
        labels[i].setJustificationType(juce::Justification::centred);
        labels[i].setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.88f));
        addAndMakeVisible(sliders[i]);
        addAndMakeVisible(labels[i]);
    }

    selectBand(0);
    selectModule(0);
    startTimerHz(30);
}

void VVChainAudioProcessorEditor::selectModule(int index)
{
    moduleIndex = juce::jlimit(0, 5, index);
    detailOpen = true;

    for (int i = 0; i < (int)moduleButtons.size(); ++i)
    {
        const bool active = i == moduleIndex;
        moduleButtons[(size_t)i].setColour(
            juce::TextButton::buttonColourId,
            active ? juce::Colour(0xff5d5649) : juce::Colour(0xff38362f));
    }

    rebuildControls();
    resized();
    repaint();
}

void VVChainAudioProcessorEditor::selectBand(int index)
{
    bandIndex = juce::jlimit(0, 3, index);

    for (int i = 0; i < (int)bandButtons.size(); ++i)
    {
        const bool active = i == bandIndex;
        bandButtons[(size_t)i].setColour(
            juce::TextButton::buttonColourId,
            active ? juce::Colour(0xff5d5649) : juce::Colour(0xff38362f));
    }

    if (moduleIndex == 0)
        rebuildControls();

    repaint();
}

void VVChainAudioProcessorEditor::rebuildControls()
{
    for (auto& a : attachments)
        a.reset();

    for (size_t i = 0; i < sliders.size(); ++i)
    {
        sliders[i].setVisible(false);
        labels[i].setVisible(false);
    }

    if (!detailOpen || moduleIndex == 5)
        return;

    std::vector<ControlDef> defs;

    if (moduleIndex == 0)
    {
        const auto n = juce::String(bandIndex + 1);
        defs = {
            { "EQ" + n + "_FREQ", "FREQ" },
            { "EQ" + n + "_GAIN", "GAIN" },
            { "EQ" + n + "_Q", "Q" },
            { "EQ_COLOR", "ANALOG COLOR" },
            { "HF_CORNER", "HF / HPF" }
        };
    }
    else if (moduleIndex == 1)
    {
        const auto n = juce::String(bandIndex + 1);
        defs = {
            { "OTT_DEGREE" + n, "OTT DEGREE" },
            { "OTT_LIFT_T" + n, "LIFTER THRESH" },
            { "OTT_LIFT_A" + n, "LIFTER ATTACK" },
            { "OTT_LIFT_R" + n, "LIFTER RELEASE" },
            { "OTT_LIFT_M" + n, "LIFTER MIX" },
            { "OTT_COMP_T" + n, "COMP THRESH" },
            { "OTT_COMP_A" + n, "COMP ATTACK" },
            { "OTT_COMP_R" + n, "COMP RELEASE" },
            { "OTT_COMP_M" + n, "COMP MIX" },
            { "OTT_LEVEL" + n, "BAND LEVEL" },
            { "OTT_INPUT", "INPUT" },
            { "OTT_GATE", "GATE" },
            { "OTT_X1", "XOVER 1" },
            { "OTT_X2", "XOVER 2" },
            { "OTT_X3", "XOVER 3" },
            { "OTT_MIX", "MASTER MIX" },
            { "OTT_OUTPUT", "OUTPUT" }
        };
    }
    else if (moduleIndex == 2)
    {
        defs = {
            { "ATYPE_DEGREE1", "TYPE-A B1" },
            { "ATYPE_DEGREE2", "TYPE-A B2" },
            { "ATYPE_DEGREE3", "TYPE-A B3" },
            { "ATYPE_DEGREE4", "TYPE-A B4" },
            { "ATYPE_LEVEL1", "B1 LEVEL" },
            { "ATYPE_LEVEL2", "B2 LEVEL" },
            { "ATYPE_LEVEL3", "B3 LEVEL" },
            { "ATYPE_LEVEL4", "B4 LEVEL" },
            { "ATYPE_ATTACK", "ATTACK" },
            { "ATYPE_RELEASE", "RELEASE" },
            { "ATYPE_INPUT", "INPUT" },
            { "ATYPE_MIX", "MIX" },
            { "ATYPE_OUTPUT", "OUTPUT" }
        };
    }
    else if (moduleIndex == 3)
    {
        defs = {
            { "DEESS_LOW", "LOW XOVER" },
            { "DEESS_HIGH", "HIGH XOVER" },
            { "DEESS_RANGE", "RANGE" },
            { "DEESS_STRENGTH", "STRENGTH" },
            { "DEESS_ATTACK", "ATTACK" },
            { "DEESS_RELEASE", "RELEASE" }
        };
    }
    else
    {
        defs = {
            { "DRY_WET", "DRY / WET" },
            { "OUTPUT_LEVEL", "OUTPUT" }
        };
    }

    for (size_t i = 0; i < defs.size() && i < sliders.size(); ++i)
    {
        sliders[i].setVisible(true);
        labels[i].setVisible(true);
        labels[i].setText(defs[i].label, juce::dontSendNotification);
        attachments[i] = std::make_unique<Attachment>(
            audioProcessor.apvts, defs[i].id, sliders[i]);
    }
}

float VVChainAudioProcessorEditor::graphFrequencyToX(
    const juce::Rectangle<float>& graph, float hz) const
{
    return graph.getX() + graph.getWidth() * juce::jlimit(0.f, 1.f, logMap(hz, 20.f, 20000.f));
}

float VVChainAudioProcessorEditor::graphXToFrequency(
    const juce::Rectangle<float>& graph, float x) const
{
    const float t = juce::jlimit(0.f, 1.f, (x - graph.getX()) / graph.getWidth());
    return fromLogMap(t, 20.f, 20000.f);
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
        0.f, 100.f,
        100.f * (graph.getBottom() - y) / graph.getHeight());
}

void VVChainAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    dragTarget = DragNone;
    const auto graph = graphBounds(*this);

    if (!graph.contains(e.position))
        return;

    const float x = e.position.x;
    const float y = e.position.y;

    if (moduleIndex == 0)
    {
        float nearest = 18.f;
        for (int i = 0; i < 4; ++i)
        {
            const juce::String n = juce::String(i + 1);
            const float f = audioProcessor.apvts.getRawParameterValue("EQ" + n + "_FREQ")->load();
            const float gain = audioProcessor.apvts.getRawParameterValue("EQ" + n + "_GAIN")->load();
            const juce::Point<float> p(
                graphFrequencyToX(graph, f),
                graph.getCentreY() - gain / 36.f * graph.getHeight());

            const float d = p.getDistanceFrom(e.position);
            if (d < nearest)
            {
                nearest = d;
                dragTarget = EqBand1 + i;
            }
        }

        if (dragTarget != DragNone)
            return;
    }
    else if (moduleIndex == 1)
    {
        const float x1 = audioProcessor.apvts.getRawParameterValue("OTT_X1")->load();
        const float x2 = audioProcessor.apvts.getRawParameterValue("OTT_X2")->load();
        const float x3 = audioProcessor.apvts.getRawParameterValue("OTT_X3")->load();

        const float line1 = graphFrequencyToX(graph, x1);
        const float line2 = graphFrequencyToX(graph, x2);
        const float line3 = graphFrequencyToX(graph, x3);

        if (std::abs(x - line1) < 12.f) { dragTarget = OttXover1; return; }
        if (std::abs(x - line2) < 12.f) { dragTarget = OttXover2; return; }
        if (std::abs(x - line3) < 12.f) { dragTarget = OttXover3; return; }

        const float centers[] = {
            std::sqrt(20.f * x1),
            std::sqrt(x1 * x2),
            std::sqrt(x2 * x3),
            std::sqrt(x3 * 18000.f)
        };

        float nearest = 22.f;
        for (int i = 0; i < 4; ++i)
        {
            const float degree = audioProcessor.apvts.getRawParameterValue(
                "OTT_DEGREE" + juce::String(i + 1))->load();
            const juce::Point<float> p(
                graphFrequencyToX(graph, juce::jlimit(20.f, 20000.f, centers[i])),
                graphPercentToY(graph, degree));

            const float d = p.getDistanceFrom(e.position);
            if (d < nearest)
            {
                nearest = d;
                dragTarget = OttDegree1 + i;
            }
        }
    }
    else if (moduleIndex == 2)
    {
        const float centers[] = { 50.f, 490.f, 5200.f, 12000.f };
        float nearest = 24.f;

        for (int i = 0; i < 4; ++i)
        {
            const float degree = audioProcessor.apvts.getRawParameterValue(
                "ATYPE_DEGREE" + juce::String(i + 1))->load();
            const juce::Point<float> p(
                graphFrequencyToX(graph, centers[i]),
                graphPercentToY(graph, degree));

            const float d = p.getDistanceFrom(e.position);
            if (d < nearest)
            {
                nearest = d;
                dragTarget = TypeDegree1 + i;
            }
        }
    }
    else if (moduleIndex == 3)
    {
        const float low = audioProcessor.apvts.getRawParameterValue("DEESS_LOW")->load();
        const float high = audioProcessor.apvts.getRawParameterValue("DEESS_HIGH")->load();

        const float lowX = graphFrequencyToX(graph, low);
        const float highX = graphFrequencyToX(graph, high);

        if (std::abs(x - lowX) < 12.f) { dragTarget = DeessLow; return; }
        if (std::abs(x - highX) < 12.f) { dragTarget = DeessHigh; return; }

        const float strength = audioProcessor.apvts.getRawParameterValue("DEESS_STRENGTH")->load();
        const float range = audioProcessor.apvts.getRawParameterValue("DEESS_RANGE")->load();

        const juce::Point<float> strengthP(
            graph.getCentreX(), graphPercentToY(graph, strength));
        const juce::Point<float> rangeP(
            graph.getCentreX() + 70.f, graph.getY() + 22.f + range / 24.f * 90.f);

        if (strengthP.getDistanceFrom(e.position) < 22.f)
            dragTarget = DeessStrength;
        else if (rangeP.getDistanceFrom(e.position) < 22.f)
            dragTarget = DeessRange;
    }
    else if (moduleIndex == 4)
    {
        const juce::Point<float> dryP(graph.getWidth() * 0.35f + graph.getX(),
                                      graphPercentToY(graph,
                                          audioProcessor.apvts.getRawParameterValue("DRY_WET")->load()));
        const juce::Point<float> outP(graph.getWidth() * 0.65f + graph.getX(),
                                      graphPercentToY(graph,
                                          juce::jmap(
                                              audioProcessor.apvts.getRawParameterValue("OUTPUT_LEVEL")->load(),
                                              -24.f, 12.f, 0.f, 100.f)));

        if (dryP.getDistanceFrom(e.position) < 24.f) dragTarget = MixDryWet;
        else if (outP.getDistanceFrom(e.position) < 24.f) dragTarget = MixOutput;
    }
}

void VVChainAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    const auto graph = graphBounds(*this);
    if (dragTarget == DragNone || !graph.contains(e.position))
        return;

    auto setFloat = [this](const juce::String& id, float value)
    {
        if (auto* p = audioProcessor.apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    };

    const float x = e.position.x;
    const float y = e.position.y;

    if (dragTarget >= EqBand1 && dragTarget <= EqBand4)
    {
        const int i = dragTarget - EqBand1;
        const juce::String n = juce::String(i + 1);
        const float freq = graphXToFrequency(graph, x);
        const float gain = juce::jlimit(-24.f, 24.f,
            graphYToPercent(graph, y) * 0.48f - 24.f);
        setFloat("EQ" + n + "_FREQ", freq);
        setFloat("EQ" + n + "_GAIN", gain);
        selectBand(i);
        return;
    }

    if (dragTarget >= OttDegree1 && dragTarget <= OttDegree4)
    {
        const int i = dragTarget - OttDegree1;
        const float degree = graphYToPercent(graph, y);
        setFloat("OTT_DEGREE" + juce::String(i + 1), degree);
        bandIndex = i;
        rebuildControls();
        return;
    }

    if (dragTarget == OttXover1)
    {
        const float f = juce::jlimit(80.f, 600.f, graphXToFrequency(graph, x));
        setFloat("OTT_X1", f);
        return;
    }

    if (dragTarget == OttXover2)
    {
        const float x1 = audioProcessor.apvts.getRawParameterValue("OTT_X1")->load();
        const float f = juce::jlimit(std::max(750.f, x1 + 80.f), 3000.f,
                                     graphXToFrequency(graph, x));
        setFloat("OTT_X2", f);
        return;
    }

    if (dragTarget == OttXover3)
    {
        const float x2 = audioProcessor.apvts.getRawParameterValue("OTT_X2")->load();
        const float f = juce::jlimit(std::max(6000.f, x2 + 200.f), 12000.f,
                                     graphXToFrequency(graph, x));
        setFloat("OTT_X3", f);
        return;
    }

    if (dragTarget >= TypeDegree1 && dragTarget <= TypeDegree4)
    {
        const int i = dragTarget - TypeDegree1;
        setFloat("ATYPE_DEGREE" + juce::String(i + 1), graphYToPercent(graph, y));
        return;
    }

    if (dragTarget == DeessLow)
    {
        const float f = graphXToFrequency(graph, x);
        const float high = audioProcessor.apvts.getRawParameterValue("DEESS_HIGH")->load();
        setFloat("DEESS_LOW", juce::jlimit(2500.f, high - 100.f, f));
        return;
    }

    if (dragTarget == DeessHigh)
    {
        const float f = graphXToFrequency(graph, x);
        const float low = audioProcessor.apvts.getRawParameterValue("DEESS_LOW")->load();
        setFloat("DEESS_HIGH", juce::jlimit(low + 100.f, 15000.f, f));
        return;
    }

    if (dragTarget == DeessStrength)
    {
        setFloat("DEESS_STRENGTH", graphYToPercent(graph, y));
        return;
    }

    if (dragTarget == DeessRange)
    {
        const float r = juce::jlimit(0.f, 24.f,
            graphYToPercent(graph, y) * 0.24f);
        setFloat("DEESS_RANGE", r);
        return;
    }

    if (dragTarget == MixDryWet)
    {
        setFloat("DRY_WET", graphYToPercent(graph, y));
        return;
    }

    if (dragTarget == MixOutput)
    {
        setFloat("OUTPUT_LEVEL",
                 juce::jmap(graphYToPercent(graph, y), 0.f, 100.f, -24.f, 12.f));
    }
}

void VVChainAudioProcessorEditor::timerCallback()
{
    repaint();
}

void VVChainAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff11100d));

    g.setColour(juce::Colour(0xff2a2924));
    g.fillRect(0, 0, getWidth(), 62);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.f));
    g.drawText("VVChain", 18, 0, 160, 62, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffb7b0a2));
    g.setFont(juce::FontOptions(10.f));
    g.drawText("ANALOG COLOR EQ  |  4-BAND OTT  |  TYPE-A  |  SPLIT-BAND DE-ESSER",
               18, 45, getWidth() - 36, 14, juce::Justification::centredLeft);

    auto graph = graphBounds(*this);
    g.setColour(juce::Colour(0xff171510));
    g.fillRoundedRectangle(graph, 10.f);

    g.setColour(juce::Colour(0xff514d3f).withAlpha(0.55f));
    for (int i = 0; i <= 10; ++i)
    {
        const float x = graph.getX() + graph.getWidth() * i / 10.f;
        g.drawVerticalLine((int)x, graph.getY(), graph.getBottom());
    }
    for (int i = 0; i <= 8; ++i)
    {
        const float y = graph.getY() + graph.getHeight() * i / 8.f;
        g.drawHorizontalLine((int)y, graph.getX(), graph.getRight());
    }

    const std::array<juce::Colour, 4> colors {
        juce::Colour(0xff30a7ff), juce::Colour(0xff26d0c8),
        juce::Colour(0xffd95fff), juce::Colour(0xff83d44d)
    };

    if (moduleIndex == 0)
    {
        for (int band = 0; band < 4; ++band)
        {
            const juce::String n = juce::String(band + 1);
            const float f = audioProcessor.apvts.getRawParameterValue(
                "EQ" + n + "_FREQ")->load();
            const float gain = audioProcessor.apvts.getRawParameterValue(
                "EQ" + n + "_GAIN")->load();
            const float t = juce::jlimit(0.f, 1.f, logMap(f, 20.f, 20000.f));

            const float px = graph.getX() + t * graph.getWidth();
            const float py = graph.getCentreY() - gain / 36.f * graph.getHeight();

            juce::Path path;
            for (int i = 0; i <= 260; ++i)
            {
                const float xNorm = i / 260.f;
                const float bump = gain * 3.f *
                    std::exp(-std::pow((xNorm - t) / 0.075f, 2.f));
                const float y = graph.getCentreY() - bump
                    + 2.5f * std::sin(xNorm * 7.f + (float)band);
                const float x = graph.getX() + xNorm * graph.getWidth();

                if (i == 0) path.startNewSubPath(x, y);
                else path.lineTo(x, y);
            }

            g.setColour(colors[(size_t)band].withAlpha(0.86f));
            g.strokePath(path, juce::PathStrokeType(2.2f));
            drawHandle(g, { px, py }, colors[(size_t)band], band == bandIndex);
        }

        g.setColour(juce::Colour(0xffbcae82));
        g.setFont(juce::FontOptions(10.f));
        g.drawText("ANALOG COLOR", graph.getX() + 12, graph.getY() + 12, 100, 16,
                   juce::Justification::left);
    }
    else if (moduleIndex == 1)
    {
        const float x1 = audioProcessor.apvts.getRawParameterValue("OTT_X1")->load();
        const float x2 = audioProcessor.apvts.getRawParameterValue("OTT_X2")->load();
        const float x3 = audioProcessor.apvts.getRawParameterValue("OTT_X3")->load();
        const float xs[3] = {
            graphFrequencyToX(graph, x1),
            graphFrequencyToX(graph, x2),
            graphFrequencyToX(graph, x3)
        };

        g.setColour(juce::Colour(0xffd5a62f).withAlpha(0.65f));
        for (float xx : xs)
            g.drawVerticalLine((int)xx, graph.getY(), graph.getBottom());

        const float centers[] = {
            std::sqrt(20.f * x1),
            std::sqrt(x1 * x2),
            std::sqrt(x2 * x3),
            std::sqrt(x3 * 18000.f)
        };

        for (int i = 0; i < 4; ++i)
        {
            const float degree = audioProcessor.apvts.getRawParameterValue(
                "OTT_DEGREE" + juce::String(i + 1))->load();
            const float cx = graphFrequencyToX(graph, juce::jlimit(20.f, 20000.f, centers[i]));
            const float cy = graphPercentToY(graph, degree);

            g.setColour(colors[(size_t)i].withAlpha(0.25f));
            g.fillRect(cx - 28.f, cy, 56.f, graph.getBottom() - cy);

            g.setColour(colors[(size_t)i]);
            g.drawLine(cx - 36.f, cy, cx + 36.f, cy, 2.4f);
            drawHandle(g, { cx, cy }, colors[(size_t)i], dragTarget == OttDegree1 + i);
        }

        g.setColour(juce::Colours::white.withAlpha(0.86f));
        g.drawText("LIFTER 6:1 → COMPRESSOR 8:1", graph.getX() + 12, graph.getY() + 12,
                   240, 16, juce::Justification::left);
    }
    else if (moduleIndex == 2)
    {
        const float centers[] = { 50.f, 500.f, 4500.f, 12000.f };
        const float boundaries[] = { 80.f, 3000.f, 9000.f };

        g.setColour(juce::Colour(0xffd5a62f).withAlpha(0.45f));
        for (float f : boundaries)
        {
            const float xx = graphFrequencyToX(graph, f);
            g.drawVerticalLine((int)xx, graph.getY(), graph.getBottom());
        }

        for (int i = 0; i < 4; ++i)
        {
            const float degree = audioProcessor.apvts.getRawParameterValue(
                "ATYPE_DEGREE" + juce::String(i + 1))->load();
            const float cx = graphFrequencyToX(graph, centers[i]);
            const float cy = graphPercentToY(graph, degree);

            g.setColour(colors[(size_t)i].withAlpha(0.22f));
            g.fillRect(cx - 30.f, cy, 60.f, graph.getBottom() - cy);

            g.setColour(colors[(size_t)i]);
            g.drawLine(cx - 38.f, cy, cx + 38.f, cy, 2.4f);
            drawHandle(g, { cx, cy }, colors[(size_t)i], dragTarget == TypeDegree1 + i);
        }

        g.setColour(juce::Colours::white.withAlpha(0.86f));
        g.drawText("TYPE-A  |  80 Hz  /  3 kHz  /  9 kHz  |  OVERLAPPED UPPER BANDS",
                   graph.getX() + 12, graph.getY() + 12, 430, 16,
                   juce::Justification::left);
    }
    else if (moduleIndex == 3)
    {
        const float low = audioProcessor.apvts.getRawParameterValue("DEESS_LOW")->load();
        const float high = audioProcessor.apvts.getRawParameterValue("DEESS_HIGH")->load();
        const float strength = audioProcessor.apvts.getRawParameterValue("DEESS_STRENGTH")->load();
        const float range = audioProcessor.apvts.getRawParameterValue("DEESS_RANGE")->load();

        const float lowX = graphFrequencyToX(graph, low);
        const float highX = graphFrequencyToX(graph, high);

        g.setColour(juce::Colour(0xffd5a62f).withAlpha(0.12f));
        g.fillRect(lowX, graph.getY(), highX - lowX, graph.getHeight());

        g.setColour(juce::Colour(0xffd5a62f));
        g.drawVerticalLine((int)lowX, graph.getY(), graph.getBottom());
        g.drawVerticalLine((int)highX, graph.getY(), graph.getBottom());

        const float sx = graph.getCentreX();
        const float sy = graphPercentToY(graph, strength);
        const float ry = graph.getY() + 22.f + range / 24.f * 90.f;

        drawHandle(g, { sx, sy }, juce::Colour(0xfff2c24b), dragTarget == DeessStrength);
        drawHandle(g, { sx + 70.f, ry }, juce::Colour(0xffd6a64b), dragTarget == DeessRange);

        g.setColour(juce::Colours::white.withAlpha(0.86f));
        g.drawText("DE-ESSER ACTIVE BAND", graph.getX() + 12, graph.getY() + 12,
                   180, 16, juce::Justification::left);
    }
    else if (moduleIndex == 4)
    {
        const float wet = audioProcessor.apvts.getRawParameterValue("DRY_WET")->load();
        const float out = audioProcessor.apvts.getRawParameterValue("OUTPUT_LEVEL")->load();

        const float xDry = graph.getX() + graph.getWidth() * 0.35f;
        const float xOut = graph.getX() + graph.getWidth() * 0.65f;
        const float yDry = graphPercentToY(graph, wet);
        const float yOut = graphPercentToY(
            graph, juce::jmap(out, -24.f, 12.f, 0.f, 100.f));

        g.setColour(juce::Colour(0xff30a7ff).withAlpha(0.25f));
        g.fillRect(xDry - 55.f, yDry, 110.f, graph.getBottom() - yDry);
        g.setColour(juce::Colour(0xff83d44d).withAlpha(0.25f));
        g.fillRect(xOut - 55.f, yOut, 110.f, graph.getBottom() - yOut);

        drawHandle(g, { xDry, yDry }, juce::Colour(0xff30a7ff), dragTarget == MixDryWet);
        drawHandle(g, { xOut, yOut }, juce::Colour(0xff83d44d), dragTarget == MixOutput);

        g.setColour(juce::Colours::white.withAlpha(0.86f));
        g.drawText("DRY / WET", xDry - 50.f, graph.getY() + 12.f, 100, 16,
                   juce::Justification::centred);
        g.drawText("OUTPUT", xOut - 50.f, graph.getY() + 12.f, 100, 16,
                   juce::Justification::centred);
    }
    else
    {
        g.setColour(juce::Colour(0xffc7bca7).withAlpha(0.78f));
        g.setFont(juce::FontOptions(12.f));
        g.drawText("ANALYZER — live spectrum / transient view", graph.getX() + 20.f,
                   graph.getCentreY() - 12.f, graph.getWidth() - 40.f, 24,
                   juce::Justification::centred);
    }

    if (detailOpen && moduleIndex != 5)
    {
        g.setColour(juce::Colours::white.withAlpha(0.68f));
        g.setFont(juce::FontOptions(9.f));
        const char* hint = moduleIndex == 0 ? "上方直接拖 EQ node：Freq + Gain；Q / Color / HF 在下方細控"
                         : moduleIndex == 1 ? "上方拖 4 段 OTT Degree 與 3 條 Xover；下方是 PunkOTT-style advanced controls"
                         : moduleIndex == 2 ? "上方拖 4 段 Type-A Degree；頻段固定符合 Type-A topology"
                         : moduleIndex == 3 ? "上方拖兩個交越點；Strength / Range 直接拖 handle"
                         : "上方拖 Dry/Wet 與 Output";
        g.drawText(hint, (int)graph.getX() + 12, (int)graph.getBottom() - 23,
                   (int)graph.getWidth() - 24, 16, juce::Justification::left);
    }

    if (moduleIndex != 5)
    {
        g.setColour(juce::Colour(0xff211f1b));
        g.fillRoundedRectangle(
            24.f, (float)getHeight() - 220.f,
            (float)getWidth() - 48.f, 196.f, 12.f);
    }
}

void VVChainAudioProcessorEditor::resized()
{
    int x = 205;
    for (auto& b : moduleButtons)
    {
        b.setBounds(x, 13, 128, 34);
        x += 134;
    }

    int bx = 24;
    for (auto& b : bandButtons)
    {
        b.setBounds(bx, 68, 82, 22);
        b.setVisible(moduleIndex == 0);
        bx += 88;
    }

    for (size_t i = 0; i < sliders.size(); ++i)
    {
        sliders[i].setVisible(false);
        labels[i].setVisible(false);
    }

    if (!detailOpen || moduleIndex == 5)
        return;

    const int panelTop = getHeight() - 205;
    const int margin = 32;
    const int cols = 8;
    const int gap = 8;
    const int usable = getWidth() - margin * 2 - gap * (cols - 1);
    const int w = std::max(96, usable / cols);

    int shown = 0;
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        if (!attachments[i] || !sliders[i].isVisible())
            continue;

        const int col = shown % cols;
        const int row = shown / cols;
        sliders[i].setBounds(
            margin + col * (w + gap), panelTop + 54 + row * 76, w, 58);
        labels[i].setBounds(
            margin + col * (w + gap), panelTop + 32 + row * 76, w, 18);
        ++shown;
    }

    if (moduleIndex != 0)
        for (auto& b : bandButtons)
            b.setVisible(false);
}
