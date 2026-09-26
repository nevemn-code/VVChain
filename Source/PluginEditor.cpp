#include "PluginEditor.h"
#include "VVChain_DynEQ_Engine.h"
#if VVCHAIN_HAS_MASTER_LOCK_UI
#include "VVChainMasterAssets.h"
#else
#include "VVChainAssets.h"
#endif

namespace
{
const std::array<juce::Colour, 4> kBandColours
{
    juce::Colour(0xffef4444), // red
    juce::Colour(0xfffacc15), // yellow
    juce::Colour(0xff3b82f6), // blue
    juce::Colour(0xff22c55e)  // green
};

float logMap(float value, float min, float max)
{
    return std::log(juce::jlimit(min, max, value) / min) / std::log(max / min);
}

float invLogMap(float t, float min, float max)
{
    return min * std::pow(max / min, juce::jlimit(0.f, 1.f, t));
}
}


juce::Image VVChainAudioProcessorEditor::MetalLookAndFeel::loadImage(
    const void* data, int size)
{
    return juce::ImageCache::getFromMemory(data, size);
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawImage(
    juce::Graphics& g, const juce::Image& image,
    juce::Rectangle<float> bounds, float opacity)
{
    if (!image.isValid())
        return;

    g.setOpacity(opacity);
    g.drawImage(image, bounds, juce::RectanglePlacement::stretchToFit);
    g.setOpacity(1.0f);
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawKnobFrame(
    juce::Graphics& g, const juce::Image& strip, int frameIndex,
    juce::Rectangle<float> bounds, float opacity)
{
    if (!strip.isValid())
        return;

    constexpr int columns = 8;
    constexpr int rows = 8;
    const int frameWidth = strip.getWidth() / columns;
    const int frameHeight = strip.getHeight() / rows;
    constexpr int frameCount = columns * rows;

    jassert(frameWidth > 0 && frameHeight > 0);
    jassert(strip.getWidth() == frameWidth * columns);
    jassert(strip.getHeight() == frameHeight * rows);

    frameIndex = juce::jlimit(0, frameCount - 1, frameIndex);
    const int sx = (frameIndex % columns) * frameWidth;
    const int sy = (frameIndex / columns) * frameHeight;

    g.setOpacity(opacity);
    g.drawImage(strip,
                (int) std::round(bounds.getX()),
                (int) std::round(bounds.getY()),
                (int) std::round(bounds.getWidth()),
                (int) std::round(bounds.getHeight()),
                sx, sy, frameWidth, frameHeight, false);
    g.setOpacity(1.0f);
}

VVChainAudioProcessorEditor::MetalLookAndFeel::MetalLookAndFeel()
#if !VVCHAIN_HAS_MASTER_LOCK_UI
{
#define VV_IMG(TARGET, PREFIX, MEMBER, FILE) \
    TARGET.MEMBER = loadImage( \
        VVChainAssets::PREFIX##_##FILE##_png, \
        VVChainAssets::PREFIX##_##FILE##_pngSize)

    VV_IMG(studioAssets, studio, panel, panel);
    VV_IMG(studioAssets, studio, module, module);
    VV_IMG(studioAssets, studio, graph, graph);
    VV_IMG(studioAssets, studio, knobStrip, knob_strip);
    VV_IMG(studioAssets, studio, buttonOff, button_off);
    VV_IMG(studioAssets, studio, buttonOn, button_on);
    VV_IMG(studioAssets, studio, buttonPressed, button_pressed);
    VV_IMG(studioAssets, studio, ledOff, led_off);
    VV_IMG(studioAssets, studio, ledOn, led_on);
    VV_IMG(studioAssets, studio, powerOff, power_off);
    VV_IMG(studioAssets, studio, powerOn, power_on);
    VV_IMG(studioAssets, studio, screw, screw);
    VV_IMG(studioAssets, studio, sliderTrack, slider_track);
    VV_IMG(studioAssets, studio, sliderThumb, slider_thumb);

    VV_IMG(ivoryAssets, ivory, panel, panel);
    VV_IMG(ivoryAssets, ivory, module, module);
    VV_IMG(ivoryAssets, ivory, graph, graph);
    VV_IMG(ivoryAssets, ivory, knobStrip, knob_strip);
    VV_IMG(ivoryAssets, ivory, buttonOff, button_off);
    VV_IMG(ivoryAssets, ivory, buttonOn, button_on);
    VV_IMG(ivoryAssets, ivory, buttonPressed, button_pressed);
    VV_IMG(ivoryAssets, ivory, ledOff, led_off);
    VV_IMG(ivoryAssets, ivory, ledOn, led_on);
    VV_IMG(ivoryAssets, ivory, powerOff, power_off);
    VV_IMG(ivoryAssets, ivory, powerOn, power_on);
    VV_IMG(ivoryAssets, ivory, screw, screw);
    VV_IMG(ivoryAssets, ivory, sliderTrack, slider_track);
    VV_IMG(ivoryAssets, ivory, sliderThumb, slider_thumb);

    VV_IMG(mutedAssets, muted, panel, panel);
    VV_IMG(mutedAssets, muted, module, module);
    VV_IMG(mutedAssets, muted, graph, graph);
    VV_IMG(mutedAssets, muted, knobStrip, knob_strip);
    VV_IMG(mutedAssets, muted, buttonOff, button_off);
    VV_IMG(mutedAssets, muted, buttonOn, button_on);
    VV_IMG(mutedAssets, muted, buttonPressed, button_pressed);
    VV_IMG(mutedAssets, muted, ledOff, led_off);
    VV_IMG(mutedAssets, muted, ledOn, led_on);
    VV_IMG(mutedAssets, muted, powerOff, power_off);
    VV_IMG(mutedAssets, muted, powerOn, power_on);
    VV_IMG(mutedAssets, muted, screw, screw);
    VV_IMG(mutedAssets, muted, sliderTrack, slider_track);
    VV_IMG(mutedAssets, muted, sliderThumb, slider_thumb);
#undef VV_IMG

    disabledButton = loadImage(
        VVChainAssets::button_disabled_png,
        VVChainAssets::button_disabled_pngSize);
    bypassLedRed = loadImage(
        VVChainAssets::bypass_led_red_png,
        VVChainAssets::bypass_led_red_pngSize);

#endif

#if VVCHAIN_HAS_MASTER_LOCK_UI
#define VVCHAIN_MASTER_IMG(MEMBER, FILE) \
    MEMBER = loadImage( \
        VVChainMasterAssets::FILE##_png, \
        VVChainMasterAssets::FILE##_pngSize)

    VVCHAIN_MASTER_IMG(blackFullPanel, black_full_panel);
    VVCHAIN_MASTER_IMG(ivoryFullPanel, ivory_full_panel);
    VVCHAIN_MASTER_IMG(knobGold, knob_gold_64);
    VVCHAIN_MASTER_IMG(knobBlue, knob_blue_64);
    VVCHAIN_MASTER_IMG(knobGreen, knob_green_64);
    VVCHAIN_MASTER_IMG(knobRed, knob_red_64);
    VVCHAIN_MASTER_IMG(knobBlack, knob_black_64);
    VVCHAIN_MASTER_IMG(knobSilver, knob_silver_64);
    VVCHAIN_MASTER_IMG(knobPlatinumMaster, knob_platinum_master_gain_64);

    studioAssets.buttonOff = loadImage(
        VVChainMasterAssets::black_button_off_png,
        VVChainMasterAssets::black_button_off_pngSize);
    studioAssets.buttonOn = loadImage(
        VVChainMasterAssets::black_button_on_png,
        VVChainMasterAssets::black_button_on_pngSize);
    studioAssets.buttonPressed = loadImage(
        VVChainMasterAssets::black_button_pressed_png,
        VVChainMasterAssets::black_button_pressed_pngSize);
    studioAssets.ledOff = loadImage(
        VVChainMasterAssets::black_led_off_png,
        VVChainMasterAssets::black_led_off_pngSize);
    studioAssets.ledOn = loadImage(
        VVChainMasterAssets::black_led_on_png,
        VVChainMasterAssets::black_led_on_pngSize);
    studioAssets.screw = loadImage(
        VVChainMasterAssets::black_screw_png,
        VVChainMasterAssets::black_screw_pngSize);
    studioAssets.sliderTrack = loadImage(
        VVChainMasterAssets::black_slider_track_png,
        VVChainMasterAssets::black_slider_track_pngSize);
    studioAssets.sliderThumb = loadImage(
        VVChainMasterAssets::black_slider_thumb_png,
        VVChainMasterAssets::black_slider_thumb_pngSize);

    ivoryAssets.buttonOff = loadImage(
        VVChainMasterAssets::ivory_button_off_png,
        VVChainMasterAssets::ivory_button_off_pngSize);
    ivoryAssets.buttonOn = loadImage(
        VVChainMasterAssets::ivory_button_on_png,
        VVChainMasterAssets::ivory_button_on_pngSize);
    ivoryAssets.buttonPressed = loadImage(
        VVChainMasterAssets::ivory_button_pressed_png,
        VVChainMasterAssets::ivory_button_pressed_pngSize);
    ivoryAssets.ledOff = loadImage(
        VVChainMasterAssets::ivory_led_off_png,
        VVChainMasterAssets::ivory_led_off_pngSize);
    ivoryAssets.ledOn = loadImage(
        VVChainMasterAssets::ivory_led_on_png,
        VVChainMasterAssets::ivory_led_on_pngSize);
    ivoryAssets.screw = loadImage(
        VVChainMasterAssets::ivory_screw_png,
        VVChainMasterAssets::ivory_screw_pngSize);
    ivoryAssets.sliderTrack = loadImage(
        VVChainMasterAssets::ivory_slider_track_png,
        VVChainMasterAssets::ivory_slider_track_pngSize);
    ivoryAssets.sliderThumb = loadImage(
        VVChainMasterAssets::ivory_slider_thumb_png,
        VVChainMasterAssets::ivory_slider_thumb_pngSize);

    disabledButton = loadImage(
        VVChainMasterAssets::black_button_disabled_png,
        VVChainMasterAssets::black_button_disabled_pngSize);
    disabledButtonIvory = loadImage(
        VVChainMasterAssets::ivory_button_disabled_png,
        VVChainMasterAssets::ivory_button_disabled_pngSize);
    bypassLedRed = loadImage(
        VVChainMasterAssets::bypass_red_png,
        VVChainMasterAssets::bypass_red_pngSize);
#undef VVCHAIN_MASTER_IMG
#endif
}

bool VVChainAudioProcessorEditor::MetalLookAndFeel::isMasterRuntimeActive() const noexcept
{
#if VVCHAIN_HAS_MASTER_LOCK_UI
    return blackFullPanel.isValid() && ivoryFullPanel.isValid()
        && knobSilver.isValid() && knobPlatinumMaster.isValid();
#else
    return false;
#endif
}

const VVChainAudioProcessorEditor::MetalLookAndFeel::HardwareAssets&
VVChainAudioProcessorEditor::MetalLookAndFeel::assets(
    bool localMuted) const noexcept
{
#if VVCHAIN_HAS_MASTER_LOCK_UI
    if (isMasterRuntimeActive())
    {
        juce::ignoreUnused(localMuted);
        return ivoryTheme ? ivoryAssets : studioAssets;
    }
#endif
    if (monochrome || localMuted)
        return mutedAssets;
    return ivoryTheme ? ivoryAssets : studioAssets;
}

const juce::Image&
VVChainAudioProcessorEditor::MetalLookAndFeel::selectKnobStrip(
    const juce::Slider& slider, const HardwareAssets& fallback) const noexcept
{
#if VVCHAIN_HAS_MASTER_LOCK_UI
    const auto id = slider.getProperties().getWithDefault(
        "vvParameterId", juce::var()).toString();

    if (id == "OUTPUT_LEVEL" && knobPlatinumMaster.isValid())
        return knobPlatinumMaster;
    if (id.startsWith("EQ_COLOR") && knobGold.isValid())
        return knobGold;
    if (id.startsWith("DYN_") && knobBlue.isValid())
        return knobBlue;
    if (id.startsWith("UDMBC_") && knobGreen.isValid())
        return knobGreen;
    if (id.startsWith("TAPE_") && knobRed.isValid())
        return knobRed;
    if ((id.startsWith("TRANSIENT") || id.startsWith("XOVER_"))
        && knobBlack.isValid())
        return knobBlack;
    if (knobSilver.isValid())
        return knobSilver;
#endif
    return fallback.knobStrip;
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawPanelSurface(
    juce::Graphics& g, juce::Rectangle<float> bounds,
    bool localMuted) const
{
#if VVCHAIN_HAS_MASTER_LOCK_UI
    if (isMasterRuntimeActive())
    {
        drawImage(g, ivoryTheme ? ivoryFullPanel : blackFullPanel, bounds, 1.0f);
        return;
    }
#endif
    drawImage(g, assets(localMuted).panel, bounds, 1.0f);
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawModuleSurface(
    juce::Graphics& g, juce::Rectangle<float> bounds,
    bool localMuted) const
{
#if VVCHAIN_HAS_MASTER_LOCK_UI
    if (isMasterRuntimeActive())
        return; // The full approved panel already contains the module metalwork.
#endif
    drawImage(g, assets(localMuted).module, bounds, 1.0f);
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawGraphSurface(
    juce::Graphics& g, juce::Rectangle<float> bounds) const
{
#if VVCHAIN_HAS_MASTER_LOCK_UI
    if (isMasterRuntimeActive() && !monochrome)
        return;
#endif
    const auto& a = assets();
    if (a.graph.isValid())
        drawImage(g, a.graph, bounds, 1.0f);
    else
    {
        g.setColour(juce::Colour(0xff0b0c0c));
        g.fillRoundedRectangle(bounds, 8.0f);
    }
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawScrew(
    juce::Graphics& g, juce::Rectangle<float> bounds,
    bool localMuted) const
{
    drawImage(g, assets(localMuted).screw, bounds, 1.0f);
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float rotaryStartAngle,
    float rotaryEndAngle, juce::Slider& slider)
{
    juce::ignoreUnused(rotaryStartAngle, rotaryEndAngle);

    auto area = juce::Rectangle<float>(
        (float) x, (float) y, (float) width, (float) height).reduced(0.5f);

    const bool localMuted = static_cast<bool>(
        slider.getProperties().getWithDefault("moduleMuted", false));
    const auto& a = assets(localMuted);
    const auto& knobStrip = selectKnobStrip(slider, a);

    const float side = juce::jmin(area.getWidth(), area.getHeight());
    auto knobBounds = juce::Rectangle<float>(side, side)
        .withCentre({ area.getCentreX(), area.getCentreY() });

    const int frame = juce::jlimit(
        0, 63,
        (int) std::lround(
            juce::jlimit(0.0f, 1.0f, sliderPosProportional) * 63.0f));

    const float knobOpacity = (localMuted || monochrome) ? 0.58f : 1.0f;
    drawKnobFrame(g, knobStrip, frame, knobBounds, knobOpacity);

    if (auto* wheelSlider = dynamic_cast<WheelSlider*>(&slider))
    {
        if (wheelSlider->isGraphControlActive())
        {
            const bool moving = wheelSlider->isGraphControlMoving();
            const bool visible = !moving
                || ((juce::Time::getMillisecondCounter() / 180u) % 2u == 0u);

            if (visible)
            {
                g.setColour(juce::Colours::white.withAlpha(.96f));
                g.drawEllipse(
                    knobBounds.reduced(5.0f),
                    moving ? 2.0f : 1.6f);
            }
        }
    }
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawLinearSlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float sliderAsymmetry,
    float sliderStart, juce::Slider::SliderStyle style,
    juce::Slider& slider)
{
    juce::ignoreUnused(sliderAsymmetry, sliderStart, style);

    const bool localMuted = static_cast<bool>(
        slider.getProperties().getWithDefault("moduleMuted", false));
    const auto& a = assets(localMuted);

    auto r = juce::Rectangle<float>(
        (float) x, (float) y, (float) width, (float) height).reduced(1.0f);

    drawImage(g, a.sliderTrack, r, 1.0f);

    const float t = juce::jlimit(0.0f, 1.0f, sliderPosProportional);
    const float thumbSide = juce::jlimit(
        12.0f, 22.0f, r.getHeight() * 1.15f);
    const float px = r.getX() + t * r.getWidth();

    juce::Rectangle<float> thumb {
        px - thumbSide * 0.5f,
        r.getCentreY() - thumbSide * 0.5f,
        thumbSide, thumbSide
    };
    drawImage(g, a.sliderThumb, thumb, 1.0f);

    if (slider.getComponentID() == "DYN_DETECT_BLEND")
    {
        g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
        g.setColour((monochrome || localMuted)
            ? juce::Colour(0xffe0e0e0)
            : (ivoryTheme ? juce::Colour(0xff33291f)
                          : juce::Colour(0xffeaf4f4)));

        auto left = r.removeFromLeft(r.getWidth() * 0.5f);
        g.drawText("PEAK", left.toNearestInt(),
                   juce::Justification::centred);
        g.drawText("RMS", r.toNearestInt(),
                   juce::Justification::centred);
    }
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted);

    const bool localMuted = static_cast<bool>(
        button.getProperties().getWithDefault("moduleMuted", false));
    const auto& a = assets(localMuted);
    const auto r = button.getLocalBounds().toFloat().reduced(.5f);

    const auto drawButton = [&](bool on)
    {
        const juce::Image* image = nullptr;
        if (localMuted || monochrome)
            image = (ivoryTheme && disabledButtonIvory.isValid())
                ? &disabledButtonIvory
                : (disabledButton.isValid() ? &disabledButton : &a.buttonOff);
        else if (shouldDrawButtonAsDown)
            image = &a.buttonPressed;
        else
            image = on ? &a.buttonOn : &a.buttonOff;

        if (image != nullptr)
            drawImage(g, *image, r, 1.0f);
    };

    const auto drawLed = [&](juce::Rectangle<float> ledBounds, bool on)
    {
        drawImage(g, on ? a.ledOn : a.ledOff, ledBounds, 1.0f);
    };

    if (button.getWidth() <= 36 && button.getHeight() <= 36)
    {
        const bool forceOff = static_cast<bool>(
            button.getProperties().getWithDefault("forceLedOff", false));
        drawLed(r.reduced(1.0f),
                !button.getToggleState() && !forceOff);
        return;
    }

    if (button.getComponentID() == "MODULE_BYPASS")
    {
        const bool bypassed = button.getToggleState();
        drawButton(!bypassed);

        const float ledSide = juce::jmin(
            16.0f, r.getHeight() - 5.0f);
        const juce::Rectangle<float> ledBounds {
            r.getX() + 1.5f,
            r.getCentreY() - ledSide * .5f,
            ledSide, ledSide
        };

        if (bypassed && !localMuted && !monochrome
            && bypassLedRed.isValid())
            drawImage(g, bypassLedRed, ledBounds, 1.0f);
        else
            drawLed(ledBounds, !bypassed);

        g.setColour((monochrome || localMuted)
            ? juce::Colour(0xffececec)
            : (ivoryTheme ? juce::Colour(0xfffff0d2)
                          : juce::Colour(0xffeef7f7)));
        g.setFont(juce::FontOptions(
            button.getWidth() <= 54 ? 7.2f : 7.9f)
            .withStyle("Bold"));
        g.drawText(
            button.getButtonText(),
            juce::Rectangle<int> {
                (int) r.getX() + 14,
                (int) r.getY(),
                juce::jmax(8, (int) r.getWidth() - 16),
                (int) r.getHeight()
            },
            juce::Justification::centred);
        return;
    }

    if (button.getComponentID() == "ANALOG_MODE")
    {
        drawButton(true);
        const bool ss = button.getToggleState();
        const auto half = r.getWidth() * .5f;

        g.setFont(juce::FontOptions(7.2f).withStyle("Bold"));
        g.setColour(juce::Colours::white.withAlpha(
            ss ? .58f : .98f));
        g.drawText("TT", r.withWidth(half).toNearestInt(),
                   juce::Justification::centred);
        g.setColour(juce::Colours::white.withAlpha(
            ss ? .98f : .58f));
        g.drawText(
            "SS",
            r.withX(r.getX() + half)
                .withWidth(half).toNearestInt(),
            juce::Justification::centred);
        return;
    }

    if (button.getComponentID() == "ANALOG_X2")
    {
        const bool on = button.getToggleState();
        drawButton(on);
        g.setColour(juce::Colours::white.withAlpha(
            on ? .98f : .62f));
        g.setFont(juce::FontOptions(7.2f).withStyle("Bold"));
        g.drawText("X2", r.toNearestInt(),
                   juce::Justification::centred);
        return;
    }

    const bool on = button.getToggleState();
    drawButton(on);

    if (on && r.getWidth() > 38
        && !localMuted && !monochrome)
    {
        const float ledSide = juce::jmin(
            14.0f, r.getHeight() - 6.0f);
        drawLed({
            r.getX() + 2.0f,
            r.getCentreY() - ledSide * .5f,
            ledSide, ledSide
        }, true);
    }

    g.setColour((monochrome || localMuted)
        ? juce::Colour(0xffececec)
        : (ivoryTheme ? juce::Colour(0xfffff0d2)
                      : juce::Colour(0xffeef7f7)));
    g.setFont(juce::FontOptions(8.2f).withStyle("Bold"));

    auto textArea = r.toNearestInt().reduced(4, 1);
    if (on && textArea.getWidth() > 38)
    {
        textArea.setX(textArea.getX() + 10);
        textArea.setWidth(
            juce::jmax(8, textArea.getWidth() - 10));
    }

    g.drawText(button.getButtonText(), textArea,
               juce::Justification::centred);
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& button,
    const juce::Colour& backgroundColour,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(
        backgroundColour, shouldDrawButtonAsHighlighted);

    const bool localMuted = static_cast<bool>(
        button.getProperties().getWithDefault("moduleMuted", false));
    const auto& a = assets(localMuted);
    const auto r = button.getLocalBounds().toFloat().reduced(.5f);

    if (localMuted || monochrome)
    {
        drawImage(
            g,
            (ivoryTheme && disabledButtonIvory.isValid())
                ? disabledButtonIvory
                : (disabledButton.isValid() ? disabledButton : a.buttonOff),
            r, 1.0f);
        return;
    }

    const auto& image = shouldDrawButtonAsDown
        ? a.buttonPressed
        : (button.getToggleState()
            ? a.buttonOn : a.buttonOff);

    drawImage(g, image, r, 1.0f);
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawButtonText(
    juce::Graphics& g, juce::TextButton& button,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(
        shouldDrawButtonAsHighlighted,
        shouldDrawButtonAsDown);

    const bool localMuted = static_cast<bool>(
        button.getProperties().getWithDefault("moduleMuted", false));

    g.setColour((monochrome || localMuted)
        ? juce::Colour(0xffe7e7e7)
        : (ivoryTheme
            ? juce::Colour(0xfffff0d2)
            : juce::Colour(0xffeef7f7)));

    g.setFont(juce::FontOptions(
        juce::jlimit(
            7.0f, 10.0f,
            button.getHeight() * .34f))
        .withStyle("Bold"));

    g.drawFittedText(
        button.getButtonText(),
        button.getLocalBounds().reduced(5, 2),
        juce::Justification::centred,
        1, .85f);
}

VVChainAudioProcessorEditor::VVChainAudioProcessorEditor(VVChainAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      globalGraphMouseListener(this)
{
    setLookAndFeel(&metalLook);
    setResizable(false, false);
#if VVCHAIN_HAS_MASTER_LOCK_UI
    setSize(1470, 1070);
#else
    setSize(1500, 930);
#endif
    setWantsKeyboardFocus(true);

    settingsDismissOverlay = std::make_unique<SettingsDismissOverlay>();
    settingsDismissOverlay->onDismiss = [this]
    {
        setSettingsPanelVisible(false);
    };
    addAndMakeVisible(*settingsDismissOverlay);
    settingsDismissOverlay->setVisible(false);

    settingsButton = std::make_unique<SettingsGearButton>();
    settingsButton->onClick = [this]
    {
        setSettingsPanelVisible(!settingsPanelVisible);
    };
    addAndMakeVisible(*settingsButton);

    uiThemeButton = std::make_unique<juce::TextButton>("UI BLACK");
    uiThemeButton->setTooltip(
        "UI SKIN：BLACK GOLD / IVORY GOLD；只改介面，不改任何音訊參數");
    uiThemeButton->onClick = [this]
    {
        setIvoryTheme(!ivoryTheme);
    };
    addAndMakeVisible(*uiThemeButton);

    settingsPanel = std::make_unique<SettingsPanel>();
    settingsPanel->onThemeChanged = [this](bool ivory)
    {
        setIvoryTheme(ivory);
    };
    settingsPanel->onAnalyzerChanged = [this](bool enabled)
    {
        setAnalyzerEnabled(enabled);
    };
    settingsPanel->setIvoryTheme(true);
    settingsPanel->setAnalyzerEnabled(true);
    analyzerDb.fill(-90.0f);
    audioProcessor.setAnalyzerEnabled(true);
    addAndMakeVisible(*settingsPanel);
    settingsPanel->setVisible(false);

    const std::array<juce::String, 4> bypassIds
    {
        "EQ_BYPASS", "UDMBC_BYPASS", "EQ_COLOR_GLOBAL_BYPASS",
        "TAPE_BYPASS"
    };

    const std::array<juce::String, 4> bypassLabels
    {
        "EQ", "UDMBC", "ANALOG", "TAPE COLOR"
    };

    const std::array<juce::Colour, 4> bypassColours
    {
        juce::Colour(0xff38bdf8),
        juce::Colour(0xfffacc15),
        juce::Colour(0xff60a5fa),
        juce::Colour(0xfff472b6)
    };

    for (int i = 0; i < 4; ++i)
        addBypass(i, bypassIds[(size_t) i], bypassLabels[(size_t) i],
                  bypassColours[(size_t) i]);

    masterBypassButton = std::make_unique<juce::ToggleButton>("BYPASS");
    masterBypassButton->setLookAndFeel(&metalLook);
    masterBypassButton->setButtonText("BYPASS");
    masterBypassButton->setColour(juce::ToggleButton::tickColourId,
                                  juce::Colour(0xffdfe7ef));
    masterBypassButton->setTooltip(
        "整個 VVCHAIN 完全旁通；固定 PDC，切換使用短交叉淡化避免斷音/爆音");
    masterBypassButton->onClick = [this]
    {
        if (auto* parameter = audioProcessor.apvts.getParameter("MASTER_BYPASS"))
            parameter->setValueNotifyingHost(
                masterBypassButton->getToggleState() ? 1.0f : 0.0f);
        updateBypassVisuals();
    };
    masterBypassAttachment = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, "MASTER_BYPASS", *masterBypassButton);
    addAndMakeVisible(*masterBypassButton);

    soloModeButton = std::make_unique<juce::ToggleButton>("SOLO PRE");
    soloModeButton->setLookAndFeel(&metalLook);
    soloModeButton->setButtonText("SOLO PRE");
    soloModeButton->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffdfe7ef));
    soloModeButton->setTooltip("SOLO 路徑：PRE = 所有處理前；POST = 所有處理後");
    soloModeButton->onClick = [this]
    {
        const bool post = parameterValue("SOLO_MODE") < 0.5f;
        if (auto* parameter = audioProcessor.apvts.getParameter("SOLO_MODE"))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(post ? 1.f : 0.f));
    };
    addAndMakeVisible(*soloModeButton);

    for (int b = 0; b < 4; ++b)
    {
        const auto c = uiColour(kBandColours[(size_t) b]);
        const auto n = juce::String(b + 1);

        // Main EQ controls.
        addKnob("EQ" + n + "_GAIN", "GAIN", -18, 18, .1,
                parameterValue("EQ" + n + "_GAIN"), " dB", b, 0, c);
        addKnob("EQ" + n + "_FREQ", "FREQ", 20, 20000, 1,
                parameterValue("EQ" + n + "_FREQ"), " Hz", b, 1, c);
        addKnob("EQ" + n + "_Q", "Q", .1, 18, .01,
                parameterValue("EQ" + n + "_Q"), "", b, 2, c);

        // DYNAMICS is the single user-facing dynamic macro.
        // Threshold is automatically linked to DYNAMICS in the DSP.
        addKnob("DYN_DYNAMICS" + n, "DYNAMICS", -100, 100, .1,
                parameterValue("DYN_DYNAMICS" + n), " %", b, 8, c);
        if (auto* dynamicsKnob = findKnob("DYN_DYNAMICS" + n))
            dynamicsKnob->slider->setTooltip(
                "DYNAMICS：中心 0% = 靜態；逆時鐘 = 壓縮；順時鐘 = 擴展。Threshold 會依 DYNAMICS 自動連動，不再獨立操作。");
        addKnob("DYN_ATTACK" + n, "ATTACK", .1, 200, .1,
                parameterValue("DYN_ATTACK" + n), " ms", b, 9, c);
        addKnob("DYN_RELEASE" + n, "RELEASE", 5, 2000, 1,
                parameterValue("DYN_RELEASE" + n), " ms", b, 10, c);

        auto detectBlendSlider = std::make_unique<WheelSlider>();
        detectBlendSlider->setSliderStyle(juce::Slider::LinearHorizontal);
        detectBlendSlider->setTextBoxStyle(
            juce::Slider::NoTextBox, false, 0, 0);
        detectBlendSlider->setSliderSnapsToMousePosition(false);
        detectBlendSlider->setVerticalValueDrag(true, 133.0);
        detectBlendSlider->setWheelBehaviour(0.5, false);
        dynDetectSliders[(size_t) b] = std::move(detectBlendSlider);
        dynDetectSliders[(size_t) b]->setComponentID("DYN_DETECT_BLEND");
        dynDetectSliders[(size_t) b]->setLookAndFeel(&metalLook);
        dynDetectSliders[(size_t) b]->setRange(0.0, 100.0, 0.1);
        dynDetectSliders[(size_t) b]->setValue(
            parameterValue("DYN_DETECT_ONSETS" + n),
            juce::dontSendNotification);
        dynDetectSliders[(size_t) b]->setTooltip(
            "PEAK ↔ RMS detector blend；上下拖曳：上 = RMS / 右，下 = PEAK / 左；50% = equal blend");
        dynDetectAttachments[(size_t) b] =
            std::make_unique<Attachment>(
                audioProcessor.apvts,
                "DYN_DETECT_ONSETS" + n,
                *dynDetectSliders[(size_t) b]);
        addAndMakeVisible(*dynDetectSliders[(size_t) b]);

        dynTriggerButtons[(size_t) b] =
            std::make_unique<juce::ToggleButton>("ABOVE");
        dynTriggerButtons[(size_t) b]->setComponentID("DYN_MODE");
        dynTriggerButtons[(size_t) b]->setLookAndFeel(&metalLook);
        dynTriggerButtons[(size_t) b]->setColour(
            juce::ToggleButton::tickColourId, c);
        dynTriggerButtons[(size_t) b]->setTooltip(
            "TRIGGER：ABOVE / BELOW");
        dynTriggerButtons[(size_t) b]->onClick = [this, b]
        {
            if (auto* parameter = audioProcessor.apvts.getParameter(
                    "DYN_TRIGGER_BELOW" + juce::String(b + 1)))
                parameter->setValueNotifyingHost(
                    parameter->convertTo0to1(
                        dynTriggerButtons[(size_t)b]->getToggleState()
                            ? 1.f : 0.f));
            repaint();
        };
        dynTriggerAttachments[(size_t) b] =
            std::make_unique<BoolAttachment>(
                audioProcessor.apvts,
                "DYN_TRIGGER_BELOW" + n,
                *dynTriggerButtons[(size_t) b]);
        addAndMakeVisible(*dynTriggerButtons[(size_t) b]);

        // The two EQ / ANALOG controls live inside every BAND card.
        // They intentionally remain attached to the shared DSP parameters.
        addKnob("EQ_COLOR_B" + n, "ANALOG COLOR", 0, 60, .1,
                parameterValue("EQ_COLOR" + n), " %", b, 6,
                juce::Colour(0xff60a5fa), false, "EQ_COLOR" + n);

        analogModeButtons[(size_t) b] = std::make_unique<juce::ToggleButton>();
        analogModeButtons[(size_t) b]->setLookAndFeel(&metalLook);
        analogModeButtons[(size_t) b]->setComponentID("ANALOG_MODE");
        analogModeButtons[(size_t) b]->setButtonText("");
        analogModeButtons[(size_t) b]->setColour(
            juce::ToggleButton::tickColourId, kBandColours[(size_t) b]);
        analogModeButtons[(size_t) b]->setTooltip(
            "ANALOG COLOR：TT = Tube Saturation；SS = Solid-State Saturation");
        analogModeAttachments[(size_t) b] =
            std::make_unique<BoolAttachment>(
                audioProcessor.apvts, "EQ_COLOR_MODE" + n,
                *analogModeButtons[(size_t) b]);
        addAndMakeVisible(*analogModeButtons[(size_t) b]);

        analogX2Buttons[(size_t) b] =
            std::make_unique<juce::ToggleButton>("X2");
        analogX2Buttons[(size_t) b]->setLookAndFeel(&metalLook);
        analogX2Buttons[(size_t) b]->setComponentID("ANALOG_X2");
        analogX2Buttons[(size_t) b]->setButtonText("X2");
        analogX2Buttons[(size_t) b]->setColour(
            juce::ToggleButton::tickColourId,
            juce::Colour(0xff60a5fa));
        analogX2Buttons[(size_t) b]->setTooltip(
            "ANALOG COLOR X2：亮起時，0–60% 的 ANALOG COLOR delta × 2");
        analogX2Attachments[(size_t) b] =
            std::make_unique<BoolAttachment>(
                audioProcessor.apvts, "EQ_COLOR_X2" + n,
                *analogX2Buttons[(size_t) b]);
        addAndMakeVisible(*analogX2Buttons[(size_t) b]);

        analogBypassButtons[(size_t) b] = std::make_unique<juce::ToggleButton>();
        analogBypassButtons[(size_t) b]->setLookAndFeel(&metalLook);
        analogBypassButtons[(size_t) b]->setButtonText("");
        analogBypassButtons[(size_t) b]->setColour(
            juce::ToggleButton::tickColourId, juce::Colour(0xff60a5fa));
        analogBypassButtons[(size_t) b]->setTooltip(
            "BAND " + n + " ANALOG COLOR：亮 = 啟用；按下 = BYPASS");
        analogBypassAttachments[(size_t) b] =
            std::make_unique<BoolAttachment>(
                audioProcessor.apvts, "EQ_COLOR_BYPASS" + n,
                *analogBypassButtons[(size_t) b]);
        addAndMakeVisible(*analogBypassButtons[(size_t) b]);

        soloButtons[(size_t) b] = std::make_unique<juce::ToggleButton>("SOLO");
        soloButtons[(size_t) b]->setLookAndFeel(&metalLook);
        soloButtons[(size_t) b]->setButtonText("SOLO");
        soloButtons[(size_t) b]->setTooltip(
            "SOLO BAND " + n + "：使用共享 X-OVER 頻段，不使用 EQ FREQ");
        soloButtons[(size_t) b]->setColour(
            juce::ToggleButton::tickColourId, juce::Colour(0xffdfe7ef));
        soloButtons[(size_t) b]->onClick = [this, b]
        {
            const int requested = parameterValue("SOLO_BAND") >= 0.5f
                ? static_cast<int>(juce::roundToInt(parameterValue("SOLO_BAND")))
                : 0;
            const int next = (requested == b + 1) ? 0 : b + 1;
            if (auto* parameter = audioProcessor.apvts.getParameter("SOLO_BAND"))
                parameter->setValueNotifyingHost(
                    parameter->convertTo0to1(static_cast<float>(next)));
        };
        addAndMakeVisible(*soloButtons[(size_t) b]);

        // Main screen intentionally keeps only the three UDMBC performance knobs.
        addKnob("UDMBC_DEGREE" + n, "UDMBC %", 0, 100, .1,
                parameterValue("UDMBC_DEGREE" + n), " %", b, 3, juce::Colour(0xfffacc15));
        addKnob("UDMBC_COMP_A" + n, "ATTACK", .1, 250, .1,
                parameterValue("UDMBC_COMP_A" + n), " ms", b, 4, juce::Colour(0xfffacc15));
        addKnob("UDMBC_COMP_R" + n, "RELEASE", 10, 2500, 1,
                parameterValue("UDMBC_COMP_R" + n), " ms", b, 5, juce::Colour(0xfffacc15));

        addKnob("TAPE_DEGREE" + n, "TAPE COLOR +", 0, 100, .1,
                parameterValue("TAPE_DEGREE" + n), "", b, 7, c, true);

        addKnob("TRANSIENT" + n, "TRANSIENT", -100, 100, .1,
                parameterValue("TRANSIENT" + n), " %", b, 11,
                juce::Colour(0xff34c759));

        // Independent per-band bypass LEDs. False = active/lit; true = bypass/dim.
        udmbcBandBypassButtons[(size_t) b] = std::make_unique<juce::ToggleButton>();
        udmbcBandBypassButtons[(size_t) b]->setLookAndFeel(&metalLook);
        udmbcBandBypassButtons[(size_t) b]->setButtonText("");
        udmbcBandBypassButtons[(size_t) b]->setColour(
            juce::ToggleButton::tickColourId, juce::Colour(0xfffacc15));
        udmbcBandBypassButtons[(size_t) b]->setTooltip(
            "BAND " + n + " UDMBC：亮 = 啟用；按下 = BYPASS");
        udmbcBandBypassAttachments[(size_t) b] =
            std::make_unique<BoolAttachment>(
                audioProcessor.apvts, "UDMBC_BAND_BYPASS" + n,
                *udmbcBandBypassButtons[(size_t) b]);
        addAndMakeVisible(*udmbcBandBypassButtons[(size_t) b]);

        tapeBandBypassButtons[(size_t) b] = std::make_unique<juce::ToggleButton>();
        tapeBandBypassButtons[(size_t) b]->setLookAndFeel(&metalLook);
        tapeBandBypassButtons[(size_t) b]->setButtonText("");
        tapeBandBypassButtons[(size_t) b]->setColour(
            juce::ToggleButton::tickColourId, juce::Colour(0xfff472b6));
        tapeBandBypassButtons[(size_t) b]->setTooltip(
            "BAND " + n + " TAPE：亮 = 啟用；按下 = BYPASS");
        tapeBandBypassAttachments[(size_t) b] =
            std::make_unique<BoolAttachment>(
                audioProcessor.apvts, "TAPE_BAND_BYPASS" + n,
                *tapeBandBypassButtons[(size_t) b]);
        addAndMakeVisible(*tapeBandBypassButtons[(size_t) b]);

        // Seven band-specific UDMBC advanced controls. The defaults remain in DSP.
        addKnob("UDMBC_LIFT_T" + n, "LIFT THRESH", -80, 0, .1,
                parameterValue("UDMBC_LIFT_T" + n), " dB", b, 20, c);
        addKnob("UDMBC_LIFT_A" + n, "LIFT ATT", 1, 500, .1,
                parameterValue("UDMBC_LIFT_A" + n), " ms", b, 21, c);
        addKnob("UDMBC_LIFT_R" + n, "LIFT REL", 10, 2500, 1,
                parameterValue("UDMBC_LIFT_R" + n), " ms", b, 22, c);
        addKnob("UDMBC_LIFT_M" + n, "LIFT MIX", 0, 100, .1,
                parameterValue("UDMBC_LIFT_M" + n), " %", b, 23, c);
        addKnob("UDMBC_COMP_T" + n, "COMP THRESH", -24, 0, .1,
                parameterValue("UDMBC_COMP_T" + n), " dB", b, 24, c);
        addKnob("UDMBC_COMP_M" + n, "COMP MIX", 0, 100, .1,
                parameterValue("UDMBC_COMP_M" + n), " %", b, 25, c);
        addKnob("UDMBC_LEVEL" + n, "BAND LEVEL", -24, 12, .1,
                parameterValue("UDMBC_LEVEL" + n), " dB", b, 26, c);

        advancedButtons[(size_t) b] = std::make_unique<juce::TextButton>("+ ADV");
        advancedButtons[(size_t) b]->setTooltip("開啟 BAND " + n + " 的 UDMBC ADVANCED");
        advancedButtons[(size_t) b]->onClick = [this, b]
        {
            setExpandedBand(expandedBand == b ? -1 : b);
        };
        addAndMakeVisible(*advancedButtons[(size_t) b]);
    }

    // Global monitor controls: DELTA + MIX / OUT.
    deltaMonitorButton = std::make_unique<juce::ToggleButton>("DELTA");
    deltaMonitorButton->setLookAndFeel(&metalLook);
    deltaMonitorButton->setComponentID("DELTA_MONITOR");
    deltaMonitorButton->setColour(
        juce::ToggleButton::tickColourId, juce::Colour(0xffffffff));
    deltaMonitorButton->setTooltip(
        "DELTA：輸出聲音 − 原始聲音；使用已對齊乾聲，不增加額外延遲");
    deltaMonitorAttachment = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, "DELTA_MONITOR", *deltaMonitorButton);
    addAndMakeVisible(*deltaMonitorButton);

    // Shared UDMBC advanced controls appear inside the currently expanded BAND.
    addKnob("UDMBC_X1", "XOVER 1", 80, 600, 1,
            parameterValue("UDMBC_X1"), " Hz", -1, 30, juce::Colour(0xfffacc15));
    addKnob("UDMBC_X2", "XOVER 2", 750, 3000, 1,
            parameterValue("UDMBC_X2"), " Hz", -1, 31, juce::Colour(0xfffacc15));
    addKnob("UDMBC_X3", "XOVER 3", 6000, 12000, 1,
            parameterValue("UDMBC_X3"), " Hz", -1, 32, juce::Colour(0xfffacc15));
    addKnob("XOVER_OVERLAP", "OVERLAP", 0, 100, 1,
            parameterValue("XOVER_OVERLAP"), " %", -1, 33, juce::Colour(0xfffacc15));
    addKnob("UDMBC_INPUT", "INPUT", -24, 24, .1,
            parameterValue("UDMBC_INPUT"), " dB", -1, 33, juce::Colour(0xfffacc15));
    addKnob("UDMBC_GATE", "GATE", -90, 0, .1,
            parameterValue("UDMBC_GATE"), " dB", -1, 34, juce::Colour(0xfffacc15));
    addKnob("UDMBC_MIX", "MASTER MIX", 0, 100, .1,
            parameterValue("UDMBC_MIX"), " %", -1, 35, juce::Colour(0xfffacc15));
    addKnob("UDMBC_OUTPUT", "OUTPUT", -24, 24, .1,
            parameterValue("UDMBC_OUTPUT"), " dB", -1, 36, juce::Colour(0xfffacc15));

    udmbcClipper = std::make_unique<juce::ToggleButton>("CLIPPER");
    udmbcClipper->setLookAndFeel(&metalLook);
    udmbcClipper->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xfffacc15));
    udmbcClipper->setTooltip("UDMBC Clipper");
    addAndMakeVisible(*udmbcClipper);
    udmbcClipperAttachment = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, "UDMBC_CLIPPER", *udmbcClipper);

    closeAdvanced = std::make_unique<juce::TextButton>("CLOSE");
    closeAdvanced->setButtonText("CLOSE");
    closeAdvanced->onClick = [this] { setExpandedBand(-1); };
    addAndMakeVisible(*closeAdvanced);

    startTimerHz(30);
    updateBypassVisuals();

    // Final MIX / OUT remain in the global monitor card.
    addKnob("DRY_WET", "MIX", 0, 100, .1,
            parameterValue("DRY_WET"), " %", 4, 2, juce::Colour(0xff38bdf8));
    addKnob("OUTPUT_LEVEL", "OUT", -24, 12, .1,
            parameterValue("OUTPUT_LEVEL"), " dB", 4, 3, juce::Colour(0xff9ed85c));

    setExpandedBand(-1);
    setIvoryTheme(true);

    // Hover value box sits above the graph but never intercepts the graph mouse.
    floatingValueBox.setAlwaysOnTop(true);
    floatingValueBox.setCommitHandler(
        [this](int line, const juce::String& text)
        {
            commitFloatingValueEdit(line, text);
        });
    addAndMakeVisible(floatingValueBox);
    floatingValueBox.hideInstantly();

    // Graph hover must see mouse moves even when the pointer is over child
    // components or the non-intercepting overlay.
    juce::Desktop::getInstance().addGlobalMouseListener(
        &globalGraphMouseListener);
}

VVChainAudioProcessorEditor::~VVChainAudioProcessorEditor()
{
    // Analyzer work is editor-only. Closing the editor removes all analyzer
    // FIFO/FFT overhead from the realtime path.
    audioProcessor.setAnalyzerEnabled(false);

    juce::Desktop::getInstance().removeGlobalMouseListener(
        &globalGraphMouseListener);

    for (auto& k : knobs)
    {
        k.attachment.reset();
        if (k.slider)
            k.slider->setLookAndFeel(nullptr);
        if (k.label)
            k.label->setLookAndFeel(nullptr);
    }

    for (auto& b : bypassButtons)
        if (b) b->setLookAndFeel(nullptr);

    if (masterBypassButton) masterBypassButton->setLookAndFeel(nullptr);
    masterBypassAttachment.reset();
    if (deltaMonitorButton) deltaMonitorButton->setLookAndFeel(nullptr);
    deltaMonitorAttachment.reset();

    for (auto& b : udmbcBandBypassButtons)
        if (b) b->setLookAndFeel(nullptr);
    for (auto& b : tapeBandBypassButtons)
        if (b) b->setLookAndFeel(nullptr);

    for (auto& b : analogModeButtons)
        if (b) b->setLookAndFeel(nullptr);
    for (auto& b : analogBypassButtons)
        if (b) b->setLookAndFeel(nullptr);
    for (auto& b : soloButtons)
        if (b) b->setLookAndFeel(nullptr);
    for (auto& b : dynDetectSliders)
        if (b) b->setLookAndFeel(nullptr);
    for (auto& b : dynTriggerButtons)
        if (b) b->setLookAndFeel(nullptr);
    if (soloModeButton) soloModeButton->setLookAndFeel(nullptr);
    uiThemeButton.reset();

    for (auto& a : analogBypassAttachments)
        a.reset();

    for (auto& a : dynDetectAttachments)
        a.reset();
    for (auto& a : dynTriggerAttachments)
        a.reset();

    for (auto& a : udmbcBandBypassAttachments)
        a.reset();

    for (auto& a : analogModeAttachments)
        a.reset();
    for (auto& a : tapeBandBypassAttachments)
        a.reset();

    if (advancedButtons.size() > 0)
        for (auto& b : advancedButtons)
            if (b) b->setLookAndFeel(nullptr);

    if (udmbcClipper)
        udmbcClipper->setLookAndFeel(nullptr);
    udmbcClipperAttachment.reset();
    closeAdvanced.reset();

    stopTimer();
    setLookAndFeel(nullptr);
}

void VVChainAudioProcessorEditor::addKnob(
    const juce::String& id, const juce::String& title,
    double min, double max, double step, double defaultValue,
    const juce::String& suffix, int band, int slot,
    juce::Colour accent, bool tapeDisplayDb,
    const juce::String& attachmentId)
{
    Knob k;
    k.id = id;
    k.title = title;
    k.suffix = suffix;
    k.accent = accent;
    k.band = band;
    k.slot = slot;
    k.tapeDisplayDb = tapeDisplayDb;
    k.slider = std::make_unique<WheelSlider>();
    k.label = std::make_unique<juce::Label>();

    k.slider->setLookAndFeel(&metalLook);
    k.slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider->setTextBoxStyle(
        juce::Slider::TextBoxBelow, false, 68, 17);
    k.slider->setTextBoxIsEditable(true);
    k.slider->setScrollWheelEnabled(true);
    k.slider->setRange(min, max, step);
    k.slider->getProperties().set("vvParameterId", id);
    if (id.endsWith("_FREQ") || id.startsWith("UDMBC_X"))
        k.slider->setSkewFactorFromMidPoint(632.f);

    if (auto* wheelSlider = dynamic_cast<WheelSlider*>(k.slider.get()))
    {
        const bool logarithmic =
            id.endsWith("_FREQ") || id.startsWith("UDMBC_X");

        if (id.endsWith("_FREQ"))
            wheelSlider->setDragSensitivity(900, 9000);
        else if ((id.startsWith("EQ") && id.endsWith("_GAIN"))
                 || id.startsWith("DYN_DYNAMICS"))
            wheelSlider->setDragSensitivity(257, 2570);
        else if (id.endsWith("_Q"))
            wheelSlider->setDragSensitivity(225, 2250);
        else
            wheelSlider->setDragSensitivity(180, 1800);

        double wheelStep = std::max(
            0.01, (max - min) * 0.01);

        if (id.startsWith("DYN_DYNAMICS"))
            wheelStep = 0.7;
        else if (id.startsWith("EQ") && id.endsWith("_GAIN"))
            wheelStep = 0.35;
        else if (id.endsWith("_Q"))
            wheelStep = 0.016;
        else if (id.contains("GAIN") || id.contains("LEVEL")
                 || id.contains("THRESH") || id.endsWith("_OUTPUT")
                 || id == "OUTPUT_LEVEL" || id == "TAPE_LEVEL")
            wheelStep = 0.5;
        else if (id.contains("DYNAMICS"))
            wheelStep = 1.0;
        else if (id.contains("ATTACK"))
            wheelStep = 1.0;
        else if (id.contains("RELEASE"))
            wheelStep = 5.0;
        else if (id.contains("DEGREE") || id.contains("MIX")
                 || id.contains("COLOR") || id == "DRY_WET")
            wheelStep = 1.0;

        wheelSlider->setWheelBehaviour(wheelStep, logarithmic);
    }
    k.slider->setDoubleClickReturnValue(
        true,
        id.startsWith("DYN_DYNAMICS") ? 0.0 : defaultValue);
    k.slider->setColour(juce::Slider::rotarySliderFillColourId, accent);
    k.slider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff08090b));
    k.slider->setNumDecimalPlacesToDisplay(
        step < .01 ? 3 : step < .1 ? 2 : step < 1 ? 1 : 0);

    if (id.startsWith("EQ_COLOR_B"))
    {
        // ANALOG COLOR keeps the same 0..60 DSP parameter; display only is 0.0..10.0.
        k.slider->textFromValueFunction = [](double value)
        {
            return juce::String(value / 6.0, 1);
        };
        k.slider->valueFromTextFunction = [](const juce::String& text)
        {
            return text.retainCharacters("0123456789.-").getDoubleValue() * 6.0;
        };
    }
    else if (tapeDisplayDb)
    {
        // TAPE keeps its preset/automation parameter range; display only is 0.0..10.0.
        k.slider->textFromValueFunction = [max](double value)
        {
            const double norm =
                juce::jlimit(0.0, 1.0, value / juce::jmax(0.000001, max));
            return juce::String(norm * 10.0, 1);
        };
        k.slider->valueFromTextFunction = [max](const juce::String& text)
        {
            const double shown =
                text.retainCharacters("0123456789.-").getDoubleValue();
            return juce::jlimit(0.0, 10.0, shown) * max / 10.0;
        };
    }
    else
    {
        k.slider->setTextValueSuffix(suffix);
    }

    k.label->setText(title, juce::dontSendNotification);
    k.label->setColour(juce::Label::textColourId, accent.brighter(.35f));
    k.label->setJustificationType(juce::Justification::centred);
    k.label->setFont(juce::FontOptions(9.0f).withStyle("Bold"));

    const auto parameterId = attachmentId.isNotEmpty() ? attachmentId : id;
    k.attachment = std::make_unique<Attachment>(
        audioProcessor.apvts, parameterId, *k.slider);

    // Value=0 means the corresponding processing band/module is bypassed.
    // Any value above 0 immediately re-enables it.
    if (id.startsWith("UDMBC_DEGREE") && k.band >= 0)
    {
        const int band = k.band;
        auto* slider = k.slider.get();
        k.slider->onValueChange = [this, band, slider]
        {
            udmbcUiTouched[(size_t) band] = true;
            const float target =
                slider->getValue() <= 0.0001 ? 1.0f : 0.0f;
            const auto bypassId =
                "UDMBC_BAND_BYPASS" + juce::String(band + 1);
            if (auto* parameter = audioProcessor.apvts.getParameter(bypassId))
            {
                if (std::abs(parameter->getValue() - target) > 1.0e-6f)
                    parameter->setValueNotifyingHost(target);
            }
        };
    }
    else if (id.startsWith("TAPE_DEGREE") && k.band >= 0)
    {
        const int band = k.band;
        auto* slider = k.slider.get();
        k.slider->onValueChange = [this, band, slider]
        {
            tapeUiTouched[(size_t) band] = true;
            const float target =
                slider->getValue() <= 0.0001 ? 1.0f : 0.0f;
            const auto bypassId =
                "TAPE_BAND_BYPASS" + juce::String(band + 1);
            if (auto* parameter = audioProcessor.apvts.getParameter(bypassId))
            {
                if (std::abs(parameter->getValue() - target) > 1.0e-6f)
                    parameter->setValueNotifyingHost(target);
            }
        };
    }
    else if (id.startsWith("EQ_COLOR_B") && k.band >= 0)
    {
        const int band = k.band;
        k.slider->onValueChange = [this, band]
        {
            analogUiTouched[(size_t) band] = true;
        };
    }
    addAndMakeVisible(*k.slider);
    addAndMakeVisible(*k.label);
    knobs.push_back(std::move(k));
}

void VVChainAudioProcessorEditor::addBypass(
    int index, const juce::String& parameterId,
    const juce::String& tooltip, juce::Colour accent)
{
    auto b = std::make_unique<juce::ToggleButton>();
    b->setLookAndFeel(&metalLook);
    b->setComponentID("MODULE_BYPASS");
    b->setButtonText(tooltip);
    b->setColour(juce::ToggleButton::tickColourId, accent);
    b->setTooltip(tooltip + "：亮 = 運作，暗 = BYPASS");
    bypassAttachments[(size_t) index] =
        std::make_unique<BoolAttachment>(audioProcessor.apvts, parameterId, *b);
    addAndMakeVisible(*b);
    bypassButtons[(size_t) index] = std::move(b);
}

VVChainAudioProcessorEditor::Knob* VVChainAudioProcessorEditor::findKnob(const juce::String& id)
{
    for (auto& k : knobs)
        if (k.id == id)
            return &k;
    return nullptr;
}

void VVChainAudioProcessorEditor::placeKnob(const juce::String& id, juce::Rectangle<int> area)
{
    if (auto* k = findKnob(id))
    {
        k->label->setBounds(area.removeFromTop(12));

        // The original fixed 68 px textbox was wider than the four-column
        // dynamic row (~65 px at 1500 px editor width), causing value text
        // to overlap neighbouring knobs. Size the editor to the actual cell.
        const int textBoxW =
            juce::jmax(48, juce::jmin(62, area.getWidth() - 8));
        k->slider->setTextBoxStyle(
            juce::Slider::TextBoxBelow, false, textBoxW, 15);
        k->slider->setBounds(area);
    }
}

float VVChainAudioProcessorEditor::parameterValue(const juce::String& id) const
{
    if (auto* value = audioProcessor.apvts.getRawParameterValue(id))
        return value->load();
    return 0.f;
}

void VVChainAudioProcessorEditor::setParameter(const juce::String& id, float value)
{
    if (auto* parameter = audioProcessor.apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

juce::Rectangle<float> VVChainAudioProcessorEditor::eqGraphBounds() const
{
    // The approved Master panel has a fixed 1470x1070 hardware frame.
#if VVCHAIN_HAS_MASTER_LOCK_UI
    if (metalLook.isMasterRuntimeActive())
        return { 27.f, 101.f, (float) getWidth() - 54.f, 281.f };
#endif
    return { 18.f, 78.f, (float) getWidth() - 36.f, 315.f };
}

float VVChainAudioProcessorEditor::graphFrequencyToX(
    const juce::Rectangle<float>& graph, float hz) const
{
    return graph.getX() + graph.getWidth() * logMap(hz, 20.f, 20000.f);
}

float VVChainAudioProcessorEditor::graphXToFrequency(
    const juce::Rectangle<float>& graph, float x) const
{
    const float t = (x - graph.getX()) / juce::jmax(1.f, graph.getWidth());
    return invLogMap(t, 20.f, 20000.f);
}

float VVChainAudioProcessorEditor::constrainXoverFrequency(int index, float hz) const
{
    const float x1 = parameterValue("UDMBC_X1");
    const float x2 = parameterValue("UDMBC_X2");
    const float x3 = parameterValue("UDMBC_X3");
    const double sampleRate = audioProcessor.getSampleRate();
    const float upper = sampleRate > 0.0
        ? juce::jmin(18000.f, static_cast<float>(sampleRate * 0.42))
        : 18000.f;

    if (index == 0)
        return juce::jlimit(40.f, juce::jmax(41.f, x2 - 80.f), hz);
    if (index == 1)
        return juce::jlimit(x1 + 80.f, juce::jmax(x1 + 81.f, x3 - 200.f), hz);
    return juce::jlimit(x2 + 200.f, upper, hz);
}

float VVChainAudioProcessorEditor::eqDbToY(
    const juce::Rectangle<float>& graph, float db) const
{
    const float clamped = juce::jlimit(-18.f, 18.f, db);
    const float magnitude = std::abs(clamped);

    float t = 0.f;
    if (magnitude <= 3.f)
        t = 0.34f * (magnitude / 3.f);
    else if (magnitude <= 6.f)
        t = 0.34f + 0.22f * ((magnitude - 3.f) / 3.f);
    else if (magnitude <= 12.f)
        t = 0.56f + 0.24f * ((magnitude - 6.f) / 6.f);
    else
        t = 0.80f + 0.20f * ((magnitude - 12.f) / 6.f);

    const float direction = clamped >= 0.f ? -1.f : 1.f;
    return graph.getCentreY() + direction * t * graph.getHeight() * 0.5f;
}

float VVChainAudioProcessorEditor::eqYToDb(
    const juce::Rectangle<float>& graph, float y) const noexcept
{
    const float halfH = juce::jmax(1.f, graph.getHeight() * 0.5f);
    const float signedDistance = graph.getCentreY() - y;
    const float t = juce::jlimit(0.f, 1.f, std::abs(signedDistance) / halfH);

    float magnitude = 0.f;
    if (t <= 0.34f)
        magnitude = 3.f * (t / 0.34f);
    else if (t <= 0.56f)
        magnitude = 3.f + 3.f * ((t - 0.34f) / 0.22f);
    else if (t <= 0.80f)
        magnitude = 6.f + 6.f * ((t - 0.56f) / 0.24f);
    else
        magnitude = 12.f + 6.f * ((t - 0.80f) / 0.20f);

    return juce::jlimit(
        -18.f, 18.f,
        signedDistance >= 0.f ? magnitude : -magnitude);
}

float VVChainAudioProcessorEditor::qFromWheel(
    float q, float deltaY, bool fine) const noexcept
{
    const float safeQ = juce::jmax(0.1f, q);
    const float wheelUnits = juce::jlimit(-1.0f, 1.0f, deltaY);
    const float speed = fine ? 0.0075f : 0.075f;
    return juce::jlimit(
        0.1f, 18.f,
        safeQ * std::exp(wheelUnits * speed));
}

float VVChainAudioProcessorEditor::dynamicThresholdFromDynamics(float dynamics) const
{
    const float signedDynamics =
        juce::jlimit(-100.f, 100.f, dynamics);

    // DYNAMICS uses a truly linear bipolar map:
    // -100% = -24 dB, 0% = -12 dB, +100% = 0 dB.
    // This keeps equal mouse/knob movement equal in parameter space,
    // including across the zero crossing.
    return juce::jlimit(
        -60.f, 0.f,
        -12.f + signedDynamics * 0.12f);
}

float VVChainAudioProcessorEditor::dynamicEffectiveTargetGain(int band) const
{
    if (band < 0 || band >= 4)
        return 0.f;

    const auto n = juce::String(band + 1);
    const float offset =
        juce::jlimit(-18.f, 18.f,
                     parameterValue("EQ" + n + "_GAIN"));
    const float dynamicRangeDb = 18.0f;
    const float dynamics =
        juce::jlimit(-100.f, 100.f,
                     parameterValue("DYN_DYNAMICS" + n));

    const float amount = std::abs(dynamics) * 0.01f;
    const float direction = dynamics < 0.f ? -1.f : 1.f;
    const float dynamicOffsetDb =
        direction * dynamicRangeDb * amount;

    return juce::jlimit(
        -18.f, 18.f,
        offset + dynamicOffsetDb);
}

juce::Rectangle<float> VVChainAudioProcessorEditor::dynamicMsPopupBounds(int band) const
{
    const auto graph = eqGraphBounds();
    const auto n = juce::String(band + 1);
    const float x = graphFrequencyToX(
        graph, parameterValue("EQ" + n + "_FREQ"));
    const float y = eqDbToY(
        graph, parameterValue("EQ" + n + "_GAIN"));

    const float w = 230.f;
    const float h = 96.f;
    float px = x - w * 0.5f;
    float py = y - h - 34.f;

    if (py < graph.getY() + 6.f)
        py = y + 34.f;

    px = juce::jlimit(graph.getX() + 6.f,
                      graph.getRight() - w - 6.f, px);
    py = juce::jlimit(graph.getY() + 6.f,
                      graph.getBottom() - h - 6.f, py);
    return { px, py, w, h };
}

juce::Point<float> VVChainAudioProcessorEditor::dynamicTargetPoint(int band) const
{
    const auto graph = eqGraphBounds();
    const auto n = juce::String(band + 1);
    const float x = graphFrequencyToX(
        graph, parameterValue("EQ" + n + "_FREQ"));
    const float target = dynamicEffectiveTargetGain(band);
    return { x, eqDbToY(graph, target) };
}

bool VVChainAudioProcessorEditor::pointNearDynamicNode(
    juce::Point<float> p, int& band) const
{
    constexpr float hitRadius = 12.0f;
    constexpr float staticNodeRadius = 7.0f;

    band = -1;
    float best = hitRadius;

    // The Dynamic handle is the OUTER ring around the EQ node when
    // DYNAMICS is at 0%. This keeps the static Gain point and Dynamic
    // control from stealing the same click.
    for (int b = 0; b < 4; ++b)
    {
        const auto target = dynamicTargetPoint(b);
        const float d = p.getDistanceFrom(target);
        const float dynamics =
            juce::jlimit(-100.0f, 100.0f,
                         parameterValue("DYN_DYNAMICS" + juce::String(b + 1)));

        // The entire visible Dynamic target circle is draggable,
        // including its centre. The Static EQ point is resolved separately
        // before this hit-test, so it only wins when the two nodes overlap.
        const auto n = juce::String(b + 1);
        const float staticX = graphFrequencyToX(
            eqGraphBounds(), parameterValue("EQ" + n + "_FREQ"));
        const float staticY = eqDbToY(
            eqGraphBounds(), parameterValue("EQ" + n + "_GAIN"));
        const bool onStaticNode =
            p.getDistanceFrom({ staticX, staticY }) < staticNodeRadius;
        const bool hit =
            std::abs(dynamics) > 0.05f
            && !onStaticNode
            && d < hitRadius;

        if (hit && d < best)
        {
            best = d;
            band = b;
        }
    }

    return band >= 0;
}

float VVChainAudioProcessorEditor::dynamicAverageGainChangeDb(int band) const
{
    return audioProcessor.getDynamicAverageGainChangeDb(band);
}

float VVChainAudioProcessorEditor::dynamicMidGainChangeDb(int band) const
{
    return audioProcessor.getDynamicMidGainChangeDb(band);
}

float VVChainAudioProcessorEditor::dynamicSideGainChangeDb(int band) const
{
    return audioProcessor.getDynamicSideGainChangeDb(band);
}

void VVChainAudioProcessorEditor::drawEqGraph(
    juce::Graphics& g, juce::Rectangle<float> graph)
{
    metalLook.drawGraphSurface(g, graph);

    if (analyzerEnabled && !analyzerPath.isEmpty())
    {
        auto fill = analyzerPath;
        fill.lineTo(graph.getRight(), graph.getBottom());
        fill.lineTo(graph.getX(), graph.getBottom());
        fill.closeSubPath();

        g.setColour(uiColour(ivoryTheme
            ? juce::Colour(0xffe7dfd0).withAlpha(0.09f)
            : juce::Colour(0xffb8edf0).withAlpha(0.10f)));
        g.fillPath(fill);

        // Main Spectrum only. Module contribution analyzers were removed.
        g.setColour(uiColour(ivoryTheme
            ? juce::Colour(0xffd6cfbf).withAlpha(0.44f)
            : juce::Colour(0xffbceef0).withAlpha(0.42f)));
        g.strokePath(analyzerPath, juce::PathStrokeType(1.0f));
    }

    if (!metalLook.isMasterRuntimeActive())
    {
        g.setColour(uiColour(ivoryTheme ? juce::Colour(0xffb58839)
                                        : juce::Colour(0xff9e6042)).withAlpha(.92f));
        g.drawRoundedRectangle(graph, 8.f, 1.f);
    }

    // Use the entire available gain range: -18 dB at the bottom,
    // +18 dB at the top. Every 3 dB has a real grid line.
    for (int db = -18; db <= 18; db += 3)
    {
        const float y = eqDbToY(graph, static_cast<float>(db));
        const bool centre = (db == 0);
        g.setColour(centre
            ? juce::Colour(0xffffd84d).withAlpha(.45f)
            : (ivoryTheme ? juce::Colour(0xff8e887f).withAlpha(.30f)
                          : juce::Colour(0xff69717c).withAlpha(.34f)));
        g.drawHorizontalLine((int)y, graph.getX(), graph.getRight());
    }

    // Dense logarithmic frequency references so the EQ nodes are easy to place.
    constexpr std::array<float, 19> frequencyTicks
    {
        20.f, 30.f, 40.f, 50.f, 70.f, 100.f, 150.f, 200.f, 300.f,
        500.f, 700.f, 1000.f, 2000.f, 3000.f, 5000.f, 7000.f,
        10000.f, 15000.f, 20000.f
    };

    for (const auto f : frequencyTicks)
    {
        const float x = graphFrequencyToX(graph, f);
        g.setColour(uiColour(juce::Colour(0xff738087)).withAlpha(.27f));
        g.drawVerticalLine((int)x, graph.getY(), graph.getBottom());
    }

    const float xovers[3]
    {
        graphFrequencyToX(graph, parameterValue("UDMBC_X1")),
        graphFrequencyToX(graph, parameterValue("UDMBC_X2")),
        graphFrequencyToX(graph, parameterValue("UDMBC_X3"))
    };

    const float overlap = juce::jlimit(
        0.f, 100.f, parameterValue("XOVER_OVERLAP"));

    const float boundaries[5]
    {
        graph.getX(), xovers[0], xovers[1], xovers[2], graph.getRight()
    };

    for (int band = 0; band < 4; ++band)
    {
        const float amount =
            juce::jlimit(
                0.f, 100.f,
                parameterValue("EQ_COLOR" + juce::String(band + 1)))
            / 100.f;

        const float alpha = amount * .70f;
        if (alpha > 0.f && boundaries[band + 1] > boundaries[band])
        {
            g.setColour(
                uiColour(kBandColours[(size_t)band]).withAlpha(alpha));
            g.fillRect(
                boundaries[band], graph.getY(),
                boundaries[band + 1] - boundaries[band],
                graph.getHeight());
        }
    }

    // X1/X2/X3 crossover controls live at the BOTTOM of the graph.
    // Frequency remains draggable on the vertical line; OVERLAP is represented
    // by one continuous quadratic figure-eight at the bottom.
    for (int i = 0; i < 3; ++i)
    {
        const float x = xovers[i];
        const float hz = parameterValue(
            i == 0 ? "UDMBC_X1" : i == 1 ? "UDMBC_X2" : "UDMBC_X3");

        g.setColour(
            uiColour(ivoryTheme ? juce::Colour(0xffffc45a)
                                 : juce::Colour(0xff45e4e0)).withAlpha(.90f));
        g.drawVerticalLine(
            (int)x, graph.getY() + 20.f, graph.getBottom() - 28.f);

        const bool xoverHovered = hoverXover == i;
        const float markerY = graph.getBottom() - 18.f;
        const float labelY = graph.getBottom() - 35.f;

        g.setColour(juce::Colours::black.withAlpha(.74f));
        g.fillRoundedRectangle(
            x - 47.f, labelY, 94.f, 15.f, 3.f);
        g.setColour(
            uiColour(ivoryTheme ? juce::Colour(0xffffd37b)
                                 : juce::Colour(0xff73f0ed)));
        g.setFont(
            juce::FontOptions(7.5f).withStyle("Bold"));
        g.drawText(
            "X" + juce::String(i + 1) + "  "
                + formatGraphFrequency(hz),
            (int)x - 45, (int)labelY + 3, 90, 11,
            juce::Justification::centred);

        const float eightWidth = 5.f + overlap * 0.10f;
        const float eightHeight = 2.8f + overlap * 0.055f;
        g.setColour(uiColour(xoverHovered
            ? (ivoryTheme ? juce::Colour(0xfffff0b0)
                           : juce::Colour(0xffb8ffff))
            : (ivoryTheme ? juce::Colour(0xffffd37b)
                           : juce::Colour(0xff73f0ed))));

        juce::Path eight;
        eight.startNewSubPath(x, markerY);
        eight.quadraticTo(
            { x - eightWidth, markerY - eightHeight },
            { x, markerY - 2.f * eightHeight });
        eight.quadraticTo(
            { x + eightWidth, markerY - eightHeight },
            { x, markerY });
        eight.quadraticTo(
            { x - eightWidth, markerY + eightHeight },
            { x, markerY + 2.f * eightHeight });
        eight.quadraticTo(
            { x + eightWidth, markerY + eightHeight },
            { x, markerY });

        g.strokePath(eight, juce::PathStrokeType(
            xoverHovered ? 2.0f : 1.45f));
    }

    // Axis labels: dB on the LEFT; frequency along the BOTTOM.
    // Do not waste graph height: +18 / -18 are the actual top / bottom limits.
    g.setFont(juce::FontOptions(7.0f).withStyle("Bold"));
    g.setColour(uiColour(ivoryTheme ? juce::Colour(0xffe8deca)
                                    : juce::Colour(0xffc6d5d7)));

    for (int db = 18; db >= -18; db -= 3)
    {
        const float y = eqDbToY(graph, static_cast<float>(db));
        if (db == 15 || db == -15)
            continue;
        const auto label = (db > 0 ? "+" : "") + juce::String(db) + " dB";
        const int labelY = juce::jlimit(
            (int)graph.getY(),
            (int)graph.getBottom() - 11,
            (int)std::lround(y - 5.0f));

        g.drawText(label,
                   (int)graph.getX() + 5, labelY,
                   42, 11, juce::Justification::left);
    }

    const std::array<std::pair<float, juce::String>, 19> frequencyLabels
    {{
        { 20.f, "20" }, { 30.f, "30" }, { 40.f, "40" }, { 50.f, "50" },
        { 70.f, "70" }, { 100.f, "100" }, { 150.f, "150" }, { 200.f, "200" },
        { 300.f, "300" }, { 500.f, "500" }, { 700.f, "700" },
        { 1000.f, "1k" }, { 2000.f, "2k" }, { 3000.f, "3k" },
        { 5000.f, "5k" }, { 7000.f, "7k" }, { 10000.f, "10k" },
        { 15000.f, "15k" }, { 20000.f, "20k" }
    }};

    for (const auto& tick : frequencyLabels)
    {
        const float x = graphFrequencyToX(graph, tick.first);
        const int width = 28;
        int left = (int)std::lround(x - width * 0.5f);

        if (tick.first == 20.f)
            left = (int)graph.getX() + 1;
        else if (tick.first == 20000.f)
            left = (int)graph.getRight() - width - 1;

        g.drawText(tick.second,
                   left, (int)graph.getBottom() - 13,
                   width, 10, juce::Justification::centred);
    }

    // Static Offset EQ response.
    auto qForGain = [](float baseQ, float gainDb)
    {
        return juce::jlimit(
            0.10f, 18.0f,
            baseQ / (1.0f + 0.045f * std::abs(gainDb)));
    };

    const double graphSampleRate =
        audioProcessor.getSampleRate() > 1000.0
            ? audioProcessor.getSampleRate()
            : 44100.0;

    // UI response evaluator for all selectable minimum-phase filter shapes.
    // It changes only graph rendering; the realtime DSP uses updateEqFilter().
    auto filterShapeDb =
        [graphSampleRate](int type, float f0, float q,
                          float gainDb, float hz,
                          int slopeIndex) -> float
    {
        type = juce::jlimit(0, 13, type);
        const double sf = juce::jlimit(
            20.0, graphSampleRate * 0.45, static_cast<double>(f0));
        const double xHz = juce::jlimit(
            20.0, graphSampleRate * 0.45, static_cast<double>(hz));
        const double qq = juce::jlimit(0.10, 18.0, static_cast<double>(q));
        const double gain = juce::jlimit(
            -18.0, 18.0, static_cast<double>(gainDb));

        if (type <= 1)
            return VVChain_DynEQ_Engine::peakMagnitudeDBAtFrequency(
                graphSampleRate, sf, qq, gain, xHz);

        const double ratio = juce::jmax(1.0e-9, xHz / sf);
        const double logRatio = std::log2(ratio);

        if (type == 2 || type == 3)
        {
            const double width =
                juce::jlimit(0.15, 2.5, 0.90 / std::sqrt(qq));
            const double p = type == 3 ? 12.0 : 4.0;
            const double shape =
                1.0 / (1.0 + std::pow(std::abs(logRatio) / width, p));
            return static_cast<float>(gain * shape);
        }

        if (type == 4 || type == 5 || type == 8 || type == 9)
        {
            const bool high = type == 5 || type == 9;
            const double exponent = (type == 8 || type == 9) ? 0.8 : 2.0;
            const double shape = high
                ? 1.0 / (1.0 + std::pow(1.0 / ratio, exponent))
                : 1.0 / (1.0 + std::pow(ratio, exponent));
            return static_cast<float>(gain * shape);
        }

        if (type == 6 || type == 7)
        {
            const bool high = type == 7;
            const double shape = high
                ? 1.0 / (1.0 + std::pow(1.0 / ratio, 2.0))
                : 1.0 / (1.0 + std::pow(ratio, 2.0));
            const double resonanceGain =
                (gain >= 0.0 ? 1.0 : -1.0)
                * juce::jmin(6.0, std::abs(gain) * 0.35);
            const double resonance =
                VVChain_DynEQ_Engine::peakMagnitudeDBAtFrequency(
                    graphSampleRate, sf, qq, resonanceGain, xHz);
            return static_cast<float>(gain * shape + resonance);
        }

        if (type == 10)
        {
            const double width =
                juce::jlimit(0.04, 1.2, 0.55 / std::sqrt(qq));
            const double attenuation =
                -10.0 * std::log10(
                    1.0 + std::pow(std::abs(logRatio) / width, 4.0));
            return static_cast<float>(gain + attenuation);
        }

        if (type == 11)
        {
            const double width =
                juce::jlimit(0.015, 0.60, 0.16 / std::sqrt(qq));
            const double z = logRatio / width;
            return static_cast<float>(
                -60.0 * std::exp(-0.5 * z * z));
        }

        slopeIndex = juce::jlimit(0, 6, slopeIndex);
        const double order =
            slopeIndex == 0
                ? 1.0
                : 2.0 * static_cast<double>(slopeIndex);
        const double exponent = 2.0 * order;

        if (type == 12)
            return static_cast<float>(
                -10.0 * std::log10(
                    1.0 + std::pow(ratio, exponent)));

        return static_cast<float>(
            -10.0 * std::log10(
                1.0 + std::pow(1.0 / ratio, exponent)));
    };

    juce::Path offsetResponse;

    for (int i = 0; i <= 420; ++i)
    {
        const float hz = invLogMap(
            i / 420.f, 20.f, 20000.f);
        float db = 0.f;

        for (int band = 0; band < 4; ++band)
        {
            const auto n = juce::String(band + 1);
            const float f0 =
                parameterValue("EQ" + n + "_FREQ");
            const float gain =
                parameterValue("EQ" + n + "_GAIN");
            const float q =
                qForGain(
                    parameterValue("EQ" + n + "_Q"), gain);
            int type = juce::jlimit(
                0, 13,
                juce::roundToInt(
                    parameterValue("EQ" + n + "_TYPE")));
            if (type == 3)
                type = 2;
            else if (type == 1 || type == 10 || type == 11)
                type = 0;
            if ((band == 1 || band == 2) && type >= 12)
                type = 0;
            const int slopeIndex = juce::jlimit(
                0, 6,
                juce::roundToInt(
                    parameterValue("EQ" + n + "_SLOPE")));
            db += filterShapeDb(
                type, f0, q, gain, hz, slopeIndex);
        }

        const auto pt = juce::Point<float>(
            graphFrequencyToX(graph, hz),
            eqDbToY(graph, db));

        if (i == 0)
            offsetResponse.startNewSubPath(pt);
        else
            offsetResponse.lineTo(pt);
    }

    g.setColour(uiColour(ivoryTheme
        ? juce::Colour(0xffe8deca).withAlpha(.34f)
        : juce::Colour(0xffdff8f8).withAlpha(.31f)));
    g.strokePath(
        offsetResponse, juce::PathStrokeType(1.15f));

    // Sonnox-style Target/Offset dynamic fill.
    for (int band = 0; band < 4; ++band)
    {
        const auto n = juce::String(band + 1);
        const float f0 =
            parameterValue("EQ" + n + "_FREQ");
        const float offset =
            parameterValue("EQ" + n + "_GAIN");
        const float target =
            dynamicEffectiveTargetGain(band);
        const float baseQ =
            parameterValue("EQ" + n + "_Q");
        int filterType = juce::jlimit(
            0, 13,
            juce::roundToInt(
                parameterValue("EQ" + n + "_TYPE")));
        if (filterType == 3)
            filterType = 2;
        else if (filterType == 1 || filterType == 10 || filterType == 11)
            filterType = 0;
        if ((band == 1 || band == 2) && filterType >= 12)
            filterType = 0;
        const int slopeIndex = juce::jlimit(
            0, 6,
            juce::roundToInt(
                parameterValue("EQ" + n + "_SLOPE")));
        const auto c =
            uiColour(kBandColours[(size_t)band]);

        juce::Path top;
        juce::Path bottom;

        for (int i = 0; i <= 220; ++i)
        {
            const float hz =
                invLogMap(i / 220.f, 20.f, 20000.f);
            const float offsetQ =
                qForGain(baseQ, offset);
            const float targetQ =
                qForGain(baseQ, target);
            const float offsetDb =
                filterShapeDb(
                    filterType, f0, offsetQ, offset, hz,
                    slopeIndex);
            const float targetDb =
                filterShapeDb(
                    filterType, f0, targetQ, target, hz,
                    slopeIndex);
            const float gx =
                graphFrequencyToX(graph, hz);

            const float yA =
                eqDbToY(graph, offsetDb);
            const float yB =
                eqDbToY(graph, targetDb);

            if (i == 0)
            {
                top.startNewSubPath(gx, yA);
                bottom.startNewSubPath(gx, yB);
            }
            else
            {
                top.lineTo(gx, yA);
                bottom.lineTo(gx, yB);
            }
        }

        bottom = bottom.createPathWithRoundedCorners(0.f);

        juce::Path fill = top;
        fill.addPath(bottom);
        fill.closeSubPath();

        g.setColour(c.withAlpha(.16f));
        g.fillPath(fill);

        // Target response itself is visible as a thin coloured contour.
        juce::Path targetCurve;

        for (int i = 0; i <= 220; ++i)
        {
            const float hz =
                invLogMap(i / 220.f, 20.f, 20000.f);
            const float targetQ =
                qForGain(baseQ, target);
            const float y =
                eqDbToY(
                    graph,
                    filterShapeDb(
                        filterType, f0, targetQ, target, hz,
                        slopeIndex));
            const float gx =
                graphFrequencyToX(graph, hz);

            if (i == 0)
                targetCurve.startNewSubPath(gx, y);
            else
                targetCurve.lineTo(gx, y);
        }

        g.setColour(c.withAlpha(.58f));
        g.strokePath(
            targetCurve, juce::PathStrokeType(1.0f));

        const float x =
            graphFrequencyToX(graph, f0);
        const float offsetY =
            eqDbToY(graph, offset);
        const float targetY =
            eqDbToY(graph, target);

        const float liveGain =
            offset + dynamicAverageGainChangeDb(band);
        const float liveY =
            eqDbToY(graph, liveGain);

        // Offset / Target vertical span.
        g.setColour(c.withAlpha(.42f));
        g.drawLine(
            x, offsetY, x, targetY, 2.0f);

        // Offset handle.
        g.setColour(c.withAlpha(.85f));
        g.fillEllipse(
            x - 6.f, offsetY - 6.f, 12.f, 12.f);

        // Dynamic range handle: controls the same DYNAMICS parameter as the knob.
        // At 0%, the handle is an outer grab ring around the static EQ node.
        // At non-zero values it follows the dynamic target vertically.
        g.setColour(
            juce::Colours::black.withAlpha(.92f));
        g.fillEllipse(
            x - 10.f, targetY - 10.f, 20.f, 20.f);
        g.setColour(c.withAlpha(.98f));
        g.drawEllipse(
            x - 9.f, targetY - 9.f, 18.f, 18.f, 2.0f);

        // Live gain point = the actual dynamic state.
        g.setColour(juce::Colours::white);
        g.fillEllipse(
            x - 6.f, liveY - 6.f, 12.f, 12.f);

        // Dedicated DYNAMICS arrow exists only while the Dynamic target
        // is still coincident with the EQ point (DYNAMICS ~= 0). Once the
        // target is pulled out, the target circle itself becomes the control.
        const bool showDynamicsArrow =
            std::abs(parameterValue("DYN_DYNAMICS" + n)) <= 0.05f;
        // 30% closer to the EQ/Dynamic node than the previous 44 px spacing.
        const float handleX =
            juce::jlimit(graph.getX() + 18.f,
                         graph.getRight() - 12.f,
                         x + 31.f);
        const float handleY = targetY;

        if (showDynamicsArrow)
        {
            g.setColour(c.withAlpha(.92f));
            g.drawLine(handleX, handleY - 8.f,
                       handleX, handleY + 8.f, 1.4f);
            juce::Path upArrow;
            upArrow.startNewSubPath(handleX - 4.f, handleY - 4.f);
            upArrow.lineTo(handleX, handleY - 8.f);
            upArrow.lineTo(handleX + 4.f, handleY - 4.f);
            g.strokePath(upArrow, juce::PathStrokeType(1.6f));
            juce::Path downArrow;
            downArrow.startNewSubPath(handleX - 4.f, handleY + 4.f);
            downArrow.lineTo(handleX, handleY + 8.f);
            downArrow.lineTo(handleX + 4.f, handleY + 4.f);
            g.strokePath(downArrow, juce::PathStrokeType(1.6f));
        }

        const float midChange =
            dynamicMidGainChangeDb(band);
        const float sideChange =
            dynamicSideGainChangeDb(band);

        const float midMag =
            juce::jlimit(0.f, 12.f, std::abs(midChange));
        const float sideMag =
            juce::jlimit(0.f, 12.f, std::abs(sideChange));

        juce::Path midRing;
        midRing.addCentredArc(
            x, liveY, 15.f, 15.f, 0.f,
            -juce::MathConstants<float>::halfPi,
            -juce::MathConstants<float>::halfPi
                + juce::MathConstants<float>::pi
                    * (midMag / 12.f),
            true);
        g.setColour(c.brighter(.30f));
        g.strokePath(
            midRing, juce::PathStrokeType(2.7f));

        juce::Path sideRing;
        sideRing.addCentredArc(
            x, liveY, 15.f, 15.f, 0.f,
            juce::MathConstants<float>::halfPi,
            juce::MathConstants<float>::halfPi
                + juce::MathConstants<float>::pi
                    * (sideMag / 12.f),
            true);
        g.setColour(
            juce::Colours::white.withAlpha(.82f));
        g.strokePath(
            sideRing, juce::PathStrokeType(2.3f));

        // Compact two-line FloatingValueBox is the only EQ/Dynamic EQ hover readout.

        if (expandedDynamicBand == band)
        {
            const auto popup =
                dynamicMsPopupBounds(band);

            g.setColour(
                juce::Colours::black.withAlpha(.80f));
            g.fillRoundedRectangle(
                popup.translated(5.f, 6.f), 8.f);

            juce::ColourGradient panel(
                juce::Colour(0xff30343a),
                popup.getX(), popup.getY(),
                juce::Colour(0xff12151a),
                popup.getRight(), popup.getBottom(),
                false);
            g.setGradientFill(panel);
            g.fillRoundedRectangle(
                popup, 8.f);

            g.setColour(c.withAlpha(.94f));
            g.drawRoundedRectangle(
                popup, 8.f, 1.1f);

            g.setColour(juce::Colours::white);
            g.setFont(
                juce::FontOptions(11.f).withStyle("Bold"));
            g.drawText(
                "BAND " + n + " · DYNAMIC MID / SIDE",
                (int)popup.getX() + 12,
                (int)popup.getY() + 9,
                204, 16,
                juce::Justification::left);

            const float midPct =
                juce::jlimit(
                    0.f, 100.f,
                    parameterValue("DYN_MS" + n));
            const float sidePct =
                100.f - midPct;

            g.setColour(c.brighter(.25f));
            g.setFont(
                juce::FontOptions(10.5f).withStyle("Bold"));
            g.drawText(
                "MID " + juce::String(midPct, 0) + "%",
                (int)popup.getX() + 12,
                (int)popup.getY() + 30,
                88, 15,
                juce::Justification::left);

            g.setColour(
                juce::Colours::white.withAlpha(.92f));
            g.drawText(
                "SIDE " + juce::String(sidePct, 0) + "%",
                (int)popup.getRight() - 100,
                (int)popup.getY() + 30,
                88, 15,
                juce::Justification::right);

            const auto bar =
                popup.reduced(12.f)
                    .withY(popup.getY() + 52.f)
                    .withHeight(11.f);

            g.setColour(juce::Colour(0xff090c10));
            g.fillRoundedRectangle(bar, 5.f);

            const float splitX =
                bar.getX()
                + bar.getWidth() * midPct / 100.f;

            if (splitX > bar.getX())
            {
                g.setColour(c.withAlpha(.94f));
                g.fillRoundedRectangle(
                    { bar.getX(), bar.getY(),
                      splitX - bar.getX(),
                      bar.getHeight() }, 5.f);
            }

            if (splitX < bar.getRight())
            {
                g.setColour(
                    juce::Colours::white.withAlpha(.56f));
                g.fillRoundedRectangle(
                    { splitX, bar.getY(),
                      bar.getRight() - splitX,
                      bar.getHeight() }, 5.f);
            }

            g.setColour(juce::Colours::white);
            g.fillEllipse(
                splitX - 6.f,
                bar.getCentreY() - 6.f,
                12.f, 12.f);

            g.setColour(
                juce::Colour(0xffc0c7d0));
            g.setFont(
                juce::FontOptions(8.3f).withStyle("Bold"));
            g.drawText(
                "DRAG BAR = MID / SIDE",
                (int)popup.getX() + 12,
                (int)popup.getBottom() - 18,
                (int)popup.getWidth() - 24, 12,
                juce::Justification::centred);
        }
    }

    // Right-click SOLO spotlight: keep the exact mouse position bright,
    // then smoothly darken every other element in the upper graph.
    if (rightSoloBand >= 0
        && parameterValue("GRAPH_SOLO_ACTIVE") > 0.5f)
    {
        const float cx = juce::jlimit(
            graph.getX(), graph.getRight(), rightSoloPosition.x);
        const float cy = juce::jlimit(
            graph.getY(), graph.getBottom(), rightSoloPosition.y);
        constexpr float spotlightRadius = 155.0f;

        juce::ColourGradient spotlight(
            juce::Colours::black.withAlpha(0.0f),
            cx, cy,
            juce::Colours::black.withAlpha(0.504f),
            cx + spotlightRadius, cy,
            true);
        spotlight.addColour(0.22, juce::Colours::black.withAlpha(0.0f));
        spotlight.addColour(0.52, juce::Colours::black.withAlpha(0.126f));
        spotlight.addColour(0.76, juce::Colours::black.withAlpha(0.336f));

        g.setGradientFill(spotlight);
        g.fillRoundedRectangle(graph, 8.0f);

        // A faint halo makes the SOLO focus location immediately readable
        // without covering the EQ / Dynamic node itself.
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.drawEllipse(cx - 46.0f, cy - 46.0f, 92.0f, 92.0f, 1.2f);

        // SOLO dimming must never reduce the reference grid/readability.
        // Redraw only the frequency/dB standard lines + axis labels above
        // the spotlight overlay; EQ/Dynamic content remains dimmed.
        for (int db = -18; db <= 18; db += 3)
        {
            const float y = eqDbToY(graph, static_cast<float>(db));
            const bool centre = (db == 0);
            g.setColour(centre
                ? juce::Colour(0xffffd84d).withAlpha(.45f)
                : juce::Colour(0xff69717c).withAlpha(.34f));
            g.drawHorizontalLine((int)y, graph.getX(), graph.getRight());
        }

        for (const auto f : frequencyTicks)
        {
            const float x = graphFrequencyToX(graph, f);
            g.setColour(juce::Colour(0xff68727d).withAlpha(.28f));
            g.drawVerticalLine((int)x, graph.getY(), graph.getBottom());
        }

        g.setFont(juce::FontOptions(7.0f).withStyle("Bold"));
        g.setColour(juce::Colour(0xffb8bec6));

        for (int db = 18; db >= -18; db -= 3)
        {
            if (db == 15 || db == -15)
                continue;

            const float y = eqDbToY(graph, static_cast<float>(db));
            const auto label = (db > 0 ? "+" : "") + juce::String(db) + " dB";
            const int labelY = juce::jlimit(
                (int)graph.getY(),
                (int)graph.getBottom() - 11,
                (int)std::lround(y - 5.0f));

            g.drawText(label,
                       (int)graph.getX() + 5, labelY,
                       42, 11, juce::Justification::left);
        }

        for (const auto& tick : frequencyLabels)
        {
            const float x = graphFrequencyToX(graph, tick.first);
            const int width = 28;
            int left = (int)std::lround(x - width * 0.5f);

            if (tick.first == 20.f)
                left = (int)graph.getX() + 1;
            else if (tick.first == 20000.f)
                left = (int)graph.getRight() - width - 1;

            g.drawText(tick.second,
                       left, (int)graph.getBottom() - 13,
                       width, 10, juce::Justification::centred);
        }
    }


    // Mirror the per-band BYPASS state in the upper frequency zone.
    for (int band = 0; band < 4; ++band)
    {
        const bool bypassed =
            parameterValue("UDMBC_BAND_BYPASS" + juce::String(band + 1)) > 0.5f;
        if (!bypassed)
            continue;

        const float leftX = boundaries[band];
        const float rightX = boundaries[band + 1];
        g.setColour(juce::Colour(0xff5b6066).withAlpha(.62f));
        g.fillRect(leftX, graph.getY(),
                   juce::jmax(0.f, rightX - leftX),
                   graph.getHeight());
    }


    drawGraphDragHint(g, graph);
}

void VVChainAudioProcessorEditor::drawCard(
    juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent,
    const juce::String& title, const juce::String& subtitle)
{
    const bool masterRuntime = metalLook.isMasterRuntimeActive();
    const auto frame = uiColour(
        ivoryTheme ? juce::Colour(0xffa9792d)
                   : juce::Colour(0xff9b593d));

    if (!masterRuntime)
    {
        g.setColour(juce::Colours::black.withAlpha(
            ivoryTheme ? .24f : .52f));
        g.fillRoundedRectangle(r.translated(0.f, 3.f), 8.f);

        metalLook.drawModuleSurface(g, r);

        g.setColour(frame.withAlpha(.90f));
        g.drawRoundedRectangle(r, 8.f, 1.35f);
        g.setColour(juce::Colours::white.withAlpha(.15f));
        g.drawLine(r.getX() + 7.f, r.getY() + 2.f,
                   r.getRight() - 7.f, r.getY() + 2.f, .8f);

        accent = uiColour(accent);
        g.setColour(accent.withAlpha(.44f));
        g.fillRoundedRectangle(r.getX() + 3.f, r.getY() + 3.f,
                               2.5f, r.getHeight() - 6.f, 1.2f);
    }

    const bool monitorCard = title == "MASTER";
    const int titleY = monitorCard ? 36 : 8;
    g.setColour(uiColour(ivoryTheme ? juce::Colour(0xff2a231d)
                                    : juce::Colour(0xfff0f3f3)));
    g.setFont(juce::FontOptions(12.5f).withStyle("Bold"));
    if (monitorCard)
        g.drawText(title, (int)r.getX(), (int)r.getY() + titleY,
                   (int)r.getWidth(), 18, juce::Justification::centred);
    else
        g.drawText(title, (int)r.getX() + 13, (int)r.getY() + titleY,
                   100, 18, juce::Justification::left);

    if (!monitorCard)
    {
        g.setColour(uiColour(ivoryTheme ? juce::Colour(0xff63584b)
                                        : juce::Colour(0xffb3bec0)));
        g.setFont(juce::FontOptions(8.1f).withStyle("Bold"));
        g.drawText(subtitle, (int)r.getX() + 13, (int)r.getY() + 26,
                   (int)r.getWidth() - 80, 12, juce::Justification::left);
    }

    g.setColour(frame.withAlpha(.22f));
    g.drawRoundedRectangle(r.reduced(2.f), 6.f, 1.f);
}

void VVChainAudioProcessorEditor::drawPanel(
    juce::Graphics& g, juce::Rectangle<float> r,
    const juce::String& title, const juce::String& subtitle, juce::Colour accent)
{
    metalLook.drawModuleSurface(g, r);
    g.setColour(uiColour(accent).withAlpha(.58f));
    g.fillRoundedRectangle(r.getX(), r.getY(), 4.f, r.getHeight(), 2.f);

    g.setColour(uiColour(ivoryTheme ? juce::Colour(0xff33291f)
                                    : juce::Colour(0xffeef4f4)));
    g.setFont(juce::FontOptions(10.f).withStyle("Bold"));
    g.drawText(title, (int)r.getX()+12, (int)r.getY()+7, 280, 16,
               juce::Justification::left);
    g.setColour(uiColour(ivoryTheme ? juce::Colour(0xff685a49)
                                    : juce::Colour(0xffa9b7b8)));
    g.setFont(juce::FontOptions(7.5f));
    g.drawText(subtitle, (int)r.getX()+12, (int)r.getY()+23,
               (int)r.getWidth()-20, 12, juce::Justification::left);
}

void VVChainAudioProcessorEditor::drawModuleLeds(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

juce::Colour VVChainAudioProcessorEditor::uiColour(juce::Colour c) const noexcept
{
    return metalLook.monochrome ? c.withSaturation(0.0f) : c;
}

bool VVChainAudioProcessorEditor::isMasterBypassed() const noexcept
{
    return parameterValue("MASTER_BYPASS") > 0.5f;
}

void VVChainAudioProcessorEditor::updateBypassVisuals()
{
    const bool bypassed = isMasterBypassed();
    if (bypassed == lastMasterBypassUi && metalLook.monochrome == bypassed)
        return;

    lastMasterBypassUi = bypassed;
    metalLook.monochrome = bypassed;

    for (auto& k : knobs)
    {
        const auto c = uiColour(k.accent);
        k.slider->setColour(juce::Slider::rotarySliderFillColourId, c);
        k.label->setColour(juce::Label::textColourId,
                           uiColour(k.accent.brighter(.35f)));
    }

    const std::array<juce::Colour, 4> moduleColours
    {{
        juce::Colour(0xff38bdf8),
        juce::Colour(0xfffacc15),
        juce::Colour(0xff60a5fa),
        juce::Colour(0xfff472b6)
    }};

    for (int i = 0; i < 4; ++i)
        if (bypassButtons[(size_t) i])
            bypassButtons[(size_t) i]->setColour(
                juce::ToggleButton::tickColourId,
                uiColour(moduleColours[(size_t) i]));

    if (masterBypassButton)
        masterBypassButton->setColour(
            juce::ToggleButton::tickColourId, uiColour(juce::Colour(0xffdfe7ef)));

    for (auto& b : udmbcBandBypassButtons)
        if (b)
            b->setColour(juce::ToggleButton::tickColourId,
                         uiColour(juce::Colour(0xfffacc15)));

    for (auto& b : tapeBandBypassButtons)
        if (b)
            b->setColour(juce::ToggleButton::tickColourId,
                         uiColour(juce::Colour(0xfff472b6)));

    for (size_t band = 0; band < analogModeButtons.size(); ++band)
    {
        if (analogModeButtons[band])
            analogModeButtons[band]->setColour(
                juce::ToggleButton::tickColourId,
                uiColour(kBandColours[band]));
        if (analogBypassButtons[band])
            analogBypassButtons[band]->setColour(
                juce::ToggleButton::tickColourId,
                uiColour(juce::Colour(0xff60a5fa)));
        if (analogX2Buttons[band])
            analogX2Buttons[band]->setColour(
                juce::ToggleButton::tickColourId,
                uiColour(juce::Colour(0xff60a5fa)));
    }

    if (soloModeButton)
        soloModeButton->setColour(juce::ToggleButton::tickColourId,
                                  uiColour(juce::Colour(0xffdfe7ef)));

    for (auto& b : advancedButtons)
        if (b)
        {
            b->setColour(
                juce::TextButton::buttonColourId,
                uiColour(b->getToggleState()
                    ? juce::Colour(0xff3f3517)
                    : juce::Colour(0xff17191d)));
            b->setColour(
                juce::TextButton::textColourOffId,
                uiColour(juce::Colour(0xffc0c5cb)));
        }

    if (udmbcClipper)
        udmbcClipper->setColour(
            juce::ToggleButton::tickColourId, uiColour(juce::Colour(0xfffacc15)));

    repaint();
}

void VVChainAudioProcessorEditor::setGraphControlState(
    const juce::StringArray& ids, bool moving)
{
    for (auto& k : knobs)
    {
        if (auto* slider = dynamic_cast<WheelSlider*>(k.slider.get()))
        {
            const bool active = ids.contains(k.id);
            slider->setGraphControlState(active, active && moving);
        }
    }
}

void VVChainAudioProcessorEditor::clearGraphControlState()
{
    juce::StringArray none;
    setGraphControlState(none, false);
}

void VVChainAudioProcessorEditor::setGraphControlMoving(bool moving)
{
    for (auto& k : knobs)
    {
        if (auto* slider = dynamic_cast<WheelSlider*>(k.slider.get()))
        {
            if (slider->isGraphControlActive())
                slider->setGraphControlState(true, moving);
        }
    }
}

void VVChainAudioProcessorEditor::pulseGraphControlMovement(
    const juce::StringArray& ids)
{
    if (ids.isEmpty())
    {
        clearGraphControlState();
        return;
    }

    setGraphControlState(ids, true);
    const int generation = ++graphMovementPulseGeneration;
    auto safeThis =
        juce::Component::SafePointer<VVChainAudioProcessorEditor>(this);

    juce::Timer::callAfterDelay(
        110,
        [safeThis, generation]
        {
            if (safeThis != nullptr
                && safeThis->graphMovementPulseGeneration == generation)
                safeThis->clearGraphControlState();
        });
}

void VVChainAudioProcessorEditor::timerCallback()
{
    audioProcessor.setAnalyzerEnabled(analyzerEnabled && isShowing());
    updateAnalyzer();
    for (auto& k : knobs)
        if (auto* slider = dynamic_cast<WheelSlider*>(k.slider.get()))
            if (slider->isGraphControlActive())
                slider->repaint();

    if (isMasterBypassed() != lastMasterBypassUi)
        updateBypassVisuals();

    const int soloRaw = static_cast<int>(std::lround(parameterValue("SOLO_BAND")));
    const int soloBand = soloRaw - 1;
    for (int b = 0; b < 4; ++b)
        if (soloButtons[(size_t) b])
            soloButtons[(size_t) b]->setToggleState(soloBand == b,
                                                    juce::dontSendNotification);

    if (soloModeButton)
        soloModeButton->setButtonText(
            parameterValue("SOLO_MODE") > 0.5f ? "SOLO POST" : "SOLO PRE");

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        if (dynTriggerButtons[(size_t) b])
            dynTriggerButtons[(size_t) b]->setButtonText(
                parameterValue("DYN_TRIGGER_BELOW" + n) > 0.5f
                    ? "BELOW" : "ABOVE");
    }

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const bool eqMuted =
            parameterValue("EQ_BYPASS") > 0.5f;
        const bool udmbcMuted =
            parameterValue("UDMBC_BYPASS") > 0.5f
            || (udmbcUiTouched[(size_t)b]
                && (parameterValue("UDMBC_BAND_BYPASS" + n) > 0.5f
                    || parameterValue("UDMBC_DEGREE" + n) <= 0.0001f));
        const bool analogMuted =
            parameterValue("EQ_COLOR_GLOBAL_BYPASS") > 0.5f
            || (analogUiTouched[(size_t)b]
                && (parameterValue("EQ_COLOR_BYPASS" + n) > 0.5f
                    || parameterValue("EQ_COLOR" + n) <= 0.0001f));
        const bool tapeMuted =
            parameterValue("TAPE_BYPASS") > 0.5f
            || (tapeUiTouched[(size_t)b]
                && (parameterValue("TAPE_BAND_BYPASS" + n) > 0.5f
                    || parameterValue("TAPE_DEGREE" + n) <= 0.0001f));
        const auto setKnobAlpha = [this](const juce::String& id, bool muted)
        {
            if (auto* knob = findKnob(id))
            {
                const float alpha = 1.0f;
                knob->slider->getProperties().set("moduleMuted", muted);
                knob->slider->setAlpha(alpha);
                knob->label->setAlpha(muted ? 0.68f : 1.0f);
                knob->label->setColour(
                    juce::Label::textColourId,
                    muted
                        ? (ivoryTheme ? juce::Colour(0xff706b64)
                                      : juce::Colour(0xff858d8f))
                        : (ivoryTheme ? juce::Colour(0xff3b3026)
                                      : uiColour(knob->accent.brighter(.28f))));
                knob->slider->repaint();
            }
        };
        setKnobAlpha("EQ" + n + "_GAIN", eqMuted);
        setKnobAlpha("EQ" + n + "_FREQ", eqMuted);

        if (auto* qKnob = findKnob("EQ" + n + "_Q"))
        {
            const int filterType = juce::jlimit(
                0, 13,
                juce::roundToInt(
                    parameterValue("EQ" + n + "_TYPE")));
            const bool wantsSlope = filterType >= 12;

            if (qKnob->slopeMode != wantsSlope)
            {
                qKnob->attachment.reset();

                if (wantsSlope)
                {
                    qKnob->label->setText(
                        "OCT", juce::dontSendNotification);
                    qKnob->slider->setRange(0.0, 6.0, 1.0);
                    qKnob->slider->setNumDecimalPlacesToDisplay(0);
                    qKnob->slider->setDoubleClickReturnValue(true, 1.0);
                    qKnob->slider->textFromValueFunction =
                        [](double value)
                        {
                            const int index =
                                juce::jlimit(
                                    0, 6,
                                    juce::roundToInt(value));
                            const int slope =
                                index == 0 ? 6 : index * 12;
                            return juce::String(slope) + " dB/oct";
                        };
                    qKnob->slider->valueFromTextFunction =
                        [](const juce::String& text)
                        {
                            const double slope =
                                text.retainCharacters(
                                        "0123456789.")
                                    .getDoubleValue();
                            if (slope <= 9.0)
                                return 0.0;
                            return static_cast<double>(
                                juce::jlimit(
                                    1, 6,
                                    juce::roundToInt(
                                        slope / 12.0)));
                        };

                    if (auto* wheel =
                            dynamic_cast<WheelSlider*>(
                                qKnob->slider.get()))
                    {
                        wheel->setDragSensitivity(90, 900);
                        wheel->setWheelBehaviour(1.0, false);
                        wheel->setWheelSingleStepPerEvent(true);
                    }

                    qKnob->attachment =
                        std::make_unique<Attachment>(
                            audioProcessor.apvts,
                            "EQ" + n + "_SLOPE",
                            *qKnob->slider);
                }
                else
                {
                    qKnob->label->setText(
                        "Q", juce::dontSendNotification);
                    qKnob->slider->setRange(0.10, 18.0, 0.01);
                    qKnob->slider->setNumDecimalPlacesToDisplay(2);
                    qKnob->slider->setDoubleClickReturnValue(true, 0.707);
                    qKnob->slider->textFromValueFunction =
                        [](double value)
                        {
                            return juce::String(value, 2);
                        };
                    qKnob->slider->valueFromTextFunction =
                        [](const juce::String& text)
                        {
                            return text.retainCharacters(
                                           "0123456789.-")
                                .getDoubleValue();
                        };

                    if (auto* wheel =
                            dynamic_cast<WheelSlider*>(
                                qKnob->slider.get()))
                    {
                        wheel->setDragSensitivity(225, 2250);
                        wheel->setWheelBehaviour(0.016, false);
                        wheel->setWheelSingleStepPerEvent(false);
                    }

                    qKnob->attachment =
                        std::make_unique<Attachment>(
                            audioProcessor.apvts,
                            "EQ" + n + "_Q",
                            *qKnob->slider);
                }

                qKnob->slopeMode = wantsSlope;
                if (auto* wheel =
                        dynamic_cast<WheelSlider*>(
                            qKnob->slider.get()))
                    wheel->refreshDisplayedText();
            }
        }

        setKnobAlpha("EQ" + n + "_Q", eqMuted);
        setKnobAlpha("DYN_DYNAMICS" + n, eqMuted);
        setKnobAlpha("DYN_ATTACK" + n, eqMuted);
        setKnobAlpha("DYN_RELEASE" + n, eqMuted);
        if (dynDetectSliders[(size_t)b])
        {
            dynDetectSliders[(size_t)b]->getProperties().set(
                "moduleMuted", eqMuted);
            dynDetectSliders[(size_t)b]->setAlpha(1.0f);
            dynDetectSliders[(size_t)b]->repaint();
        }
        if (dynTriggerButtons[(size_t)b])
        {
            dynTriggerButtons[(size_t)b]->getProperties().set(
                "moduleMuted", eqMuted);
            dynTriggerButtons[(size_t)b]->setAlpha(1.0f);
            dynTriggerButtons[(size_t)b]->repaint();
        }
        setKnobAlpha("UDMBC_DEGREE" + n, udmbcMuted);
        setKnobAlpha("UDMBC_COMP_A" + n, udmbcMuted);
        setKnobAlpha("UDMBC_COMP_R" + n, udmbcMuted);
        setKnobAlpha("EQ_COLOR_B" + n, analogMuted);
        setKnobAlpha("TAPE_DEGREE" + n, tapeMuted);
        if (analogModeButtons[(size_t)b])
        {
            analogModeButtons[(size_t)b]->getProperties().set(
                "moduleMuted", analogMuted);
            analogModeButtons[(size_t)b]->setAlpha(
                analogMuted ? 0.62f : 1.0f);
            analogModeButtons[(size_t)b]->repaint();
        }
        if (analogX2Buttons[(size_t)b])
        {
            analogX2Buttons[(size_t)b]->getProperties().set(
                "moduleMuted", analogMuted);
            analogX2Buttons[(size_t)b]->setAlpha(
                analogMuted ? 0.62f : 1.0f);
            analogX2Buttons[(size_t)b]->repaint();
        }

        const auto setLedForceOff =
            [](juce::ToggleButton* button, bool forceOff)
            {
                if (button == nullptr)
                    return;

                button->getProperties().set(
                    "forceLedOff", forceOff);
                button->repaint();
            };

        // LED indicates actual processing amount, not merely "not bypassed".
        // At the untouched default of 0 the knob stays fully visible but the
        // LED remains dark. Moving above 0 lights it; returning to 0 turns it
        // dark again.
        setLedForceOff(
            udmbcBandBypassButtons[(size_t)b].get(),
            parameterValue("UDMBC_DEGREE" + n) <= 0.0001f);
        setLedForceOff(
            analogBypassButtons[(size_t)b].get(),
            parameterValue("EQ_COLOR" + n) <= 0.0001f);
        setLedForceOff(
            tapeBandBypassButtons[(size_t)b].get(),
            parameterValue("TAPE_DEGREE" + n) <= 0.0001f);
    }

    if (deltaMonitorButton)
        deltaMonitorButton->setToggleState(
            parameterValue("DELTA_MONITOR") > 0.5f,
            juce::dontSendNotification);

    repaint();
}

juce::String VVChainAudioProcessorEditor::formatGraphFrequency(float hz) const
{
    hz = juce::jmax(20.0f, hz);
    if (hz >= 1000.0f)
        return juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String(hz, hz >= 100.0f ? 0 : 1) + " Hz";
}

void VVChainAudioProcessorEditor::drawGraphDragHint(
    juce::Graphics& g, juce::Rectangle<float> graph)
{
    if (!showGraphDragHint)
        return;

    // Only X-over drags use the old graph-anchored hint.
    if (dragXover < 0 && dragOverlapXover < 0)
        return;

    const float boxH = 30.0f;

    if (graphDragHint.isEmpty())
        return;

    g.setFont(juce::FontOptions(8.5f).withStyle("Bold"));
    const float boxW = juce::jlimit(
        175.0f, graph.getWidth() - 12.0f,
        static_cast<float>(graphDragHint.length()) * 4.7f + 16.0f);

    float bx = graphDragHintPosition.x + 14.0f;
    float by = graphDragHintPosition.y - boxH - 10.0f;
    if (bx + boxW > graph.getRight() - 6.0f)
        bx = graphDragHintPosition.x - boxW - 14.0f;
    if (by < graph.getY() + 6.0f)
        by = graphDragHintPosition.y + 14.0f;

    bx = juce::jlimit(graph.getX() + 6.0f,
                      graph.getRight() - boxW - 6.0f, bx);
    by = juce::jlimit(graph.getY() + 6.0f,
                      graph.getBottom() - boxH - 6.0f, by);

    g.setColour(juce::Colour(0xff07090c).withAlpha(.95f));
    g.fillRoundedRectangle(bx, by, boxW, boxH, 5.0f);
    g.setColour(juce::Colours::white.withAlpha(.92f));
    g.drawRoundedRectangle(bx, by, boxW, boxH, 5.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.drawText(graphDragHint,
               juce::Rectangle<int>((int) bx + 7, (int) by + 1,
                                    (int) boxW - 14, (int) boxH - 2),
               juce::Justification::centred);
}

void VVChainAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(uiColour(ivoryTheme ? juce::Colour(0xffded4c4)
                                  : juce::Colour(0xff101719)));
    metalLook.drawPanelSurface(g, getLocalBounds().toFloat());

    const bool masterRuntime = metalLook.isMasterRuntimeActive();
    const auto shellFrame = uiColour(
        ivoryTheme ? juce::Colour(0xffad7c2f)
                   : juce::Colour(0xffa55d3d));
    const bool uiMuted = metalLook.monochrome;

    if (!masterRuntime)
    {
        g.setColour(shellFrame.withAlpha(.92f));
        g.fillRect(0, 68, getWidth(), 2);
        g.setColour(juce::Colours::white.withAlpha(.18f));
        g.fillRect(0, 1, getWidth(), 1);

        constexpr float screwSize = 22.0f;
        metalLook.drawScrew(g, { 6.0f, 5.0f, screwSize, screwSize }, uiMuted);
        metalLook.drawScrew(g, { (float)getWidth() - 28.0f, 5.0f, screwSize, screwSize }, uiMuted);
        metalLook.drawScrew(g, { 6.0f, (float)getHeight() - 28.0f, screwSize, screwSize }, uiMuted);
        metalLook.drawScrew(g, { (float)getWidth() - 28.0f, (float)getHeight() - 28.0f, screwSize, screwSize }, uiMuted);
    }

    g.setColour(uiColour(ivoryTheme ? juce::Colour(0xff3b2c20)
                                    : juce::Colour(0xfff1f3f3)));
    g.setFont(juce::FontOptions(23.f).withStyle("Bold"));
    g.drawText("VVCHAIN", 18, 8, 240, 27, juce::Justification::left);

    g.setColour(uiColour(ivoryTheme ? juce::Colour(0xff6a5b49)
                                    : juce::Colour(0xffb8c1c3)));
    g.setFont(juce::FontOptions(8.2f).withStyle("Bold"));
    g.drawText("VVCHAIN v1.0.83", 20, 39, 180, 12,
               juce::Justification::left);

    const auto graph = eqGraphBounds();
    drawEqGraph(g, graph);

    const int cardY = masterRuntime ? 392 : 404;
    const int gap = masterRuntime ? 10 : 8;
    const int left = masterRuntime ? 17 : 18;
    const int unitW = masterRuntime
        ? (getWidth() - left * 2 - gap * 4) / 5
        : (getWidth() - left * 2 - gap * 5) / 5;
    const int cardW = unitW;
    const int cardH = masterRuntime ? 615 : 510;

    for (int b = 0; b < 4; ++b)
    {
        const int x = left + b * (cardW + gap);
        drawCard(g,
                 { (float) x, (float) cardY, (float) cardW, (float) cardH },
                 uiColour(kBandColours[(size_t) b]),
                 "BAND " + juce::String(b + 1),
                 "TRANSIENT · ANALOG · UDMBC · TAPE COLOR");
    }

    {
        const int x = left + 4 * (cardW + gap);
        drawCard(g,
                 { (float) x, (float) cardY, (float) cardW, (float) cardH },
                 uiColour(juce::Colour(0xffe5e7eb)),
                 "MASTER",
                 "GLOBAL BYPASS · DELTA · MIX / OUT");
    }

    // Floating UDMBC Advanced popup: it overlays the controls and never changes band height.
    if (expandedBand >= 0)
    {
        const float popupW = juce::jmin(900.f, (float) getWidth() - 40.f);
        const float popupH = 330.f;
        const float popupX = ((float) getWidth() - popupW) * 0.5f;
        const float popupY = ((float) getHeight() - popupH) * 0.5f;

        g.setColour(juce::Colours::black.withAlpha(.72f));
        g.fillRoundedRectangle(popupX + 7.f, popupY + 9.f, popupW, popupH, 10.f);

        juce::ColourGradient popup(
            juce::Colour(0xff28231a), popupX, popupY,
            juce::Colour(0xff101216), popupX, popupY + popupH, false);
        g.setGradientFill(popup);
        g.fillRoundedRectangle(popupX, popupY, popupW, popupH, 10.f);

        g.setColour(uiColour(juce::Colour(0xfffacc15)).withAlpha(.8f));
        g.drawRoundedRectangle(popupX, popupY, popupW, popupH, 10.f, 1.2f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(12.f).withStyle("Bold"));
        g.drawText("UDMBC ADVANCED · BAND " + juce::String(expandedBand + 1),
                   (int) popupX + 14, (int) popupY + 9, 330, 18,
                   juce::Justification::left);

        g.setColour(juce::Colour(0xff8a9098));
        g.setFont(juce::FontOptions(7.5f));
        g.drawText("完整進階參數 · 關閉後主畫面比例不變",
                   (int) popupX + 14, (int) popupY + 28, 300, 12,
                   juce::Justification::left);
    }

    g.setColour(juce::Colour(0xff606873));
    g.setFont(juce::FontOptions(7.f));
    g.drawText("SHARED X-OVER = 3 LINES / 4 ZONES · WHEEL ON LINE = OVERLAP",
               18, 919, 780, 10, juce::Justification::left);
}

void VVChainAudioProcessorEditor::setExpandedBand(int band)
{
    expandedBand = band;

    for (int b = 0; b < 4; ++b)
    {
        if (advancedButtons[(size_t) b])
        {
            advancedButtons[(size_t) b]->setButtonText(
                expandedBand == b ? "- ADV" : "+ ADV");
            advancedButtons[(size_t) b]->setColour(
                juce::TextButton::buttonColourId,
                uiColour(expandedBand == b ? juce::Colour(0xff3f3517)
                                            : juce::Colour(0xff17191d)));
            advancedButtons[(size_t) b]->setColour(
                juce::TextButton::textColourOffId,
                uiColour(expandedBand == b ? juce::Colour(0xffffdf62)
                                            : juce::Colour(0xffc0c5cb)));
        }
    }

    for (auto& k : knobs)
    {
        const bool isBandAdvanced = k.band >= 0 && k.slot >= 20;
        const bool isSharedAdvanced = k.band < 0 && k.slot >= 30 && k.slot < 40;
        const bool visible = (expandedBand >= 0)
            && ((isBandAdvanced && k.band == expandedBand)
                || isSharedAdvanced);

        if (isBandAdvanced || isSharedAdvanced)
        {
            k.slider->setVisible(visible);
            k.label->setVisible(visible);
        }
    }

    if (udmbcClipper)
        udmbcClipper->setVisible(expandedBand >= 0);
    if (closeAdvanced)
        closeAdvanced->setVisible(expandedBand >= 0);

    resized();
    repaint();
}

void VVChainAudioProcessorEditor::setAnalyzerEnabled(bool enabled)
{
    analyzerEnabled = enabled;
    audioProcessor.setAnalyzerEnabled(enabled && isShowing());

    if (settingsPanel)
        settingsPanel->setAnalyzerEnabled(enabled);

    if (!enabled)
    {
        analyzerInputCount = 0;
        analyzerPath.clear();
        analyzerDb.fill(-90.0f);
        std::array<float, 4096> discard {};
        while (audioProcessor.popAnalyzerSamples(
                   discard.data(), static_cast<int>(discard.size())) > 0)
        {
        }
    }

    repaint(eqGraphBounds().toNearestInt());
}

void VVChainAudioProcessorEditor::updateAnalyzer()
{
    if (!analyzerEnabled || !isShowing())
        return;

    std::array<float, 4096> incoming {};
    const int received = audioProcessor.popAnalyzerSamples(
        incoming.data(), static_cast<int>(incoming.size()));

    if (received <= 0)
        return;

    const double sr = audioProcessor.getSampleRate() > 1000.0
        ? audioProcessor.getSampleRate() : 48000.0;

    const float frameSeconds =
        static_cast<float>(analyzerHopSize / sr);
    const float attackCoeff =
        1.0f - std::exp(-frameSeconds / 0.035f);
    const float releaseCoeff =
        1.0f - std::exp(-frameSeconds / 0.180f);

    bool updated = false;
    int src = 0;

    while (src < received)
    {
        const int room = analyzerFftSize - analyzerInputCount;
        const int take = juce::jmin(room, received - src);

        std::copy_n(incoming.data() + src, take,
                    analyzerInput.data() + analyzerInputCount);
        analyzerInputCount += take;
        src += take;

        if (analyzerInputCount < analyzerFftSize)
            continue;

        std::fill(analyzerFftData.begin(), analyzerFftData.end(), 0.0f);
        std::copy(analyzerInput.begin(), analyzerInput.end(),
                  analyzerFftData.begin());

        analyzerWindow.multiplyWithWindowingTable(
            analyzerFftData.data(), analyzerFftSize);
        analyzerFft.performFrequencyOnlyForwardTransform(
            analyzerFftData.data());

        const int maxBin = analyzerFftSize / 2;
        const float norm = 4.0f / static_cast<float>(analyzerFftSize);

        analyzerPowerPrefix[0] = 0.0;
        for (int bin = 0; bin <= maxBin; ++bin)
        {
            const double magnitude =
                static_cast<double>(analyzerFftData[(size_t)bin] * norm);
            const double power = magnitude * magnitude;
            analyzerPower[(size_t)bin] = power;
            analyzerPowerPrefix[(size_t)bin + 1] =
                analyzerPowerPrefix[(size_t)bin] + power;
        }

        std::array<float, analyzerDisplayPoints> rawDb {};
        std::array<float, analyzerDisplayPoints> frequencySmoothed {};

        for (int p = 0; p < analyzerDisplayPoints; ++p)
        {
            const float t = static_cast<float>(p)
                / static_cast<float>(analyzerDisplayPoints - 1);
            const float centreHz = 20.0f * std::pow(1000.0f, t);

            // Narrow fractional-octave averaging retains useful resonances
            // while the following display-space kernel removes pixel-scale
            // FFT teeth. 4.5 dB/oct around 1 kHz matches the common natural
            // analyzer presentation used by mastering EQs.
            constexpr float smoothingOctaves = 1.0f / 24.0f;
            const float halfOctave = smoothingOctaves * 0.5f;
            const float f0 = centreHz / std::pow(2.0f, halfOctave);
            const float f1 = centreHz * std::pow(2.0f, halfOctave);

            const double binScale =
                static_cast<double>(analyzerFftSize) / sr;
            int b0 = juce::jlimit(
                1, maxBin,
                static_cast<int>(std::floor(f0 * binScale)));
            int b1 = juce::jlimit(
                b0, maxBin,
                static_cast<int>(std::ceil(f1 * binScale)));

            // At very low frequencies interpolate neighboring FFT powers
            // instead of forcing a wide low-frequency bucket.
            double meanPower = 0.0;
            if (b1 <= b0 + 1)
            {
                const double centreBin = juce::jlimit(
                    1.0, static_cast<double>(maxBin),
                    static_cast<double>(centreHz) * binScale);
                const int lo = juce::jlimit(
                    1, maxBin, static_cast<int>(std::floor(centreBin)));
                const int hi = juce::jmin(maxBin, lo + 1);
                const double frac = centreBin - static_cast<double>(lo);
                meanPower =
                    analyzerPower[(size_t)lo] * (1.0 - frac)
                    + analyzerPower[(size_t)hi] * frac;
            }
            else
            {
                const double sum =
                    analyzerPowerPrefix[(size_t)b1 + 1]
                    - analyzerPowerPrefix[(size_t)b0];
                meanPower =
                    sum / static_cast<double>(b1 - b0 + 1);
            }

            const float rms =
                static_cast<float>(std::sqrt(juce::jmax(0.0, meanPower)));
            float db = juce::Decibels::gainToDecibels(rms, -90.0f);
            db += 4.5f
                * std::log2(juce::jmax(20.0f, centreHz) / 1000.0f);
            rawDb[(size_t)p] = juce::jlimit(-90.0f, 0.0f, db);
        }

        constexpr std::array<float, 7> kernel
        {
            1.0f / 64.0f, 6.0f / 64.0f, 15.0f / 64.0f,
            20.0f / 64.0f,
            15.0f / 64.0f, 6.0f / 64.0f, 1.0f / 64.0f
        };

        for (int p = 0; p < analyzerDisplayPoints; ++p)
        {
            float value = 0.0f;
            for (int k = -3; k <= 3; ++k)
            {
                const int index =
                    juce::jlimit(0, analyzerDisplayPoints - 1, p + k);
                value += rawDb[(size_t)index]
                    * kernel[(size_t)(k + 3)];
            }
            frequencySmoothed[(size_t)p] = value;
        }

        for (int p = 0; p < analyzerDisplayPoints; ++p)
        {
            const float target = frequencySmoothed[(size_t)p];
            const float old = analyzerDb[(size_t)p];
            const float coeff =
                target > old ? attackCoeff : releaseCoeff;
            analyzerDb[(size_t)p] =
                old + coeff * (target - old);
        }

        // 50% overlap: High/4096 low-frequency resolution at roughly half
        // the FFT work of the previous 75% overlap implementation.
        std::move(analyzerInput.begin() + analyzerHopSize,
                  analyzerInput.end(),
                  analyzerInput.begin());
        analyzerInputCount =
            analyzerFftSize - analyzerHopSize;
        updated = true;
    }

    if (!updated)
        return;

    const auto graph = eqGraphBounds();
    std::array<juce::Point<float>, analyzerDisplayPoints> points {};

    for (int p = 0; p < analyzerDisplayPoints; ++p)
    {
        const float x = graph.getX()
            + graph.getWidth()
                * static_cast<float>(p)
                / static_cast<float>(analyzerDisplayPoints - 1);
        const float yNorm =
            juce::jlimit(0.0f, 1.0f,
                (analyzerDb[(size_t)p] + 90.0f) / 90.0f);
        points[(size_t)p] = {
            x,
            graph.getBottom() - yNorm * graph.getHeight()
        };
    }

    std::array<float, analyzerDisplayPoints - 1> slopes {};
    std::array<float, analyzerDisplayPoints> tangents {};

    for (int i = 0; i < analyzerDisplayPoints - 1; ++i)
    {
        const float dx = juce::jmax(
            1.0e-6f,
            points[(size_t)i + 1].x - points[(size_t)i].x);
        slopes[(size_t)i] =
            (points[(size_t)i + 1].y - points[(size_t)i].y) / dx;
    }

    tangents[0] = slopes[0];
    tangents[(size_t)analyzerDisplayPoints - 1] =
        slopes[(size_t)analyzerDisplayPoints - 2];

    for (int i = 1; i < analyzerDisplayPoints - 1; ++i)
    {
        const float a = slopes[(size_t)i - 1];
        const float b = slopes[(size_t)i];

        tangents[(size_t)i] =
            a * b <= 0.0f ? 0.0f : (2.0f * a * b / (a + b));
    }

    analyzerPath.clear();
    analyzerPath.startNewSubPath(points[0]);

    for (int i = 0; i < analyzerDisplayPoints - 1; ++i)
    {
        const auto p1 = points[(size_t)i];
        const auto p2 = points[(size_t)i + 1];
        const float dx = p2.x - p1.x;

        juce::Point<float> c1 {
            p1.x + dx / 3.0f,
            p1.y + tangents[(size_t)i] * dx / 3.0f
        };
        juce::Point<float> c2 {
            p2.x - dx / 3.0f,
            p2.y - tangents[(size_t)i + 1] * dx / 3.0f
        };

        c1.y = juce::jlimit(graph.getY(), graph.getBottom(), c1.y);
        c2.y = juce::jlimit(graph.getY(), graph.getBottom(), c2.y);
        analyzerPath.cubicTo(c1, c2, p2);
    }

    repaint(graph.toNearestInt());
}


void VVChainAudioProcessorEditor::setIvoryTheme(bool ivory)
{
    ivoryTheme = ivory;
    metalLook.ivoryTheme = ivory;

    if (settingsButton)
        settingsButton->setIvoryTheme(ivory);
    if (settingsPanel)
        settingsPanel->setIvoryTheme(ivory);

    if (uiThemeButton)
    {
        uiThemeButton->setButtonText(
            ivory ? "UI IVORY" : "UI BLACK");
        uiThemeButton->setColour(
            juce::TextButton::buttonColourId,
            ivory ? juce::Colour(0xff211b14)
                  : juce::Colour(0xff10181b));
        uiThemeButton->setColour(
            juce::TextButton::buttonOnColourId,
            ivory ? juce::Colour(0xff352619)
                  : juce::Colour(0xff153033));
        uiThemeButton->setColour(
            juce::TextButton::textColourOffId,
            ivory ? juce::Colour(0xffffe1a1)
                  : juce::Colour(0xff76f0ed));
        uiThemeButton->setColour(
            juce::TextButton::textColourOnId,
            juce::Colours::white);
        uiThemeButton->repaint();
    }

    setExpandedBand(expandedBand);
    repaint();

    for (auto& k : knobs)
    {
        if (k.slider)
            k.slider->repaint();
        if (k.label)
            k.label->setColour(
                juce::Label::textColourId,
                ivory ? juce::Colour(0xff3b3026)
                      : juce::Colour(0xffdce7e7));
    }

    for (auto& b : bypassButtons) if (b) b->repaint();
    for (auto& b : soloButtons) if (b) b->repaint();
    for (auto& b : dynTriggerButtons) if (b) b->repaint();
    for (auto& b : analogModeButtons) if (b) b->repaint();
    for (auto& b : analogX2Buttons) if (b) b->repaint();
    if (masterBypassButton) masterBypassButton->repaint();
    if (soloModeButton) soloModeButton->repaint();

    // Force all active/bypass colours through the newly selected skin.
    lastMasterBypassUi = !isMasterBypassed();
    updateBypassVisuals();
}

void VVChainAudioProcessorEditor::setSettingsPanelVisible(bool visible)
{
    settingsPanelVisible = visible;

    if (settingsDismissOverlay)
        settingsDismissOverlay->setVisible(visible);
    if (settingsPanel)
        settingsPanel->setVisible(visible);
    if (settingsButton)
        settingsButton->setPanelOpen(visible);

    if (visible)
    {
        if (settingsDismissOverlay)
            settingsDismissOverlay->toFront(false);
        if (settingsPanel)
            settingsPanel->toFront(false);
        if (settingsButton)
            settingsButton->toFront(false);
        grabKeyboardFocus();
    }

    repaint();
}

bool VVChainAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (settingsPanelVisible
        && key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        setSettingsPanelVisible(false);
        return true;
    }

    return false;
}

void VVChainAudioProcessorEditor::resized()
{
    const int w = getWidth();

    const bool masterRuntime = metalLook.isMasterRuntimeActive();
    const int cardY = masterRuntime ? 392 : 404;
    const int gap = masterRuntime ? 10 : 8;
    const int left = masterRuntime ? 17 : 18;
    const int unitW = masterRuntime
        ? (w - left * 2 - gap * 4) / 5
        : (w - left * 2 - gap * 4) / 5;
    const int cardW = unitW;

    const std::array<int, 4> moduleWidths { 50, 52, 64, 66 };
    constexpr int topGap = 5;
    constexpr int masterW = 78;
    int total = masterW;
    for (auto mw : moduleWidths)
        total += topGap + mw;
    constexpr int soloModeW = 68;
    total += topGap + soloModeW;

    constexpr int settingsW = 28;
    constexpr int themeW = 76;
    constexpr int settingsRight = 10;
    constexpr int settingsGap = 5;
    constexpr int topY = 17;
    const int settingsX = w - settingsRight - settingsW;
    const int themeX = settingsX - settingsGap - themeW;
    const int topX = themeX - settingsGap - total;

    if (settingsDismissOverlay)
        settingsDismissOverlay->setBounds(getLocalBounds());
    if (settingsButton)
        settingsButton->setBounds(settingsX, topY - 1, settingsW, settingsW);
    if (uiThemeButton)
        uiThemeButton->setBounds(themeX, topY - 1, themeW, settingsW);
    if (settingsPanel)
    {
        constexpr int panelW = 330;
        const int panelY = topY + settingsW + 7;
        const int panelH = juce::jmin(480, getHeight() - panelY - 10);
        settingsPanel->setBounds(settingsX + settingsW - panelW,
                                 panelY, panelW, panelH);
    }
    if (masterBypassButton)
        masterBypassButton->setBounds(topX, topY, masterW, 25);
    if (soloModeButton)
        soloModeButton->setBounds(topX + masterW + topGap, topY, soloModeW, 25);

    int xTop = topX + masterW + topGap + soloModeW + topGap;
    for (int i = 0; i < 4; ++i)
    {
        if (bypassButtons[(size_t) i])
            bypassButtons[(size_t) i]->setBounds(
                xTop, topY, moduleWidths[(size_t) i], 25);
        xTop += moduleWidths[(size_t) i] + topGap;
    }

    for (int b = 0; b < 4; ++b)
    {
        const int x = left + b * (cardW + gap);

        if (advancedButtons[(size_t) b])
            advancedButtons[(size_t) b]->setBounds(
                x + cardW - 108, cardY + 8, 50, 20);
        if (soloButtons[(size_t) b])
            soloButtons[(size_t) b]->setBounds(
                x + cardW - 58, cardY + 8, 46, 20);

        const int innerX = x + 8;
        const int innerTop = cardY + (masterRuntime ? 56 : 56);
        const int innerW = cardW - 16;
        const int cellGap = 6;
        const int cellW = (innerW - cellGap * 2) / 3;
        const int rowH = masterRuntime ? 91 : 70;
        const int knobH = masterRuntime ? 80 : 62;

        constexpr int dynamicSectionShiftY = 14;
        constexpr int lowerSectionShiftY = 8;
        const auto cell = [&](int row, int col)
        {
            // The taller PEAK/RMS + ABOVE row needs real vertical space.
            // Everything below the static EQ row moves down together.
            const int dynamicShift =
                row >= 1 ? dynamicSectionShiftY : 0;
            const int lowerShift =
                row >= 3 ? lowerSectionShiftY : 0;
            return juce::Rectangle<int>(
                innerX + col * (cellW + cellGap),
                innerTop + row * rowH
                    + dynamicShift + lowerShift,
                cellW, knobH);
        };

        const auto n = juce::String(b + 1);

        // ROW 1 — static EQ: GAIN / FREQ / Q
        placeKnob("EQ" + n + "_GAIN", cell(0, 0));
        placeKnob("EQ" + n + "_FREQ", cell(0, 1));
        placeKnob("EQ" + n + "_Q",    cell(0, 2));

        // ROW 2 — Dynamic EQ
        placeKnob("DYN_DYNAMICS" + n, cell(1, 0));
        placeKnob("DYN_ATTACK" + n,   cell(1, 1));
        placeKnob("DYN_RELEASE" + n,  cell(1, 2));

        // Dynamic detector mode row follows the supplied reference:
        // PEAK/RMS ~= 31% left, ABOVE/BELOW ~= 31% right,
        // with the large centre gap preserved. Height matches + ADV.
        const int modeW =
            juce::jmax(44, juce::roundToInt(innerW * 0.31f));
        const int modeY = cell(1, 0).getY() - 21;

        if (dynDetectSliders[(size_t) b])
        {
            dynDetectSliders[(size_t) b]->setBounds(
                innerX, modeY, modeW, 21);
        }

        if (dynTriggerButtons[(size_t) b])
        {
            dynTriggerButtons[(size_t) b]->setBounds(
                innerX + innerW - modeW, modeY, modeW, 21);
            dynTriggerButtons[(size_t) b]->setButtonText(
                parameterValue("DYN_TRIGGER_BELOW" + n) > 0.5f
                    ? "BELOW" : "ABOVE");
        }

        // ROW 4 — UDMBC
        placeKnob("UDMBC_DEGREE" + n, cell(3, 0));
        placeKnob("UDMBC_COMP_A" + n, cell(3, 1));
        placeKnob("UDMBC_COMP_R" + n, cell(3, 2));

        // ROW 5 — ANALOG / TYPE-A / TRANSIENT
        placeKnob("EQ_COLOR_B" + n, cell(4, 0));
        placeKnob("TAPE_DEGREE" + n, cell(4, 1));
        placeKnob("TRANSIENT" + n, cell(4, 2));

        if (analogModeButtons[(size_t) b])
            if (auto* knob = findKnob("EQ_COLOR_B" + n))
            {
                const auto r = knob->slider->getBounds();
                analogModeButtons[(size_t) b]->setBounds(
                    r.getCentreX() - 18, r.getY() - 25, 36, 12);
            }

        if (udmbcBandBypassButtons[(size_t) b])
            if (auto* knob = findKnob("UDMBC_DEGREE" + n))
            {
                const auto r = knob->slider->getBounds();
                udmbcBandBypassButtons[(size_t) b]->setBounds(
                    r.getCentreX() - 7, r.getY() - 10, 14, 14);
            }

        if (analogX2Buttons[(size_t) b])
            if (auto* knob = findKnob("EQ_COLOR_B" + n))
            {
                const auto r = knob->slider->getBounds();
                analogX2Buttons[(size_t) b]->setBounds(
                    r.getX() + 1, r.getY() - 13, 24, 14);
            }

        if (analogBypassButtons[(size_t) b])
            if (auto* knob = findKnob("EQ_COLOR_B" + n))
            {
                const auto r = knob->slider->getBounds();
                // Same LED geometry/logic as TYPE-A: upper-right,
                // lit = active, dim = bypass.
                analogBypassButtons[(size_t) b]->setBounds(
                    r.getRight() - 14, r.getY() - 10, 14, 14);
            }

        if (tapeBandBypassButtons[(size_t) b])
            if (auto* knob = findKnob("TAPE_DEGREE" + n))
            {
                const auto r = knob->slider->getBounds();
                tapeBandBypassButtons[(size_t) b]->setBounds(
                    r.getRight() - 14, r.getY() - 10, 14, 14);
            }
    }

    // Fifth unit: global MASTER controls only.
    {
        const int x = left + 4 * (cardW + gap);
        const int innerX = x + 14;
        const int innerW = cardW - 28;

        if (masterRuntime)
        {
            const int halfGap = 8;
            const int halfW = (innerW - halfGap) / 2;

            placeKnob("DRY_WET",
                      { innerX, cardY + 70,
                        halfW, 142 });
            placeKnob("OUTPUT_LEVEL",
                      { innerX + halfW + halfGap, cardY + 70,
                        halfW, 142 });

            if (deltaMonitorButton)
                deltaMonitorButton->setBounds(
                    innerX, cardY + 228, halfW, 34);
            if (masterBypassButton)
                masterBypassButton->setBounds(
                    innerX + halfW + halfGap, cardY + 228, halfW, 34);
        }
        else
        {
            if (masterBypassButton)
                masterBypassButton->setBounds(
                    innerX, cardY + 64, innerW, 32);

            if (deltaMonitorButton)
                deltaMonitorButton->setBounds(
                    innerX, cardY + 118, innerW, 30);

            placeKnob("DRY_WET",
                      { innerX, cardY + 190,
                        innerW, 108 });
            placeKnob("OUTPUT_LEVEL",
                      { innerX, cardY + 330,
                        innerW, 108 });
        }
    }

    if (expandedBand >= 0)
    {
        const int popupW = juce::jmin(900, w - 40);
        const int popupH = 330;
        const int popupX = (w - popupW) / 2;
        const int popupY = (getHeight() - popupH) / 2;

        if (closeAdvanced)
            closeAdvanced->setBounds(popupX + popupW - 76, popupY + 8, 62, 21);

        const int innerX = popupX + 12;
        const int gridTop = popupY + 46;
        const int gapX = 6;
        const int cols = 8;
        const int cellW = (popupW - 24 - gapX * 7) / cols;
        const int rowH = 112;

        const auto p = [&](int slot)
        {
            const int row = slot / cols;
            const int col = slot % cols;
            return juce::Rectangle<int>(
                innerX + col * (cellW + gapX),
                gridTop + row * rowH,
                cellW, 100);
        };

        const auto n = juce::String(expandedBand + 1);
        const std::array<juce::String, 7> bandAdv
        {{
            "UDMBC_LIFT_T", "UDMBC_LIFT_A", "UDMBC_LIFT_R", "UDMBC_LIFT_M",
            "UDMBC_COMP_T", "UDMBC_COMP_M", "UDMBC_LEVEL"
        }};
        const std::array<juce::String, 8> sharedAdv
        {{
            "UDMBC_X1", "UDMBC_X2", "UDMBC_X3", "XOVER_OVERLAP",
            "UDMBC_INPUT", "UDMBC_GATE", "UDMBC_MIX", "UDMBC_OUTPUT"
        }};

        for (int i = 0; i < 7; ++i)
            placeKnob(bandAdv[(size_t)i] + n, p(i));
        for (int i = 0; i < 8; ++i)
            placeKnob(sharedAdv[(size_t)i], p(i + 8));

        if (udmbcClipper)
            udmbcClipper->setBounds(innerX, popupY + popupH - 34, 82, 24);
    }

    repaint();
}

void VVChainAudioProcessorEditor::showFloatingValueBoxForBand(
    int band, bool dynamicReadout, float displayedGain,
    juce::Point<float> position)
{
    if (band < 0 || band >= 4)
    {
        floatingValueBox.hideInstantly();
        return;
    }

    floatingValueBand = band;
    floatingValueDynamic = dynamicReadout;

    const auto n = juce::String(band + 1);
    const float frequency = parameterValue("EQ" + n + "_FREQ");
    const float q = juce::jmax(0.1f, parameterValue("EQ" + n + "_Q"));

    const juce::String signedDb =
        juce::String(displayedGain >= 0.0f ? "+" : "")
        + juce::String(displayedGain, 2) + " dB";

    // Compact graph readout: values only.
    // EQ and Dynamic EQ share the same presentation; the supplied gain value
    // still comes from the correct static or dynamic target.
    juce::ignoreUnused(dynamicReadout);
    const juce::String shortFrequency = frequency >= 1000.f
        ? juce::String(frequency / 1000.f, 2) + " kHz"
        : juce::String(frequency, 2) + " Hz";
    const juce::String line1 = signedDb;
    const juce::String line2 = shortFrequency;
    const int filterType = juce::jlimit(
        0, 13,
        juce::roundToInt(parameterValue("EQ" + n + "_TYPE")));
    const juce::String line3 =
        filterType >= 12
            ? juce::String(
                  ([](int index)
                   {
                       index = juce::jlimit(0, 6, index);
                       return index == 0 ? 6 : index * 12;
                   })(juce::roundToInt(
                       parameterValue("EQ" + n + "_SLOPE"))))
                  + " dB/oct"
            : "Q " + juce::String(q, 3);

    const auto graph = eqGraphBounds();
    const float nodeX =
        graphFrequencyToX(graph, frequency);

    float nodeY = eqDbToY(
        graph,
        dynamicReadout
            ? dynamicEffectiveTargetGain(band)
            : parameterValue("EQ" + n + "_GAIN"));

    float anchorX = nodeX;

    // When DYNAMICS is still exactly at the static point, the only distinct
    // Dynamic control is the arrow handle to the right. Anchor the box there
    // so the mouse tunnel starts where the user actually hovered.
    if (dynamicReadout
        && std::abs(parameterValue("DYN_DYNAMICS" + n)) <= 0.05f)
    {
        anchorX = juce::jlimit(
            graph.getX() + 18.0f,
            graph.getRight() - 12.0f,
            nodeX + 31.0f);
    }

    juce::ignoreUnused(position);
    floatingValueBox.updateInfo(
        line1, line2, line3,
        juce::Point<float>(anchorX, nodeY).toInt(),
        getLocalBounds());
}

void VVChainAudioProcessorEditor::commitFloatingValueEdit(
    int line, const juce::String& rawText)
{
    if (floatingValueBand < 0 || floatingValueBand >= 4)
        return;

    const int band = floatingValueBand;
    const auto n = juce::String(band + 1);
    const auto text = rawText.trim().toLowerCase();

    auto numericValue = [](const juce::String& valueText)
    {
        return valueText
            .retainCharacters("0123456789.-")
            .getDoubleValue();
    };

    if (line == 0)
    {
        const float gain =
            juce::jlimit(
                -18.0f, 18.0f,
                static_cast<float>(numericValue(text)));

        if (floatingValueDynamic)
        {
            const float offset =
                parameterValue("EQ" + n + "_GAIN");
            const float dynamics =
                juce::jlimit(
                    -100.0f, 100.0f,
                    (gain - offset) / 18.0f * 100.0f);
            setParameter("DYN_DYNAMICS" + n, dynamics);
        }
        else
        {
            setParameter("EQ" + n + "_GAIN", gain);
        }
    }
    else if (line == 1)
    {
        double frequency = numericValue(text);
        if (text.containsChar('k'))
            frequency *= 1000.0;

        setParameter(
            "EQ" + n + "_FREQ",
            static_cast<float>(
                juce::jlimit(20.0, 20000.0, frequency)));
    }
    else if (line == 2)
    {
        const int filterType = juce::jlimit(
            0, 13,
            juce::roundToInt(
                parameterValue("EQ" + n + "_TYPE")));

        if (filterType >= 12)
        {
            const double requested = numericValue(text);
            constexpr std::array<double, 7> slopes
            {{ 6.0, 12.0, 24.0, 36.0, 48.0, 60.0, 72.0 }};

            int best = 0;
            double bestDistance =
                std::abs(requested - slopes[0]);

            for (int i = 1; i < (int) slopes.size(); ++i)
            {
                const double distance =
                    std::abs(requested - slopes[(size_t) i]);
                if (distance < bestDistance)
                {
                    best = i;
                    bestDistance = distance;
                }
            }

            setParameter(
                "EQ" + n + "_SLOPE",
                static_cast<float>(best));
        }
        else
        {
            setParameter(
                "EQ" + n + "_Q",
                static_cast<float>(
                    juce::jlimit(
                        0.10, 18.0,
                        numericValue(text))));
        }
    }

    repaint();
}

void VVChainAudioProcessorEditor::updateFloatingValueBoxAt(
    juce::Point<float> position)
{
    const auto graph = eqGraphBounds();

    const bool graphGestureActive =
        rightSoloBand >= 0
        || dragOffsetBand >= 0
        || dragBand >= 0
        || dragDynamicHandleBand >= 0;

    if (!graphGestureActive
        && (floatingValueBox.isEditing()
            || floatingValueBox.isWithinInteractionZone(
                position.toInt())))
        return;

    if (!graph.contains(position))
    {
        floatingValueBox.hideInstantly();
        floatingValueBand = -1;
        return;
    }

    // A mouse drag keeps ownership of the node that received mouseDown.
    // Geometric hover can cross the other node while the target is moving.
    if (rightSoloBand >= 0)
    {
        const int band = rightSoloBand;
        const auto n = juce::String(band + 1);
        showFloatingValueBoxForBand(
            band,
            rightSoloDynamic,
            rightSoloDynamic
                ? dynamicEffectiveTargetGain(band)
                : parameterValue("EQ" + n + "_GAIN"),
            position);
        return;
    }

    if (dragOffsetBand >= 0)
    {
        const int band = dragOffsetBand;
        showFloatingValueBoxForBand(
            band, false,
            parameterValue(
                "EQ" + juce::String(band + 1) + "_GAIN"),
            position);
        return;
    }
    if (dragBand >= 0 || dragDynamicHandleBand >= 0)
    {
        const int band = dragBand >= 0 ? dragBand : dragDynamicHandleBand;
        showFloatingValueBoxForBand(band, true,
            dynamicEffectiveTargetGain(band), position);
        return;
    }

    // v1.0.37: explicit hit priority.
    // 1) Static EQ point always wins when the pointer is actually on it.
    // 2) Only the Dynamic target or its dedicated arrow can produce DYN EQ.
    // The live gain marker is visual only and never steals the value box.
    constexpr float staticHitRadius = 7.0f;
    constexpr float dynamicHitRadius = 12.0f;
    constexpr float handleHitX = 6.0f;
    constexpr float handleHitY = 10.0f;

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float frequency = parameterValue("EQ" + n + "_FREQ");
        const float offsetGain = parameterValue("EQ" + n + "_GAIN");
        const float x = graphFrequencyToX(graph, frequency);
        const float y = eqDbToY(graph, offsetGain);

        if (position.getDistanceFrom({ x, y }) <= staticHitRadius)
        {
            showFloatingValueBoxForBand(
                b, false, offsetGain, position);
            return;
        }
    }

    int bestBand = -1;
    float bestDistance = dynamicHitRadius;

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float dynamics = parameterValue("DYN_DYNAMICS" + n);
        const float frequency = parameterValue("EQ" + n + "_FREQ");
        const float x = graphFrequencyToX(graph, frequency);
        const float targetGain = dynamicEffectiveTargetGain(b);
        const float targetY = eqDbToY(graph, targetGain);

        // At 0% the Dynamic target sits on the Static EQ point, so the
        // target itself is intentionally disabled; the arrow remains available.
        if (std::abs(dynamics) > 0.05f)
        {
            const float d = position.getDistanceFrom({ x, targetY });
            const float staticY = eqDbToY(graph, parameterValue("EQ" + n + "_GAIN"));
            if (d < bestDistance && position.getDistanceFrom({ x, staticY }) >= staticHitRadius)
            {
                bestDistance = d;
                bestBand = b;
            }
        }

        const float handleX = juce::jlimit(
            graph.getX() + 18.f, graph.getRight() - 12.f, x + 31.f);
        if (std::abs(dynamics) <= 0.05f
            && std::abs(position.x - handleX) <= handleHitX
            && std::abs(position.y - targetY) <= handleHitY)
        {
            showFloatingValueBoxForBand(
                b, true, targetGain, position);
            return;
        }
    }

    if (bestBand >= 0)
    {
        showFloatingValueBoxForBand(
            bestBand, true, dynamicEffectiveTargetGain(bestBand), position);
        return;
    }

    floatingValueBox.hideInstantly();
}

void VVChainAudioProcessorEditor::mouseDoubleClick(
    const juce::MouseEvent& event)
{
    if (!event.mods.isLeftButtonDown())
        return;

    const auto graph = eqGraphBounds();
    if (!graph.contains(event.position))
        return;

    auto resetParameter = [this](const juce::String& id, float value)
    {
        if (auto* parameter = audioProcessor.apvts.getParameter(id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }

        // Keep the lower linked control visually in lockstep immediately.
        if (auto* knob = findKnob(id))
            knob->slider->setValue(value, juce::dontSendNotification);
    };

    constexpr float staticHitRadius = 8.0f;
    constexpr float dynamicHitRadius = 13.0f;

    int dynamicBand = -1;
    float dynamicDistance = dynamicHitRadius;

    // An already-pulled Dynamic target owns its own double-click, even when it
    // is still visually close to the EQ node. DYNAMICS=0 has no separate
    // target point, so the static EQ point naturally wins in that state.
    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float dynamics =
            juce::jlimit(-100.0f, 100.0f,
                         parameterValue("DYN_DYNAMICS" + n));
        if (std::abs(dynamics) <= 0.05f)
            continue;

        const auto target = dynamicTargetPoint(b);
        const float d = event.position.getDistanceFrom(target);
        if (d <= dynamicDistance)
        {
            dynamicDistance = d;
            dynamicBand = b;
        }
    }

    int staticBand = -1;
    float staticDistance = staticHitRadius;
    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float x = graphFrequencyToX(
            graph, parameterValue("EQ" + n + "_FREQ"));
        const float y = eqDbToY(
            graph, parameterValue("EQ" + n + "_GAIN"));
        const float distance = event.position.getDistanceFrom({ x, y });

        if (distance <= staticDistance)
        {
            staticDistance = distance;
            staticBand = b;
        }
    }

    if (dynamicBand >= 0
        && (staticBand < 0 || dynamicDistance <= staticDistance))
    {
        const auto n = juce::String(dynamicBand + 1);
        resetParameter("DYN_DYNAMICS" + n, 0.0f);
        clearGraphControlState();
        updateFloatingValueBoxAt(event.position);
        repaint();
        return;
    }

    if (staticBand >= 0)
    {
        const auto n = juce::String(staticBand + 1);
        resetParameter("EQ" + n + "_GAIN", 0.0f);
        clearGraphControlState();
        updateFloatingValueBoxAt(event.position);
        repaint();
        return;
    }
}

void VVChainAudioProcessorEditor::mouseMove(
    const juce::MouseEvent& event)
{
    const auto graph = eqGraphBounds();

    if (!graph.contains(event.position))
    {
        // The editable value box is a child of the editor and can sit on top
        // of the graph. Always let the directional tunnel decide first.
        updateFloatingValueBoxAt(event.position);

        if (hoverDynamicBand != -1 || hoverXover != -1)
        {
            hoverDynamicBand = -1;
            hoverXover = -1;
            repaint();
        }
        return;
    }

    hoverXover = -1;
    const float markerY = graph.getBottom() - 18.f;
    const float xovers[3]
    {
        graphFrequencyToX(graph, parameterValue("UDMBC_X1")),
        graphFrequencyToX(graph, parameterValue("UDMBC_X2")),
        graphFrequencyToX(graph, parameterValue("UDMBC_X3"))
    };
    for (int i = 0; i < 3; ++i)
    {
        if (event.position.getDistanceFrom({ xovers[i], markerY }) <= 10.f)
        {
            hoverXover = i;
            break;
        }
    }

    int band = -1;
    if (pointNearDynamicNode(event.position, band))
    {
        hoverDynamicBand = band;
    }
    else
    {
        hoverDynamicBand = -1;
    }

    updateFloatingValueBoxAt(event.position);
    repaint();
}

void VVChainAudioProcessorEditor::mouseExit(
    const juce::MouseEvent& event)
{
    hoverDynamicBand = -1;
    hoverXover = -1;

    const auto localPosition =
        getLocalPoint(
            nullptr,
            event.getScreenPosition().toFloat());

    if (!floatingValueBox.isEditing()
        && !floatingValueBox.isWithinInteractionZone(
            localPosition.toInt()))
    {
        floatingValueBox.hideInstantly();
        floatingValueBand = -1;
    }

    repaint();
}


void VVChainAudioProcessorEditor::showEqTypeMenu(
    int band, juce::Point<float> position)
{
    if (band < 0 || band >= 4)
        return;

    static const std::array<juce::String, 14> names
    {{
        "Parametric Bell",
        "Matched Bell",
        "Wide Plateau",
        "Steep Plateau 72",
        "Low Shelf",
        "High Shelf",
        "Low Shelf + Res",
        "High Shelf + Res",
        "Low Contour",
        "High Contour",
        "Focus Pass",
        "Deep Reject",
        "HF Roll-Off",
        "LF Roll-Off"
    }};

    // Original VVChain presentation order.  Parameter IDs/types stay unchanged
    // so old presets and host automation retain the same DSP meaning.
    static constexpr std::array<int, 10> displayOrder
    {{
        0, 2, 4, 5, 6, 7, 8, 9, 13, 12
    }};

    const auto n = juce::String(band + 1);
    int current = juce::jlimit(
        0, 13,
        juce::roundToInt(parameterValue("EQ" + n + "_TYPE")));
    if (current == 1 || current == 10 || current == 11)
        current = 0;
    else if (current == 3)
        current = 2;

    juce::PopupMenu menu;
    for (const int type : displayOrder)
    {
        // Middle bands deliberately omit the two roll-off filters.
        if ((band == 1 || band == 2) && type >= 12)
            continue;

        const bool selected = current == type;
        menu.addItem(
            type + 1,
            (selected ? juce::String("● ") : juce::String("   "))
                + names[(size_t) type],
            true,
            selected);
    }

    const auto screen = localPointToGlobal(position.toInt());
    auto options = juce::PopupMenu::Options()
        .withTargetScreenArea({ screen.x, screen.y, 2, 2 })
        .withMaximumNumColumns(2)
        .withStandardItemHeight(28);

    auto safeThis =
        juce::Component::SafePointer<VVChainAudioProcessorEditor>(this);

    menu.showMenuAsync(
        options,
        [safeThis, band](int result)
        {
            if (safeThis == nullptr || result <= 0)
                return;

            const int type = result - 1;
            if ((band == 1 || band == 2) && type >= 12)
                return;

            const auto n = juce::String(band + 1);
            safeThis->setParameter(
                "EQ" + n + "_TYPE",
                static_cast<float>(type));
            safeThis->repaint();
        });
}

void VVChainAudioProcessorEditor::beginRightSolo(
    int band, juce::Point<float> position, bool dynamicTarget)
{
    if (band < 0 || band >= 4 || rightSoloBand >= 0)
        return;

    const auto n = juce::String(band + 1);
    rightSoloBand = band;
    rightSoloDynamic = dynamicTarget;
    rightSoloPosition = position;
    expandedDynamicBand = -1;
    dragDynamicMsBand = -1;
    dragBand = -1;
    dragOffsetBand = -1;

    setParameter(
        "GRAPH_SOLO_FREQ",
        parameterValue("EQ" + n + "_FREQ"));
    const int filterType = juce::jlimit(
        0, 13,
        juce::roundToInt(
            parameterValue("EQ" + n + "_TYPE")));
    setParameter(
        "GRAPH_SOLO_Q",
        filterType >= 12
            ? 0.70710678f
            : parameterValue("EQ" + n + "_Q"));
    setParameter("GRAPH_SOLO_ACTIVE", 1.f);

    if (auto* pFreq =
            audioProcessor.apvts.getParameter("EQ" + n + "_FREQ"))
        pFreq->beginChangeGesture();

    if (dynamicTarget)
    {
        if (auto* pDynamics =
                audioProcessor.apvts.getParameter("DYN_DYNAMICS" + n))
            pDynamics->beginChangeGesture();
    }
    else
    {
        if (auto* pGain =
                audioProcessor.apvts.getParameter("EQ" + n + "_GAIN"))
            pGain->beginChangeGesture();
    }

    graphLastDragPosition = position;
    clearGraphControlState();
    showGraphDragHint = false;
    graphDragHint.clear();
}

void VVChainAudioProcessorEditor::mouseDown(
    const juce::MouseEvent& event)
{
    const auto pos = event.position;
    const auto graph = eqGraphBounds();

    // Any click outside the open M/S popup closes it immediately.
    // This check is intentionally before the graph bounds check so a click
    // anywhere else in the editor also closes the popup.
    if (expandedDynamicBand >= 0
        && !event.mods.isRightButtonDown()
        && !dynamicMsPopupBounds(expandedDynamicBand).contains(pos))
    {
        expandedDynamicBand = -1;
        dragDynamicMsBand = -1;
        dragBand = -1;
        dragXover = -1;
        showGraphDragHint = false;
        repaint();
        return;
    }

    if (!graph.contains(pos))
    {
        floatingValueBox.hideInstantly();
        return;
    }

    updateFloatingValueBoxAt(pos);

    int band = -1;

    // Right-click remembers exactly which node was hit:
    // Static EQ and Dynamic Target must never share the same drag path.
    if (event.mods.isRightButtonDown())
    {
        float bestDistance = 24.0f;
        bool bestIsDynamic = false;
        band = -1;

        for (int b = 0; b < 4; ++b)
        {
            const auto n = juce::String(b + 1);
            const auto staticPoint = juce::Point<float>(
                graphFrequencyToX(
                    graph, parameterValue("EQ" + n + "_FREQ")),
                eqDbToY(
                    graph, parameterValue("EQ" + n + "_GAIN")));
            const float dStatic =
                pos.getDistanceFrom(staticPoint);

            const bool dynamicIsSeparate =
                std::abs(parameterValue("DYN_DYNAMICS" + n)) > 0.05f;
            const float dDynamic =
                dynamicIsSeparate
                    ? pos.getDistanceFrom(dynamicTargetPoint(b))
                    : 1.0e9f;

            // Static wins its own centre/overlap. Otherwise the nearer
            // Dynamic target keeps Dynamic identity for the entire gesture.
            if (dStatic < bestDistance)
            {
                bestDistance = dStatic;
                band = b;
                bestIsDynamic = false;
            }

            if (dDynamic < bestDistance && dDynamic < dStatic)
            {
                bestDistance = dDynamic;
                band = b;
                bestIsDynamic = true;
            }
        }

        if (band >= 0)
        {
            pendingRightClickBand = band;
            pendingRightClickDynamic = bestIsDynamic;
            pendingRightClickPosition = pos;
            pendingRightClickDragged = false;
            floatingValueBox.hideInstantly();
            repaint();
            return;
        }
    }

    if (expandedDynamicBand >= 0)
    {
        const auto popup =
            dynamicMsPopupBounds(expandedDynamicBand);

        if (!popup.contains(pos))
        {
            expandedDynamicBand = -1;
            repaint();
            return;
        }

        const auto bar =
            popup.reduced(12.f)
                .withY(popup.getY() + 52.f)
                .withHeight(11.f);

        if (event.mods.isLeftButtonDown()
            && bar.expanded(0.f, 12.f).contains(pos))
        {
            const float midPct = juce::jlimit(
                0.f, 100.f,
                (pos.x - bar.getX())
                    / juce::jmax(1.f, bar.getWidth())
                    * 100.f);

            setParameter(
                "DYN_MS" + juce::String(expandedDynamicBand + 1),
                midPct);

            dragDynamicMsBand = expandedDynamicBand;
            if (auto* parameter = audioProcessor.apvts.getParameter(
                    "DYN_MS" + juce::String(expandedDynamicBand + 1)))
                parameter->beginChangeGesture();
            repaint();
        }

        return;
    }

    // Dedicated DYNAMICS arrow handle. Priority is above the EQ XY node.
    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float x = graphFrequencyToX(
            graph, parameterValue("EQ" + n + "_FREQ"));
        const float dynamicsValue =
            juce::jlimit(-100.f, 100.f,
                         parameterValue("DYN_DYNAMICS" + n));
        const float handleX =
            juce::jlimit(graph.getX() + 18.f,
                         graph.getRight() - 12.f,
                         x + 31.f);
        const float targetY =
            eqDbToY(graph, dynamicEffectiveTargetGain(b));
        const auto handleRect =
            juce::Rectangle<float>(handleX - 3.f, targetY - 7.f,
                                   6.f, 14.f);

        if (event.mods.isLeftButtonDown()
            && std::abs(dynamicsValue) <= 0.05f
            && handleRect.contains(pos))
        {
            dragDynamicHandleBand = b;
            graphLastDragPosition = pos;
            clearGraphControlState();
            dragBand = -1;
            dragOffsetBand = -1;
            dragXover = -1;
            dragDynamicMsBand = -1;
            dynamicHandleDragStartY = pos.y;
            dynamicHandleDragStartValue =
                parameterValue("DYN_DYNAMICS" + n);
            if (auto* parameter =
                    audioProcessor.apvts.getParameter("DYN_DYNAMICS" + n))
                parameter->beginChangeGesture();
            juce::StringArray graphIds;
            graphIds.add("DYN_DYNAMICS" + n);
            setGraphControlState(graphIds, false);
            showGraphDragHint = false;
            graphDragHint.clear();
            repaint();
            return;
        }
    }

    // Direct DYNAMICS target control.
    // The full coloured target circle, including its centre, is draggable.
    // Static EQ still wins only when the two visible nodes overlap.
    // Dynamic target drag matches the EQ node interaction:
    // - horizontal = linked Frequency;
    // - vertical = Dynamic target gain via DYNAMICS;
    // - Threshold is never edited independently.
    int staticPriorityBand = -1;
    float staticPriorityDistance = 7.0f;
    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const auto staticPoint = juce::Point<float>(
            graphFrequencyToX(graph, parameterValue("EQ" + n + "_FREQ")),
            eqDbToY(graph, parameterValue("EQ" + n + "_GAIN")));
        const float d = pos.getDistanceFrom(staticPoint);
        if (d < staticPriorityDistance)
        {
            staticPriorityDistance = d;
            staticPriorityBand = b;
        }
    }

    if (event.mods.isLeftButtonDown()
        && staticPriorityBand < 0
        && pointNearDynamicNode(pos, band))
    {
        const auto n = juce::String(band + 1);
        const float hz =
            parameterValue("EQ" + n + "_FREQ");

        dragBand = band;
        dragXover = -1;
        dragDynamicMsBand = -1;
        dragOffsetBand = -1;

        // Anchor the gesture to the exact pixel where the mouse was
        // pressed. Using target.y here causes an immediate fake jump when
        // the user grabs anywhere on the 24 px Dynamic ring.
        dynamicTargetDragStartY = pos.y;
        dynamicDragStartDynamics =
            parameterValue("DYN_DYNAMICS" + n);

        if (auto* parameter =
                audioProcessor.apvts.getParameter("DYN_DYNAMICS" + n))
            parameter->beginChangeGesture();
        graphFreqDragStartX = pos.x;
        graphFreqDragStartHz = hz;
        graphFreqDragGrabOffsetX =
            pos.x - graphFrequencyToX(graph, hz);
        if (auto* parameter =
                audioProcessor.apvts.getParameter("EQ" + n + "_FREQ"))
            parameter->beginChangeGesture();

        graphLastDragPosition = pos;
        clearGraphControlState();

            showGraphDragHint = false;
            graphDragHint.clear();
        repaint();
        return;
    }


    // Offset handle: edit the normal/static EQ gain only.
    // It must not move Target or change Dynamic EQ settings.
    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float x = graphFrequencyToX(
            graph, parameterValue("EQ" + n + "_FREQ"));
        const float y = eqDbToY(
            graph, parameterValue("EQ" + n + "_GAIN"));

        if (event.mods.isLeftButtonDown()
            && pos.getDistanceFrom({ x, y }) < 7.0f)
        {
            dragOffsetBand = b;
            dragBand = -1;
            dragXover = -1;
            dragDynamicMsBand = -1;
            graphEqDragAxis = GraphEqDragAxis::Undetermined;

            dynamicGainDragStartY = pos.y;
            dynamicGainDragStartOffset =
                parameterValue("EQ" + n + "_GAIN");
            graphFreqDragStartHz =
                parameterValue("EQ" + n + "_FREQ");
            const float nodeStartX =
                graphFrequencyToX(graph, graphFreqDragStartHz);
            graphFreqDragStartX = pos.x;
            graphFreqDragGrabOffsetX = pos.x - nodeStartX;

            if (auto* parameter = audioProcessor.apvts.getParameter("EQ" + n + "_FREQ"))
                parameter->beginChangeGesture();
            if (auto* parameter = audioProcessor.apvts.getParameter("EQ" + n + "_GAIN"))
                parameter->beginChangeGesture();

            graphLastDragPosition = pos;
            clearGraphControlState();

            showGraphDragHint = false;
            graphDragHint.clear();
            repaint();
            return;
        }
    }

    // Bottom figure-eight marker = continuous shared OVERLAP control.
    const float xovers[3]
    {
        graphFrequencyToX(
            graph, parameterValue("UDMBC_X1")),
        graphFrequencyToX(
            graph, parameterValue("UDMBC_X2")),
        graphFrequencyToX(
            graph, parameterValue("UDMBC_X3"))
    };

    const float markerY = graph.getBottom() - 18.f;
    for (int i = 0; i < 3; ++i)
    {
        if (pos.getDistanceFrom({ xovers[i], markerY }) < 14.f)
        {
            dragOverlapXover = i;
            dragXover = -1;
            overlapDragStartY = pos.y;
            overlapDragStartValue = parameterValue("XOVER_OVERLAP");

            if (auto* parameter =
                    audioProcessor.apvts.getParameter("XOVER_OVERLAP"))
                parameter->beginChangeGesture();

            juce::StringArray graphIds;
            graphIds.add("XOVER_OVERLAP");
            setGraphControlState(graphIds, false);

            showGraphDragHint = true;
            graphDragHintPosition = pos;
            graphDragHint =
                "X" + juce::String(i + 1)
                + "  OVERLAP "
                + juce::String(overlapDragStartValue, 1) + "%";
            repaint();
            return;
        }
    }

    // X1/X2/X3 frequency remains independently draggable on the line.
    float bestXover = 11.f;
    dragXover = -1;

    for (int i = 0; i < 3; ++i)
    {
        const float d =
            std::abs(pos.x - xovers[i]);

        if (d < bestXover)
        {
            bestXover = d;
            dragXover = i;
        }
    }

    if (dragXover >= 0)
    {
        dragBand = -1;

        const auto xoverId =
            dragXover == 0 ? "UDMBC_X1"
            : dragXover == 1 ? "UDMBC_X2"
                             : "UDMBC_X3";
        if (auto* parameter = audioProcessor.apvts.getParameter(xoverId))
            parameter->beginChangeGesture();

        juce::StringArray xoverGraphIds;
        xoverGraphIds.add(xoverId);
        setGraphControlState(xoverGraphIds, false);
        showGraphDragHint = true;
        graphDragHintPosition = pos;

        const auto hz = parameterValue(
            dragXover == 0 ? "UDMBC_X1"
            : dragXover == 1 ? "UDMBC_X2"
                             : "UDMBC_X3");

        graphDragHint =
            "X" + juce::String(dragXover + 1)
            + "  " + formatGraphFrequency(hz)
            + "   OVERLAP "
            + juce::String(
                parameterValue("XOVER_OVERLAP"), 0)
            + "%";
        repaint();
        return;
    }
}

void VVChainAudioProcessorEditor::mouseDrag(
    const juce::MouseEvent& event)
{
    const auto graph = eqGraphBounds();

    if (pendingRightClickBand >= 0
        && event.mods.isRightButtonDown()
        && event.position.getDistanceFrom(pendingRightClickPosition) > 3.0f)
    {
        const int band = pendingRightClickBand;
        const bool dynamicTarget = pendingRightClickDynamic;
        const auto start = pendingRightClickPosition;
        pendingRightClickBand = -1;
        pendingRightClickDynamic = false;
        pendingRightClickDragged = true;
        beginRightSolo(band, start, dynamicTarget);
    }

    if (rightSoloBand >= 0 && event.mods.isRightButtonDown())
    {
        rightSoloPosition = {
            juce::jlimit(graph.getX(), graph.getRight(), event.position.x),
            juce::jlimit(graph.getY(), graph.getBottom(), event.position.y)
        };

        const auto n = juce::String(rightSoloBand + 1);
        const float x =
            juce::jlimit(graph.getX(), graph.getRight(), event.position.x);
        const float hz = graphXToFrequency(graph, x);

        constexpr float movementThreshold = 0.35f;
        const bool freqMoved =
            std::abs(event.position.x - graphLastDragPosition.x)
                > movementThreshold;
        const bool verticalMoved =
            std::abs(event.position.y - graphLastDragPosition.y)
                > movementThreshold;

        juce::StringArray movingIds;

        if (freqMoved)
        {
            setParameter("EQ" + n + "_FREQ", hz);
            setParameter("GRAPH_SOLO_FREQ", hz);
            movingIds.add("EQ" + n + "_FREQ");

            if (auto* freqKnob = findKnob("EQ" + n + "_FREQ"))
                freqKnob->slider->setValue(
                    hz, juce::dontSendNotification);
        }

        float displayedGain =
            rightSoloDynamic
                ? dynamicEffectiveTargetGain(rightSoloBand)
                : parameterValue("EQ" + n + "_GAIN");

        if (verticalMoved && rightSoloDynamic)
        {
            const float targetGain = eqYToDb(graph, event.position.y);
            const float offset = parameterValue("EQ" + n + "_GAIN");
            const float dynamics =
                juce::jlimit(
                    -100.f, 100.f,
                    (targetGain - offset) / 18.f * 100.f);

            setParameter("DYN_DYNAMICS" + n, dynamics);
            movingIds.add("DYN_DYNAMICS" + n);
            displayedGain =
                dynamicEffectiveTargetGain(rightSoloBand);

            if (auto* dynamicsKnob =
                    findKnob("DYN_DYNAMICS" + n))
                dynamicsKnob->slider->setValue(
                    parameterValue("DYN_DYNAMICS" + n),
                    juce::sendNotificationSync);
        }
        else if (verticalMoved)
        {
            const float gain = eqYToDb(graph, event.position.y);
            setParameter("EQ" + n + "_GAIN", gain);
            movingIds.add("EQ" + n + "_GAIN");
            displayedGain = gain;

            if (auto* gainKnob = findKnob("EQ" + n + "_GAIN"))
                gainKnob->slider->setValue(
                    gain, juce::dontSendNotification);
        }

        if (!movingIds.isEmpty())
        {
            graphLastDragPosition = event.position;
            pulseGraphControlMovement(movingIds);
        }

        if (!freqMoved)
            setParameter(
                "GRAPH_SOLO_FREQ",
                parameterValue("EQ" + n + "_FREQ"));
        const int filterType = juce::jlimit(
            0, 13,
            juce::roundToInt(
                parameterValue("EQ" + n + "_TYPE")));
        setParameter(
            "GRAPH_SOLO_Q",
            filterType >= 12
                ? 0.70710678f
                : parameterValue("EQ" + n + "_Q"));
        setParameter("GRAPH_SOLO_ACTIVE", 1.f);

        showGraphDragHint = false;
        graphDragHint.clear();
        showFloatingValueBoxForBand(
            rightSoloBand, rightSoloDynamic,
            displayedGain, event.position);
        repaint();
        return;
    }

        // Dedicated DYNAMICS arrow handle = Y-only.
    // Up = +DYNAMICS, down = -DYNAMICS. Same sensitivity as the existing
    // Dynamic graph drag; Shift provides the same fine 0.1x adjustment.
    if (dragDynamicHandleBand >= 0)
    {
        const auto n = juce::String(dragDynamicHandleBand + 1);
        const float dragScale =
            event.mods.isShiftDown() ? 0.1f : 1.0f;
        const float deltaDynamics =
            -(event.position.y - dynamicHandleDragStartY)
            / juce::jmax(1.f, graph.getHeight())
            * 200.f * dragScale;
        const float dynamics =
            juce::jlimit(-100.f, 100.f,
                         dynamicHandleDragStartValue + deltaDynamics);

        if (std::abs(event.position.y - graphLastDragPosition.y) > 0.35f)
        {
            graphLastDragPosition = event.position;
            juce::StringArray movingIds;
            movingIds.add("DYN_DYNAMICS" + n);
            pulseGraphControlMovement(movingIds);
        }
        setParameter("DYN_DYNAMICS" + n, dynamics);
        if (auto* dynamicsKnob = findKnob("DYN_DYNAMICS" + n))
            dynamicsKnob->slider->setValue(
                parameterValue("DYN_DYNAMICS" + n),
                juce::sendNotificationSync);

        showGraphDragHint = false;
        graphDragHint.clear();
        showFloatingValueBoxForBand(
            dragDynamicHandleBand, true,
            dynamicEffectiveTargetGain(dragDynamicHandleBand),
            event.position);
        repaint();
        return;
    }

    // Static EQ graph node drag = XY:
    // horizontal = Frequency, vertical = Gain.
    if (dragOffsetBand >= 0)
    {
        const auto n = juce::String(dragOffsetBand + 1);
        // Nonlinear mastering scale with exact cursor tracking:
        // ±3 dB gets the most vertical travel, then ±3–6, ±6–12,
        // and ±12–18 progressively accelerate.
        const float correctedX =
            juce::jlimit(graph.getX(), graph.getRight(), event.position.x);
        const float hz =
            graphXToFrequency(graph, correctedX);
        const float offset =
            eqYToDb(graph, event.position.y);

        constexpr float movementThreshold = 0.35f;
        const bool freqMoved =
            std::abs(event.position.x - graphLastDragPosition.x)
                > movementThreshold;
        const bool gainMoved =
            std::abs(event.position.y - graphLastDragPosition.y)
                > movementThreshold;

        juce::StringArray movingIds;

        if (freqMoved)
        {
            setParameter("EQ" + n + "_FREQ", hz);
            movingIds.add("EQ" + n + "_FREQ");
            if (auto* freqKnob = findKnob("EQ" + n + "_FREQ"))
                freqKnob->slider->setValue(
                    hz, juce::dontSendNotification);
        }

        if (gainMoved)
        {
            setParameter("EQ" + n + "_GAIN", offset);
            movingIds.add("EQ" + n + "_GAIN");
            if (auto* gainKnob = findKnob("EQ" + n + "_GAIN"))
                gainKnob->slider->setValue(
                    offset, juce::dontSendNotification);
        }

        if (!movingIds.isEmpty())
        {
            graphLastDragPosition = event.position;
            pulseGraphControlMovement(movingIds);
        }

        showGraphDragHint = false;
        graphDragHint.clear();
        showFloatingValueBoxForBand(
            dragOffsetBand, false, offset, event.position);
        repaint();
        return;
    }

    // Dynamic M/S popup drag.
    if (dragDynamicMsBand >= 0)
    {
        const auto popup =
            dynamicMsPopupBounds(dragDynamicMsBand);

        const auto bar =
            popup.reduced(12.f)
                .withY(popup.getY() + 52.f)
                .withHeight(11.f);

        const float midPct =
            juce::jlimit(
                0.f, 100.f,
                (event.position.x - bar.getX())
                    / juce::jmax(1.f, bar.getWidth())
                    * 100.f);

        setParameter(
            "DYN_MS" + juce::String(dragDynamicMsBand + 1),
            midPct);

        repaint();
        return;
    }

    // Continuous XOVER overlap drag: vertical movement maps linearly 0..100%.
    if (dragOverlapXover >= 0)
    {
        const float dragScale =
            event.mods.isShiftDown() ? 0.1f : 1.0f;
        const float next =
            juce::jlimit(
                0.f, 100.f,
                overlapDragStartValue
                    - (event.position.y - overlapDragStartY)
                        / juce::jmax(1.f, graph.getHeight())
                        * 200.f * dragScale);

        setGraphControlMoving(true);
        setParameter("XOVER_OVERLAP", next);
        if (auto* knob = findKnob("XOVER_OVERLAP"))
            knob->slider->setValue(
                next, juce::dontSendNotification);

        graphDragHintPosition = event.position;
        graphDragHint =
            "XOVER OVERLAP "
            + juce::String(next, 1) + "%";
        showGraphDragHint = true;
        repaint();
        return;
    }

    // X1/X2/X3 crossover drag.
    if (dragXover >= 0)
    {
        const auto xoverId =
            dragXover == 0 ? "UDMBC_X1"
            : dragXover == 1 ? "UDMBC_X2"
                              : "UDMBC_X3";

        const float startX =
            graphFrequencyToX(
                graph,
                parameterValue(xoverId));

        const float dragScale =
            event.mods.isShiftDown() ? 0.1f : 1.0f;

        const float effectiveX =
            startX
            + (event.position.x - graphDragHintPosition.x)
                * dragScale;

        const float hz =
            constrainXoverFrequency(
                dragXover,
                graphXToFrequency(graph, effectiveX));

        setGraphControlMoving(true);
        setParameter(xoverId, hz);

        graphDragHintPosition = event.position;
        graphDragHint =
            "X" + juce::String(dragXover + 1)
            + "  " + formatGraphFrequency(hz)
            + "   OVERLAP "
            + juce::String(
                parameterValue("XOVER_OVERLAP"), 0)
            + "%";

        repaint();
        return;
    }

    // Dynamic Target node drag = XY control.
    // Horizontal = the same linked EQ Frequency parameter.
    // Vertical = the same linked DYNAMICS parameter.
    // The lower EQ FREQ + DYNAMICS knobs are refreshed immediately.
    if (dragBand >= 0)
    {
        const auto n = juce::String(dragBand + 1);

        // Absolute cursor mapping: Dynamic Target follows live cursor X/Y.
        const float correctedX =
            juce::jlimit(graph.getX(), graph.getRight(), event.position.x);
        const float hz =
            graphXToFrequency(graph, correctedX);
        const float targetGain =
            eqYToDb(graph, event.position.y);
        const float offset = parameterValue("EQ" + n + "_GAIN");
        const float dynamics =
            juce::jlimit(
                -100.f, 100.f,
                (targetGain - offset) / 18.f * 100.f);

        constexpr float movementThreshold = 0.35f;
        const bool freqMoved =
            std::abs(event.position.x - graphLastDragPosition.x)
                > movementThreshold;
        const bool dynamicsMoved =
            std::abs(event.position.y - graphLastDragPosition.y)
                > movementThreshold;

        juce::StringArray movingIds;

        if (freqMoved)
        {
            setParameter("EQ" + n + "_FREQ", hz);
            movingIds.add("EQ" + n + "_FREQ");
            if (auto* freqKnob = findKnob("EQ" + n + "_FREQ"))
                freqKnob->slider->setValue(
                    parameterValue("EQ" + n + "_FREQ"),
                    juce::sendNotificationSync);
        }

        if (dynamicsMoved)
        {
            setParameter("DYN_DYNAMICS" + n, dynamics);
            movingIds.add("DYN_DYNAMICS" + n);

            if (auto* dynamicsKnob =
                    findKnob("DYN_DYNAMICS" + n))
                dynamicsKnob->slider->setValue(
                    parameterValue("DYN_DYNAMICS" + n),
                    juce::sendNotificationSync);
        }

        if (!movingIds.isEmpty())
        {
            graphLastDragPosition = event.position;
            pulseGraphControlMovement(movingIds);
        }

        showGraphDragHint = false;
        graphDragHint.clear();
        showFloatingValueBoxForBand(
            dragBand, true, dynamicEffectiveTargetGain(dragBand),
            event.position);
        repaint();
        return;
    }
}

void VVChainAudioProcessorEditor::mouseUp(
    const juce::MouseEvent&)
{
    if (pendingRightClickBand >= 0 && !pendingRightClickDragged)
    {
        const int band = pendingRightClickBand;
        const auto position = pendingRightClickPosition;
        pendingRightClickBand = -1;
        pendingRightClickDynamic = false;
        pendingRightClickDragged = false;
        showEqTypeMenu(band, position);
    }

    if (rightSoloBand >= 0)
    {
        const auto n = juce::String(rightSoloBand + 1);
        if (auto* pFreq =
                audioProcessor.apvts.getParameter("EQ" + n + "_FREQ"))
            pFreq->endChangeGesture();

        if (rightSoloDynamic)
        {
            if (auto* pDynamics =
                    audioProcessor.apvts.getParameter("DYN_DYNAMICS" + n))
                pDynamics->endChangeGesture();
        }
        else
        {
            if (auto* pGain =
                    audioProcessor.apvts.getParameter("EQ" + n + "_GAIN"))
                pGain->endChangeGesture();
        }

        setParameter("GRAPH_SOLO_ACTIVE", 0.f);
        rightSoloBand = -1;
        rightSoloDynamic = false;
    }
    if (dragDynamicHandleBand >= 0)
    {
        const auto n = juce::String(dragDynamicHandleBand + 1);
        if (auto* parameter =
                audioProcessor.apvts.getParameter("DYN_DYNAMICS" + n))
            parameter->endChangeGesture();
    }

    if (dragBand >= 0)
    {
        const auto n = juce::String(dragBand + 1);
        if (auto* parameter =
                audioProcessor.apvts.getParameter("DYN_DYNAMICS" + n))
            parameter->endChangeGesture();
        if (auto* parameter =
                audioProcessor.apvts.getParameter("EQ" + n + "_FREQ"))
            parameter->endChangeGesture();
    }

    if (dragOffsetBand >= 0)
    {
        const auto n = juce::String(dragOffsetBand + 1);
        if (auto* parameter =
                audioProcessor.apvts.getParameter("EQ" + n + "_FREQ"))
            parameter->endChangeGesture();
        if (auto* parameter =
                audioProcessor.apvts.getParameter("EQ" + n + "_GAIN"))
            parameter->endChangeGesture();
    }

    if (dragOverlapXover >= 0)
    {
        if (auto* parameter =
                audioProcessor.apvts.getParameter("XOVER_OVERLAP"))
            parameter->endChangeGesture();
    }

    if (dragXover >= 0)
    {
        const auto xoverId =
            dragXover == 0 ? "UDMBC_X1"
            : dragXover == 1 ? "UDMBC_X2"
                             : "UDMBC_X3";
        if (auto* parameter = audioProcessor.apvts.getParameter(xoverId))
            parameter->endChangeGesture();
    }

    if (dragDynamicMsBand >= 0)
    {
        if (auto* parameter = audioProcessor.apvts.getParameter(
                "DYN_MS" + juce::String(dragDynamicMsBand + 1)))
            parameter->endChangeGesture();
    }

    ++graphMovementPulseGeneration;
    clearGraphControlState();
    graphLastDragPosition = { -1.0f, -1.0f };

    dragBand = -1;
    dragOffsetBand = -1;
    graphEqDragAxis = GraphEqDragAxis::Undetermined;
    dragXover = -1;
    dragOverlapXover = -1;
    dragDynamicMsBand = -1;
    dragDynamicHandleBand = -1;
    pendingRightClickDragged = false;
    showGraphDragHint = false;
    graphDragHint.clear();
    repaint();
}

void VVChainAudioProcessorEditor::mouseWheelMove(
    const juce::MouseEvent& event,
    const juce::MouseWheelDetails& wheel)
{
    const auto graph = eqGraphBounds();

    if (!graph.contains(event.position)
        || std::abs(wheel.deltaY) < 0.0001f)
        return;

    // Right-button + wheel uses the exact same Q direction/speed as normal EQ wheel while auditioning it.
    if (event.mods.isRightButtonDown())
    {
        if (pendingRightClickBand >= 0)
        {
            const int pendingBand = pendingRightClickBand;
            const bool dynamicTarget = pendingRightClickDynamic;
            const auto pendingPosition = pendingRightClickPosition;
            pendingRightClickBand = -1;
            pendingRightClickDynamic = false;
            pendingRightClickDragged = true;
            beginRightSolo(
                pendingBand, pendingPosition, dynamicTarget);
        }

        int band = rightSoloBand;
        bool dynamicTarget = rightSoloDynamic;
        float bestDistance = 24.0f;

        if (band < 0)
        {
            for (int b = 0; b < 4; ++b)
            {
                const auto n = juce::String(b + 1);
                const auto staticPoint = juce::Point<float>(
                    graphFrequencyToX(
                        graph, parameterValue("EQ" + n + "_FREQ")),
                    eqDbToY(
                        graph, parameterValue("EQ" + n + "_GAIN")));
                const float dStatic =
                    event.position.getDistanceFrom(staticPoint);

                const bool dynamicIsSeparate =
                    std::abs(parameterValue("DYN_DYNAMICS" + n)) > 0.05f;
                const float dDynamic =
                    dynamicIsSeparate
                        ? event.position.getDistanceFrom(dynamicTargetPoint(b))
                        : 1.0e9f;

                if (dStatic < bestDistance)
                {
                    bestDistance = dStatic;
                    band = b;
                    dynamicTarget = false;
                }
                if (dDynamic < bestDistance && dDynamic < dStatic)
                {
                    bestDistance = dDynamic;
                    band = b;
                    dynamicTarget = true;
                }
            }
        }

        if (band >= 0)
        {
            if (rightSoloBand < 0)
                beginRightSolo(
                    band, event.position, dynamicTarget);
            band = rightSoloBand >= 0 ? rightSoloBand : band;

            rightSoloPosition = {
                juce::jlimit(graph.getX(), graph.getRight(), event.position.x),
                juce::jlimit(graph.getY(), graph.getBottom(), event.position.y)
            };
            const auto n = juce::String(band + 1);
            const int filterType = juce::jlimit(
                0, 13,
                juce::roundToInt(
                    parameterValue("EQ" + n + "_TYPE")));

            float graphSoloQ = 0.70710678f;
            if (filterType >= 12)
            {
                const int slope = juce::jlimit(
                    0, 6,
                    juce::roundToInt(
                        parameterValue("EQ" + n + "_SLOPE")));
                const int delta = wheel.deltaY > 0.0f ? 1 : -1;
                setParameter(
                    "EQ" + n + "_SLOPE",
                    static_cast<float>(
                        juce::jlimit(0, 6, slope + delta)));
            }
            else
            {
                const float q = juce::jmax(
                    0.1f,
                    parameterValue("EQ" + n + "_Q"));
                const float nextQ =
                    qFromWheel(
                        q, wheel.deltaY,
                        event.mods.isShiftDown());
                setParameter("EQ" + n + "_Q", nextQ);
                graphSoloQ = nextQ;
            }

            setParameter("GRAPH_SOLO_FREQ", parameterValue("EQ" + n + "_FREQ"));
            setParameter("GRAPH_SOLO_Q", graphSoloQ);
            setParameter("GRAPH_SOLO_ACTIVE", 1.f);
            if (rightSoloBand < 0)
                rightSoloBand = band;
            showGraphDragHint = false;
            graphDragHint.clear();
            showFloatingValueBoxForBand(
                band,
                rightSoloDynamic,
                rightSoloDynamic
                    ? dynamicEffectiveTargetGain(band)
                    : parameterValue("EQ" + n + "_GAIN"),
                event.position);
            repaint();
            return;
        }
    }

    // Crossover curvature / OVERLAP is controlled ONLY by the
    // small cross marker at the bottom of each X1/X2/X3 line.
    {
        const float markerY = graph.getBottom() - 18.f;
        int xover = -1;
        float bestMarker = 10.f;
        const float xovers[3]
        {
            graphFrequencyToX(graph, parameterValue("UDMBC_X1")),
            graphFrequencyToX(graph, parameterValue("UDMBC_X2")),
            graphFrequencyToX(graph, parameterValue("UDMBC_X3"))
        };
        for (int i = 0; i < 3; ++i)
        {
            const float d = event.position.getDistanceFrom({ xovers[i], markerY });
            if (d < bestMarker)
            {
                bestMarker = d;
                xover = i;
            }
        }
        if (xover >= 0)
        {
            const float next = juce::jlimit(
                0.f, 100.f,
                parameterValue("XOVER_OVERLAP") - wheel.deltaY * .5f);
            juce::StringArray overlapGraphIds;
            overlapGraphIds.add("XOVER_OVERLAP");
            setGraphControlState(overlapGraphIds, false);
            setParameter("XOVER_OVERLAP", next);
            repaint();
            return;
        }
    }

    // Wheel on either the static EQ point or the DYNAMICS target point
    // adjusts the same shared Q parameter.
    int band = -1;
    bool dynamicWheelReadout = false;
    float bestDistance = 13.0f;

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float x = graphFrequencyToX(
            graph, parameterValue("EQ" + n + "_FREQ"));
        const float eqY = eqDbToY(
            graph, parameterValue("EQ" + n + "_GAIN"));
        const float eqDistance =
            event.position.getDistanceFrom({ x, eqY });

        if (eqDistance < bestDistance)
        {
            bestDistance = eqDistance;
            band = b;
            dynamicWheelReadout = false;
        }

        const float dynamics =
            parameterValue("DYN_DYNAMICS" + n);
        if (std::abs(dynamics) > 0.01f)
        {
            const float dynY = eqDbToY(
                graph, dynamicEffectiveTargetGain(b));
            const float dynDistance =
                event.position.getDistanceFrom({ x, dynY });

            if (dynDistance < bestDistance)
            {
                bestDistance = dynDistance;
                band = b;
                dynamicWheelReadout = true;
            }
        }
    }

    if (band < 0)
        return;

    const auto n =
        juce::String(band + 1);
    const int filterType = juce::jlimit(
        0, 13,
        juce::roundToInt(
            parameterValue("EQ" + n + "_TYPE")));

    juce::StringArray qGraphIds;
    qGraphIds.add("EQ" + n + "_Q");
    setGraphControlState(qGraphIds, false);
    showGraphDragHint = false;
    graphDragHint.clear();

    if (filterType >= 12)
    {
        const int slope = juce::jlimit(
            0, 5,
            juce::roundToInt(
                parameterValue("EQ" + n + "_SLOPE")));
        const int delta = wheel.deltaY > 0.0f ? 1 : -1;
        setParameter(
            "EQ" + n + "_SLOPE",
            static_cast<float>(
                juce::jlimit(0, 6, slope + delta)));
    }
    else
    {
        const float q =
            juce::jmax(
                0.1f,
                parameterValue(
                    "EQ" + n + "_Q"));
        const float nextQ =
            qFromWheel(
                q, wheel.deltaY,
                event.mods.isShiftDown());
        setParameter(
            "EQ" + n + "_Q", nextQ);

        if (auto* knob =
                findKnob("EQ" + n + "_Q"))
            knob->slider->setValue(
                nextQ,
                juce::dontSendNotification);
    }

    showFloatingValueBoxForBand(
        band,
        dynamicWheelReadout,
        dynamicWheelReadout
            ? dynamicEffectiveTargetGain(band)
            : parameterValue("EQ" + n + "_GAIN"),
        event.position);
    repaint();
}
