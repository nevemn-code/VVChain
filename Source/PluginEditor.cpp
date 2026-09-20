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
    detailOpen = (moduleIndex == 0 || moduleIndex == 4);

    for (int i = 0; i < (int)moduleButtons.size(); ++i)
    {
        const bool active = i == moduleIndex;
        moduleButtons[(size_t)i].setColour(
            juce::TextButton::buttonColourId,
            active ? juce::Colour(0xff5d5649) : juce::Colour(0xff38362f));
    }

    rebuildControls();
    resized();
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
        defs = {
            { "OTT_B1", "OTT B1" },
            { "OTT_B2", "OTT B2" },
            { "OTT_B3", "OTT B3" },
            { "OTT_B4", "OTT B4" },
            { "OTT_THRESHOLD", "THRESHOLD" },
            { "OTT_UP_RATIO", "UP RATIO" },
            { "OTT_DOWN_RATIO", "DOWN RATIO" },
            { "OTT_ATTACK", "ATTACK" },
            { "OTT_RELEASE", "RELEASE" },
            { "OTT_X1", "XOVER 1" },
            { "OTT_X2", "XOVER 2" },
            { "OTT_X3", "XOVER 3" },
            { "OTT_MIX", "MIX" },
            { "OTT_INPUT", "INPUT GAIN" },
            { "OTT_POST", "POST GAIN" }
        };
    }
    else if (moduleIndex == 2)
    {
        defs = {
            { "ATYPE_B1", "TYPE-A B1" },
            { "ATYPE_B2", "TYPE-A B2" },
            { "ATYPE_B3", "TYPE-A B3" },
            { "ATYPE_B4", "TYPE-A B4" },
            { "ATYPE_GAIN1", "B1 GAIN" },
            { "ATYPE_GAIN2", "B2 GAIN" },
            { "ATYPE_GAIN3", "B3 GAIN" },
            { "ATYPE_GAIN4", "B4 GAIN" },
            { "ATYPE_ATTACK", "ATTACK" },
            { "ATYPE_RELEASE", "RELEASE" },
            { "ATYPE_MIX", "MIX" }
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

    for (int band = 0; band < 4; ++band)
    {
        const juce::String n = juce::String(band + 1);
        const float f = audioProcessor.apvts.getRawParameterValue("EQ" + n + "_FREQ")->load();
        const float gain = audioProcessor.apvts.getRawParameterValue("EQ" + n + "_GAIN")->load();
        const float t = juce::jlimit(0.f, 1.f,
            std::log10(std::max(20.f, f) / 20.f) / std::log10(1000.f));

        const float px = graph.getX() + t * graph.getWidth();
        const float py = graph.getCentreY() - juce::jlimit(-18.f, 18.f, gain)
            / 36.f * graph.getHeight();

        juce::Path path;
        for (int i = 0; i <= 260; ++i)
        {
            const float xNorm = i / 260.f;
            const float bump = gain * 3.0f
                * std::exp(-std::pow((xNorm - t) / 0.075f, 2.f));
            const float y = graph.getCentreY() - bump
                + 2.5f * std::sin(xNorm * 7.f + (float)band);
            const float x = graph.getX() + xNorm * graph.getWidth();

            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        g.setColour(colors[(size_t)band].withAlpha(0.86f));
        g.strokePath(path, juce::PathStrokeType(2.2f));
        g.setColour(colors[(size_t)band]);
        g.fillEllipse(px - 9.f, py - 9.f, 18.f, 18.f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.72f));
    g.setFont(juce::FontOptions(10.f));
    g.drawText(
        "Four manual bands + non-linear analog coloration. UI language follows the ear-first / manual-search idea of flowEQ.",
        (int)graph.getX() + 12, (int)graph.getBottom() - 24,
        (int)graph.getWidth() - 24, 18, juce::Justification::centredLeft);

    if (moduleIndex == 5)
    {
        g.setColour(juce::Colour(0xff211f1b));
        g.fillRoundedRectangle(
            24.f, (float)getHeight() - 208.f,
            (float)getWidth() - 48.f, 184.f, 12.f);

        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawText(
            "ANALYZER  — live spectrum / transient view reserved for the web + native analyzer stage",
            42, getHeight() - 178, getWidth() - 84, 24, juce::Justification::centred);
    }
    else if (detailOpen)
    {
        g.setColour(juce::Colour(0xff211f1b));
        g.fillRoundedRectangle(
            24.f, (float)getHeight() - 220.f,
            (float)getWidth() - 48.f, 196.f, 12.f);
    }
    else
    {
        g.setColour(juce::Colour(0xff211f1b));
        g.fillRoundedRectangle(
            24.f, (float)getHeight() - 120.f,
            (float)getWidth() - 48.f, 96.f, 12.f);

        g.setColour(juce::Colours::white.withAlpha(0.82f));
        g.setFont(juce::FontOptions(12.f));
        const char* text = moduleIndex == 1 ? "OTT — press the button to open 4-band OTT controls"
                         : moduleIndex == 2 ? "TYPE-A — press the button to open 4-band Type-A controls"
                         : moduleIndex == 3 ? "DE-ESSER — press the button to open crossover / range / strength controls"
                         : "Press a module button to open its detailed controls";
        g.drawText(text, 42, getHeight() - 90, getWidth() - 84, 24,
                   juce::Justification::centred);
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

    if (moduleIndex == 1 || moduleIndex == 2 || moduleIndex == 3)
        for (auto& b : bandButtons)
            b.setVisible(false);
}
