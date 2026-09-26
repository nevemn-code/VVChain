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

void SettingsGearButton::setIvoryTheme(bool ivory)
{
    ivoryTheme = ivory;
    repaint();
}

void SettingsGearButton::paintButton(
    juce::Graphics& g, bool highlighted, bool down)
{
    // Use the editor LookAndFeel for the button body. In v1.0.74 that body is
    // the same clean PNG button asset as the rest of the hardware UI.
    getLookAndFeel().drawButtonBackground(
        g, *this, juce::Colours::transparentBlack,
        highlighted, down || panelOpen);

    auto r = getLocalBounds().toFloat().reduced(0.5f);
    const auto icon = ivoryTheme
        ? juce::Colour(0xffffefd2)
        : juce::Colour(0xffeef7f7);

    const float cx = r.getCentreX();
    const float cy = r.getCentreY();
    const float outer = 6.7f;
    const float inner = 2.25f;

    g.setColour(icon);
    for (int i = 0; i < 8; ++i)
    {
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(
            juce::MathConstants<float>::twoPi
                * static_cast<float>(i) / 8.0f,
            cx, cy));
        g.fillRoundedRectangle(
            cx - 1.35f, cy - outer - 1.1f,
            2.7f, 3.1f, 0.7f);
        g.restoreState();
    }

    g.drawEllipse(cx - 5.1f, cy - 5.1f, 10.2f, 10.2f, 1.6f);
    g.drawEllipse(cx - inner, cy - inner,
                  inner * 2.0f, inner * 2.0f, 1.5f);
}

void SettingsDismissOverlay::mouseDown(const juce::MouseEvent&)
{
    if (onDismiss)
        onDismiss();
}

SettingsPanel::Content::Content()
{
    setInterceptsMouseClicks(true, true);
    setSize(310, 937);

    addAndMakeVisible(themeButton);
    addAndMakeVisible(analyzerButton);
    themeButton.setTooltip("UI THEME ONLY · DOES NOT CHANGE AUDIO");
    analyzerButton.setTooltip("BACKGROUND SPECTRUM ANALYZER · UI ONLY");
    themeButton.onClick = [this]
    {
        setIvoryTheme(!ivoryTheme);
        if (onThemeChanged)
            onThemeChanged(ivoryTheme);
    };
    analyzerButton.onClick = [this]
    {
        setAnalyzerEnabled(!analyzerEnabled);
        if (onAnalyzerChanged)
            onAnalyzerChanged(analyzerEnabled);
    };
}

void SettingsPanel::Content::setIvoryTheme(bool ivory)
{
    ivoryTheme = ivory;
    themeButton.setButtonText(ivoryTheme ? "THEME  IVORY GOLD" : "THEME  STUDIO TEAL");
    themeButton.setColour(juce::TextButton::buttonColourId,
        ivoryTheme ? juce::Colour(0xffddd3c5) : juce::Colour(0xff142023));
    themeButton.setColour(juce::TextButton::buttonOnColourId,
        ivoryTheme ? juce::Colour(0xffd3c6b5) : juce::Colour(0xff242a31));
    themeButton.setColour(juce::TextButton::textColourOffId,
        ivoryTheme ? juce::Colour(0xff2a2d31) : juce::Colour(0xffd9e0e7));
    setAnalyzerEnabled(analyzerEnabled);
    repaint();
}

void SettingsPanel::Content::setAnalyzerEnabled(bool enabled)
{
    analyzerEnabled = enabled;
    analyzerButton.setButtonText(analyzerEnabled ? "ANALYZER  ON" : "ANALYZER  OFF");
    analyzerButton.setColour(juce::TextButton::buttonColourId,
        ivoryTheme ? juce::Colour(0xffddd3c5) : juce::Colour(0xff142023));
    analyzerButton.setColour(juce::TextButton::textColourOffId,
        ivoryTheme ? juce::Colour(0xff2a2d31) : juce::Colour(0xffd9e0e7));
    repaint();
}

void SettingsPanel::Content::resized()
{
    themeButton.setBounds(7, 30, getWidth() - 14, 28);
    analyzerButton.setBounds(7, 61, getWidth() - 14, 28);
}

void SettingsPanel::Content::drawRow(juce::Graphics& g, int y,
                                    const juce::String& name,
                                    const juce::String& value,
                                    bool enabled)
{
    auto row = juce::Rectangle<int>(7, y, getWidth() - 14, 28);
    g.setColour(ivoryTheme ? juce::Colour(0xffeee7dc) : juce::Colour(0xff142023));
    g.fillRoundedRectangle(row.toFloat(), 3.0f);
    g.setColour(ivoryTheme ? juce::Colour(0xffc8bdaf) : juce::Colour(0xff343a42));
    g.drawRoundedRectangle(row.toFloat(), 3.0f, 1.0f);

    auto textArea = row.reduced(8, 0);
    auto leftArea = textArea.removeFromLeft(182);

    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.setColour(ivoryTheme
        ? (enabled ? juce::Colour(0xff262a2e) : juce::Colour(0xff68645f))
        : (enabled ? juce::Colour(0xffd9e0e7) : juce::Colour(0xff7e8791)));
    g.drawText(name, leftArea, juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(8.0f));
    g.setColour(ivoryTheme
        ? (enabled ? juce::Colour(0xff4f5358) : juce::Colour(0xff80796f))
        : (enabled ? juce::Colour(0xffbac4cf) : juce::Colour(0xff626b75)));
    g.drawText(value, textArea, juce::Justification::centredRight);
}

void SettingsPanel::Content::drawSection(juce::Graphics& g, int& y,
                                        const juce::String& title,
                                        const juce::StringArray& rows,
                                        bool reserved)
{
    g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
    g.setColour(ivoryTheme
        ? (reserved ? juce::Colour(0xff81796f) : juce::Colour(0xff34383d))
        : (reserved ? juce::Colour(0xff7f8790) : juce::Colour(0xffe5eaf0)));
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
    g.fillAll(ivoryTheme ? juce::Colour(0xfff3eee4) : juce::Colour(0xff101719));
    int y = 8;

    g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
    g.setColour(ivoryTheme ? juce::Colour(0xff34383d) : juce::Colour(0xffe5eaf0));
    g.drawText("INTERFACE", 8, y, getWidth() - 16, 20,
               juce::Justification::centredLeft);
    y = 92;
    for (const auto& row : juce::StringArray {
          "UI Scale|100% · RESERVED",
          "Animation|ON · RESERVED",
          "LED Brightness|100% · RESERVED",
          "Value Popup|ON · RESERVED",
          "Value Popup mouse tunnel|26 px · RESERVED" })
    {
        auto parts = juce::StringArray::fromTokens(row, "|", "");
        drawRow(g, y, parts[0], parts[1], false);
        y += 31;
    }
    y += 7;

    drawSection(g, y, "CONTROL",
        { "Knob Drag Sensitivity|DEFAULT · RESERVED",
          "Mouse Wheel Sensitivity|DEFAULT · RESERVED",
          "Shift Fine Adjust|0.10× · RESERVED",
          "Double-click Reset|ON · RESERVED",
          "EQ Node movement feedback|ON · RESERVED" }, false);

    drawSection(g, y, "ANALYZER / GRAPH",
        { "Main Spectrum|4096 FFT · HIGH",
          "Main Smoothing|PRO-Q STYLE · MEDIUM",
          "Module Contribution Analyzer|REMOVED · CPU SAVE",
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
    content.onThemeChanged = [this](bool ivory)
    {
        setIvoryTheme(ivory);
        if (onThemeChanged)
            onThemeChanged(ivory);
    };
    content.onAnalyzerChanged = [this](bool enabled)
    {
        if (onAnalyzerChanged)
            onAnalyzerChanged(enabled);
    };
    setInterceptsMouseClicks(true, true);
    setOpaque(true);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollOnDragEnabled(true);
    addAndMakeVisible(viewport);
}

void SettingsPanel::setIvoryTheme(bool ivory)
{
    ivoryTheme = ivory;
    content.setIvoryTheme(ivory);
    repaint();
}

void SettingsPanel::setAnalyzerEnabled(bool enabled)
{
    content.setAnalyzerEnabled(enabled);
}

void SettingsPanel::resized()
{
    viewport.setBounds(getLocalBounds().reduced(7, 34).withTrimmedBottom(1));
    content.setSize(juce::jmax(286, viewport.getMaximumVisibleWidth()), 968);
}

void SettingsPanel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(ivoryTheme ? juce::Colour(0xfff3eee4) : juce::Colour(0xff101719));
    g.fillRoundedRectangle(r, 7.0f);
    g.setColour(ivoryTheme ? juce::Colour(0xffa99e91) : juce::Colour(0xff8d5a40));
    g.drawRoundedRectangle(r.reduced(0.5f), 7.0f, 1.0f);

    g.setColour(ivoryTheme ? juce::Colour(0xffe5dccf) : juce::Colour(0xff171b20));
    g.fillRoundedRectangle(juce::Rectangle<float>(1.0f, 1.0f,
        static_cast<float>(getWidth() - 2), 31.0f), 6.0f);

    g.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
    g.setColour(ivoryTheme ? juce::Colour(0xff2a2d31) : juce::Colour(0xffeef2f6));
    g.drawText("SETTINGS", 12, 5, getWidth() - 24, 24,
               juce::Justification::centredLeft);
}
