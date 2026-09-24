#include "PluginEditor.h"
#include "VVChain_DynEQ_Engine.h"

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

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider)
{
    const auto area = juce::Rectangle<float>((float) x, (float) y,
                                             (float) width, (float) height).reduced(3.f);
    const float cx = area.getCentreX();
    const float cy = area.getCentreY() - 3.f;
    const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f - 6.f;
    const auto accent = monochrome
        ? slider.findColour(juce::Slider::rotarySliderFillColourId).withSaturation(0.0f)
        : slider.findColour(juce::Slider::rotarySliderFillColourId);
    const float angle = juce::jmap(sliderPosProportional, rotaryStartAngle, rotaryEndAngle);

    if (slider.getComponentID() == "DEESS_MODE")
    {
        static constexpr std::array<const char*, 4> labels { "I", "II", "III", "IV" };
        const float labelRadius = radius + 12.0f;

        for (size_t i = 0; i < labels.size(); ++i)
        {
            const float t = static_cast<float>(i)
                / static_cast<float>(labels.size() - 1);
            const float tickAngle =
                juce::jmap(t, rotaryStartAngle, rotaryEndAngle);
            const float screenAngle =
                tickAngle - juce::MathConstants<float>::halfPi;

            const float inner = radius + 5.0f;
            const float outer = radius + 9.0f;
            const float x1 = cx + std::cos(screenAngle) * inner;
            const float y1 = cy + std::sin(screenAngle) * inner;
            const float x2 = cx + std::cos(screenAngle) * outer;
            const float y2 = cy + std::sin(screenAngle) * outer;

            g.setColour(accent.withAlpha(0.92f));
            g.drawLine(x1, y1, x2, y2, 1.4f);

            const float tx = cx + std::cos(screenAngle) * labelRadius - 7.0f;
            const float ty = cy + std::sin(screenAngle) * labelRadius - 5.0f;
            g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
            g.drawText(labels[i],
                       juce::Rectangle<float>(tx, ty, 14.0f, 10.0f).toNearestInt(),
                       juce::Justification::centred);
        }
    }

    g.setColour(juce::Colours::black.withAlpha(0.92f));
    g.fillEllipse(cx - radius - 3.f, cy - radius - 3.f,
                  (radius + 3.f) * 2.f, (radius + 3.f) * 2.f);

    juce::ColourGradient rim(juce::Colour(0xff70757e), cx, cy - radius,
                             juce::Colour(0xff1a1d22), cx, cy + radius, false);
    g.setGradientFill(rim);
    g.fillEllipse(cx - radius, cy - radius, radius * 2.f, radius * 2.f);

    juce::ColourGradient face(juce::Colour(0xff4a4f57), cx, cy - radius * .8f,
                              juce::Colour(0xff171a1f), cx, cy + radius, false);
    g.setGradientFill(face);
    g.fillEllipse(cx - radius + 4.f, cy - radius + 4.f,
                  (radius - 4.f) * 2.f, (radius - 4.f) * 2.f);

    // Full accent ring is intentionally independent of the knob value.
    // The value itself is shown by the rotating pointer.
    juce::Path ring;
    ring.addCentredArc(cx, cy, radius + 3.f, radius + 3.f, 0.f,
                       rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(accent.withAlpha(.96f));
    g.strokePath(ring, juce::PathStrokeType(2.4f));

    if (auto* wheelSlider = dynamic_cast<WheelSlider*>(&slider))
    {
        if (wheelSlider->isGraphControlActive())
        {
            const bool moving = wheelSlider->isGraphControlMoving();
            const bool visible = !moving
                || ((juce::Time::getMillisecondCounter() / 180u) % 2u == 0u);
            if (visible)
            {
                g.setColour(juce::Colours::white.withAlpha(.98f));
                g.drawEllipse(cx - radius - 6.f, cy - radius - 6.f,
                              (radius + 6.f) * 2.f,
                              (radius + 6.f) * 2.f,
                              moving ? 2.2f : 2.0f);
            }
        }
    }

    const float pointerLength = radius * .29f;
    const float px = cx + std::cos(angle - juce::MathConstants<float>::halfPi) * pointerLength;
    const float py = cy + std::sin(angle - juce::MathConstants<float>::halfPi) * pointerLength;
    // White pointer / scale mark for clear visibility on the dark knob face.
    g.setColour(juce::Colours::black.withAlpha(.85f));
    g.drawLine(cx, cy, px, py, 4.4f);
    g.setColour(juce::Colours::white.withAlpha(.96f));
    g.drawLine(cx, cy, px, py, 2.4f);
    g.setColour(juce::Colours::white.withAlpha(.70f));
    g.fillEllipse(cx - 2.2f, cy - 2.2f, 4.4f, 4.4f);
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawLinearSlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float sliderAsymmetry,
    float sliderStart,
    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (slider.getComponentID() == "DYN_DETECT_BLEND")
    {
        juce::ignoreUnused(sliderAsymmetry, sliderStart, style);
        auto r = juce::Rectangle<float>(
            static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
        const auto blue = monochrome
            ? juce::Colour(0xff60a5fa).withSaturation(0.0f)
            : juce::Colour(0xff60a5fa);
        g.setColour(juce::Colour(0xff090b0e));
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(juce::Colour(0xff343941));
        g.drawRoundedRectangle(r, 4.0f, 1.0f);

        const float t = juce::jlimit(0.f, 1.f, sliderPosProportional);
        const auto fill = r.reduced(2.0f).withWidth(
            juce::jmax(2.0f, r.reduced(2.0f).getWidth() * t));
        g.setColour(blue.withAlpha(.62f));
        g.fillRoundedRectangle(fill, 3.0f);

        g.setFont(juce::FontOptions(7.0f).withStyle("Bold"));
        g.setColour(juce::Colours::white.withAlpha(.94f));
        g.drawText("PEAK", r.removeFromLeft(r.getWidth() * 0.5f).toNearestInt(),
                   juce::Justification::centred);
        g.drawText("ONSETS", r.toNearestInt(), juce::Justification::centred);
        return;
    }

    if (slider.getComponentID() == "DEESS_MODE_SWITCH")
    {
        juce::ignoreUnused(sliderPosProportional, sliderAsymmetry,
                           sliderStart, style);
        auto r = juce::Rectangle<float>(
            static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
        const auto accent = monochrome
            ? slider.findColour(juce::Slider::thumbColourId).withSaturation(0.0f)
            : slider.findColour(juce::Slider::thumbColourId);
        g.setColour(juce::Colours::black.withAlpha(.78f));
        g.fillRoundedRectangle(r, 5.0f);
        g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
        g.setColour(juce::Colour(0xffcfd5dc));
        g.drawText("DE-ESS MODE", r.withHeight(14.0f).toNearestInt(),
                   juce::Justification::left);
        const auto track = r.withY(r.getY() + 17.0f).withHeight(31.0f);
        g.setColour(juce::Colour(0xff0c0f13));
        g.fillRoundedRectangle(track, 5.0f);
        g.setColour(accent.withAlpha(.45f));
        g.drawRoundedRectangle(track, 5.0f, 1.0f);
        static constexpr std::array<const char*, 4> labels { "I", "II", "III", "IV" };
        const int selected = juce::jlimit(
            1, 4, static_cast<int>(std::lround(slider.getValue()))) - 1;
        const float segW = track.getWidth() / 4.0f;
        for (int i = 0; i < 4; ++i)
        {
            auto seg = track.withX(track.getX() + segW * static_cast<float>(i))
                            .withWidth(segW);
            if (i == selected)
            {
                g.setColour(accent.withAlpha(.28f));
                g.fillRoundedRectangle(seg.reduced(2.0f), 4.0f);
                g.setColour(accent);
                g.drawRoundedRectangle(seg.reduced(2.0f), 4.0f, 1.1f);
            }
            g.setFont(juce::FontOptions(8.5f).withStyle("Bold"));
            g.setColour(i == selected ? juce::Colours::white
                                      : juce::Colour(0xffaeb5bd));
            g.drawText(labels[i], seg.toNearestInt(),
                       juce::Justification::centred);
        }
        return;
    }
    juce::LookAndFeel_V4::drawLinearSlider(
        g, x, y, width, height,
        sliderPosProportional, sliderAsymmetry,
        sliderStart, style, slider);
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    if (button.getComponentID() == "MODULE_BYPASS")
    {
        const auto r = button.getLocalBounds().toFloat().reduced(1.0f);
        const auto accent = monochrome
            ? button.findColour(juce::ToggleButton::tickColourId).withSaturation(0.0f)
            : button.findColour(juce::ToggleButton::tickColourId);
        const bool active = !button.getToggleState();

        g.setColour(juce::Colour(0xff171a1f));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(accent.withAlpha(active ? .78f : .28f));
        g.drawRoundedRectangle(r, 5.0f, 1.0f);

        const float cy = r.getCentreY();
        const float ledD = 9.0f;
        const float ledX = r.getX() + 7.0f;
        g.setColour(juce::Colours::black.withAlpha(.75f));
        g.fillEllipse(ledX - 1.5f, cy - ledD * 0.5f - 1.5f, ledD + 3.0f, ledD + 3.0f);
        g.setColour(active ? accent : juce::Colour(0xff5d636b));
        g.fillEllipse(ledX, cy - ledD * 0.5f, ledD, ledD);

        g.setColour(juce::Colour(0xffe2e7ec));
        g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
        g.drawText(button.getButtonText(),
                   juce::Rectangle<int>((int) r.getX() + 21, (int) r.getY(),
                                        (int) r.getWidth() - 23, (int) r.getHeight()),
                   juce::Justification::centred);
        return;
    }

    if (button.getComponentID() == "DYN_MODE")
    {
        const auto r = button.getLocalBounds().toFloat().reduced(1.0f);
        const auto accent = monochrome
            ? button.findColour(juce::ToggleButton::tickColourId).withSaturation(0.0f)
            : button.findColour(juce::ToggleButton::tickColourId);
        const bool on = button.getToggleState();

        g.setColour(on ? juce::Colour(0xff353a42) : juce::Colour(0xff20242a));
        g.fillRoundedRectangle(r, 5.0f);
        g.setColour(accent.withAlpha(on ? .82f : .38f));
        g.drawRoundedRectangle(r, 5.0f, on ? 1.1f : 1.0f);
        g.setColour(juce::Colour(0xffe8edf2));
        g.setFont(juce::FontOptions(7.2f).withStyle("Bold"));
        g.drawText(button.getButtonText(),
                   r.toNearestInt().reduced(3, 1),
                   juce::Justification::centred);
        return;
    }

    if (button.getComponentID() == "ANALOG_MODE")
    {
        auto r = button.getLocalBounds().toFloat().reduced(1.0f);
        const auto accent = monochrome
            ? button.findColour(juce::ToggleButton::tickColourId).withSaturation(0.0f)
            : button.findColour(juce::ToggleButton::tickColourId);
        const bool ss = button.getToggleState();
        g.setColour(juce::Colour(0xff090b0e));
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(juce::Colour(0xff343941));
        g.drawRoundedRectangle(r, 4.0f, 1.0f);
        const float half = r.getWidth() * 0.5f;
        g.setColour(ss ? juce::Colour(0xff171a1e) : accent.withAlpha(.26f));
        g.fillRoundedRectangle(r.getX(), r.getY(), half, r.getHeight(), 4.0f);
        g.setColour(ss ? accent.withAlpha(.26f) : juce::Colour(0xff171a1e));
        g.fillRoundedRectangle(r.getX() + half, r.getY(), half, r.getHeight(), 4.0f);
        g.setFont(juce::FontOptions(7.0f).withStyle("Bold"));
        g.setColour(ss ? accent : juce::Colour(0xffedf1f5));
        g.drawText("TT", r.withWidth(half).toNearestInt(), juce::Justification::centred);
        g.setColour(ss ? juce::Colour(0xffedf1f5) : accent);
        g.drawText("SS", r.withX(r.getX() + half).withWidth(half).toNearestInt(),
                   juce::Justification::centred);
        return;
    }

    if (button.getComponentID() == "DEESS_ROUND_BYPASS")
    {
        const auto r = button.getLocalBounds().toFloat().reduced(1.f);
        const float d = juce::jmin(r.getWidth(), r.getHeight()) - 6.f;
        const float cx = r.getCentreX();
        const float cy = r.getCentreY();
        const auto accent = monochrome
            ? button.findColour(juce::ToggleButton::tickColourId).withSaturation(0.0f)
            : button.findColour(juce::ToggleButton::tickColourId);
        const bool active = !button.getToggleState();

        g.setColour(juce::Colours::black.withAlpha(.92f));
        g.fillEllipse(cx - d * .5f - 5.f, cy - d * .5f - 5.f,
                      d + 10.f, d + 10.f);

        juce::ColourGradient glow(active ? accent.withAlpha(.95f)
                                          : juce::Colour(0xff4b5058),
                                  cx, cy - d * .5f,
                                  active ? accent.withAlpha(.16f)
                                         : juce::Colour(0xff15181c),
                                  cx, cy + d * .5f, false);
        g.setGradientFill(glow);
        g.fillEllipse(cx - d * .5f, cy - d * .5f, d, d);
        g.setColour(active ? accent : juce::Colour(0xff626870));
        g.drawEllipse(cx - d * .5f, cy - d * .5f, d, d, 1.8f);

        g.setColour(active ? juce::Colours::white
                           : juce::Colour(0xff8b929a));

        g.drawLine(cx, cy - d * .27f,
                   cx, cy - d * .02f, 3.f);

        juce::Path powerArc;
        const float pi = juce::MathConstants<float>::pi;
        powerArc.addCentredArc(
            cx, cy + d * .01f,
            d * .22f, d * .22f, 0.f,
            pi * 0.23f, pi * 1.77f, true);
        g.strokePath(powerArc, juce::PathStrokeType(3.f));
        return;
    }

    if (button.getComponentID() == "ANALOG_X2")
    {
        const auto r = button.getLocalBounds().toFloat().reduced(1.0f);
        const bool on = button.getToggleState();
        const auto accent = monochrome
            ? button.findColour(juce::ToggleButton::tickColourId).withSaturation(0.0f)
            : button.findColour(juce::ToggleButton::tickColourId);

        g.setColour(juce::Colours::black.withAlpha(0.92f));
        g.fillRoundedRectangle(r, 3.5f);
        g.setColour(accent.withAlpha(on ? 0.95f : 0.35f));
        g.drawRoundedRectangle(r, 3.5f, on ? 1.2f : 1.0f);

        if (on)
        {
            g.setColour(accent.withAlpha(0.28f));
            g.fillRoundedRectangle(r.reduced(1.5f), 3.0f);
        }

        g.setColour(on ? juce::Colours::white : juce::Colour(0xff7f8790));
        g.setFont(juce::FontOptions(7.0f).withStyle("Bold"));
        g.drawText("X2", r.toNearestInt(), juce::Justification::centred);
        return;
    }

    if (button.getWidth() <= 36 && button.getHeight() <= 36)
    {
        const float d = juce::jmin(button.getWidth(), button.getHeight()) - 10.f;
        const float cx = button.getLocalBounds().getCentreX();
        const float cy = button.getLocalBounds().getCentreY();
        const auto accent = monochrome
            ? button.findColour(juce::ToggleButton::tickColourId).withSaturation(0.0f)
            : button.findColour(juce::ToggleButton::tickColourId);
        const bool active = !button.getToggleState();

        g.setColour(juce::Colours::black.withAlpha(.8f));
        g.fillEllipse(cx - d * .5f - 3.f, cy - d * .5f - 3.f, d + 6.f, d + 6.f);

        juce::ColourGradient glow(active ? accent.withAlpha(.95f)
                                          : juce::Colour(0xff4b5058),
                                  cx, cy - d * .5f,
                                  active ? accent.withAlpha(.18f)
                                         : juce::Colour(0xff17191d),
                                  cx, cy + d * .5f, false);
        g.setGradientFill(glow);
        g.fillEllipse(cx - d * .5f, cy - d * .5f, d, d);

        g.setColour(active ? accent : juce::Colour(0xff666b74));
        g.drawEllipse(cx - d * .5f, cy - d * .5f, d, d, 1.2f);
        return;
    }

    auto r = button.getLocalBounds().toFloat().reduced(1.f);
    const bool on = button.getToggleState();
    const auto accent = monochrome
        ? button.findColour(juce::ToggleButton::tickColourId).withSaturation(0.0f)
        : button.findColour(juce::ToggleButton::tickColourId);

    g.setColour(on ? juce::Colour(0xff353a42) : juce::Colour(0xff23262b));
    g.fillRoundedRectangle(r, 5.f);
    g.setColour(accent.withAlpha(on ? .7f : .35f));
    g.drawRoundedRectangle(r, 5.f, on ? 1.2f : 1.f);
    g.setColour(juce::Colour(0xffc2c7ce));
    g.setFont(juce::FontOptions(8.f).withStyle("Bold"));
    g.drawText(button.getButtonText(), r.toNearestInt(), juce::Justification::centred);
}

VVChainAudioProcessorEditor::VVChainAudioProcessorEditor(VVChainAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      globalGraphMouseListener(this)
{
    setLookAndFeel(&metalLook);
    setResizable(false, false);
    setSize(1500, 930);

    const std::array<juce::String, 5> bypassIds
    {
        "EQ_BYPASS", "OTT_BYPASS", "EQ_COLOR_GLOBAL_BYPASS",
        "ATYPE_BYPASS", "DEESS_BYPASS"
    };

    const std::array<juce::String, 5> bypassLabels
    {
        "EQ", "OTT", "ANALOG", "TAPE-A", "DE-ESS"
    };

    const std::array<juce::Colour, 5> bypassColours
    {
        juce::Colour(0xff38bdf8),
        juce::Colour(0xfffacc15),
        juce::Colour(0xff60a5fa),
        juce::Colour(0xfff472b6),
        juce::Colour(0xff67d3aa)
    };

    for (int i = 0; i < 5; ++i)
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
        detectBlendSlider->setDragSensitivity(133, 1330);
        detectBlendSlider->setWheelBehaviour(0.5, false);
        dynDetectSliders[(size_t) b] = std::move(detectBlendSlider);
        dynDetectSliders[(size_t) b]->setComponentID("DYN_DETECT_BLEND");
        dynDetectSliders[(size_t) b]->setLookAndFeel(&metalLook);
        dynDetectSliders[(size_t) b]->setRange(0.0, 100.0, 0.1);
        dynDetectSliders[(size_t) b]->setValue(
            parameterValue("DYN_DETECT_ONSETS" + n),
            juce::dontSendNotification);
        dynDetectSliders[(size_t) b]->setTooltip(
            "PEAK ↔ ONSETS detector blend；50% = equal blend");
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

        // Main screen intentionally keeps only the three OTT performance knobs.
        addKnob("OTT_DEGREE" + n, "OTT %", 0, 100, .1,
                parameterValue("OTT_DEGREE" + n), " %", b, 3, juce::Colour(0xfffacc15));
        addKnob("OTT_COMP_A" + n, "ATTACK", .1, 250, .1,
                parameterValue("OTT_COMP_A" + n), " ms", b, 4, juce::Colour(0xfffacc15));
        addKnob("OTT_COMP_R" + n, "RELEASE", 10, 2500, 1,
                parameterValue("OTT_COMP_R" + n), " ms", b, 5, juce::Colour(0xfffacc15));

        addKnob("ATYPE_DEGREE" + n, "TAPE-A +", 0, 100, .1,
                parameterValue("ATYPE_DEGREE" + n), "", b, 7, c, true);

        // Independent per-band bypass LEDs. False = active/lit; true = bypass/dim.
        ottBandBypassButtons[(size_t) b] = std::make_unique<juce::ToggleButton>();
        ottBandBypassButtons[(size_t) b]->setLookAndFeel(&metalLook);
        ottBandBypassButtons[(size_t) b]->setButtonText("");
        ottBandBypassButtons[(size_t) b]->setColour(
            juce::ToggleButton::tickColourId, juce::Colour(0xfffacc15));
        ottBandBypassButtons[(size_t) b]->setTooltip(
            "BAND " + n + " OTT：亮 = 啟用；按下 = BYPASS");
        ottBandBypassAttachments[(size_t) b] =
            std::make_unique<BoolAttachment>(
                audioProcessor.apvts, "OTT_BAND_BYPASS" + n,
                *ottBandBypassButtons[(size_t) b]);
        addAndMakeVisible(*ottBandBypassButtons[(size_t) b]);

        atypeBandBypassButtons[(size_t) b] = std::make_unique<juce::ToggleButton>();
        atypeBandBypassButtons[(size_t) b]->setLookAndFeel(&metalLook);
        atypeBandBypassButtons[(size_t) b]->setButtonText("");
        atypeBandBypassButtons[(size_t) b]->setColour(
            juce::ToggleButton::tickColourId, juce::Colour(0xfff472b6));
        atypeBandBypassButtons[(size_t) b]->setTooltip(
            "BAND " + n + " TYPE-A：亮 = 啟用；按下 = BYPASS");
        atypeBandBypassAttachments[(size_t) b] =
            std::make_unique<BoolAttachment>(
                audioProcessor.apvts, "ATYPE_BAND_BYPASS" + n,
                *atypeBandBypassButtons[(size_t) b]);
        addAndMakeVisible(*atypeBandBypassButtons[(size_t) b]);

        // Seven band-specific OTT advanced controls. The defaults remain in DSP.
        addKnob("OTT_LIFT_T" + n, "LIFT THRESH", -80, 0, .1,
                parameterValue("OTT_LIFT_T" + n), " dB", b, 20, c);
        addKnob("OTT_LIFT_A" + n, "LIFT ATT", 1, 500, .1,
                parameterValue("OTT_LIFT_A" + n), " ms", b, 21, c);
        addKnob("OTT_LIFT_R" + n, "LIFT REL", 10, 2500, 1,
                parameterValue("OTT_LIFT_R" + n), " ms", b, 22, c);
        addKnob("OTT_LIFT_M" + n, "LIFT MIX", 0, 100, .1,
                parameterValue("OTT_LIFT_M" + n), " %", b, 23, c);
        addKnob("OTT_COMP_T" + n, "COMP THRESH", -24, 0, .1,
                parameterValue("OTT_COMP_T" + n), " dB", b, 24, c);
        addKnob("OTT_COMP_M" + n, "COMP MIX", 0, 100, .1,
                parameterValue("OTT_COMP_M" + n), " %", b, 25, c);
        addKnob("OTT_LEVEL" + n, "BAND LEVEL", -24, 12, .1,
                parameterValue("OTT_LEVEL" + n), " dB", b, 26, c);

        advancedButtons[(size_t) b] = std::make_unique<juce::TextButton>("+ ADV");
        advancedButtons[(size_t) b]->setTooltip("開啟 BAND " + n + " 的 OTT ADVANCED");
        advancedButtons[(size_t) b]->onClick = [this, b]
        {
            setExpandedBand(expandedBand == b ? -1 : b);
        };
        addAndMakeVisible(*advancedButtons[(size_t) b]);
    }

    // Dedicated fifth zone: DE-ESSER is separate from BAND 4.
    addKnob("DEESS_FREQ", "DE-ESS FREQ", 6000, 18000, 10,
            parameterValue("DEESS_FREQ"), " Hz", 4, 0,
            juce::Colour(0xff67d3aa));
    addKnob("DEESS_INTENSITY", "MAXIMUM REDUCTION", 0, 8, .1,
            parameterValue("DEESS_INTENSITY"), " dB", 4, 1,
            juce::Colour(0xff67d3aa));
    deessModeSwitch = std::make_unique<juce::Slider>();
    deessModeSwitch->setLookAndFeel(&metalLook);
    deessModeSwitch->setComponentID("DEESS_MODE_SWITCH");
    deessModeSwitch->setSliderStyle(juce::Slider::LinearHorizontal);
    deessModeSwitch->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    deessModeSwitch->setRange(1.0, 4.0, 1.0);
    deessModeSwitch->setValue(
        parameterValue("DEESS_MODE"), juce::dontSendNotification);
    deessModeSwitch->setColour(
        juce::Slider::thumbColourId, juce::Colour(0xff67d3aa));
    deessModeSwitch->setTooltip(
        "I SAFE 5/120 ms 3:1 · II VOCAL 2/70 ms 4:1 · "
        "III FAST 0.75/35 ms 8:1 · IV HARD 0.25/20 ms 10:1");
    deessModeAttachment =
        std::make_unique<Attachment>(
            audioProcessor.apvts, "DEESS_MODE", *deessModeSwitch);
    addAndMakeVisible(*deessModeSwitch);



    deessBypassButton = std::make_unique<juce::ToggleButton>();
    deessBypassButton->setLookAndFeel(&metalLook);
    deessBypassButton->setComponentID("DEESS_ROUND_BYPASS");
    deessBypassButton->setButtonText("");
    deessBypassButton->setColour(
        juce::ToggleButton::tickColourId, juce::Colour(0xffdfe7ef));
    deessBypassButton->setTooltip("整體 BYPASS：亮 = 作動中；暗 = BYPASS");
    deessBypassAttachment = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, "MASTER_BYPASS", *deessBypassButton);
    addAndMakeVisible(*deessBypassButton);

    deessLocalBypassButton = std::make_unique<juce::ToggleButton>();
    deessLocalBypassButton->setLookAndFeel(&metalLook);
    deessLocalBypassButton->setComponentID("DEESS_LOCAL_BYPASS");
    deessLocalBypassButton->setButtonText("");
    deessLocalBypassButton->setColour(
        juce::ToggleButton::tickColourId, juce::Colour(0xff67d3aa));
    deessLocalBypassButton->setTooltip(
        "DE-ESSER：亮 = 啟用；按下 = BYPASS");
    deessLocalBypassAttachment = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, "DEESS_BYPASS", *deessLocalBypassButton);
    addAndMakeVisible(*deessLocalBypassButton);

    deltaMonitorButton = std::make_unique<juce::ToggleButton>("DELTA");
    deltaMonitorButton->setLookAndFeel(&metalLook);
    deltaMonitorButton->setComponentID("DEESS_DELTA");
    deltaMonitorButton->setColour(
        juce::ToggleButton::tickColourId, juce::Colour(0xffffffff));
    deltaMonitorButton->setTooltip(
        "DELTA：輸出聲音 − 原始聲音；使用已對齊乾聲，不增加額外延遲");
    deltaMonitorAttachment = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, "DELTA_MONITOR", *deltaMonitorButton);
    addAndMakeVisible(*deltaMonitorButton);

    // Shared OTT advanced controls appear inside the currently expanded BAND.
    addKnob("OTT_X1", "XOVER 1", 80, 600, 1,
            parameterValue("OTT_X1"), " Hz", -1, 30, juce::Colour(0xfffacc15));
    addKnob("OTT_X2", "XOVER 2", 750, 3000, 1,
            parameterValue("OTT_X2"), " Hz", -1, 31, juce::Colour(0xfffacc15));
    addKnob("OTT_X3", "XOVER 3", 6000, 12000, 1,
            parameterValue("OTT_X3"), " Hz", -1, 32, juce::Colour(0xfffacc15));
    addKnob("XOVER_OVERLAP", "OVERLAP", 0, 100, 1,
            parameterValue("XOVER_OVERLAP"), " %", -1, 33, juce::Colour(0xfffacc15));
    addKnob("OTT_INPUT", "INPUT", -24, 24, .1,
            parameterValue("OTT_INPUT"), " dB", -1, 33, juce::Colour(0xfffacc15));
    addKnob("OTT_GATE", "GATE", -90, 0, .1,
            parameterValue("OTT_GATE"), " dB", -1, 34, juce::Colour(0xfffacc15));
    addKnob("OTT_MIX", "MASTER MIX", 0, 100, .1,
            parameterValue("OTT_MIX"), " %", -1, 35, juce::Colour(0xfffacc15));
    addKnob("OTT_OUTPUT", "OUTPUT", -24, 24, .1,
            parameterValue("OTT_OUTPUT"), " dB", -1, 36, juce::Colour(0xfffacc15));

    ottClipper = std::make_unique<juce::ToggleButton>("CLIPPER");
    ottClipper->setLookAndFeel(&metalLook);
    ottClipper->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xfffacc15));
    ottClipper->setTooltip("OTT Clipper");
    addAndMakeVisible(*ottClipper);
    ottClipperAttachment = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, "OTT_CLIPPER", *ottClipper);

    closeAdvanced = std::make_unique<juce::TextButton>("CLOSE");
    closeAdvanced->setButtonText("CLOSE");
    closeAdvanced->onClick = [this] { setExpandedBand(-1); };
    addAndMakeVisible(*closeAdvanced);

    startTimerHz(30);
    updateBypassVisuals();

    // DE-ESSER also owns the final MIX / OUT controls.
    // All four controls are rotary knobs and stay strictly vertical.
    addKnob("DRY_WET", "MIX", 0, 100, .1,
            parameterValue("DRY_WET"), " %", 4, 2, juce::Colour(0xff38bdf8));
    addKnob("OUTPUT_LEVEL", "OUT", -24, 12, .1,
            parameterValue("OUTPUT_LEVEL"), " dB", 4, 3, juce::Colour(0xff9ed85c));

    setExpandedBand(-1);

    // Hover value box sits above the graph but never intercepts the graph mouse.
    floatingValueBox.setAlwaysOnTop(true);
    addAndMakeVisible(floatingValueBox);
    floatingValueBox.hideInstantly();

    // Graph hover must see mouse moves even when the pointer is over child
    // components or the non-intercepting overlay.
    juce::Desktop::getInstance().addGlobalMouseListener(
        &globalGraphMouseListener);
}

VVChainAudioProcessorEditor::~VVChainAudioProcessorEditor()
{
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
    if (deessBypassButton) deessBypassButton->setLookAndFeel(nullptr);
    deessBypassAttachment.reset();
    if (deessLocalBypassButton) deessLocalBypassButton->setLookAndFeel(nullptr);
    deessLocalBypassAttachment.reset();
    if (deltaMonitorButton) deltaMonitorButton->setLookAndFeel(nullptr);
    deltaMonitorAttachment.reset();

    for (auto& b : ottBandBypassButtons)
        if (b) b->setLookAndFeel(nullptr);
    for (auto& b : atypeBandBypassButtons)
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

    for (auto& a : analogBypassAttachments)
        a.reset();

    for (auto& a : dynDetectAttachments)
        a.reset();
    for (auto& a : dynTriggerAttachments)
        a.reset();

    for (auto& a : ottBandBypassAttachments)
        a.reset();

    for (auto& a : analogModeAttachments)
        a.reset();
    for (auto& a : atypeBandBypassAttachments)
        a.reset();

    if (advancedButtons.size() > 0)
        for (auto& b : advancedButtons)
            if (b) b->setLookAndFeel(nullptr);

    if (ottClipper)
        ottClipper->setLookAndFeel(nullptr);
    ottClipperAttachment.reset();
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
    k.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 17);
    k.slider->setScrollWheelEnabled(true);
    k.slider->setRange(min, max, step);
    if (id.endsWith("_FREQ") || id.startsWith("OTT_X"))
        k.slider->setSkewFactorFromMidPoint(632.f);

    if (auto* wheelSlider = dynamic_cast<WheelSlider*>(k.slider.get()))
    {
        const bool logarithmic =
            id.endsWith("_FREQ") || id.startsWith("OTT_X");

        if (id.endsWith("_FREQ"))
            wheelSlider->setDragSensitivity(900, 9000);
        else
            wheelSlider->setDragSensitivity(180, 1800);

        if (id == "DEESS_MODE")
            wheelSlider->setDiscreteArc(
                4,
                7.0f * juce::MathConstants<float>::pi / 6.0f,
                11.0f * juce::MathConstants<float>::pi / 6.0f);

        double wheelStep = std::max(
            0.01, (max - min) * 0.01);

        if (id.contains("GAIN") || id.contains("LEVEL")
            || id.contains("THRESH") || id.endsWith("_OUTPUT")
            || id == "OUTPUT_LEVEL" || id == "ATYPE_LEVEL")
            wheelStep = 0.5;
        else if (id.endsWith("_Q"))
            wheelStep = 0.02;
        else if (id.contains("DYNAMICS"))
            wheelStep = 1.0;
        else if (id.contains("ATTACK"))
            wheelStep = 1.0;
        else if (id.contains("RELEASE"))
            wheelStep = 5.0;
        else if (id == "DEESS_MODE")
            wheelStep = 1.0;
        else if (id.contains("DEGREE") || id.contains("MIX")
                 || id.contains("COLOR") || id == "DRY_WET")
            wheelStep = 1.0;

        wheelSlider->setWheelBehaviour(wheelStep, logarithmic);
    }
    k.slider->setDoubleClickReturnValue(true, defaultValue);
    k.slider->setColour(juce::Slider::rotarySliderFillColourId, accent);
    k.slider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff08090b));
    k.slider->setNumDecimalPlacesToDisplay(
        step < .01 ? 3 : step < .1 ? 2 : step < 1 ? 1 : 0);

    if (tapeDisplayDb)
    {
        k.slider->textFromValueFunction = [](double value)
        {
            return juce::String(value * .06, 1) + " dB";
        };
        k.slider->valueFromTextFunction = [](const juce::String& text)
        {
            return text.retainCharacters("0123456789.-").getDoubleValue() / .06;
        };
    }
    else
    {
        k.slider->setTextValueSuffix(suffix);
    }

    k.label->setText(title, juce::dontSendNotification);
    k.label->setColour(juce::Label::textColourId, accent.brighter(.35f));
    k.label->setJustificationType(juce::Justification::centred);
    k.label->setFont(juce::FontOptions(8.0f).withStyle("Bold"));

    const auto parameterId = attachmentId.isNotEmpty() ? attachmentId : id;
    k.attachment = std::make_unique<Attachment>(
        audioProcessor.apvts, parameterId, *k.slider);

    // Value=0 means the corresponding processing band/module is bypassed.
    // Any value above 0 immediately re-enables it.
    if (id.startsWith("OTT_DEGREE") && k.band >= 0)
    {
        const int band = k.band;
        auto* slider = k.slider.get();
        k.slider->onValueChange = [this, band, slider]
        {
            const float target =
                slider->getValue() <= 0.0001 ? 1.0f : 0.0f;
            const auto bypassId =
                "OTT_BAND_BYPASS" + juce::String(band + 1);
            if (auto* parameter = audioProcessor.apvts.getParameter(bypassId))
            {
                if (std::abs(parameter->getValue() - target) > 1.0e-6f)
                    parameter->setValueNotifyingHost(target);
            }
        };
    }
    else if (id.startsWith("ATYPE_DEGREE") && k.band >= 0)
    {
        const int band = k.band;
        auto* slider = k.slider.get();
        k.slider->onValueChange = [this, band, slider]
        {
            const float target =
                slider->getValue() <= 0.0001 ? 1.0f : 0.0f;
            const auto bypassId =
                "ATYPE_BAND_BYPASS" + juce::String(band + 1);
            if (auto* parameter = audioProcessor.apvts.getParameter(bypassId))
            {
                if (std::abs(parameter->getValue() - target) > 1.0e-6f)
                    parameter->setValueNotifyingHost(target);
            }
        };
    }
    else if (id == "DEESS_INTENSITY")
    {
        auto* slider = k.slider.get();
        k.slider->onValueChange = [this, slider]
        {
            const float target =
                slider->getValue() <= 0.0001 ? 1.0f : 0.0f;
            if (auto* parameter =
                    audioProcessor.apvts.getParameter("DEESS_BYPASS"))
            {
                if (std::abs(parameter->getValue() - target) > 1.0e-6f)
                    parameter->setValueNotifyingHost(target);
            }
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
    // Intentionally large: the EQ response is the visual focus at the top.
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
    const float x1 = parameterValue("OTT_X1");
    const float x2 = parameterValue("OTT_X2");
    const float x3 = parameterValue("OTT_X3");
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

        // At/near 0% Dynamics the EQ point owns the whole central area;
        // Dynamics is then adjusted with the separate arrow handle.
        const bool hit =
            std::abs(dynamics) > 0.5f
            && d >= staticNodeRadius
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
    juce::ColourGradient bg(
        juce::Colour(0xff0f1216), graph.getX(), graph.getY(),
        juce::Colour(0xff20242a), graph.getRight(), graph.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(graph, 8.f);

    g.setColour(juce::Colours::black.withAlpha(.95f));
    g.drawRoundedRectangle(graph, 8.f, 1.f);

    for (int i = 0; i <= 6; ++i)
    {
        const float db = 36.f - i * 12.0f;
        const float y = eqDbToY(graph, db);
        g.setColour(juce::Colour(0xff69717c).withAlpha(.42f));
        g.drawHorizontalLine(
            (int)y, graph.getX(), graph.getRight());
    }

    for (float f : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f,
                     2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = graphFrequencyToX(graph, f);
        g.setColour(juce::Colour(0xff68727d).withAlpha(.33f));
        g.drawVerticalLine(
            (int)x, graph.getY(), graph.getBottom());
    }

    const float xovers[3]
    {
        graphFrequencyToX(graph, parameterValue("OTT_X1")),
        graphFrequencyToX(graph, parameterValue("OTT_X2")),
        graphFrequencyToX(graph, parameterValue("OTT_X3"))
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
            i == 0 ? "OTT_X1" : i == 1 ? "OTT_X2" : "OTT_X3");

        g.setColour(
            uiColour(juce::Colour(0xffffd84d)).withAlpha(.92f));
        g.drawVerticalLine(
            (int)x, graph.getY() + 20.f, graph.getBottom() - 28.f);

        const bool xoverHovered = hoverXover == i;
        const float markerY = graph.getBottom() - 18.f;
        const float labelY = graph.getBottom() - 35.f;

        g.setColour(juce::Colours::black.withAlpha(.72f));
        g.fillRoundedRectangle(
            x - 47.f, labelY, 94.f, 15.f, 3.f);
        g.setColour(
            uiColour(juce::Colour(0xffffdf67)));
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
            ? juce::Colour(0xfffff3a8)
            : juce::Colour(0xffffdf67)));

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

    // Restored graph axis labels: dB scale + frequency scale.
    g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
    g.setColour(juce::Colour(0xffaab0ba));
    for (float db : { 18.f, 9.f, 0.f, -9.f, -18.f })
    {
        const float y = eqDbToY(graph, db);
        const auto label = (db > 0.f ? "+" : "")
            + juce::String(db, 0) + " dB";
        g.drawText(label,
                   (int)graph.getX() + 5,
                   (int)juce::jmax(graph.getY() + 1.f, y - 7.f),
                   44, 12,
                   juce::Justification::left);
    }
    for (const auto& tick : std::initializer_list<std::pair<float, juce::String>>{
        { 20.f, "20 Hz" }, { 100.f, "100 Hz" }, { 1000.f, "1 kHz" },
        { 10000.f, "10 kHz" }, { 20000.f, "20 kHz" } })
    {
        const float x = graphFrequencyToX(graph, tick.first);
        const int width = 40;
        const int left = tick.first == 20.f
            ? (int)x
            : tick.first == 20000.f
                ? (int)x - width
                : (int)x - width / 2;
        g.drawText(tick.second, left, (int)graph.getBottom() - 12,
                   width, 10,
                   juce::Justification::centred);
    }

    // Static Offset EQ response.
    auto qForGain = [](float baseQ, float gainDb)
    {
        // Oxford Type-3 philosophy: Q reduces as gain moves away from 0,
        // making stronger boosts/cuts progressively wider and softer.
        return juce::jlimit(
            0.10f, 18.0f,
            baseQ / (1.0f + 0.045f * std::abs(gainDb)));
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
            const float width =
                juce::jmax(.02f, 1.f / (q * 1.8f));
            const float xx = std::log(
                std::max(hz, 20.f) / std::max(f0, 20.f));
            const float shape =
                std::exp(
                    -(xx * xx) /
                    (2.f * width * width));
            db += gain * shape;
        }

        const auto pt = juce::Point<float>(
            graphFrequencyToX(graph, hz),
            eqDbToY(graph, db));

        if (i == 0)
            offsetResponse.startNewSubPath(pt);
        else
            offsetResponse.lineTo(pt);
    }

    g.setColour(
        juce::Colours::white.withAlpha(.35f));
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
        const auto c =
            uiColour(kBandColours[(size_t)band]);

        juce::Path top;
        juce::Path bottom;

        const double graphSampleRate =
            audioProcessor.getSampleRate() > 1000.0
                ? audioProcessor.getSampleRate()
                : 44100.0;

        for (int i = 0; i <= 220; ++i)
        {
            const float hz =
                invLogMap(i / 220.f, 20.f, 20000.f);
            const float offsetQ =
                qForGain(baseQ, offset);
            const float targetQ =
                qForGain(baseQ, target);
            const float offsetDb =
                VVChain_DynEQ_Engine::peakMagnitudeDBAtFrequency(
                    graphSampleRate, f0, offsetQ, offset, hz);
            const float targetDb =
                VVChain_DynEQ_Engine::peakMagnitudeDBAtFrequency(
                    graphSampleRate, f0, targetQ, target, hz);
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
                    VVChain_DynEQ_Engine::peakMagnitudeDBAtFrequency(
                        graphSampleRate, f0, targetQ, target, hz));
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

        // Dedicated DYNAMICS drag handle follows the DYNAMICS target point.
        // It is always visible, including the default 0% state.
        const bool dynamicsEnabled = true;
        const float handleX =
            juce::jlimit(graph.getX() + 18.f,
                         graph.getRight() - 12.f,
                         x + 44.f);
        const float handleY = targetY;

        if (dynamicsEnabled)
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

    // Right-click SOLO keeps the selected EQ region in full colour and
    // linearly fades the rest of the graph into grey with distance.
    if (rightSoloBand >= 0
        && parameterValue("GRAPH_SOLO_ACTIVE") > 0.5f)
    {
        const auto soloN = juce::String(rightSoloBand + 1);
        const float centreX = graphFrequencyToX(
            graph, parameterValue("EQ" + soloN + "_FREQ"));
        constexpr float colourRadius = 70.0f;

        const float leftEdge = juce::jmax(graph.getX(), centreX - colourRadius);
        if (leftEdge > graph.getX())
        {
            juce::ColourGradient fade(
                juce::Colour(0xff777b80).withAlpha(0.72f),
                graph.getX(), graph.getCentreY(),
                juce::Colour(0xff777b80).withAlpha(0.0f),
                leftEdge, graph.getCentreY(), false);
            g.setGradientFill(fade);
            g.fillRect(graph.getX(), graph.getY(),
                       leftEdge - graph.getX(), graph.getHeight());
        }

        const float rightEdge = juce::jmin(graph.getRight(), centreX + colourRadius);
        if (rightEdge < graph.getRight())
        {
            juce::ColourGradient fade(
                juce::Colour(0xff777b80).withAlpha(0.0f),
                rightEdge, graph.getCentreY(),
                juce::Colour(0xff777b80).withAlpha(0.72f),
                graph.getRight(), graph.getCentreY(), false);
            g.setGradientFill(fade);
            g.fillRect(rightEdge, graph.getY(),
                       graph.getRight() - rightEdge, graph.getHeight());
        }
    }

    g.setColour(juce::Colour(0xffc4cad2));
    g.setFont(juce::FontOptions(9.f).withStyle("Bold"));
    g.drawText(
        "OXFORD-STYLE DYNAMIC EQ · OFFSET / TARGET / LIVE GAIN · 20 Hz — 20 kHz",
        (int)graph.getX() + 12,
        (int)graph.getY() + 9,
        470, 14, juce::Justification::left);

    // Mirror the per-band BYPASS state in the upper frequency zone.
    for (int band = 0; band < 4; ++band)
    {
        const bool bypassed =
            parameterValue("OTT_BAND_BYPASS" + juce::String(band + 1)) > 0.5f;
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
    juce::ColourGradient bg(juce::Colour(0xff292d33), r.getX(), r.getY(),
                            juce::Colour(0xff12151a), r.getRight(), r.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(r, 8.f);

    g.setColour(juce::Colours::black.withAlpha(.94f));
    g.drawRoundedRectangle(r, 8.f, 1.f);

    accent = uiColour(accent);
    if (title == "DE-ESSER"
        && (parameterValue("DEESS_BYPASS") > 0.5f
            || parameterValue("DEESS_INTENSITY") <= 0.0001f))
        accent = juce::Colour(0xff747b84);
    g.setColour(accent.withAlpha(.8f));
    g.fillRoundedRectangle(r.getX(), r.getY(), 4.f, r.getHeight(), 2.f);

    const bool monitorCard = title == "BYPASS";
    const int titleY = monitorCard ? 36 : 8;
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(12.f).withStyle("Bold"));
    if (monitorCard)
        g.drawText(title, (int) r.getX(), (int) r.getY() + titleY,
                   (int) r.getWidth(), 17, juce::Justification::centred);
    else
        g.drawText(title, (int) r.getX() + 13, (int) r.getY() + titleY,
                   100, 17, juce::Justification::left);

    if (!monitorCard)
    {
        g.setColour(juce::Colour(0xff8b929c));
        g.setFont(juce::FontOptions(7.5f));
        g.drawText(subtitle, (int) r.getX() + 13, (int) r.getY() + 25,
                   (int) r.getWidth() - 80, 12, juce::Justification::left);
    }

    g.setColour(accent.withAlpha(.28f));
    g.drawRoundedRectangle(r.reduced(2.f), 6.f, 1.f);

}

void VVChainAudioProcessorEditor::drawPanel(
    juce::Graphics& g, juce::Rectangle<float> r,
    const juce::String& title, const juce::String& subtitle, juce::Colour accent)
{
    juce::ColourGradient bg(juce::Colour(0xff24282e), r.getX(), r.getY(),
                            juce::Colour(0xff12151a), r.getX(), r.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(r, 8.f);
    g.setColour(juce::Colours::black.withAlpha(.92f));
    g.drawRoundedRectangle(r, 8.f, 1.f);
    g.setColour(accent.withAlpha(.72f));
    g.fillRoundedRectangle(r.getX(), r.getY(), 4.f, r.getHeight(), 2.f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(10.f).withStyle("Bold"));
    g.drawText(title, (int) r.getX() + 12, (int) r.getY() + 7, 280, 16,
               juce::Justification::left);
    g.setColour(juce::Colour(0xff7f8791));
    g.setFont(juce::FontOptions(7.5f));
    g.drawText(subtitle, (int) r.getX() + 12, (int) r.getY() + 23,
               (int) r.getWidth() - 20, 12, juce::Justification::left);
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

    const std::array<juce::Colour, 5> moduleColours
    {{
        juce::Colour(0xff38bdf8),
        juce::Colour(0xfffacc15),
        juce::Colour(0xff60a5fa),
        juce::Colour(0xfff472b6),
        juce::Colour(0xff67d3aa)
    }};

    for (int i = 0; i < 5; ++i)
        if (bypassButtons[(size_t) i])
            bypassButtons[(size_t) i]->setColour(
                juce::ToggleButton::tickColourId,
                uiColour(moduleColours[(size_t) i]));

    if (deessBypassButton)
        deessBypassButton->setColour(
            juce::ToggleButton::tickColourId,
            uiColour(juce::Colour(0xffdfe7ef)));

    if (masterBypassButton)
        masterBypassButton->setColour(
            juce::ToggleButton::tickColourId, uiColour(juce::Colour(0xffdfe7ef)));

    for (auto& b : ottBandBypassButtons)
        if (b)
            b->setColour(juce::ToggleButton::tickColourId,
                         uiColour(juce::Colour(0xfffacc15)));

    for (auto& b : atypeBandBypassButtons)
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

    if (ottClipper)
        ottClipper->setColour(
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

void VVChainAudioProcessorEditor::timerCallback()
{
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

    // The round POWER control is the same global MASTER_BYPASS as the
    // upper-right BYPASS control.
    const bool masterBypassed = parameterValue("MASTER_BYPASS") > 0.5f;
    if (deessBypassButton)
        deessBypassButton->setToggleState(masterBypassed,
                                          juce::dontSendNotification);

    const bool deessBypassed = parameterValue("DEESS_BYPASS") > 0.5f;
    if (bypassButtons[4])
        bypassButtons[4]->setToggleState(deessBypassed,
                                         juce::dontSendNotification);
    if (deessLocalBypassButton)
    {
        deessLocalBypassButton->setToggleState(
            deessBypassed, juce::dontSendNotification);
        deessLocalBypassButton->setAlpha(
            parameterValue("DEESS_INTENSITY") <= 0.0001f ? 0.42f : 1.0f);
    }

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const bool eqMuted =
            parameterValue("EQ_BYPASS") > 0.5f;
        const bool ottMuted =
            parameterValue("OTT_BYPASS") > 0.5f
            || parameterValue("OTT_BAND_BYPASS" + n) > 0.5f
            || parameterValue("OTT_DEGREE" + n) <= 0.0001f;
        const bool analogMuted =
            parameterValue("EQ_COLOR_GLOBAL_BYPASS") > 0.5f
            || parameterValue("EQ_COLOR_BYPASS" + n) > 0.5f
            || parameterValue("EQ_COLOR" + n) <= 0.0001f;
        const bool tapeMuted =
            parameterValue("ATYPE_BYPASS") > 0.5f
            || parameterValue("ATYPE_BAND_BYPASS" + n) > 0.5f
            || parameterValue("ATYPE_DEGREE" + n) <= 0.0001f;
        const auto setKnobAlpha = [this](const juce::String& id, bool muted)
        {
            if (auto* knob = findKnob(id))
            {
                const float alpha = muted ? 0.42f : 1.0f;
                knob->slider->setAlpha(alpha);
                knob->label->setAlpha(alpha);
            }
        };
        setKnobAlpha("EQ" + n + "_GAIN", eqMuted);
        setKnobAlpha("EQ" + n + "_FREQ", eqMuted);
        setKnobAlpha("EQ" + n + "_Q", eqMuted);
        setKnobAlpha("DYN_DYNAMICS" + n, eqMuted);
        setKnobAlpha("DYN_ATTACK" + n, eqMuted);
        setKnobAlpha("DYN_RELEASE" + n, eqMuted);
        if (dynDetectSliders[(size_t)b])
            dynDetectSliders[(size_t)b]->setAlpha(eqMuted ? 0.42f : 1.0f);
        if (dynTriggerButtons[(size_t)b])
            dynTriggerButtons[(size_t)b]->setAlpha(eqMuted ? 0.42f : 1.0f);
        setKnobAlpha("OTT_DEGREE" + n, ottMuted);
        setKnobAlpha("OTT_COMP_A" + n, ottMuted);
        setKnobAlpha("OTT_COMP_R" + n, ottMuted);
        setKnobAlpha("EQ_COLOR_B" + n, analogMuted);
        setKnobAlpha("ATYPE_DEGREE" + n, tapeMuted);
        if (analogModeButtons[(size_t)b])
            analogModeButtons[(size_t)b]->setAlpha(
                analogMuted ? 0.42f : 1.0f);
        if (analogX2Buttons[(size_t)b])
            analogX2Buttons[(size_t)b]->setAlpha(
                analogMuted ? 0.42f : 1.0f);
    }

    const bool deessMuted =
        parameterValue("DEESS_BYPASS") > 0.5f
        || parameterValue("DEESS_INTENSITY") <= 0.0001f;
    for (const auto& id : { juce::String("DEESS_FREQ"),
                            juce::String("DEESS_INTENSITY"),
                            juce::String("DEESS_MODE") })
        if (auto* knob = findKnob(id))
        {
            knob->slider->setAlpha(deessMuted ? 0.42f : 1.0f);
            knob->label->setAlpha(deessMuted ? 0.42f : 1.0f);
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
    g.fillAll(juce::Colour(0xff080a0d));

    juce::ColourGradient top(juce::Colour(0xff343840), 0.f, 0.f,
                             juce::Colour(0xff17191e), 0.f, 70.f, false);
    g.setGradientFill(top);
    g.fillRect(0, 0, getWidth(), 70);

    g.setColour(juce::Colours::black.withAlpha(.86f));
    g.fillRect(0, 68, getWidth(), 2);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(22.f).withStyle("Bold"));
    g.drawText("VVCHAIN", 18, 8, 240, 27, juce::Justification::left);

    g.setColour(juce::Colour(0xff8b949f));
    g.setFont(juce::FontOptions(8.f));
    g.drawText("4-BAND DYNAMIC EQ · OTT · ANALOG · TAPE-A · DE-ESSER", 20, 37, 430, 13,
               juce::Justification::left);
    g.setColour(juce::Colour(0xff7f8893));
    g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
    g.drawText("VVCHAIN v1.0.33 · TYPE-A SHARED XOVER + ANALOG 4-BAND + DEESS PRESETS",
               510, 38, 700, 12, juce::Justification::left);

    const auto graph = eqGraphBounds();
    drawEqGraph(g, graph);

    const int cardY = 404;
    const int gap = 8;
    const int left = 18;
    const int unitW = (getWidth() - left * 2 - gap * 5) / 5;
    const int cardW = unitW;
    const int cardH = 510;

    for (int b = 0; b < 4; ++b)
    {
        const int x = left + b * (cardW + gap);
        drawCard(g,
                 { (float) x, (float) cardY, (float) cardW, (float) cardH },
                 uiColour(kBandColours[(size_t) b]),
                 "BAND " + juce::String(b + 1),
                 "OTT · ANALOG · TAPE-A");
    }

    {
        const int x = left + 4 * (cardW + gap);
        const int halfW = (cardW - gap) / 2;
        drawCard(g,
                 { (float) x, (float) cardY, (float) halfW, (float) cardH },
                 uiColour(juce::Colour(0xff67d3aa)),
                 "DE-ESSER",
                 "PRECISION SIBILANCE · 6–18 kHz");

        drawCard(g,
                 { (float) x + halfW + gap, (float) cardY,
                   (float) halfW, (float) cardH },
                 uiColour(juce::Colour(0xffe5e7eb)),
                 "BYPASS",
                 "GLOBAL BYPASS · DELTA · MIX / OUT");
    }

    // Floating OTT Advanced popup: it overlays the controls and never changes band height.
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
        g.drawText("OTT ADVANCED · BAND " + juce::String(expandedBand + 1),
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

    if (ottClipper)
        ottClipper->setVisible(expandedBand >= 0);
    if (closeAdvanced)
        closeAdvanced->setVisible(expandedBand >= 0);

    resized();
    repaint();
}

void VVChainAudioProcessorEditor::resized()
{
    const int w = getWidth();

    const int cardY = 404;
    const int gap = 8;
    const int left = 18;
    const int unitW = (w - left * 2 - gap * 5) / 5;
    const int cardW = unitW;

    const std::array<int, 5> moduleWidths { 50, 52, 64, 66, 62 };
    constexpr int topGap = 5;
    constexpr int masterW = 78;
    int total = masterW;
    for (auto mw : moduleWidths)
        total += topGap + mw;
    constexpr int soloModeW = 68;
    total += topGap + soloModeW;

    const int topX = w - 18 - total;
    constexpr int topY = 17;
    if (masterBypassButton)
        masterBypassButton->setBounds(topX, topY, masterW, 25);
    if (soloModeButton)
        soloModeButton->setBounds(topX + masterW + topGap, topY, soloModeW, 25);

    int xTop = topX + masterW + topGap + soloModeW + topGap;
    for (int i = 0; i < 5; ++i)
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
        const int innerTop = cardY + 56;
        const int innerW = cardW - 16;
        const int cellGap = 6;
        const int cellW = (innerW - cellGap * 2) / 3;
        const int rowH = 70;
        const int knobH = 62;

        const auto cell = [&](int row, int col)
        {
            return juce::Rectangle<int>(
                innerX + col * (cellW + cellGap),
                innerTop + row * rowH,
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

        // ROW 3 — detection mode buttons only; no knobs underneath them.
        const int modeY = innerTop + rowH * 2 + 7;
        const int halfW = (innerW - 8) / 2;

        if (dynDetectSliders[(size_t) b])
        {
            constexpr int detectW = 60;
            const int dynamicsCentreX = cell(1, 0).getCentreX();
            dynDetectSliders[(size_t) b]->setBounds(
                dynamicsCentreX - detectW / 2,
                cell(1, 0).getY() - 12,
                detectW, 12);
        }

        if (dynTriggerButtons[(size_t) b])
        {
            dynTriggerButtons[(size_t) b]->setBounds(
                innerX + halfW + 8, modeY, halfW, 26);
            dynTriggerButtons[(size_t) b]->setButtonText(
                parameterValue("DYN_TRIGGER_BELOW" + n) > 0.5f
                    ? "BELOW" : "ABOVE");
        }

        // ROW 4 — OTT
        placeKnob("OTT_DEGREE" + n, cell(3, 0));
        placeKnob("OTT_COMP_A" + n, cell(3, 1));
        placeKnob("OTT_COMP_R" + n, cell(3, 2));

        // ROW 5 — colour / Type-A
        placeKnob("EQ_COLOR_B" + n, cell(4, 0));
        placeKnob("ATYPE_DEGREE" + n, cell(4, 1));

        if (analogModeButtons[(size_t) b])
            analogModeButtons[(size_t) b]->setBounds(
                cell(4, 2).getX() + 18,
                cell(4, 2).getY() + 5,
                36, 12);

        if (ottBandBypassButtons[(size_t) b])
            if (auto* knob = findKnob("OTT_DEGREE" + n))
            {
                const auto r = knob->slider->getBounds();
                ottBandBypassButtons[(size_t) b]->setBounds(
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
                analogBypassButtons[(size_t) b]->setBounds(
                    r.getRight() - 12, r.getY() - 6, 12, 12);
            }

        if (atypeBandBypassButtons[(size_t) b])
            if (auto* knob = findKnob("ATYPE_DEGREE" + n))
            {
                const auto r = knob->slider->getBounds();
                atypeBandBypassButtons[(size_t) b]->setBounds(
                    r.getRight() - 14, r.getY() - 10, 14, 14);
            }
    }

    // Fifth unit is split into:
    //   left  = DE-ESSER, four knobs in one vertical column
    //   right = global BYPASS / DELTA / MIX / OUT
    {
        const int x = left + 4 * (cardW + gap);
        const int halfW = (cardW - gap) / 2;
        const int deX = x;
        const int monitorX = x + halfW + gap;

        const int deInnerX = deX + 8;
        const int deInnerW = halfW - 16;
        const int startY = cardY + 60;
        const int deKnobH = 126;
        const int deKnobGap = 6;

        placeKnob("DEESS_FREQ",
                  { deInnerX, startY,
                    deInnerW, deKnobH });
        placeKnob("DEESS_INTENSITY",
                  { deInnerX, startY + deKnobH + deKnobGap,
                    deInnerW, deKnobH });
        if (deessModeSwitch)
            deessModeSwitch->setBounds(
                deInnerX,
                startY + (deKnobH + deKnobGap) * 2 + 22,
                deInnerW, 72);

        // MIX / OUT are intentionally removed from the DE-ESSER column and
        // live in the right-side global-BYPASS block below DELTA.
        if (deessBypassButton)
            deessBypassButton->setBounds(
                monitorX + halfW / 2 - 25,
                cardY + 58, 50, 50);
        
        if (deessLocalBypassButton)
            if (auto* knob = findKnob("DEESS_INTENSITY"))
            {
                const auto r = knob->slider->getBounds();
                deessLocalBypassButton->setBounds(
                    r.getRight() - 14, r.getY() - 10, 14, 14);
            }

        if (deltaMonitorButton)
            deltaMonitorButton->setBounds(
                monitorX + 9, cardY + 131, halfW - 18, 28);

        placeKnob("DRY_WET",
                  { monitorX + 8, cardY + 223,
                    halfW - 16, 102 });
        placeKnob("OUTPUT_LEVEL",
                  { monitorX + 8, cardY + 336,
                    halfW - 16, 102 });
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
            "OTT_LIFT_T", "OTT_LIFT_A", "OTT_LIFT_R", "OTT_LIFT_M",
            "OTT_COMP_T", "OTT_COMP_M", "OTT_LEVEL"
        }};
        const std::array<juce::String, 8> sharedAdv
        {{
            "OTT_X1", "OTT_X2", "OTT_X3", "XOVER_OVERLAP",
            "OTT_INPUT", "OTT_GATE", "OTT_MIX", "OTT_OUTPUT"
        }};

        for (int i = 0; i < 7; ++i)
            placeKnob(bandAdv[(size_t)i] + n, p(i));
        for (int i = 0; i < 8; ++i)
            placeKnob(sharedAdv[(size_t)i], p(i + 8));

        if (ottClipper)
            ottClipper->setBounds(innerX, popupY + popupH - 34, 82, 24);
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

    const auto n = juce::String(band + 1);
    const float frequency = parameterValue("EQ" + n + "_FREQ");
    const float q = juce::jmax(0.1f, parameterValue("EQ" + n + "_Q"));

    const juce::String signedDb =
        juce::String(displayedGain >= 0.0f ? "+" : "")
        + juce::String(displayedGain, 1) + " dB";

    // Exactly two compact lines. No TARGET / OFFSET / DYN % / AUTO THR.
    const juce::String line1 =
        juce::String(dynamicReadout ? "DYN EQ" : "EQ")
        + "  GAIN " + signedDb;
    const juce::String line2 =
        "FREQ " + formatGraphFrequency(frequency)
        + "  Q " + juce::String(q, 2);

    floatingValueBox.updateInfo(
        line1, line2, position.toInt(), getLocalBounds());
}

void VVChainAudioProcessorEditor::updateFloatingValueBoxAt(
    juce::Point<float> position)
{
    const auto graph = eqGraphBounds();
    if (!graph.contains(position))
    {
        floatingValueBox.hideInstantly();
        return;
    }

    // v1.0.33: explicit hit priority.
    // 1) Static EQ point always wins when the pointer is actually on it.
    // 2) Only the Dynamic target or its dedicated arrow can produce DYN EQ.
    // The live gain marker is visual only and never steals the value box.
    constexpr float staticHitRadius = 9.0f;
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
        if (std::abs(dynamics) > 0.01f)
        {
            const float d = position.getDistanceFrom({ x, targetY });
            if (d < bestDistance)
            {
                bestDistance = d;
                bestBand = b;
            }
        }

        const float handleX = juce::jlimit(
            graph.getX() + 18.f, graph.getRight() - 12.f, x + 44.f);
        if (std::abs(position.x - handleX) <= handleHitX
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

void VVChainAudioProcessorEditor::mouseMove(
    const juce::MouseEvent& event)
{
    const auto graph = eqGraphBounds();

    if (!graph.contains(event.position))
    {
        floatingValueBox.hideInstantly();
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
        graphFrequencyToX(graph, parameterValue("OTT_X1")),
        graphFrequencyToX(graph, parameterValue("OTT_X2")),
        graphFrequencyToX(graph, parameterValue("OTT_X3"))
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
    const juce::MouseEvent&)
{
    hoverDynamicBand = -1;
    hoverXover = -1;
    floatingValueBox.hideInstantly();
    repaint();
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

    // Right-click auditions the EQ point itself. While the right button is
    // held, dragging edits Frequency/Gain and the audition filter follows it.
    if (event.mods.isRightButtonDown())
    {
        float bestDistance = 24.0f;
        band = -1;
        for (int b = 0; b < 4; ++b)
        {
            const auto n = juce::String(b + 1);
            const float x = graphFrequencyToX(
                graph, parameterValue("EQ" + n + "_FREQ"));
            const float y = eqDbToY(
                graph, parameterValue("EQ" + n + "_GAIN"));
            const float dStatic = pos.getDistanceFrom({ x, y });
            const float dDynamic = pos.getDistanceFrom(dynamicTargetPoint(b));
            const float d = juce::jmin(dStatic, dDynamic);
            if (d < bestDistance)
            {
                bestDistance = d;
                band = b;
            }
        }

        if (band >= 0)
        {
            const auto n = juce::String(band + 1);
            rightSoloBand = band;
            expandedDynamicBand = -1;
            dragDynamicMsBand = -1;
            dragBand = -1;
            dragOffsetBand = -1;
            dynamicGainDragStartY = pos.y;
            dynamicGainDragStartOffset =
                parameterValue("EQ" + n + "_GAIN");
            setParameter("GRAPH_SOLO_FREQ", parameterValue("EQ" + n + "_FREQ"));
            setParameter("GRAPH_SOLO_Q", parameterValue("EQ" + n + "_Q"));
            setParameter("GRAPH_SOLO_ACTIVE", 1.f);
            if (auto* pFreq = audioProcessor.apvts.getParameter("EQ" + n + "_FREQ"))
                pFreq->beginChangeGesture();
            if (auto* pGain = audioProcessor.apvts.getParameter("EQ" + n + "_GAIN"))
                pGain->beginChangeGesture();
            juce::StringArray graphIds;
            graphIds.add("EQ" + n + "_FREQ");
            graphIds.add("EQ" + n + "_GAIN");
            setGraphControlState(graphIds, false);
            showGraphDragHint = false;
            graphDragHint.clear();
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
                         x + 44.f);
        const float targetY =
            eqDbToY(graph, dynamicEffectiveTargetGain(b));
        const auto handleRect =
            juce::Rectangle<float>(handleX - 3.f, targetY - 7.f,
                                   6.f, 14.f);

        if (event.mods.isLeftButtonDown() && handleRect.contains(pos))
        {
            dragDynamicHandleBand = b;
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

    // Direct DYNAMICS target control takes priority over the static center node.
    // Dynamic Gain point:
    // - click/drag the coloured Target handle;
    // - vertical = Dynamic Gain only;
    // - frequency stays locked while Gain is dragged;
    // - Threshold is never edited independently.
    int staticPriorityBand = -1;
    float staticPriorityDistance = 9.0f;
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

        juce::StringArray graphIds;
        graphIds.add("DYN_DYNAMICS" + n);
        graphIds.add("EQ" + n + "_FREQ");
        setGraphControlState(graphIds, false);

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

            juce::StringArray graphIds;
            graphIds.add("EQ" + n + "_FREQ");
            graphIds.add("EQ" + n + "_GAIN");
            setGraphControlState(graphIds, false);

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
            graph, parameterValue("OTT_X1")),
        graphFrequencyToX(
            graph, parameterValue("OTT_X2")),
        graphFrequencyToX(
            graph, parameterValue("OTT_X3"))
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
            dragXover == 0 ? "OTT_X1"
            : dragXover == 1 ? "OTT_X2"
                             : "OTT_X3";
        if (auto* parameter = audioProcessor.apvts.getParameter(xoverId))
            parameter->beginChangeGesture();

        juce::StringArray xoverGraphIds;
        xoverGraphIds.add(xoverId);
        setGraphControlState(xoverGraphIds, false);
        showGraphDragHint = true;
        graphDragHintPosition = pos;

        const auto hz = parameterValue(
            dragXover == 0 ? "OTT_X1"
            : dragXover == 1 ? "OTT_X2"
                             : "OTT_X3");

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

    if (rightSoloBand >= 0 && event.mods.isRightButtonDown())
    {
        const auto n = juce::String(rightSoloBand + 1);
        const float x = juce::jlimit(graph.getX(), graph.getRight(), event.position.x);
        const float hz = graphXToFrequency(graph, x);
        const float gain =
            eqYToDb(graph, event.position.y);
        setGraphControlMoving(true);
        setParameter("EQ" + n + "_FREQ", hz);
        setParameter("EQ" + n + "_GAIN", gain);
        setParameter("GRAPH_SOLO_FREQ", hz);
        setParameter("GRAPH_SOLO_Q", parameterValue("EQ" + n + "_Q"));
        setParameter("GRAPH_SOLO_ACTIVE", 1.f);
        if (auto* knob = findKnob("EQ" + n + "_FREQ"))
            knob->slider->setValue(hz, juce::dontSendNotification);
        if (auto* knob = findKnob("EQ" + n + "_GAIN"))
            knob->slider->setValue(gain, juce::dontSendNotification);
        showGraphDragHint = false;
        graphDragHint.clear();
        showFloatingValueBoxForBand(
            rightSoloBand, false, gain, event.position);
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

        setGraphControlMoving(true);
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

        setGraphControlMoving(true);
        setParameter("EQ" + n + "_FREQ", hz);
        setParameter("EQ" + n + "_GAIN", offset);

        if (auto* freqKnob = findKnob("EQ" + n + "_FREQ"))
            freqKnob->slider->setValue(
                hz, juce::dontSendNotification);
        if (auto* gainKnob = findKnob("EQ" + n + "_GAIN"))
            gainKnob->slider->setValue(
                offset, juce::dontSendNotification);

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
            dragXover == 0 ? "OTT_X1"
            : dragXover == 1 ? "OTT_X2"
                              : "OTT_X3";

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
    // Horizontal = Frequency; vertical = DYNAMICS.
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

        setGraphControlMoving(true);
        setParameter("EQ" + n + "_FREQ", hz);
        setParameter("DYN_DYNAMICS" + n, dynamics);

        if (auto* freqKnob = findKnob("EQ" + n + "_FREQ"))
            freqKnob->slider->setValue(
                parameterValue("EQ" + n + "_FREQ"),
                juce::sendNotificationSync);

        // APVTS is the source of truth; also refresh the visible knob
        // synchronously so graph drag and lower DYNAMICS never visually diverge.
        if (auto* dynamicsKnob = findKnob("DYN_DYNAMICS" + n))
            dynamicsKnob->slider->setValue(
                parameterValue("DYN_DYNAMICS" + n),
                juce::sendNotificationSync);

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
    if (rightSoloBand >= 0)
    {
        const auto n = juce::String(rightSoloBand + 1);
        if (auto* pFreq = audioProcessor.apvts.getParameter("EQ" + n + "_FREQ"))
            pFreq->endChangeGesture();
        if (auto* pGain = audioProcessor.apvts.getParameter("EQ" + n + "_GAIN"))
            pGain->endChangeGesture();
        setParameter("GRAPH_SOLO_ACTIVE", 0.f);
        rightSoloBand = -1;
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
            dragXover == 0 ? "OTT_X1"
            : dragXover == 1 ? "OTT_X2"
                             : "OTT_X3";
        if (auto* parameter = audioProcessor.apvts.getParameter(xoverId))
            parameter->endChangeGesture();
    }

    if (dragDynamicMsBand >= 0)
    {
        if (auto* parameter = audioProcessor.apvts.getParameter(
                "DYN_MS" + juce::String(dragDynamicMsBand + 1)))
            parameter->endChangeGesture();
    }

    clearGraphControlState();

    dragBand = -1;
    dragOffsetBand = -1;
    graphEqDragAxis = GraphEqDragAxis::Undetermined;
    dragXover = -1;
    dragOverlapXover = -1;
    dragDynamicMsBand = -1;
    dragDynamicHandleBand = -1;
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
        int band = rightSoloBand;
        float bestDistance = 24.0f;
        if (band < 0)
        {
            for (int b = 0; b < 4; ++b)
            {
                const auto n = juce::String(b + 1);
                const float x = graphFrequencyToX(
                    graph, parameterValue("EQ" + n + "_FREQ"));
                const float y = eqDbToY(
                    graph, parameterValue("EQ" + n + "_GAIN"));
                const float d = event.position.getDistanceFrom({ x, y });
                if (d < bestDistance)
                {
                    bestDistance = d;
                    band = b;
                }
            }
        }

        if (band >= 0)
        {
            const auto n = juce::String(band + 1);
            const float q = juce::jmax(0.1f, parameterValue("EQ" + n + "_Q"));
            const float nextQ =
                qFromWheel(q, wheel.deltaY, event.mods.isShiftDown());
            setParameter("EQ" + n + "_Q", nextQ);
            setParameter("GRAPH_SOLO_FREQ", parameterValue("EQ" + n + "_FREQ"));
            setParameter("GRAPH_SOLO_Q", nextQ);
            setParameter("GRAPH_SOLO_ACTIVE", 1.f);
            if (rightSoloBand < 0)
                rightSoloBand = band;
            showGraphDragHint = false;
            graphDragHint.clear();
            showFloatingValueBoxForBand(
                band, false, parameterValue("EQ" + n + "_GAIN"),
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
            graphFrequencyToX(graph, parameterValue("OTT_X1")),
            graphFrequencyToX(graph, parameterValue("OTT_X2")),
            graphFrequencyToX(graph, parameterValue("OTT_X3"))
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
    const float q =
        juce::jmax(
            0.1f,
            parameterValue(
                "EQ" + n + "_Q"));

    const float nextQ =
        qFromWheel(q, wheel.deltaY, event.mods.isShiftDown());

    juce::StringArray qGraphIds;
    qGraphIds.add("EQ" + n + "_Q");
    setGraphControlState(qGraphIds, false);
    showGraphDragHint = false;
    graphDragHint.clear();
    setParameter(
        "EQ" + n + "_Q", nextQ);

    if (auto* knob =
            findKnob("EQ" + n + "_Q"))
        knob->slider->setValue(
            nextQ,
            juce::dontSendNotification);

    showFloatingValueBoxForBand(
        band,
        dynamicWheelReadout,
        dynamicWheelReadout
            ? dynamicEffectiveTargetGain(band)
            : parameterValue("EQ" + n + "_GAIN"),
        event.position);
    repaint();
}
