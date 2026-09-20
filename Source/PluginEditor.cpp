#include "PluginEditor.h"

VVChainAudioProcessorEditor::VVChainAudioProcessorEditor(VVChainAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setResizable(true, true);
    setSize(1280, 720);
    startTimerHz(30);
}

void VVChainAudioProcessorEditor::timerCallback()
{
    repaint();
}

void VVChainAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xff11100d));

    auto top = b;
    g.setColour(juce::Colour(0xff2a2924));
    g.fillRect(top.removeFromTop(54.0f));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(19.0f));
    g.drawText("VVChain", 18, 0, 180, 54, juce::Justification::centredLeft);

    const char* modes[] = { "GENERAL", "OTT", "A-TYPE", "DE-ESSER", "ANALYZER" };
    float x = 350.f;
    for (auto mode : modes)
    {
        g.setColour(juce::Colour(0xff3c3a35));
        g.fillRoundedRectangle(x, 9.f, 118.f, 36.f, 7.f);
        g.setColour(juce::Colours::white);
        g.drawText(mode, x, 9.f, 118.f, 36.f, juce::Justification::centred);
        x += 126.f;
    }

    auto graph = juce::Rectangle<float>(44.f, 82.f, getWidth() - 88.f, getHeight() - 300.f);
    g.setColour(juce::Colour(0xff171510));
    g.fillRoundedRectangle(graph, 10.f);

    g.setColour(juce::Colour(0xff514d3f).withAlpha(0.55f));
    for (int i = 0; i < 11; ++i)
    {
        auto xx = graph.getX() + graph.getWidth() * i / 10.0f;
        g.drawVerticalLine((int)xx, graph.getY(), graph.getBottom());
    }
    for (int i = 0; i < 9; ++i)
    {
        auto yy = graph.getY() + graph.getHeight() * i / 8.0f;
        g.drawHorizontalLine((int)yy, graph.getX(), graph.getRight());
    }

    const std::array<juce::Colour, 4> cols {
        juce::Colour(0xff30a7ff), juce::Colour(0xff26d0c8),
        juce::Colour(0xffd95fff), juce::Colour(0xff83d44d)
    };

    for (int band = 0; band < 4; ++band)
    {
        juce::Path path;
        const float center = 0.14f + 0.22f * band;
        for (int i = 0; i <= 300; ++i)
        {
            const float t = (float)i / 300.f;
            const float y = graph.getCentreY() - (float)band * 4.f
                - 72.f * std::exp(-std::pow((t - center) / 0.085f, 2.0f))
                + 14.f * std::sin(t * 7.0f + band);
            const float xx = graph.getX() + graph.getWidth() * t;
            const float yy = juce::jlimit(graph.getY(), graph.getBottom(), y);
            if (i == 0) path.startNewSubPath(xx, yy); else path.lineTo(xx, yy);
        }
        g.setColour(cols[(size_t)band].withAlpha(0.85f));
        g.strokePath(path, juce::PathStrokeType(2.3f));
    }

    juce::Path spectrum;
    spectrum.startNewSubPath(graph.getX(), graph.getBottom() - 45.f);
    for (int i = 0; i <= 300; ++i)
    {
        const float t = (float)i / 300.f;
        const float yy = graph.getBottom() - 30.f
            - 80.f * std::abs(std::sin(10.f * t))
            - 38.f * std::abs(std::sin(33.f * t + 0.8f))
            - 24.f * std::abs(std::sin(73.f * t));
        const float xx = graph.getX() + graph.getWidth() * t;
        spectrum.lineTo(xx, juce::jlimit(graph.getY() + 20.f, graph.getBottom() - 8.f, yy));
    }
    spectrum.lineTo(graph.getRight(), graph.getBottom());
    spectrum.lineTo(graph.getX(), graph.getBottom());
    spectrum.closeSubPath();
    g.setColour(juce::Colour(0xff6d3040).withAlpha(0.28f));
    g.fillPath(spectrum);

    const float nodesX[4] = { 0.11f, 0.33f, 0.61f, 0.84f };
    const float nodesDb[4] = { 0.15f, -0.05f, 0.08f, -0.03f };
    for (int i = 0; i < 4; ++i)
    {
        const float px = graph.getX() + graph.getWidth() * nodesX[i];
        const float py = graph.getCentreY() - 155.f * nodesDb[i];
        g.setColour(cols[(size_t)i]);
        g.fillEllipse(px - 9.f, py - 9.f, 18.f, 18.f);
        g.setColour(juce::Colours::white.withAlpha(0.65f));
        g.drawEllipse(px - 10.f, py - 10.f, 20.f, 20.f, 1.5f);
    }

    auto panel = juce::Rectangle<float>(44.f, (float)getHeight() - 204.f,
                                        (float)getWidth() - 88.f, 170.f);
    g.setColour(juce::Colour(0xff211f1b));
    g.fillRoundedRectangle(panel, 12.f);

    const char* modules[] = { "EQ", "OTT", "A-TYPE", "DE-ESSER", "MIX", "LEVEL" };
    float mx = panel.getX() + 16.f;
    for (int i = 0; i < 6; ++i)
    {
        g.setColour(juce::Colour(0xff2e2c26));
        g.fillRoundedRectangle(mx, panel.getY() + 12.f, 92.f, 28.f, 6.f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(13.f));
        g.drawText(modules[i], mx, panel.getY() + 12.f, 92.f, 28.f, juce::Justification::centred);
        mx += 102.f;
    }

    const char* knobs[] = { "FREQ", "GAIN", "Q", "OTT DEPTH", "A-TYPE", "DE-ESS" };
    const char* values[] = { "250", "+1.2", "0.71", "52%", "24%", "6.0 dB" };
    float kx = panel.getX() + 18.f;
    for (int i = 0; i < 6; ++i)
    {
        const float cx = kx + 48.f;
        const float cy = panel.getY() + 103.f;
        g.setColour(juce::Colour(0xff0d0c0a));
        g.fillEllipse(cx - 34.f, cy - 34.f, 68.f, 68.f);
        g.setColour(juce::Colour(0xff5a5648));
        g.drawEllipse(cx - 34.f, cy - 34.f, 68.f, 68.f, 2.f);
        g.setColour(juce::Colour(0xffd5a62f));
        g.drawLine(cx, cy - 27.f, cx + 7.f, cy - 16.f, 3.f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(11.f));
        g.drawText(knobs[i], kx, panel.getY() + 122.f, 96.f, 18.f, juce::Justification::centred);
        g.setFont(juce::FontOptions(12.f));
        g.drawText(values[i], kx, panel.getY() + 141.f, 96.f, 18.f, juce::Justification::centred);
        kx += 112.f;
    }
}

void VVChainAudioProcessorEditor::resized()
{
}
