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

        addKnob("DYN_THRESH" + n, "THRESH", -60, 0, .1,
                parameterValue("DYN_THRESH" + n), " dB", b, 8, c);
        addKnob("DYN_RATIO" + n, "RATIO", 1, 20, .01,
                parameterValue("DYN_RATIO" + n), " :1", b, 9, c);
        addKnob("DYN_ATTACK" + n, "ATTACK", .1, 200, .1,
                parameterValue("DYN_ATTACK" + n), " ms", b, 10, c);
        addKnob("DYN_RELEASE" + n, "RELEASE", 5, 2000, 1,
                parameterValue("DYN_RELEASE" + n), " ms", b, 11, c);

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
    if (soloModeButton) soloModeButton->setLookAndFeel(nullptr);

    for (auto& a : analogBypassAttachments)
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
    k.label->setFont(juce::FontOptions(8.8f).withStyle("Bold"));

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
        k->label->setBounds(area.removeFromTop(13));
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

juce::Rectangle<float> VVChainAudioProcessorEditor::dynamicMsPopupBounds(int band) const
{
    const auto graph = eqGraphBounds();
    const auto n = juce::String(band + 1);
    const float x = graphFrequencyToX(graph, parameterValue("EQ" + n + "_FREQ"));
    const float y = eqDbToY(graph, parameterValue("EQ" + n + "_GAIN"));

    const float w = 220.f;
    const float h = 92.f;
    float px = x - w * 0.5f;
    float py = y - h - 34.f;

    if (py < graph.getY() + 6.f)
        py = y + 34.f;

    px = juce::jlimit(graph.getX() + 6.f, graph.getRight() - w - 6.f, px);
    py = juce::jlimit(graph.getY() + 6.f, graph.getBottom() - h - 6.f, py);
    return { px, py, w, h };
}

juce::Point<float> VVChainAudioProcessorEditor::dynamicThresholdHandlePoint(int band) const
{
    const auto graph = eqGraphBounds();
    const auto n = juce::String(band + 1);
    const float x = graphFrequencyToX(graph, parameterValue("EQ" + n + "_FREQ"));
    const float threshold = juce::jlimit(
        -60.f, 0.f, parameterValue("DYN_THRESH" + n));
    const float t = (0.f - threshold) / 60.f;
    const float y = graph.getY() + 20.f
        + t * juce::jmax(20.f, graph.getHeight() - 40.f);
    return { x, y };
}

bool VVChainAudioProcessorEditor::pointNearDynamicNode(
    juce::Point<float> p, int& band) const
{
    const auto graph = eqGraphBounds();
    float best = 22.f;
    band = -1;

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float freq = parameterValue("EQ" + n + "_FREQ");
        const float x = graphFrequencyToX(graph, freq);
        const float staticGain = parameterValue("EQ" + n + "_GAIN");
        const float staticY = eqDbToY(graph, staticGain);
        const float currentDb = dynamicAverageReductionDb(b);
        const float liveY = eqDbToY(graph, staticGain + currentDb);
        const auto c = uiColour(kBandColours[(size_t) b]);
        const float threshold = juce::jlimit(
            -60.f, 0.f, parameterValue("DYN_THRESH" + n));
        const float thresholdY = dynamicThresholdHandlePoint(b).y;

        // DYN THRESH curve, separate from the EQ gain curve.
        const float q = juce::jmax(.1f, parameterValue("EQ" + n + "_Q"));
        const float curveWidth = juce::jlimit(24.f, 150.f, 92.f / q);
        const float curveDepth = juce::jlimit(
            7.f, 22.f, 10.f + parameterValue("DYN_RATIO" + n) * .45f);
        juce::Path thresholdCurve;
        for (int i = 0; i <= 40; ++i)
        {
            const float u = (i / 40.f) * 2.f - 1.f;
            const float sx = x + u * curveWidth;
            const float sy = thresholdY
                + curveDepth * std::exp(-(u * u) * 2.7f)
                    * (threshold <= -30.f ? 1.f : -1.f);
            if (i == 0)
                thresholdCurve.startNewSubPath(sx, sy);
            else
                thresholdCurve.lineTo(sx, sy);
        }
        g.setColour(c.withAlpha(.72f));
        g.strokePath(thresholdCurve, juce::PathStrokeType(1.35f));

        // Independent threshold handle: drag vertically to change DYN THRESH.
        const auto th = dynamicThresholdHandlePoint(b);
        g.setColour(juce::Colours::black.withAlpha(.76f));
        g.fillEllipse(th.x - 8.f, th.y - 8.f, 16.f, 16.f);
        g.setColour(c.brighter(.28f));
        g.drawEllipse(th.x - 6.f, th.y - 6.f, 12.f, 12.f, 1.7f);
        g.setColour(c.brighter(.55f));
        g.fillEllipse(th.x - 3.f, th.y - 3.f, 6.f, 6.f);

        g.setColour(c.withAlpha(.10f));
        g.fillEllipse(x - 20.f, staticY - 20.f, 40.f, 40.f);
        g.setColour(c.withAlpha(.18f));
        g.fillEllipse(x - 13.f, liveY - 13.f, 26.f, 26.f);
        g.setColour(c.withAlpha(.65f));
        g.drawLine(x, staticY, x, liveY, 1.5f);

        const float midReduction =
            juce::jlimit(0.f, 12.f, -dynamicMidReductionDb(b));
        const float sideReduction =
            juce::jlimit(0.f, 12.f, -dynamicSideReductionDb(b));

        juce::Path midRing;
        midRing.addCentredArc(
            x, liveY, 15.f, 15.f, 0.f,
            -juce::MathConstants<float>::halfPi,
            -juce::MathConstants<float>::halfPi
                + juce::MathConstants<float>::pi * (midReduction / 12.f),
            true);
        g.setColour(c.brighter(.30f));
        g.strokePath(midRing, juce::PathStrokeType(2.7f));

        juce::Path sideRing;
        sideRing.addCentredArc(
            x, liveY, 15.f, 15.f, 0.f,
            juce::MathConstants<float>::halfPi,
            juce::MathConstants<float>::halfPi
                + juce::MathConstants<float>::pi * (sideReduction / 12.f),
            true);
        g.setColour(juce::Colours::white.withAlpha(.82f));
        g.strokePath(sideRing, juce::PathStrokeType(2.3f));

        g.setColour(c);
        g.fillEllipse(x - 7.f, liveY - 7.f, 14.f, 14.f);
        g.setColour(juce::Colours::black.withAlpha(.92f));
        g.setFont(juce::FontOptions(8.f).withStyle("Bold"));
        g.drawText(n, (int)x - 6, (int)liveY - 5, 12, 10,
                   juce::Justification::centred);

        // Compression transfer curve glyph; enlarges when the node is hovered.
        const bool hovered = hoverDynamicBand == b;
        const float cw = hovered ? 82.f : 50.f;
        const float ch = hovered ? 48.f : 30.f;
        float cx = x + 15.f;
        float cy = liveY - ch - 18.f;
        if (cx + cw > graph.getRight() - 4.f)
            cx = x - cw - 15.f;
        cy = juce::jlimit(graph.getY() + 21.f,
                          graph.getBottom() - ch - 5.f, cy);

        g.setColour(juce::Colours::black.withAlpha(hovered ? .78f : .34f));
        g.fillRoundedRectangle(cx, cy, cw, ch, 4.f);
        g.setColour(c.withAlpha(hovered ? .78f : .34f));
        g.drawRoundedRectangle(cx, cy, cw, ch, 4.f, 1.f);

        const float pad = 4.f;
        const float x0 = cx + pad;
        const float x1 = cx + cw - pad;
        const float y0 = cy + ch - pad;
        const float y1 = cy + pad;
        const float t = juce::jlimit(.06f, .94f, (threshold + 60.f) / 60.f);
        const float ratio = juce::jmax(
            1.f, parameterValue("DYN_RATIO" + n));
        juce::Path transfer;
        transfer.startNewSubPath(x0, y0);
        const float kneeX = x0 + (x1 - x0) * t;
        const float kneeY = y0 - (y0 - y1) * t;
        transfer.lineTo(kneeX, kneeY);
        const float remain = x1 - kneeX;
        const float outRemain = remain / ratio;
        transfer.cubicTo(
            kneeX + remain * .25f, kneeY - outRemain * .18f,
            kneeX + remain * .70f, kneeY - outRemain * .78f,
            x1, juce::jmax(y1, kneeY - outRemain));
        g.setColour(juce::Colours::white.withAlpha(.92f));
        g.strokePath(transfer, juce::PathStrokeType(hovered ? 1.8f : 1.05f));

        if (hovered)
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(8.5f).withStyle("Bold"));
            g.drawText("COMP " + juce::String(ratio, 2) + ":1",
                       (int)cx + 4, (int)cy + 2,
                       (int)cw - 8, 11, juce::Justification::centred);

            const float boxW = juce::jmin(286.f, graph.getWidth() - 12.f);
            const float boxH = 27.f;
            float bx = x - boxW * .5f;
            float by = liveY - ch - 53.f;
            if (by < graph.getY() + 4.f)
                by = liveY + 22.f;
            bx = juce::jlimit(
                graph.getX() + 6.f, graph.getRight() - boxW - 6.f, bx);

            g.setColour(juce::Colours::black.withAlpha(.90f));
            g.fillRoundedRectangle(bx, by, boxW, boxH, 5.f);
            g.setColour(c.withAlpha(.95f));
            g.drawRoundedRectangle(bx, by, boxW, boxH, 5.f, 1.f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(8.7f).withStyle("Bold"));
            g.drawText(
                "B" + n + "  " + formatGraphFrequency(freq)
                + "  " + juce::String(staticGain >= 0.f ? "+" : "")
                + juce::String(staticGain, 1) + " dB"
                + "  DYN THR " + juce::String(threshold, 1) + " dB"
                + "  R " + juce::String(ratio, 2) + ":1"
                + "  GR " + juce::String(currentDb, 1) + " dB",
                (int)bx + 7, (int)by + 7, (int)boxW - 14, 13,
                juce::Justification::centred);
        }

        if (expandedDynamicBand == b)
        {
            const auto popup = dynamicMsPopupBounds(b);
            g.setColour(juce::Colours::black.withAlpha(.78f));
            g.fillRoundedRectangle(popup.translated(5.f, 6.f), 8.f);

            juce::ColourGradient panel(
                juce::Colour(0xff30343a), popup.getX(), popup.getY(),
                juce::Colour(0xff12151a), popup.getRight(), popup.getBottom(),
                false);
            g.setGradientFill(panel);
            g.fillRoundedRectangle(popup, 8.f);

            g.setColour(c.withAlpha(.94f));
            g.drawRoundedRectangle(popup, 8.f, 1.1f);

            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(11.f).withStyle("Bold"));
            g.drawText("BAND " + n + " · DYNAMIC M/S",
                       (int)popup.getX() + 12, (int)popup.getY() + 8,
                       196, 16, juce::Justification::left);

            const float midPct = juce::jlimit(
                0.f, 100.f, parameterValue("DYN_MS" + n));
            const float sidePct = 100.f - midPct;

            g.setColour(c.brighter(.25f));
            g.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
            g.drawText("MID " + juce::String(midPct, 0) + "%",
                       (int)popup.getX() + 12, (int)popup.getY() + 29,
                       88, 15, juce::Justification::left);
            g.setColour(juce::Colours::white.withAlpha(.92f));
            g.drawText("SIDE " + juce::String(sidePct, 0) + "%",
                       (int)popup.getRight() - 100, (int)popup.getY() + 29,
                       88, 15, juce::Justification::right);

            const auto bar = popup.reduced(12.f)
                .withY(popup.getY() + 51.f).withHeight(11.f);
            g.setColour(juce::Colour(0xff090c10));
            g.fillRoundedRectangle(bar, 5.f);

            const float splitX =
                bar.getX() + bar.getWidth() * midPct / 100.f;
            if (splitX > bar.getX())
            {
                g.setColour(c.withAlpha(.94f));
                g.fillRoundedRectangle(
                    { bar.getX(), bar.getY(),
                      splitX - bar.getX(), bar.getHeight() }, 5.f);
            }
            if (splitX < bar.getRight())
            {
                g.setColour(juce::Colours::white.withAlpha(.56f));
                g.fillRoundedRectangle(
                    { splitX, bar.getY(),
                      bar.getRight() - splitX, bar.getHeight() }, 5.f);
            }

            g.setColour(juce::Colours::white);
            g.fillEllipse(splitX - 6.f, bar.getCentreY() - 6.f, 12.f, 12.f);

            g.setColour(juce::Colour(0xffc0c7d0));
            g.setFont(juce::FontOptions(8.3f).withStyle("Bold"));
            g.drawText("右鍵 + 滾輪：MID / SIDE　　左鍵拖曳：比例",
                       (int)popup.getX() + 12, (int)popup.getBottom() - 18,
                       (int)popup.getWidth() - 24, 12,
                       juce::Justification::centred);
        }
    }

    g.setColour(juce::Colour(0xffc4cad2));
    g.setFont(juce::FontOptions(9.f).withStyle("Bold"));
    g.drawText("DYNAMIC EQ · LIVE GAIN · CLICK NODE = M/S · 20 Hz — 20 kHz",
               (int) graph.getX() + 12, (int) graph.getY() + 9,
               330, 14, juce::Justification::left);

    g.setFont(juce::FontOptions(8.f));
    g.setColour(juce::Colour(0xffaab0ba));
    g.drawText("20", (int) graph.getX() + 6, (int) graph.getBottom() - 15,
               28, 12, juce::Justification::left);
    g.drawText("1k", (int) graph.getCentreX() - 12, (int) graph.getBottom() - 15,
               24, 12, juce::Justification::centred);
    g.drawText("20k", (int) graph.getRight() - 28, (int) graph.getBottom() - 15,
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
            advancedButtons[(size_t) b]->setBounds(x + cardW - 58, cardY + 8, 50, 20);
        if (soloButtons[(size_t) b])
            soloButtons[(size_t) b]->setBounds(x + cardW - 108, cardY + 8, 46, 20);

        const int innerX = x + 8;
        const int innerTop = cardY + 48;
        const int innerW = cardW - 16;
        const int cellGap = 4;
        const int cellW = (innerW - cellGap * 2) / 3;
        const int rowH = 92;

        const auto pos3 = [&](int slot)
        {
            const int row = slot / 3;
            const int col = slot % 3;
            return juce::Rectangle<int>(
                innerX + col * (cellW + cellGap),
                innerTop + row * rowH,
                cellW, 82);
        };

        const auto n = juce::String(b + 1);

        placeKnob("EQ" + n + "_FREQ", pos3(0));
        placeKnob("EQ" + n + "_GAIN", pos3(1));
        placeKnob("EQ" + n + "_Q", pos3(2));

        const int dynGap = 3;
        const int dynW = (innerW - dynGap * 3) / 4;
        const int dynY = innerTop + rowH;
        const auto dynPos = [&](int i)
        {
            return juce::Rectangle<int>(
                innerX + i * (dynW + dynGap),
                dynY, dynW, 82);
        };

        placeKnob("DYN_THRESH" + n, dynPos(0));
        placeKnob("DYN_RATIO" + n, dynPos(1));
        placeKnob("DYN_ATTACK" + n, dynPos(2));
        placeKnob("DYN_RELEASE" + n, dynPos(3));

        placeKnob("OTT_DEGREE" + n, pos3(6));
        placeKnob("OTT_COMP_A" + n, pos3(7));
        placeKnob("OTT_COMP_R" + n, pos3(8));

        const int lowerY = innerTop + rowH * 3;
        placeKnob("EQ_COLOR_B" + n, { innerX, lowerY, cellW, 82 });
        placeKnob("ATYPE_DEGREE" + n,
                  { innerX + cellW + cellGap, lowerY, cellW, 82 });

        if (analogModeButtons[(size_t) b])
            analogModeButtons[(size_t) b]->setBounds(
                innerX + cellW - 8, lowerY + 4, 36, 12);

        if (ottBandBypassButtons[(size_t) b])
            if (auto* knob = findKnob("OTT_DEGREE" + n))
            {
                const auto r = knob->slider->getBounds();
                ottBandBypassButtons[(size_t) b]->setBounds(
                    r.getRight() - 11, r.getY() - 10, 12, 12);
            }

        if (analogBypassButtons[(size_t) b])
            if (auto* knob = findKnob("EQ_COLOR_B" + n))
            {
                const auto r = knob->slider->getBounds();
                analogBypassButtons[(size_t) b]->setBounds(
                    r.getRight() - 11, r.getY() - 10, 12, 12);
            }

        if (atypeBandBypassButtons[(size_t) b])
            if (auto* knob = findKnob("ATYPE_DEGREE" + n))
            {
                const auto r = knob->slider->getBounds();
                atypeBandBypassButtons[(size_t) b]->setBounds(
                    r.getRight() - 11, r.getY() - 4, 12, 12);
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

void VVChainAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto pos = event.position;
    const auto graph = eqGraphBounds();

    if (!graph.contains(pos))
        return;

    int dynamicBand = -1;

    // Right-click on the Dynamic EQ node is the only way to open M/S.
    if (event.mods.isRightButtonDown()
        && pointNearDynamicNode(pos, dynamicBand))
    {
        expandedDynamicBand = dynamicBand;
        dragBand = -1;
        dragDynamicThresholdBand = -1;
        dragDynamicMsBand = -1;
        showGraphDragHint = false;
        repaint();
        return;
    }

    // DYN THRESH has its own handle and vertical drag.
    if (event.mods.isLeftButtonDown()
        && pointNearDynamicThresholdHandle(pos, dynamicBand))
    {
        dragDynamicThresholdBand = dynamicBand;
        dragBand = -1;
        dragXover = -1;
        dragDynamicMsBand = -1;
        expandedDynamicBand = -1;
        repaint();
        return;
    }

    if (expandedDynamicBand >= 0)
    {
        const auto popup = dynamicMsPopupBounds(expandedDynamicBand);
        if (!popup.contains(pos))
        {
            expandedDynamicBand = -1;
            repaint();
            return;
        }

        const auto bar = popup.reduced(12.f)
            .withY(popup.getY() + 51.f).withHeight(11.f);
        if (event.mods.isLeftButtonDown()
            && bar.expanded(0.f, 12.f).contains(pos))
        {
            const float midPct = juce::jlimit(
                0.f, 100.f,
                (pos.x - bar.getX()) / juce::jmax(1.f, bar.getWidth()) * 100.f);
            setParameter(
                "DYN_MS" + juce::String(expandedDynamicBand + 1), midPct);
            dragDynamicMsBand = expandedDynamicBand;
            repaint();
        }
        return;
    }

    // Left-drag main Dynamic EQ node = Frequency + Gain.
    if (event.mods.isLeftButtonDown()
        && pointNearDynamicNode(pos, dynamicBand))
    {
        dragBand = dynamicBand;
        dragXover = -1;
        dragDynamicThresholdBand = -1;
        dragDynamicMsBand = -1;
        dynamicFreqDragStartHz =
            parameterValue("EQ" + juce::String(dragBand + 1) + "_FREQ");
        dynamicFreqDragStartX = pos.x;
        showGraphDragHint = true;
        graphDragHintPosition = pos;
        graphDragHint =
            "DYN EQ " + juce::String(dragBand + 1) + "  "
            + formatGraphFrequency(dynamicFreqDragStartHz);
        repaint();
        return;
    }

    const float xovers[3]
    {
        graphFrequencyToX(graph, parameterValue("OTT_X1")),
        graphFrequencyToX(graph, parameterValue("OTT_X2")),
        graphFrequencyToX(graph, parameterValue("OTT_X3"))
    };

    float bestXover = 11.f;
    dragXover = -1;
    for (int i = 0; i < 3; ++i)
    {
        const float d = std::abs(pos.x - xovers[i]);
        if (d < bestXover)
        {
            bestXover = d;
            dragXover = i;
        }
    }

    if (dragXover >= 0)
    {
        dragBand = -1;
        showGraphDragHint = true;
        graphDragHintPosition = pos;
        const auto hz = parameterValue(
            dragXover == 0 ? "OTT_X1"
            : dragXover == 1 ? "OTT_X2" : "OTT_X3");
        graphDragHint =
            "X" + juce::String(dragXover + 1) + "  "
            + formatGraphFrequency(hz)
            + "   OVERLAP "
            + juce::String(parameterValue("XOVER_OVERLAP"), 0) + "%";
        repaint();
        return;
    }

    float best = 22.f;
    dragBand = -1;
    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float x = graphFrequencyToX(
            graph, parameterValue("EQ" + n + "_FREQ"));
        const float y = eqDbToY(
            graph, parameterValue("EQ" + n + "_GAIN"));
        const float d = pos.getDistanceFrom({ x, y });
        if (d < best)
        {
            best = d;
            dragBand = b;
        }
    }

    if (dragBand >= 0)
    {
        dynamicFreqDragStartHz =
            parameterValue("EQ" + juce::String(dragBand + 1) + "_FREQ");
        dynamicFreqDragStartX = pos.x;
        showGraphDragHint = true;
        graphDragHintPosition = pos;
    }
}



void VVChainAudioProcessorEditor::mouseDrag(const juce::MouseEvent& event)
{
    const auto graph = eqGraphBounds();

    if (dragDynamicThresholdBand >= 0)
    {
        const float t = juce::jlimit(
            0.f, 1.f,
            (event.position.y - (graph.getY() + 20.f))
                / juce::jmax(20.f, graph.getHeight() - 40.f));
        const float threshold = juce::jlimit(0.f, 60.f, 60.f * t);
        const float db = -threshold;
        setParameter(
            "DYN_THRESH" + juce::String(dragDynamicThresholdBand + 1), db);

        graphDragHintPosition = event.position;
        graphDragHint = "DYN THRESH " + juce::String(db, 1) + " dB";
        showGraphDragHint = true;
        repaint();
        return;
    }

    if (dragDynamicMsBand >= 0)
    {
        const auto popup = dynamicMsPopupBounds(dragDynamicMsBand);
        const auto bar = popup.reduced(12.f)
            .withY(popup.getY() + 51.f).withHeight(11.f);
        const float midPct = juce::jlimit(
            0.f, 100.f,
            (event.position.x - bar.getX())
                / juce::jmax(1.f, bar.getWidth()) * 100.f);
        setParameter(
            "DYN_MS" + juce::String(dragDynamicMsBand + 1), midPct);
        repaint();
        return;
    }

    if (dragXover >= 0)
    {
        const float hz = constrainXoverFrequency(
            dragXover, graphXToFrequency(graph, event.position.x));
        setParameter(
            dragXover == 0 ? "OTT_X1"
            : dragXover == 1 ? "OTT_X2" : "OTT_X3", hz);
        graphDragHintPosition = event.position;
        graphDragHint =
            "X" + juce::String(dragXover + 1) + "  "
            + formatGraphFrequency(hz)
            + "   OVERLAP "
            + juce::String(parameterValue("XOVER_OVERLAP"), 0) + "%";
        repaint();
        return;
    }

    if (dragBand < 0)
        return;

    const auto n = juce::String(dragBand + 1);
    const float hzPerPixel =
        19980.f / juce::jmax(1.f, graph.getWidth());

    // Frequency uses constant Hz-per-pixel across the complete 20 Hz..20 kHz range.
    const float hz = juce::jlimit(
        20.f, 20000.f,
        dynamicFreqDragStartHz
            + (event.position.x - dynamicFreqDragStartX) * hzPerPixel);

    const float db = juce::jlimit(
        -24.f, 24.f,
        18.f - ((event.position.y - graph.getY()) / graph.getHeight()) * 36.f);

    setParameter("EQ" + n + "_FREQ", hz);
    setParameter("EQ" + n + "_GAIN", db);

    if (auto* freqKnob = findKnob("EQ" + n + "_FREQ"))
        freqKnob->slider->setValue(hz, juce::dontSendNotification);
    if (auto* gainKnob = findKnob("EQ" + n + "_GAIN"))
        gainKnob->slider->setValue(db, juce::dontSendNotification);

    graphDragHintPosition = event.position;
    graphDragHint =
        "DYN EQ " + n + "   " + formatGraphFrequency(hz)
        + "   " + juce::String(db >= 0.f ? "+" : "")
        + juce::String(db, 1) + " dB";
    showGraphDragHint = true;
    repaint();
}



void VVChainAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    dragBand = -1;
    dragXover = -1;
    dragDynamicMsBand = -1;
    dragDynamicThresholdBand = -1;
    showGraphDragHint = false;
    graphDragHint.clear();
    repaint();
}



void VVChainAudioProcessorEditor::mouseWheelMove(
    const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto graph = eqGraphBounds();
    if (!graph.contains(event.position) || std::abs(wheel.deltaY) < 0.0001f)
        return;

    // Right-button + wheel = Dynamic EQ Mid/Side percentage.
    if (event.mods.isRightButtonDown())
    {
        int band = hoverDynamicBand;
        if (band < 0)
            pointNearDynamicNode(event.position, band);

        if (band >= 0)
        {
            const auto id = "DYN_MS" + juce::String(band + 1);
            const float next = juce::jlimit(
                0.f, 100.f,
                parameterValue(id) + wheel.deltaY * 4.0f);
            setParameter(id, next);
            expandedDynamicBand = band;
            repaint();
            return;
        }
    }

    // Normal wheel above an EQ node = Q, unchanged from the normal EQ workflow.
    float bestXover = 12.f;
    int xover = -1;
    for (int i = 0; i < 3; ++i)
    {
        const float lineX = graphFrequencyToX(
            graph, parameterValue(
                i == 0 ? "OTT_X1" : i == 1 ? "OTT_X2" : "OTT_X3"));
        const float d = std::abs(event.position.x - lineX);
        if (d < bestXover)
        {
            bestXover = d;
            xover = i;
        }
    }

    if (xover >= 0)
    {
        const float overlap = juce::jlimit(
            0.f, 100.f,
            parameterValue("XOVER_OVERLAP") + wheel.deltaY * 2.0f);
        setParameter("XOVER_OVERLAP", overlap);
        repaint();
        return;
    }

    float best = 22.f;
    int band = -1;
    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float x = graphFrequencyToX(
            graph, parameterValue("EQ" + n + "_FREQ"));
        const float y = eqDbToY(
            graph, parameterValue("EQ" + n + "_GAIN"));
        const float d = event.position.getDistanceFrom({ x, y });
        if (d < best)
        {
            best = d;
            band = b;
        }
    }

    if (band < 0)
        return;

    const auto n = juce::String(band + 1);
    const float q = juce::jmax(0.1f, parameterValue("EQ" + n + "_Q"));
    const float nextQ = juce::jlimit(
        0.1f, 18.f, q * std::exp(-wheel.deltaY * 0.25f));
    setParameter("EQ" + n + "_Q", nextQ);
    if (auto* knob = findKnob("EQ" + n + "_Q"))
        knob->slider->setValue(nextQ, juce::dontSendNotification);
    repaint();
}
