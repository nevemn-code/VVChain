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
    return { 36.f, 76.f, (float)c.getWidth() - 72.f, (float)c.getHeight() - 288.f };
}
}

VVChainAudioProcessorEditor::VVChainAudioProcessorEditor(VVChainAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setResizable(true, true);
    setSize(1280, 820);

    const char* modules[] = { "EQ", "OTT", "A-TYPE", "DE-ESSER", "MIX", "ANALYZER" };
    for (int i = 0; i < (int)moduleButtons.size(); ++i)
    {
        moduleButtons[(size_t)i].setButtonText(modules[i]);
        moduleButtons[(size_t)i].setClickingTogglesState(false);
        moduleButtons[(size_t)i].onClick = [this, i] { selectModule(i); };
        addAndMakeVisible(moduleButtons[(size_t)i]);
    }

    const char* bands[] = { "BAND 1", "BAND 2", "BAND 3", "BAND 4" };
    for (int i = 0; i < (int)bandButtons.size(); ++i)
    {
        bandButtons[(size_t)i].setButtonText(bands[i]);
        bandButtons[(size_t)i].setClickingTogglesState(false);
        bandButtons[(size_t)i].onClick = [this, i] { selectBand(i); };
        addAndMakeVisible(bandButtons[(size_t)i]);
    }

    for (size_t i = 0; i < sliders.size(); ++i)
    {
        sliders[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        sliders[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 18);
        labels[i].setJustificationType(juce::Justification::centred);
        labels[i].setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
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
    for (auto& a : attachments) a.reset();

    for (size_t i = 0; i < sliders.size(); ++i)
    {
        sliders[i].setVisible(false);
        labels[i].setVisible(false);
    }

    if (moduleIndex == 5)
        return;

    std::vector<ControlDef> defs;

    if (moduleIndex == 0)
    {
        const auto n = juce::String(bandIndex + 1);
        defs = {
            { "EQ" + n + "_FREQ", "FREQ" },
            { "EQ" + n + "_GAIN", "GAIN" },
            { "EQ" + n + "_Q", "Q" },
            { "HF_CORNER", "HF / HPF" }
        };
    }
    else if (moduleIndex == 1)
    {
        defs = {
            { "OTT_DEPTH", "DEPTH" },
            { "OTT_MIX", "MIX" },
            { "OTT_THRESHOLD", "THRESHOLD" },
            { "OTT_UP_RATIO", "UP RATIO" },
            { "OTT_DOWN_RATIO", "DOWN RATIO" },
            { "OTT_ATTACK", "ATTACK" },
            { "OTT_RELEASE", "RELEASE" },
            { "OTT_X1", "XOVER 1" },
            { "OTT_X2", "XOVER 2" },
            { "OTT_X3", "XOVER 3" },
            { "OTT_POST", "POST GAIN" }
        };
    }
    else if (moduleIndex == 2)
    {
        defs = {
            { "ATYPE_AMOUNT", "AMOUNT" },
            { "ATYPE_DRIVE", "DRIVE" },
            { "ATYPE_BIAS", "BIAS" },
            { "ATYPE_MIX", "MIX" },
            { "ATYPE_TONE", "TONE" },
            { "ATYPE_HPF", "HPF" }
        };
    }
    else if (moduleIndex == 3)
    {
        defs = {
            { "DEESS_FREQ", "FREQ" },
            { "DEESS_Q", "Q" },
            { "DEESS_THRESHOLD", "THRESHOLD" },
            { "DEESS_RANGE", "RANGE" },
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

        attachments[i] = std::make_unique<Attachment>(audioProcessor.apvts, defs[i].id, sliders[i]);
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
    g.fillRect(0, 0, getWidth(), 58);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(19.f));
    g.drawText("VVChain", 18, 0, 180, 58, juce::Justification::centredLeft);

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
        const auto n = juce::String(band + 1);
        const float f = audioProcessor.apvts.getRawParameterValue("EQ" + n + "_FREQ")->load();
        const float gain = audioProcessor.apvts.getRawParameterValue("EQ" + n + "_GAIN")->load();
        const float t = juce::jlimit(0.f, 1.f,
            std::log10(std::max(20.f, f) / 20.f) / std::log10(1000.f));

        const float px = graph.getX() + t * graph.getWidth();
        const float py = graph.getCentreY() - juce::jlimit(-18.f, 18.f, gain) / 36.f * graph.getHeight();

        juce::Path path;
        for (int i = 0; i <= 240; ++i)
        {
            const float xNorm = i / 240.f;
            const float y = graph.getCentreY()
                - gain * 3.f * std::exp(-std::pow((xNorm - t) / 0.075f, 2.f))
                + 3.f * std::sin(xNorm * 7.f + (float)band);
            const float x = graph.getX() + xNorm * graph.getWidth();

            if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }

        g.setColour(colors[(size_t)band].withAlpha(0.86f));
        g.strokePath(path, juce::PathStrokeType(2.2f));
        g.setColour(colors[(size_t)band]);
        g.fillEllipse(px - 9.f, py - 9.f, 18.f, 18.f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.75f));
    g.setFont(juce::FontOptions(10.f));
    g.drawText("EQ nodes = visual reference; detailed control is in the module panel below.",
               (int)graph.getX() + 10, (int)graph.getBottom() - 24,
               (int)graph.getWidth() - 20, 18, juce::Justification::centredLeft);

    auto panel = juce::Rectangle<float>(24.f, (float)getHeight() - 226.f,
                                        (float)getWidth() - 48.f, 206.f);
    g.setColour(juce::Colour(0xff211f1b));
    g.fillRoundedRectangle(panel, 12.f);
}

void VVChainAudioProcessorEditor::resized()
{
    int x = 220;
    for (auto& b : moduleButtons)
    {
        b.setBounds(x, 10, 108, 36);
        x += 114;
    }

    int bx = 24;
    for (auto& b : bandButtons)
    {
        b.setBounds(bx, 50, 82, 24);
        b.setVisible(moduleIndex == 0);
        bx += 88;
    }

    const int panelTop = getHeight() - 212;
    const int margin = 28;
    const int cols = 6;
    const int gap = 8;
    const int usable = getWidth() - margin * 2 - gap * (cols - 1);
    const int w = std::max(110, usable / cols);

    int shown = 0;
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        if (!sliders[i].isVisible())
            continue;

        const int col = shown % cols;
        const int row = shown / cols;
        sliders[i].setBounds(margin + col * (w + gap), panelTop + 42 + row * 82, w, 62);
        labels[i].setBounds(margin + col * (w + gap), panelTop + 22 + row * 82, w, 18);
        ++shown;
    }
}
