#pragma once

#include <JuceHeader.h>
#include <functional>

class SettingsGearButton final : public juce::Button
{
public:
    SettingsGearButton();
    void setPanelOpen(bool open);
    void setIvoryTheme(bool ivory);
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    bool panelOpen = false;
    bool ivoryTheme = false;
};

class SettingsDismissOverlay final : public juce::Component
{
public:
    std::function<void()> onDismiss;
    void mouseDown(const juce::MouseEvent&) override;
};

class SettingsPanel final : public juce::Component
{
public:
    SettingsPanel();
    std::function<void(bool)> onThemeChanged;
    void setIvoryTheme(bool ivory);
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    class Content final : public juce::Component
    {
    public:
        Content();
        std::function<void(bool)> onThemeChanged;
        void setIvoryTheme(bool ivory);
        void resized() override;
        void paint(juce::Graphics&) override;

    private:
        void drawSection(juce::Graphics&, int& y, const juce::String& title,
                         const juce::StringArray& rows, bool reserved);
        void drawRow(juce::Graphics&, int y, const juce::String& name,
                     const juce::String& value, bool enabled);
        juce::TextButton themeButton { "THEME  DARK" };
        bool ivoryTheme = false;
    };

    juce::Viewport viewport;
    Content content;
    bool ivoryTheme = false;
};
