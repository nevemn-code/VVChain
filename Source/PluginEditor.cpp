#include "PluginEditor.h"

namespace
{
const std::array<juce::Colour, 4> kBandColours
{
    juce::Colour(0xff38bdf8),
    juce::Colour(0xff22d3ee),
    juce::Colour(0xffa3e635),
    juce::Colour(0xfff97316)
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
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
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
    g.setColour(juce::Colours::black.withAlpha(.98f));
    g.drawLine(cx, cy, px, py, 3.2f);
    g.setColour(juce::Colours::white.withAlpha(.70f));
    g.fillEllipse(cx - 2.2f, cy - 2.2f, 4.4f, 4.4f);
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    if (button.getWidth() <= 36 && button.getHeight() <= 36)
    {
        const float d = juce::jmin(button.getWidth(), button.getHeight()) - 10.f;
        const float cx = button.getLocalBounds().getCentreX();
        const float cy = button.getLocalBounds().getCentreY();
        const auto accent = button.findColour(juce::ToggleButton::tickColourId);
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
    const auto accent = button.findColour(juce::ToggleButton::tickColourId);

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
        "EQ_BYPASS", "OTT_BYPASS", "ATYPE_BYPASS", "DEESS_BYPASS", "MIX_BYPASS"
    };

    for (int i = 0; i < 5; ++i)
        addBypass(i, bypassIds[(size_t) i],
                  i == 0 ? "EQ / ANALOG" :
                  i == 1 ? "OTT" :
                  i == 2 ? "TAPE-A" :
                  i == 3 ? "DE-ESSER" : "MIX / OUT",
                  i == 0 ? juce::Colour(0xff38bdf8) :
                  i == 1 ? juce::Colour(0xfffacc15) :
                  i == 2 ? juce::Colour(0xfff472b6) :
                  i == 3 ? juce::Colour(0xff67d3aa) :
                           juce::Colour(0xff9ed85c));

    masterBypassButton = std::make_unique<juce::ToggleButton>("BYPASS");
    masterBypassButton->setLookAndFeel(&metalLook);
    masterBypassButton->setButtonText("BYPASS");
    masterBypassButton->setColour(juce::ToggleButton::tickColourId,
                                  juce::Colour(0xffdfe7ef));
    masterBypassButton->setTooltip(
        "整個 VVCHAIN 完全旁通；固定 PDC，切換使用短交叉淡化避免斷音/爆音");
    masterBypassAttachment = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, "MASTER_BYPASS", *masterBypassButton);
    addAndMakeVisible(*masterBypassButton);

    for (int b = 0; b < 4; ++b)
    {
        const auto c = kBandColours[(size_t) b];
        const auto n = juce::String(b + 1);

        // Main EQ controls.
        addKnob("EQ" + n + "_FREQ", "FREQ", 20, 20000, 1,
                parameterValue("EQ" + n + "_FREQ"), " Hz", b, 0, c);
        addKnob("EQ" + n + "_GAIN", "GAIN", -24, 24, .1,
                parameterValue("EQ" + n + "_GAIN"), " dB", b, 1, c);
        addKnob("EQ" + n + "_Q", "Q", .1, 18, .01,
                parameterValue("EQ" + n + "_Q"), "", b, 2, c);

        // The two EQ / ANALOG controls live inside every BAND card.
        // They intentionally remain attached to the shared DSP parameters.
        addKnob("EQ_COLOR_B" + n, "ANALOG COLOR", 0, 100, .1,
                parameterValue("EQ_COLOR"), " %", b, 3,
                juce::Colour(0xff60a5fa), false, "EQ_COLOR");
        addKnob("HF_CORNER_B" + n, "HP / CORNER", 40, 120, 1,
                parameterValue("HF_CORNER"), " Hz", b, 4,
                juce::Colour(0xff60a5fa), false, "HF_CORNER");

        // Main screen intentionally keeps only the three OTT performance knobs.
        addKnob("OTT_DEGREE" + n, "OTT %", 0, 100, .1,
                parameterValue("OTT_DEGREE" + n), " %", b, 5, juce::Colour(0xfffacc15));
        addKnob("OTT_COMP_A" + n, "ATTACK", .1, 250, .1,
                parameterValue("OTT_COMP_A" + n), " ms", b, 6, juce::Colour(0xfffacc15));
        addKnob("OTT_COMP_R" + n, "RELEASE", 10, 2500, 1,
                parameterValue("OTT_COMP_R" + n), " ms", b, 7, juce::Colour(0xfffacc15));

        addKnob("ATYPE_DEGREE" + n, "TAPE-A +", 0, 100, .1,
                parameterValue("ATYPE_DEGREE" + n), "", b, 8, c, true);

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
    addKnob("DEESS_INTENSITY", "DE-ESS %", 2, 10, .01,
            parameterValue("DEESS_INTENSITY"), " %", 4, 1,
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

    // MIX / OUT is deliberately tiny and horizontal.
    addKnob("DRY_WET", "MIX", 0, 100, .1,
            parameterValue("DRY_WET"), " %", -3, 40, juce::Colour(0xff38bdf8));
    addKnob("OUTPUT_LEVEL", "OUT", -24, 12, .1,
            parameterValue("OUTPUT_LEVEL"), " dB", -3, 41, juce::Colour(0xff9ed85c));

    for (const auto& id : { juce::String("DRY_WET"), juce::String("OUTPUT_LEVEL") })
    {
        if (auto* k = findKnob(id))
        {
            k->slider->setSliderStyle(juce::Slider::LinearHorizontal);
            k->slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 14);
            k->slider->setColour(juce::Slider::thumbColourId, k->accent);
            k->slider->setColour(juce::Slider::trackColourId, k->accent.withAlpha(.65f));
        }
    }

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

    for (auto& a : ottBandBypassAttachments)
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
    k.slider = std::make_unique<juce::Slider>();
    k.label = std::make_unique<juce::Label>();

    k.slider->setLookAndFeel(&metalLook);
    k.slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 17);
    k.slider->setRange(min, max, step);
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
    b->setButtonText("");
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

void VVChainAudioProcessorEditor::drawEqGraph(
    juce::Graphics& g, juce::Rectangle<float> graph)
{
    juce::ColourGradient bg(juce::Colour(0xff0f1216), graph.getX(), graph.getY(),
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
        g.drawHorizontalLine((int) y, graph.getX(), graph.getRight());
    }

    for (float f : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = graphFrequencyToX(graph, f);
        g.setColour(juce::Colour(0xff68727d).withAlpha(.33f));
        g.drawVerticalLine((int) x, graph.getY(), graph.getBottom());
    }

    juce::Path response;
    for (int i = 0; i <= 420; ++i)
    {
        const float hz = invLogMap(i / 420.f, 20.f, 20000.f);
        float db = 0.f;

        for (int b = 0; b < 4; ++b)
        {
            const auto n = juce::String(b + 1);
            const float f0 = parameterValue("EQ" + n + "_FREQ");
            const float gain = parameterValue("EQ" + n + "_GAIN");
            const float q = juce::jmax(.1f, parameterValue("EQ" + n + "_Q"));
            const float width = juce::jmax(.02f, 1.f / (q * 1.8f));
            const float xx = std::log(std::max(hz, 20.f) / std::max(f0, 20.f));
            db += gain * std::exp(-(xx * xx) / (2.f * width * width));
        }

        const auto pt = juce::Point<float>(
            graphFrequencyToX(graph, hz), eqDbToY(graph, db));

        if (i == 0) response.startNewSubPath(pt);
        else response.lineTo(pt);
    }

    g.setColour(juce::Colours::white.withAlpha(.94f));
    g.strokePath(response, juce::PathStrokeType(2.2f));

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float x = graphFrequencyToX(
            graph, parameterValue("EQ" + n + "_FREQ"));
        const float y = eqDbToY(
            graph, parameterValue("EQ" + n + "_GAIN"));
        const auto c = kBandColours[(size_t) b];

        g.setColour(c.withAlpha(.16f));
        g.fillEllipse(x - 15.f, y - 15.f, 30.f, 30.f);
        g.setColour(c);
        g.fillEllipse(x - 7.f, y - 7.f, 14.f, 14.f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions(8.f).withStyle("Bold"));
        g.drawText(juce::String(b + 1), (int) x - 6, (int) y - 5, 12, 10,
                   juce::Justification::centred);
    }

    g.setColour(juce::Colour(0xffc4cad2));
    g.setFont(juce::FontOptions(9.f).withStyle("Bold"));
    g.drawText("EQ RESPONSE · 20 Hz — 20 kHz · DRAG NODES",
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
    g.drawText("4-BAND EQ · OTT · TAPE-A · DE-ESSER", 20, 37, 300, 13,
               juce::Justification::left);

    // Very small MIX / OUT faders in the title bar.
    g.setColour(juce::Colour(0xff737b86));
    g.setFont(juce::FontOptions(7.f));
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
                 kBandColours[(size_t) b],
                 "BAND " + juce::String(b + 1),
                 "EQ / ANALOG · OTT · TAPE-A");
    }

    {
        const int x = left + 4 * (cardW + gap);
        drawCard(g,
                 { (float) x, (float) cardY, (float) cardW, (float) cardH },
                 juce::Colour(0xff67d3aa),
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

        g.setColour(juce::Colour(0xfffacc15).withAlpha(.8f));
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
    g.drawText("DOUBLE-CLICK KNOB = RESET · NO PAGE SCROLL",
               18, 919, 280, 10, juce::Justification::left);
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
                expandedBand == b ? juce::Colour(0xff3f3517)
                                  : juce::Colour(0xff17191d));
            advancedButtons[(size_t) b]->setColour(
                juce::TextButton::textColourOffId,
                expandedBand == b ? juce::Colour(0xffffdf62)
                                  : juce::Colour(0xffc0c5cb));
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

    const int ledStart = w - 250;
    for (int i = 0; i < 5; ++i)
        if (bypassButtons[(size_t) i])
            bypassButtons[(size_t) i]->setBounds(ledStart + i * 46, 10, 28, 28);

    placeKnob("DRY_WET", { w - 220, 39, 92, 29 });
    placeKnob("OUTPUT_LEVEL", { w - 116, 39, 92, 29 });

    for (int b = 0; b < 4; ++b)
    {
        const int x = left + b * (cardW + gap);
        if (advancedButtons[(size_t) b])
            advancedButtons[(size_t) b]->setBounds(x + cardW - 58, cardY + 8, 50, 20);

        const int innerX = x + 8;
        const int innerTop = cardY + 48;
        const int innerW = cardW - 16;
        const int cellGap = 4;
        const int cellW = (innerW - cellGap * 2) / 3;
        const int rowH = 92;

        const auto pos = [&](int slot)
        {
            const int row = slot / 3;
            const int col = slot % 3;
            return juce::Rectangle<int>(
                innerX + col * (cellW + cellGap),
                innerTop + row * rowH,
                cellW, 82);
        };

        const auto n = juce::String(b + 1);
        placeKnob("EQ" + n + "_FREQ", pos(0));
        placeKnob("EQ" + n + "_GAIN", pos(1));
        placeKnob("EQ" + n + "_Q", pos(2));
        placeKnob("EQ_COLOR_B" + n, pos(3));
        placeKnob("HF_CORNER_B" + n, pos(4));
        placeKnob("OTT_DEGREE" + n, pos(5));
        placeKnob("OTT_COMP_A" + n, pos(6));
        placeKnob("OTT_COMP_R" + n, pos(7));
        placeKnob("ATYPE_DEGREE" + n, pos(8));

        if (ottBandBypassButtons[(size_t) b])
        {
            const auto ottCell = pos(5);
            ottBandBypassButtons[(size_t) b]->setBounds(
                ottCell.getRight() - 18, ottCell.getY() + 1, 16, 16);
        }

        if (atypeBandBypassButtons[(size_t) b])
        {
            const auto typeCell = pos(8);
            atypeBandBypassButtons[(size_t) b]->setBounds(
                typeCell.getRight() - 18, typeCell.getY() + 1, 16, 16);
        }
    }

    // Fifth zone: the DE-ESS controls get their own full section.
    {
        const int x = left + 4 * (cardW + gap);
        const int innerX = x + 8;
        const int innerTop = cardY + 48;
        const int innerW = cardW - 16;
        const int halfW = (innerW - 4) / 2;
        placeKnob("DEESS_FREQ", { innerX, innerTop + 28, halfW, 150 });
        placeKnob("DEESS_INTENSITY", { innerX + halfW + 4, innerTop + 28, halfW, 150 });
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
        const int cols = 7;
        const int cellW = (popupW - 24 - gapX * 6) / cols;
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
        const std::array<juce::String, 7> sharedAdv
        {{
            "OTT_X1", "OTT_X2", "OTT_X3",
            "OTT_INPUT", "OTT_GATE", "OTT_MIX", "OTT_OUTPUT"
        }};

        for (int i = 0; i < 7; ++i)
            placeKnob(bandAdv[(size_t)i] + n, p(i));
        for (int i = 0; i < 7; ++i)
            placeKnob(sharedAdv[(size_t)i], p(i + 7));

        if (ottClipper)
            ottClipper->setBounds(innerX, popupY + popupH - 34, 82, 24);
    }

    repaint();
}

void VVChainAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto pos = event.position;
    const auto graph = eqGraphBounds();
    if (!graph.contains(pos))
        return;

    float best = 20.f;
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
}

void VVChainAudioProcessorEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (dragBand < 0)
        return;

    const auto graph = eqGraphBounds();
    const auto n = juce::String(dragBand + 1);

    const float hz = juce::jlimit(
        20.f, 20000.f,
        graphXToFrequency(graph, event.position.x));
    const float db = juce::jlimit(
        -24.f, 24.f,
        18.f - ((event.position.y - graph.getY()) / graph.getHeight()) * 36.f);

    setParameter("EQ" + n + "_FREQ", hz);
    setParameter("EQ" + n + "_GAIN", db);
    repaint();
}

void VVChainAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    dragBand = -1;
}

void VVChainAudioProcessorEditor::mouseWheelMove(
    const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto graph = eqGraphBounds();
    if (!graph.contains(event.position) || std::abs(wheel.deltaY) < 0.0001f)
        return;

    float best = 22.f;
    int band = -1;

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float x = graphFrequencyToX(graph, parameterValue("EQ" + n + "_FREQ"));
        const float y = eqDbToY(graph, parameterValue("EQ" + n + "_GAIN"));
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

    // Small steps: wheel up lowers Q, wheel down raises Q.
    const float nextQ = juce::jlimit(
        0.1f, 18.f, q * std::exp(-wheel.deltaY * 0.25f));
    setParameter("EQ" + n + "_Q", nextQ);
    repaint();
}
