#include "SettingsPanel.h"

#ifndef VVCHAIN_GIT_COMMIT
#define VVCHAIN_GIT_COMMIT "unknown"
#endif

SettingsGearButton::SettingsGearButton()
    : juce::Button("SETTINGS")
{
    setTooltip("SETTINGS");
    setWantsKeyboardFocus(false);
}

void SettingsGearButton::setPanelOpen(bool open)
{
    if (panelOpen == open)
        return;
    panelOpen = open;
    repaint();
}

void SettingsGearButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    const bool active = panelOpen || down;
    const auto bg = active ? juce::Colour(0xff30353d)
                           : juce::Colour(0xff171a1f);
    const auto border = highlighted || active ? juce::Colour(0xff7d8792)
                                              : juce::Colour(0xff454b53);
    const auto icon = highlighted || active ? juce::Colour(0xffedf2f7)
                                            : juce::Colour(0xff8d96a0);

    g.setColour(bg);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(border);
    g.drawRoundedRectangle(r, 4.0f, 1.0f);

    const float cx = r.getCentreX();
    const float cy = r.getCentreY();
    const float outer = 6.7f;
    const float inner = 2.25f;

    g.setColour(icon);
    for (int i = 0; i < 8; ++i)
    {
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(
            juce::MathConstants<float>::twoPi * static_cast<float>(i) / 8.0f,
            cx, cy));
        g.fillRoundedRectangle(cx - 1.35f, cy - outer - 1.1f,
                               2.7f, 3.1f, 0.7f);
        g.restoreState();
    }

    g.drawEllipse(cx - 5.1f, cy - 5.1f, 10.2f, 10.2f, 1.6f);
    g.drawEllipse(cx - inner, cy - inner, inner * 2.0f, inner * 2.0f, 1.5f);
}

void SettingsDismissOverlay::mouseDown(const juce::MouseEvent&)
{
    if (onDismiss)
        onDismiss();
}

SettingsPanel::Content::Content()
{
    setInterceptsMouseClicks(true, true);
    setSize(310, 842);
}

void SettingsPanel::Content::drawRow(juce::Graphics& g, int y,
                                    const juce::String& name,
                                    const juce::String& value,
                                    bool enabled)
{
    auto row = juce::Rectangle<int>(7, y, getWidth() - 14, 28);
    g.setColour(juce::Colour(0xff181c21));
    g.fillRoundedRectangle(row.toFloat(), 3.0f);
    g.setColour(juce::Colour(0xff343a42));
    g.drawRoundedRectangle(row.toFloat(), 3.0f, 1.0f);

    auto textArea = row.reduced(8, 0);
    auto leftArea = textArea.removeFromLeft(182);

    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.setColour(enabled ? juce::Colour(0xffd9e0e7) : juce::Colour(0xff7e8791));
    g.drawText(name, leftArea, juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(8.0f));
    g.setColour(enabled ? juce::Colour(0xffbac4cf) : juce::Colour(0xff626b75));
    g.drawText(value, textArea, juce::Justification::centredRight);
}

void SettingsPanel::Content::drawSection(juce::Graphics& g, int& y,
                                        const juce::String& title,
                                        const juce::StringArray& rows,
                                        bool reserved)
{
    g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
    g.setColour(reserved ? juce::Colour(0xff7f8790) : juce::Colour(0xffe5eaf0));
    g.drawText(title, 8, y, getWidth() - 16, 20,
               juce::Justification::centredLeft);
    y += 22;

    for (const auto& row : rows)
    {
        auto parts = juce::StringArray::fromTokens(row, "|", "");
        const auto name = parts.size() > 0 ? parts[0] : row;
        const auto value = parts.size() > 1 ? parts[1] : "DISABLED";
        drawRow(g, y, name, value, false);
        y += 31;
    }
    y += 7;
}

void SettingsPanel::Content::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111419));
    int y = 8;

    drawSection(g, y, "INTERFACE",
        { "UI Scale|100% · RESERVED",
          "Animation|ON · RESERVED",
          "LED Brightness|100% · RESERVED",
          "Value Popup|ON · RESERVED",
          "Value Popup mouse tunnel|26 px · RESERVED" }, false);

    drawSection(g, y, "CONTROL",
        { "Knob Drag Sensitivity|DEFAULT · RESERVED",
          "Mouse Wheel Sensitivity|DEFAULT · RESERVED",
          "Shift Fine Adjust|0.10× · RESERVED",
          "Double-click Reset|ON · RESERVED",
          "EQ Node movement feedback|ON · RESERVED" }, false);

    drawSection(g, y, "ANALYZER / GRAPH",
        { "Analyzer|OFF · RESERVED",
          "Analyzer Speed|RESERVED",
          "Spectrum Smoothing|RESERVED",
          "Peak Hold|RESERVED",
          "Grid|ON · RESERVED",
          "Spectrum opacity|RESERVED" }, false);

    drawSection(g, y, "AUDIO / QUALITY",
        { "Oversampling|RESERVED",
          "HQ / ECO|RESERVED",
          "Nonlinear processing quality|RESERVED",
          "CPU / latency display|RESERVED" }, true);

    drawSection(g, y, "SYSTEM / ABOUT",
        { "VVChain Version|" + juce::String(JucePlugin_VersionString),
          "Build number|" + juce::String(JucePlugin_VersionCode),
          "Commit|" + juce::String(VVCHAIN_GIT_COMMIT),
          "Reset UI Settings|DISABLED",
          "Reset All Settings|DISABLED",
          "Settings persistence|RESERVED" }, false);
}

SettingsPanel::SettingsPanel()
{
    setInterceptsMouseClicks(true, true);
    setOpaque(true);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollOnDragEnabled(true);
    addAndMakeVisible(viewport);
}

void SettingsPanel::resized()
{
    viewport.setBounds(getLocalBounds().reduced(7, 34).withTrimmedBottom(1));
    content.setSize(juce::jmax(286, viewport.getMaximumVisibleWidth()), 842);
}

void SettingsPanel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff111419));
    g.fillRoundedRectangle(r, 7.0f);
    g.setColour(juce::Colour(0xff525a64));
    g.drawRoundedRectangle(r.reduced(0.5f), 7.0f, 1.0f);

    g.setColour(juce::Colour(0xff171b20));
    g.fillRoundedRectangle(juce::Rectangle<float>(1.0f, 1.0f,
        static_cast<float>(getWidth() - 2), 31.0f), 6.0f);

    g.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
    g.setColour(juce::Colour(0xffeef2f6));
    g.drawText("SETTINGS", 12, 5, getWidth() - 24, 24,
               juce::Justification::centredLeft);
}
