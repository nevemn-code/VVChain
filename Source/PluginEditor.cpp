#include "PluginEditor.h"

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
    : AudioProcessorEditor(&p), audioProcessor(p)
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
        addKnob("EQ" + n + "_FREQ", "FREQ", 20, 20000, 1,
                parameterValue("EQ" + n + "_FREQ"), " Hz", b, 0, c);
        addKnob("EQ" + n + "_GAIN", "GAIN", -24, 24, .1,
                parameterValue("EQ" + n + "_GAIN"), " dB", b, 1, c);
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

        dynDetectButtons[(size_t) b] =
            std::make_unique<juce::ToggleButton>("PEAK");
        dynDetectButtons[(size_t) b]->setComponentID("DYN_MODE");
        dynDetectButtons[(size_t) b]->setLookAndFeel(&metalLook);
        dynDetectButtons[(size_t) b]->setColour(
            juce::ToggleButton::tickColourId, c);
        dynDetectButtons[(size_t) b]->setTooltip(
            "DETECT：PEAK / ONSETS");
        dynDetectButtons[(size_t) b]->onClick = [this, b]
        {
            if (auto* parameter = audioProcessor.apvts.getParameter(
                    "DYN_DETECT_ONSETS" + juce::String(b + 1)))
                parameter->setValueNotifyingHost(
                    parameter->convertTo0to1(
                        dynDetectButtons[(size_t)b]->getToggleState()
                            ? 1.f : 0.f));
            repaint();
        };
        dynDetectAttachments[(size_t) b] =
            std::make_unique<BoolAttachment>(
                audioProcessor.apvts,
                "DYN_DETECT_ONSETS" + n,
                *dynDetectButtons[(size_t) b]);
        addAndMakeVisible(*dynDetectButtons[(size_t) b]);

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
        addKnob("EQ_COLOR_B" + n, "ANALOG COLOR", 0, 100, .1,
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

    deessBypassButton = std::make_unique<juce::ToggleButton>();
    deessBypassButton->setLookAndFeel(&metalLook);
    deessBypassButton->setButtonText("");
    deessBypassButton->setColour(
        juce::ToggleButton::tickColourId, juce::Colour(0xff67d3aa));
    deessBypassButton->setTooltip("DE-ESSER：亮 = 啟用；按下 = BYPASS");
    deessBypassAttachment = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, "DEESS_BYPASS", *deessBypassButton);
    addAndMakeVisible(*deessBypassButton);

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
}

VVChainAudioProcessorEditor::~VVChainAudioProcessorEditor()
{
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
    for (auto& b : dynDetectButtons)
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
    return graph.getBottom() - graph.getHeight()
        * juce::jlimit(0.f, 1.f, (db + 18.f) / 36.f);
}

float VVChainAudioProcessorEditor::dynamicThresholdFromDynamics(float dynamics) const
{
    const float signedDynamics =
        juce::jlimit(-100.f, 100.f, dynamics);
    const float amount =
        std::pow(std::abs(signedDynamics) * 0.01f, 0.65f);

    // Single user-facing DYNAMICS macro:
    // negative = compression; threshold moves lower;
    // positive = expansion; threshold moves higher.
    return juce::jlimit(
        -60.f, 0.f,
        -12.f
            + (signedDynamics < 0.f ? -12.f : 12.f) * amount);
}

float VVChainAudioProcessorEditor::dynamicEffectiveTargetGain(int band) const
{
    if (band < 0 || band >= 4)
        return 0.f;

    const auto n = juce::String(band + 1);
    const float offset =
        juce::jlimit(-24.f, 24.f,
                     parameterValue("EQ" + n + "_GAIN"));
    const float storedTarget =
        juce::jlimit(-24.f, 24.f,
                     parameterValue("DYN_TARGET" + n));
    const float span =
        std::abs(storedTarget - offset);
    const float dynamics =
        juce::jlimit(-100.f, 100.f,
                     parameterValue("DYN_DYNAMICS" + n));

    // DYNAMICS is the direction + depth macro:
    //   negative = compression direction
    //   positive = expansion direction
    // Target stores the available gain span; the visual endpoint follows
    // the current DYNAMICS direction so the graph always matches the knob.
    const float direction = dynamics < 0.f ? -1.f : 1.f;

    return juce::jlimit(-24.f, 24.f,
                        offset + direction * span);
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
    constexpr float hitRadius = 22.f;
    band = -1;
    float best = hitRadius;

    // Only the coloured Target handle is editable as Dynamic Gain.
    // The white Live point is a meter; the Offset point is the static EQ Gain.
    for (int b = 0; b < 4; ++b)
    {
        const float d =
            p.getDistanceFrom(dynamicTargetPoint(b));

        if (d < best)
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

    for (int i = 0; i <= 8; ++i)
    {
        const float db = 18.f - i * 4.5f;
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

    // X1/X2/X3 live in a dedicated top strip so they never cover EQ nodes.
    const float spread = 16.f + overlap * .52f;

    for (int i = 0; i < 3; ++i)
    {
        const float x = xovers[i];
        const float hz = parameterValue(
            i == 0 ? "OTT_X1" : i == 1 ? "OTT_X2" : "OTT_X3");

        g.setColour(
            uiColour(juce::Colour(0xffffd84d)).withAlpha(.92f));
        g.drawVerticalLine(
            (int)x, graph.getY() + 20.f, graph.getBottom() - 18.f);

        const float l = juce::jmax(
            graph.getX() + 4.f, x - spread);
        const float r = juce::jmin(
            graph.getRight() - 4.f, x + spread);

        juce::Path curve;
        curve.startNewSubPath(
            l, graph.getCentreY() + 20.f);
        curve.cubicTo(
            l + spread * .35f, graph.getCentreY() + 20.f,
            x - spread * .20f, graph.getCentreY() - 12.f,
            x, graph.getCentreY() - 2.f);
        curve.cubicTo(
            x + spread * .20f, graph.getCentreY() - 12.f,
            r - spread * .35f, graph.getCentreY() + 20.f,
            r, graph.getCentreY() + 20.f);

        g.setColour(
            uiColour(juce::Colour(0xffffdf67)).withAlpha(.60f));
        g.strokePath(
            curve, juce::PathStrokeType(1.1f));

        g.setColour(juce::Colours::black.withAlpha(.70f));
        g.fillRoundedRectangle(
            x - 47.f, graph.getY() + 2.f, 94.f, 15.f, 3.f);
        g.setColour(
            uiColour(juce::Colour(0xffffdf67)));
        g.setFont(
            juce::FontOptions(7.5f).withStyle("Bold"));
        g.drawText(
            "X" + juce::String(i + 1) + "  "
                + formatGraphFrequency(hz),
            (int)x - 45, (int)graph.getY() + 3, 90, 11,
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

        for (int i = 0; i <= 220; ++i)
        {
            const float hz =
                invLogMap(i / 220.f, 20.f, 20000.f);
            const float xx =
                std::log(
                    std::max(hz, 20.f) /
                    std::max(f0, 20.f));

            const float offsetQ =
                qForGain(baseQ, offset);
            const float targetQ =
                qForGain(baseQ, target);

            const float offsetWidth =
                juce::jmax(.02f, 1.f / (offsetQ * 1.8f));
            const float targetWidth =
                juce::jmax(.02f, 1.f / (targetQ * 1.8f));

            const float offsetShape =
                std::exp(
                    -(xx * xx) /
                    (2.f * offsetWidth * offsetWidth));
            const float targetShape =
                std::exp(
                    -(xx * xx) /
                    (2.f * targetWidth * targetWidth));

            const float offsetDb =
                offset * offsetShape;
            const float targetDb =
                target * targetShape;
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
            const float xx =
                std::log(
                    std::max(hz, 20.f) /
                    std::max(f0, 20.f));
            const float targetQ =
                qForGain(baseQ, target);
            const float width =
                juce::jmax(.02f, 1.f / (targetQ * 1.8f));
            const float shape =
                std::exp(
                    -(xx * xx) /
                    (2.f * width * width));
            const float y =
                eqDbToY(
                    graph,
                    target * shape);
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

        // Dynamic Target handle: the sole editable Dynamic Gain point.
        // Static EQ Gain is the Offset handle; Live is meter-only.
        g.setColour(
            juce::Colours::black.withAlpha(.92f));
        g.fillEllipse(
            x - 9.f, targetY - 9.f, 18.f, 18.f);
        g.setColour(c.withAlpha(.98f));
        g.drawEllipse(
            x - 8.f, targetY - 8.f, 16.f, 16.f, 2.0f);

        // Live gain point = the actual dynamic state.
        g.setColour(juce::Colours::white);
        g.fillEllipse(
            x - 6.f, liveY - 6.f, 12.f, 12.f);

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

        const bool hovered =
            hoverDynamicBand == band;

        if (hovered)
        {
            const float dynamics =
                parameterValue("DYN_DYNAMICS" + n);
            const float threshold =
                dynamicThresholdFromDynamics(dynamics);
            const float q =
                parameterValue("EQ" + n + "_Q");

            const bool onsets =
                parameterValue(
                    "DYN_DETECT_ONSETS" + n) > .5f;
            const bool below =
                parameterValue(
                    "DYN_TRIGGER_BELOW" + n) > .5f;

            const juce::String text =
                "B" + n
                + "   " + formatGraphFrequency(f0)
                + "   Q " + juce::String(q, 2)
                + "   TARGET " + juce::String(target, 1)
                + " dB   OFFSET " + juce::String(offset, 1)
                + " dB   LIVE " + juce::String(liveGain, 1)
                + " dB   DYN " + juce::String(dynamics, 0)
                + "% "
                + (dynamics > 0.f ? "EXPAND" : dynamics < 0.f ? "COMPRESS" : "STATIC")
                + "   AUTO THR " + juce::String(threshold, 1)
                + " dB   "
                + (onsets ? "ONSETS" : "PEAK")
                + " / " + (below ? "BELOW" : "ABOVE");

            const float boxW =
                juce::jmin(510.f, graph.getWidth() - 12.f);
            const float boxH = 30.f;
            float bx = x - boxW * .5f;
            float by = juce::jmin(
                liveY, targetY, offsetY) - 46.f;

            if (by < graph.getY() + 22.f)
                by = juce::jmax(
                    liveY, targetY, offsetY) + 20.f;

            bx = juce::jlimit(
                graph.getX() + 6.f,
                graph.getRight() - boxW - 6.f,
                bx);

            g.setColour(
                juce::Colours::black.withAlpha(.93f));
            g.fillRoundedRectangle(
                bx, by, boxW, boxH, 5.f);
            g.setColour(
                c.withAlpha(.96f));
            g.drawRoundedRectangle(
                bx, by, boxW, boxH, 5.f, 1.f);

            g.setColour(juce::Colours::white);
            g.setFont(
                juce::FontOptions(8.6f).withStyle("Bold"));
            g.drawText(
                text,
                (int)bx + 8, (int)by + 8,
                (int)boxW - 16, 14,
                juce::Justification::centred);
        }

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
                "RIGHT CLICK + WHEEL = MID / SIDE",
                (int)popup.getX() + 12,
                (int)popup.getBottom() - 18,
                (int)popup.getWidth() - 24, 12,
                juce::Justification::centred);
        }
    }

    g.setColour(juce::Colour(0xffc4cad2));
    g.setFont(juce::FontOptions(9.f).withStyle("Bold"));
    g.drawText(
        "OXFORD-STYLE DYNAMIC EQ · OFFSET / TARGET / LIVE GAIN · 20 Hz — 20 kHz",
        (int)graph.getX() + 12,
        (int)graph.getY() + 9,
        470, 14, juce::Justification::left);

    g.setFont(juce::FontOptions(8.f));
    g.setColour(juce::Colour(0xffaab0ba));
    g.drawText(
        "20", (int)graph.getX() + 6,
        (int)graph.getBottom() - 15,
        28, 12, juce::Justification::left);
    g.drawText(
        "1k", (int)graph.getCentreX() - 12,
        (int)graph.getBottom() - 15,
        24, 12, juce::Justification::centred);
    g.drawText(
        "20k", (int)graph.getRight() - 28,
        (int)graph.getBottom() - 15,
        28, 12, juce::Justification::right);

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
    g.setColour(accent.withAlpha(.8f));
    g.fillRoundedRectangle(r.getX(), r.getY(), 4.f, r.getHeight(), 2.f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(12.f).withStyle("Bold"));
    g.drawText(title, (int) r.getX() + 13, (int) r.getY() + 8, 100, 17,
               juce::Justification::left);

    g.setColour(juce::Colour(0xff8b929c));
    g.setFont(juce::FontOptions(7.5f));
    g.drawText(subtitle, (int) r.getX() + 13, (int) r.getY() + 25,
               (int) r.getWidth() - 80, 12, juce::Justification::left);

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
            juce::ToggleButton::tickColourId, uiColour(moduleColours[4]));

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

void VVChainAudioProcessorEditor::timerCallback()
{
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
        if (dynDetectButtons[(size_t) b])
            dynDetectButtons[(size_t) b]->setButtonText(
                parameterValue("DYN_DETECT_ONSETS" + n) > 0.5f
                    ? "ONSETS" : "PEAK");
        if (dynTriggerButtons[(size_t) b])
            dynTriggerButtons[(size_t) b]->setButtonText(
                parameterValue("DYN_TRIGGER_BELOW" + n) > 0.5f
                    ? "BELOW" : "ABOVE");
    }

    // Both DE-ESSER bypass controls read the exact same APVTS parameter.
    // Keep an explicit UI sync in addition to their attachments so automation
    // or host state recall cannot leave the upper/lower indicators different.
    const bool deessBypassed = parameterValue("DEESS_BYPASS") > 0.5f;
    if (deessBypassButton)
        deessBypassButton->setToggleState(deessBypassed,
                                          juce::dontSendNotification);
    if (bypassButtons[4])
        bypassButtons[4]->setToggleState(deessBypassed,
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
    if (!showGraphDragHint || graphDragHint.isEmpty())
        return;

    const auto font = juce::FontOptions(9.0f).withStyle("Bold");
    g.setFont(font);

    const float paddingX = 9.0f;
    const float paddingY = 6.0f;
    const float boxW =
        juce::jlimit(175.0f, graph.getWidth() - 12.0f,
                     (static_cast<float>(graphDragHint.length()) * g.getCurrentFont().getHeight() * 0.55f)
                         + paddingX * 2.0f);
    const float boxH = 28.0f;

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

    g.setColour(juce::Colour(0xff07090c).withAlpha(.94f));
    g.fillRoundedRectangle(bx, by, boxW, boxH, 5.0f);
    g.setColour(juce::Colours::white.withAlpha(.92f));
    g.drawRoundedRectangle(bx, by, boxW, boxH, 5.0f, 1.0f);
    g.setColour(juce::Colours::white);
    g.drawText(graphDragHint,
               juce::Rectangle<float>(bx + paddingX, by + 1.0f,
                                      boxW - paddingX * 2.0f,
                                      boxH - 2.0f).toNearestInt(),
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

    const auto graph = eqGraphBounds();
    drawEqGraph(g, graph);

    const int cardY = 404;
    const int gap = 8;
    const int left = 18;
    const int cardCount = 5;
    const int cardW = (getWidth() - left * 2 - gap * (cardCount - 1)) / cardCount;
    const int cardH = 510;

    for (int b = 0; b < 4; ++b)
    {
        const int x = left + b * (cardW + gap);
        drawCard(g,
                 { (float) x, (float) cardY, (float) cardW, (float) cardH },
                 uiColour(kBandColours[(size_t) b]),
                 "BAND " + juce::String(b + 1),
                 "DYNAMIC EQ · OTT · ANALOG · TAPE-A");
    }

    {
        const int x = left + 4 * (cardW + gap);
        drawCard(g,
                 { (float) x, (float) cardY, (float) cardW, (float) cardH },
                 uiColour(juce::Colour(0xff67d3aa)),
                 "DE-ESSER",
                 "PRECISION SIBILANCE CONTROL · 6–18 kHz");
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
    const int cardCount = 5;
    const int cardW = (w - left * 2 - gap * (cardCount - 1)) / cardCount;

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
                x + cardW - 58, cardY + 8, 50, 20);
        if (soloButtons[(size_t) b])
            soloButtons[(size_t) b]->setBounds(
                x + cardW - 108, cardY + 8, 46, 20);

        const int innerX = x + 8;
        const int innerTop = cardY + 48;
        const int innerW = cardW - 16;
        const int cellGap = 6;
        const int cellW = (innerW - cellGap * 2) / 3;
        const int rowH = 80;
        const int knobH = 62;

        const auto cell = [&](int row, int col)
        {
            return juce::Rectangle<int>(
                innerX + col * (cellW + cellGap),
                innerTop + row * rowH,
                cellW, knobH);
        };

        const auto n = juce::String(b + 1);

        // ROW 1 — static EQ
        placeKnob("EQ" + n + "_FREQ", cell(0, 0));
        placeKnob("EQ" + n + "_GAIN", cell(0, 1));
        placeKnob("EQ" + n + "_Q",    cell(0, 2));

        // ROW 2 — Dynamic EQ
        placeKnob("DYN_DYNAMICS" + n, cell(1, 0));
        placeKnob("DYN_ATTACK" + n,   cell(1, 1));
        placeKnob("DYN_RELEASE" + n,  cell(1, 2));

        // ROW 3 — detection mode buttons only; no knobs underneath them.
        const int modeY = innerTop + rowH * 2 + 7;
        const int halfW = (innerW - 8) / 2;

        if (dynDetectButtons[(size_t) b])
        {
            dynDetectButtons[(size_t) b]->setBounds(
                innerX, modeY, halfW, 26);
            dynDetectButtons[(size_t) b]->setButtonText(
                parameterValue("DYN_DETECT_ONSETS" + n) > 0.5f
                    ? "ONSETS" : "PEAK");
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
                    r.getRight() - 12, r.getY() - 6, 12, 12);
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
                    r.getRight() - 12, r.getY() - 6, 12, 12);
            }
    }

    // Fifth zone: the DE-ESS controls get their own full section.
    {
        const int x = left + 4 * (cardW + gap);
        const int innerX = x + 8;
        const int innerTop = cardY + 48;
        const int innerW = cardW - 16;
        if (deessBypassButton)
            deessBypassButton->setBounds(x + cardW - 42, cardY + 4, 28, 28);

        // First two controls are 20% smaller; MIX / OUT use the full size.
        const int knobX = innerX + 8;
        const int knobW = innerW - 16;
        placeKnob("DEESS_FREQ",      { knobX, innerTop + 34,  knobW, 90 });
        placeKnob("DEESS_INTENSITY", { knobX, innerTop + 129, knobW, 90 });
        placeKnob("DRY_WET",         { knobX, innerTop + 224, knobW, 112 });
        placeKnob("OUTPUT_LEVEL",    { knobX, innerTop + 341, knobW, 112 });
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

void VVChainAudioProcessorEditor::mouseMove(const juce::MouseEvent& event)
{
    const auto graph = eqGraphBounds();

    if (!graph.contains(event.position))
    {
        if (hoverDynamicBand != -1)
        {
            hoverDynamicBand = -1;
            repaint();
        }
        return;
    }

    int band = -1;
    if (pointNearDynamicNode(event.position, band))
    {
        if (hoverDynamicBand != band)
        {
            hoverDynamicBand = band;
            repaint();
        }
        return;
    }

    if (hoverDynamicBand != -1)
    {
        hoverDynamicBand = -1;
        repaint();
    }
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
        return;

    int band = -1;

    // Right-click is reserved for the M/S popup.
    if (event.mods.isRightButtonDown()
        && pointNearDynamicNode(pos, band))
    {
        expandedDynamicBand = band;
        dragBand = -1;
        dragXover = -1;
        dragDynamicMsBand = -1;
            showGraphDragHint = false;
        repaint();
        return;
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
            repaint();
        }

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
            && pos.getDistanceFrom({ x, y }) < 18.f)
        {
            dragDynamicTargetBand = -1;
            dragOffsetBand = b;
            dragBand = -1;
            dragXover = -1;
            dragDynamicMsBand = -1;

            dynamicGainDragStartY = pos.y;
            dynamicGainDragStartOffset =
                parameterValue("EQ" + n + "_GAIN");

            showGraphDragHint = true;
            graphDragHintPosition = pos;
            graphDragHint =
                "EQ " + n + "   OFFSET "
                + juce::String(dynamicGainDragStartOffset, 1)
                + " dB";
            repaint();
            return;
        }
    }

    // Dynamic Gain point:
    // - click/drag the coloured Target handle;
    // - vertical = Dynamic Gain only;
    // - frequency stays locked while Gain is dragged;
    // - Threshold is never edited independently.
    if (event.mods.isLeftButtonDown()
        && pointNearDynamicNode(pos, band))
    {
        const auto n = juce::String(band + 1);
        const auto target = dynamicTargetPoint(band);

        dragBand = band;
        dragXover = -1;
        dragDynamicMsBand = -1;
        dragDynamicTargetBand = -1;
        dragOffsetBand = -1;

        dynamicTargetDragStartY = target.y;
        dynamicTargetDragStartValue =
            dynamicEffectiveTargetGain(band);

        showGraphDragHint = true;
        graphDragHintPosition = pos;
        graphDragHint =
            "DYN GAIN " + n + "   "
            + juce::String(
                dynamicTargetDragStartValue, 1)
            + " dB   "
            + formatGraphFrequency(
                parameterValue("EQ" + n + "_FREQ"));
        repaint();
        return;
    }

    // X1/X2/X3 remain independent draggable controls.
    const float xovers[3]
    {
        graphFrequencyToX(
            graph, parameterValue("OTT_X1")),
        graphFrequencyToX(
            graph, parameterValue("OTT_X2")),
        graphFrequencyToX(
            graph, parameterValue("OTT_X3"))
    };

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
        dragDynamicTargetBand = -1;
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

    // Static EQ Offset Gain drag.
    if (dragOffsetBand >= 0)
    {
        const auto n = juce::String(dragOffsetBand + 1);
        const float dragScale =
            event.mods.isShiftDown() ? 0.1f : 1.0f;

        const float deltaDb =
            -(event.position.y - dynamicGainDragStartY)
            / juce::jmax(1.f, graph.getHeight())
            * 36.f * dragScale;

        const float offset =
            juce::jlimit(
                -24.f, 24.f,
                dynamicGainDragStartOffset + deltaDb);

        setParameter("EQ" + n + "_GAIN", offset);

        if (auto* gainKnob = findKnob("EQ" + n + "_GAIN"))
            gainKnob->slider->setValue(
                offset, juce::dontSendNotification);

        graphDragHintPosition = event.position;
        graphDragHint =
            "EQ " + n + "   OFFSET "
            + juce::String(offset, 1) + " dB";
        showGraphDragHint = true;
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

    // Dynamic Gain graph drag is intentionally vertical-only.
    // Frequency belongs to the FREQ control; incidental horizontal mouse
    // movement must never alter frequency while changing Gain.
    if (dragBand >= 0)
    {
        const auto n = juce::String(dragBand + 1);

        const float dragScale =
            event.mods.isShiftDown() ? 0.1f : 1.0f;

        const float deltaDb =
            -(event.position.y - dynamicTargetDragStartY)
            / juce::jmax(1.f, graph.getHeight())
            * 36.f * dragScale;

        const float desiredGain =
            juce::jlimit(
                -24.f, 24.f,
                dynamicTargetDragStartValue + deltaDb);

        const float offset =
            juce::jlimit(
                -24.f, 24.f,
                parameterValue("EQ" + n + "_GAIN"));

        // Signed DYNAMICS selects compression / expansion direction.
        // The Target point sets only the magnitude (distance from Offset).
        const float direction =
            parameterValue("DYN_DYNAMICS" + n) < 0.f
                ? -1.f : 1.f;

        const float span =
            std::abs(desiredGain - offset);

        const float storedTarget =
            juce::jlimit(
                -24.f, 24.f,
                offset + direction * span);

        setParameter("DYN_TARGET" + n, storedTarget);

        graphDragHintPosition = event.position;
        graphDragHint =
            "DYN GAIN " + n + "   GAIN "
            + juce::String(
                dynamicEffectiveTargetGain(dragBand), 1)
            + " dB   DYN "
            + juce::String(
                parameterValue("DYN_DYNAMICS" + n), 0)
            + "%   AUTO THR "
            + juce::String(
                dynamicThresholdFromDynamics(
                    parameterValue("DYN_DYNAMICS" + n)), 1)
            + " dB";

        showGraphDragHint = true;
        repaint();
        return;
    }
}

void VVChainAudioProcessorEditor::mouseUp(
    const juce::MouseEvent&)
{
    dragBand = -1;
    dragOffsetBand = -1;
    dragXover = -1;
    dragDynamicMsBand = -1;
    dragDynamicTargetBand = -1;
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

    // Right-button + wheel = M/S. Shift makes it the minimum 0.01 step.
    if (event.mods.isRightButtonDown())
    {
        int band = hoverDynamicBand;

        if (band < 0)
            pointNearDynamicNode(
                event.position, band);

        if (band >= 0)
        {
            const auto id =
                "DYN_MS"
                + juce::String(band + 1);

            const float step =
                event.mods.isShiftDown() ? 0.01f : 4.0f;
            const float next =
                juce::jlimit(
                    0.f, 100.f,
                    parameterValue(id)
                        + wheel.deltaY * step);

            setParameter(id, next);
            expandedDynamicBand = band;
            repaint();
            return;
        }
    }

    // Normal wheel near any visible EQ / Dynamic EQ handle controls Q.
    // Do not make Q hit-testing depend on the instantaneous detector state.
    int band = -1;
    if (!pointNearDynamicNode(event.position, band))
        return;

    const auto n =
        juce::String(band + 1);
    const float q =
        juce::jmax(
            0.1f,
            parameterValue(
                "EQ" + n + "_Q"));

    const float nextQ =
        event.mods.isShiftDown()
            ? juce::jlimit(
                0.1f, 18.f,
                q + wheel.deltaY * 0.01f)
            : juce::jlimit(
                0.1f, 18.f,
                q * std::exp(
                    -wheel.deltaY * .25f));

    setParameter(
        "EQ" + n + "_Q", nextQ);

    if (auto* knob =
            findKnob("EQ" + n + "_Q"))
        knob->slider->setValue(
            nextQ,
            juce::dontSendNotification);

    repaint();
}
