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

const std::array<juce::String, 5> kModuleNames
{
    "EQ / ANALOG", "OTT", "TAPE-A", "DE-ESSER", "MIX / OUT"
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
                                             (float) width, (float) height).reduced(4.f);
    const float cx = area.getCentreX();
    const float cy = area.getCentreY() - 2.f;
    const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f - 8.f;
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const float angle = juce::jmap(sliderPosProportional, rotaryStartAngle, rotaryEndAngle);

    g.setColour(juce::Colours::black.withAlpha(0.95f));
    g.fillEllipse(cx - radius - 3.f, cy - radius - 3.f,
                  (radius + 3.f) * 2.f, (radius + 3.f) * 2.f);

    juce::ColourGradient rim(juce::Colour(0xff6c7078), cx, cy - radius,
                             juce::Colour(0xff1a1c21), cx, cy + radius, false);
    g.setGradientFill(rim);
    g.fillEllipse(cx - radius, cy - radius, radius * 2.f, radius * 2.f);

    juce::ColourGradient face(juce::Colour(0xff4b4f58), cx, cy - radius * .8f,
                              juce::Colour(0xff181a1f), cx, cy + radius, false);
    g.setGradientFill(face);
    g.fillEllipse(cx - radius + 4.f, cy - radius + 4.f,
                  (radius - 4.f) * 2.f, (radius - 4.f) * 2.f);

    juce::Path arcBg;
    arcBg.addCentredArc(cx, cy, radius + 4.f, radius + 4.f, 0.f,
                        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colours::black.withAlpha(.9f));
    g.strokePath(arcBg, juce::PathStrokeType(2.2f));

    juce::Path arc;
    arc.addCentredArc(cx, cy, radius + 4.f, radius + 4.f, 0.f,
                      rotaryStartAngle, angle, true);
    g.setColour(accent.withAlpha(.95f));
    g.strokePath(arc, juce::PathStrokeType(2.6f));

    const float tx = cx + std::cos(angle - juce::MathConstants<float>::halfPi) * radius * .66f;
    const float ty = cy + std::sin(angle - juce::MathConstants<float>::halfPi) * radius * .66f;
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(cx - 1.6f, cy - radius + 8.f, 3.2f, radius * .30f, 1.5f);
    g.setColour(accent);
    g.fillEllipse(tx - 2.f, ty - 2.f, 4.f, 4.f);
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

        if (active)
        {
            g.setColour(accent.withAlpha(.28f));
            g.fillEllipse(cx - d * .92f, cy - d * .92f, d * 1.84f, d * 1.84f);
        }
        return;
    }

    auto r = button.getLocalBounds().toFloat().reduced(1.f);
    const bool on = button.getToggleState();
    const auto accent = button.findColour(juce::ToggleButton::tickColourId);

    g.setColour(on ? juce::Colour(0xff353a42) : juce::Colour(0xff23262b));
    g.fillRoundedRectangle(r, 5.f);
    g.setColour(accent.withAlpha(on ? .7f : .35f));
    g.drawRoundedRectangle(r, 5.f, on ? 1.2f : 1.f);
    g.setColour(juce::Colour(0xffbcc3cc));
    g.setFont(juce::FontOptions(9.f).withStyle("Bold"));
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
        addBypass(i, bypassIds[(size_t) i], kModuleNames[(size_t) i],
                  i == 0 ? juce::Colour(0xff38bdf8)
                  : i == 1 ? juce::Colour(0xfffacc15)
                  : i == 2 ? juce::Colour(0xfff472b6)
                  : i == 3 ? juce::Colour(0xff67d3aa)
                            : juce::Colour(0xff9ed85c));

    for (int b = 0; b < 4; ++b)
    {
        const auto colour = kBandColours[(size_t) b];
        const auto n = juce::String(b + 1);

        addKnob("EQ" + n + "_FREQ", "FREQ", 20, 20000, 1,
                parameterValue("EQ" + n + "_FREQ"), " Hz", b, 0, colour);
        addKnob("EQ" + n + "_GAIN", "GAIN", -24, 24, .1,
                parameterValue("EQ" + n + "_GAIN"), " dB", b, 1, colour);
        addKnob("EQ" + n + "_Q", "Q", .1, 18, .01,
                parameterValue("EQ" + n + "_Q"), "", b, 2, colour);

        addKnob("OTT_DEGREE" + n, "OTT %", 0, 100, .1,
                parameterValue("OTT_DEGREE" + n), " %", b, 3, colour);
        addKnob("OTT_COMP_A" + n, "ATTACK", .1, 250, .1,
                parameterValue("OTT_COMP_A" + n), " ms", b, 4,
                juce::Colour(0xfffacc15));
        addKnob("OTT_COMP_R" + n, "RELEASE", 10, 2500, 1,
                parameterValue("OTT_COMP_R" + n), " ms", b, 5,
                juce::Colour(0xfffacc15));

        addKnob("ATYPE_DEGREE" + n, "TAPE-A +", 0, 100, .1,
                parameterValue("ATYPE_DEGREE" + n), "", b, 6, colour, true);

        if (b == 3)
        {
            addKnob("DEESS_FREQ", "DE-ESS FREQ", 6000, 18000, 10,
                    parameterValue("DEESS_FREQ"), " Hz", b, 7,
                    juce::Colour(0xff67d3aa));
            addKnob("DEESS_INTENSITY", "DE-ESS INT", 2, 10, .01,
                    parameterValue("DEESS_INTENSITY"), "", b, 8,
                    juce::Colour(0xff67d3aa));
        }

        advancedButtons[(size_t) b] = std::make_unique<juce::TextButton>("+ ADV");
        advancedButtons[(size_t) b]->setTooltip("開啟 Band " + n + " 的 OTT 進階設定");
        advancedButtons[(size_t) b]->onClick = [this, b]
        {
            setExpandedBand(expandedBand == b ? -1 : b);
        };
        addAndMakeVisible(*advancedButtons[(size_t) b]);
    }

    addKnob("EQ_COLOR", "ANALOG COLOR", 0, 100, .1,
            parameterValue("EQ_COLOR"), " %", -1, 0,
            juce::Colour(0xff60a5fa));
    addKnob("HF_CORNER", "HP / CORNER", 40, 120, 1,
            parameterValue("HF_CORNER"), " Hz", -1, 1,
            juce::Colour(0xff60a5fa));

    const std::array<std::tuple<juce::String, juce::String, double, double, double, juce::Colour>, 7> shared
    {{
        { "OTT_X1", "XOVER 1", 80, 600, 1, juce::Colour(0xfffacc15) },
        { "OTT_X2", "XOVER 2", 750, 3000, 1, juce::Colour(0xfffacc15) },
        { "OTT_X3", "XOVER 3", 6000, 12000, 1, juce::Colour(0xfffacc15) },
        { "OTT_INPUT", "INPUT", -24, 24, .1, juce::Colour(0xfffacc15) },
        { "OTT_GATE", "GATE", -90, 0, .1, juce::Colour(0xfffacc15) },
        { "OTT_MIX", "MASTER MIX", 0, 100, .1, juce::Colour(0xfffacc15) },
        { "OTT_OUTPUT", "OUTPUT", -24, 24, .1, juce::Colour(0xfffacc15) }
    }};

    for (const auto& item : shared)
    {
        const auto& id = std::get<0>(item);
        addKnob(id, std::get<1>(item),
                std::get<2>(item), std::get<3>(item), std::get<4>(item),
                parameterValue(id), id == "OTT_MIX" ? " %" : " dB",
                -2, -1, std::get<5>(item));
    }

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const auto c = kBandColours[(size_t) b];
        addKnob("OTT_LIFT_T" + n, "LIFT THRESH", -80, 0, .1,
                parameterValue("OTT_LIFT_T" + n), " dB", b, 10, c);
        addKnob("OTT_LIFT_A" + n, "LIFT ATT", 1, 500, .1,
                parameterValue("OTT_LIFT_A" + n), " ms", b, 11, c);
        addKnob("OTT_LIFT_R" + n, "LIFT REL", 10, 2500, 1,
                parameterValue("OTT_LIFT_R" + n), " ms", b, 12, c);
        addKnob("OTT_LIFT_M" + n, "LIFT MIX", 0, 100, .1,
                parameterValue("OTT_LIFT_M" + n), " %", b, 13, c);
        addKnob("OTT_COMP_T" + n, "COMP THRESH", -24, 0, .1,
                parameterValue("OTT_COMP_T" + n), " dB", b, 14, c);
        addKnob("OTT_COMP_M" + n, "COMP MIX", 0, 100, .1,
                parameterValue("OTT_COMP_M" + n), " %", b, 15, c);
        addKnob("OTT_LEVEL" + n, "BAND LEVEL", -24, 12, .1,
                parameterValue("OTT_LEVEL" + n), " dB", b, 16, c);
    }

    ottClipper = std::make_unique<juce::ToggleButton>("CLIPPER");
    ottClipper->setLookAndFeel(&metalLook);
    ottClipper->setColour(juce::ToggleButton::tickColourId,
                          juce::Colour(0xfffacc15));
    ottClipper->setTooltip("OTT Clipper");
    addAndMakeVisible(*ottClipper);
    auto clipAttach = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, "OTT_CLIPPER", *ottClipper);
    ottClipperAttachment = std::move(clipAttach);

    closeAdvanced = std::make_unique<juce::TextButton>("CLOSE");
    closeAdvanced->onClick = [this] { setExpandedBand(-1); };
    addAndMakeVisible(*closeAdvanced);

    // Global Mix / Out remains visible; Type-A global timing/trim stays at its
    // tuned defaults so the band control is the only visible Tape-A amount.
    addKnob("DRY_WET", "DRY / WET", 0, 100, .1,
            parameterValue("DRY_WET"), " %", -3, 0,
            juce::Colour(0xff38bdf8));
    addKnob("OUTPUT_LEVEL", "OUTPUT", -24, 12, .1,
            parameterValue("OUTPUT_LEVEL"), " dB", -3, 1,
            juce::Colour(0xff9ed85c));

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
    if (ottClipper)
        ottClipper->setLookAndFeel(nullptr);
    ottClipperAttachment.reset();

    setLookAndFeel(nullptr);
}

void VVChainAudioProcessorEditor::addKnob(
    const juce::String& id, const juce::String& title,
    double min, double max, double step, double defaultValue,
    const juce::String& suffix, int band, int slot,
    juce::Colour accent, bool tapeDisplayDb)
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
    k.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 16);
    k.slider->setRange(min, max, step);
    k.slider->setDoubleClickReturnValue(true, defaultValue);
    k.slider->setColour(juce::Slider::rotarySliderFillColourId, accent);
    k.slider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff08090b));
    k.slider->setNumDecimalPlacesToDisplay(
        step < .01 ? 3 : step < .1 ? 2 : step < 1 ? 1 : 0);
    if (tapeDisplayDb)
    {
        k.slider->setTextFromValueFunction([](double value)
        {
            return juce::String(value * .06, 1) + " dB";
        });
        k.slider->setValueFromTextFunction([](const juce::String& text)
        {
            return text.retainCharacters("0123456789.-").getDoubleValue() / .06;
        });
    }
    else
    {
        k.slider->setTextValueSuffix(suffix);
    }

    k.label->setText(title, juce::dontSendNotification);
    k.label->setColour(juce::Label::textColourId, accent.brighter(.35f));
    k.label->setJustificationType(juce::Justification::centred);
    k.label->setFont(juce::FontOptions(8.5f).withStyle("Bold"));

    k.attachment = std::make_unique<Attachment>(
        audioProcessor.apvts, id, *k.slider);

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
    return { 260.f, 72.f, (float) getWidth() - 280.f, 86.f };
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

float VVChainAudioProcessorEditor::eqDbToY(
    const juce::Rectangle<float>& graph, float db) const
{
    return graph.getBottom() - graph.getHeight()
        * juce::jlimit(0.f, 1.f, (db + 18.f) / 36.f);
}

void VVChainAudioProcessorEditor::drawEqGraph(
    juce::Graphics& g, juce::Rectangle<float> graph)
{
    juce::ColourGradient bg(juce::Colour(0xff111318), graph.getX(), graph.getY(),
                            juce::Colour(0xff1b1e24), graph.getRight(), graph.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(graph, 7.f);
    g.setColour(juce::Colours::black.withAlpha(.95f));
    g.drawRoundedRectangle(graph, 7.f, 1.f);

    for (int i = 0; i <= 8; ++i)
    {
        const float db = 18.f - i * 4.5f;
        const float y = eqDbToY(graph, db);
        g.setColour(juce::Colour(0xff4b5360).withAlpha(.45f));
        g.drawHorizontalLine((int) y, graph.getX(), graph.getRight());
    }

    for (float f : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = graphFrequencyToX(graph, f);
        g.setColour(juce::Colour(0xff56606b).withAlpha(.38f));
        g.drawVerticalLine((int) x, graph.getY(), graph.getBottom());
    }

    juce::Path response;
    for (int i = 0; i <= 260; ++i)
    {
        const float hz = invLogMap(i / 260.f, 20.f, 20000.f);
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

        const auto pt = juce::Point<float>(graphFrequencyToX(graph, hz), eqDbToY(graph, db));
        if (i == 0) response.startNewSubPath(pt);
        else response.lineTo(pt);
    }

    g.setColour(juce::Colours::white.withAlpha(.92f));
    g.strokePath(response, juce::PathStrokeType(2.0f));

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float x = graphFrequencyToX(graph, parameterValue("EQ" + n + "_FREQ"));
        const float y = eqDbToY(graph, parameterValue("EQ" + n + "_GAIN"));
        const auto c = kBandColours[(size_t) b];

        g.setColour(c.withAlpha(.16f));
        g.fillEllipse(x - 12.f, y - 12.f, 24.f, 24.f);
        g.setColour(c);
        g.fillEllipse(x - 6.f, y - 6.f, 12.f, 12.f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions(8.f).withStyle("Bold"));
        g.drawText(juce::String(b + 1), (int) x - 5, (int) y - 5, 10, 10,
                   juce::Justification::centred);
    }

    g.setColour(juce::Colour(0xffaab0ba));
    g.setFont(juce::FontOptions(8.f));
    g.drawText("20", (int) graph.getX() + 3, (int) graph.getBottom() - 13, 24, 12,
               juce::Justification::left);
    g.drawText("20k", (int) graph.getRight() - 25, (int) graph.getBottom() - 13, 24, 12,
               juce::Justification::right);
    g.drawText("EQ RESPONSE · DRAG NODE", (int) graph.getX() + 8, (int) graph.getY() + 5,
               190, 13, juce::Justification::left);
}

void VVChainAudioProcessorEditor::drawCard(
    juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent,
    const juce::String& title, const juce::String& subtitle)
{
    juce::ColourGradient bg(juce::Colour(0xff2d3036), r.getX(), r.getY(),
                            juce::Colour(0xff15171b), r.getRight(), r.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(r, 8.f);

    g.setColour(juce::Colours::black.withAlpha(.9f));
    g.drawRoundedRectangle(r, 8.f, 1.f);
    g.setColour(accent.withAlpha(.22f));
    g.drawRoundedRectangle(r.reduced(2.f), 6.f, 1.f);

    g.setColour(accent);
    g.fillRoundedRectangle(r.getX(), r.getY(), 4.f, r.getHeight(), 2.f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(13.f).withStyle("Bold"));
    g.drawText(title, (int) r.getX() + 14, (int) r.getY() + 8, 84, 18,
               juce::Justification::left);

    g.setColour(juce::Colour(0xff747b86));
    g.setFont(juce::FontOptions(8.f));
    g.drawText(subtitle, (int) r.getX() + 14, (int) r.getY() + 25,
               (int) r.getWidth() - 118, 13, juce::Justification::left);
}

void VVChainAudioProcessorEditor::drawPanel(
    juce::Graphics& g, juce::Rectangle<float> r,
    const juce::String& title, const juce::String& subtitle, juce::Colour accent)
{
    juce::ColourGradient bg(juce::Colour(0xff24272d), r.getX(), r.getY(),
                            juce::Colour(0xff111318), r.getX(), r.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(r, 8.f);
    g.setColour(juce::Colours::black.withAlpha(.92f));
    g.drawRoundedRectangle(r, 8.f, 1.f);
    g.setColour(accent.withAlpha(.7f));
    g.fillRoundedRectangle(r.getX(), r.getY(), 4.f, r.getHeight(), 2.f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(12.f).withStyle("Bold"));
    g.drawText(title, (int) r.getX() + 12, (int) r.getY() + 7, 280, 18,
               juce::Justification::left);
    g.setColour(juce::Colour(0xff7f8791));
    g.setFont(juce::FontOptions(8.f));
    g.drawText(subtitle, (int) r.getX() + 12, (int) r.getY() + 25,
               (int) r.getWidth() - 100, 13, juce::Justification::left);
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

    g.setColour(juce::Colours::black.withAlpha(.85f));
    g.fillRect(0, 68, getWidth(), 2);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(24.f).withStyle("Bold"));
    g.drawText("VVCHAIN", 18, 10, 250, 28, juce::Justification::left);

    g.setColour(juce::Colour(0xff85909c));
    g.setFont(juce::FontOptions(8.5f));
    g.drawText("FREQUALIZER ENGINE · 4-BAND COMPACT CHAIN", 20, 39, 330, 14,
               juce::Justification::left);

    g.setColour(juce::Colour(0xff79818c));
    g.setFont(juce::FontOptions(8.f));
    g.drawText("EQ → OTT / PUNKOTT-MB → TAPE-A → DE-ESSER → MIX / OUT",
               350, 42, 560, 14, juce::Justification::left);

    const auto graph = eqGraphBounds();
    drawEqGraph(g, graph);

    drawPanel(g, { 18.f, 72.f, 224.f, 86.f },
              "EQ / ANALOG", "SHARED", juce::Colour(0xff60a5fa));

    const auto bandsArea = juce::Rectangle<float>(
        18.f, 168.f, (float) getWidth() - 36.f, 470.f);
    const float gap = 8.f;
    const float cw = (bandsArea.getWidth() - gap * 3.f) / 4.f;

    for (int b = 0; b < 4; ++b)
    {
        const float x = bandsArea.getX() + b * (cw + gap);
        drawCard(g, { x, bandsArea.getY(), cw, bandsArea.getHeight() },
                 kBandColours[(size_t) b],
                 "BAND " + juce::String(b + 1),
                 b == 3 ? "EQ · OTT · TAPE-A · DE-ESSER" : "EQ · OTT · TAPE-A");
    }

    const auto bottom = juce::Rectangle<float>(
        18.f, 646.f, (float) getWidth() - 36.f, 264.f);
    const auto mix = juce::Rectangle<float>(
        bottom.getRight() - 290.f, bottom.getY(), 290.f, bottom.getHeight());
    const auto adv = juce::Rectangle<float>(
        bottom.getX(), bottom.getY(),
        bottom.getWidth() - 300.f, bottom.getHeight());

    if (expandedBand >= 0)
    {
        drawPanel(g, adv,
                  "OTT ADVANCED · BAND " + juce::String(expandedBand + 1),
                  "LIFTER + COMP DETAIL / CROSSOVERS / MASTER",
                  juce::Colour(0xfffacc15));
    }
    else
    {
        drawPanel(g, adv,
                  "OTT ADVANCED",
                  "點 BAND 的 + ADV 開啟；預設最佳值已套用",
                  juce::Colour(0xfffacc15));
        g.setColour(juce::Colour(0xff757d88));
        g.setFont(juce::FontOptions(9.f));
        g.drawText("DEGREE / ATTACK / RELEASE 只有必要控制；其餘 OTT 參數集中於此。",
                   (int) adv.getX() + 14, (int) adv.getY() + 56,
                   (int) adv.getWidth() - 28, 18, juce::Justification::left);
    }

    drawPanel(g, mix, "MIX / OUT", "GLOBAL", juce::Colour(0xff9ed85c));

    g.setColour(juce::Colour(0xff5f6670));
    g.setFont(juce::FontOptions(7.5f));
    g.drawText("DOUBLE-CLICK KNOB = RESET · NO PAGE SCROLL",
               18, 916, 440, 10, juce::Justification::left);
    drawModuleLeds(g);

    if (expandedBand >= 0)
    {
        g.setColour(juce::Colour(0xfffacc15));
        g.setFont(juce::FontOptions(8.f).withStyle("Bold"));
        g.drawText("BAND " + juce::String(expandedBand + 1) + " OTT ADVANCED",
                   (int) adv.getX() + 14, (int) adv.getY() + 41,
                   210, 14, juce::Justification::left);
    }
}

void VVChainAudioProcessorEditor::setExpandedBand(int band)
{
    expandedBand = band;

    for (int b = 0; b < 4; ++b)
    {
        if (advancedButtons[(size_t) b])
        {
            advancedButtons[(size_t) b]->setButtonText(expandedBand == b ? "- ADV" : "+ ADV");
            advancedButtons[(size_t) b]->setColour(
                juce::TextButton::buttonColourId,
                expandedBand == b ? juce::Colour(0xff3e3517)
                                  : juce::Colour(0xff17191d));
        }
    }

    if (closeAdvanced)
        closeAdvanced->setVisible(expandedBand >= 0);

    for (auto& k : knobs)
    {
        if (k.band >= 0 && k.slot >= 10)
            k.slider->setVisible(expandedBand >= 0 && k.band == expandedBand);
        if (k.band >= 0 && k.slot >= 10)
            k.label->setVisible(expandedBand >= 0 && k.band == expandedBand);
    }

    const std::array<juce::String, 7> sharedIds
    {
        "OTT_X1", "OTT_X2", "OTT_X3", "OTT_INPUT", "OTT_GATE", "OTT_MIX", "OTT_OUTPUT"
    };

    for (const auto& id : sharedIds)
        if (auto* k = findKnob(id))
        {
            k->slider->setVisible(expandedBand >= 0);
            k->label->setVisible(expandedBand >= 0);
        }

    if (ottClipper)
        ottClipper->setVisible(expandedBand >= 0);

    resized();
    repaint();
}

void VVChainAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int cardY = 168;
    const int cardH = 470;
    const int gap = 8;
    const int left = 18;
    const int cardW = (w - left * 2 - gap * 3) / 4;

    // Five status LEDs at the top-right. Bright = active; dark = bypass.
    for (int i = 0; i < 5; ++i)
    {
        if (bypassButtons[(size_t) i])
            bypassButtons[(size_t) i]->setBounds(w - 250 + i * 46, 12, 28, 28);
    }

    placeKnob("EQ_COLOR", { 30, 90, 90, 62 });
    placeKnob("HF_CORNER", { 132, 90, 90, 62 });

    for (int b = 0; b < 4; ++b)
    {
        const int x = left + b * (cardW + gap);
        if (advancedButtons[(size_t) b])
            advancedButtons[(size_t) b]->setBounds(x + cardW - 62, cardY + 7, 54, 22);

        const int innerX = x + 9;
        const int cellW = (cardW - 30) / 3;

        const std::array<juce::String, 9> ids =
        {
            "EQ" + juce::String(b + 1) + "_FREQ",
            "EQ" + juce::String(b + 1) + "_GAIN",
            "EQ" + juce::String(b + 1) + "_Q",
            "OTT_DEGREE" + juce::String(b + 1),
            "OTT_COMP_A" + juce::String(b + 1),
            "OTT_COMP_R" + juce::String(b + 1),
            "ATYPE_DEGREE" + juce::String(b + 1),
            "DEESS_FREQ",
            "DEESS_INTENSITY"
        };

        for (int slot = 0; slot < 7; ++slot)
        {
            const int row = slot / 3;
            const int col = slot % 3;
            placeKnob(ids[(size_t) slot],
                      { innerX + col * (cellW + 5),
                        cardY + 46 + row * 125,
                        cellW, 108 });
        }

        if (b == 3)
        {
            placeKnob(ids[7], { innerX + 1 * (cellW + 5), cardY + 421, cellW, 0 });
            placeKnob(ids[8], { innerX + 2 * (cellW + 5), cardY + 421, cellW, 0 });
            // Third-row de-esser controls are deliberately compact and share the TAPE-A row.
            placeKnob(ids[7], { innerX + cellW + 5, cardY + 305, cellW, 108 });
            placeKnob(ids[8], { innerX + 2 * (cellW + 5), cardY + 305, cellW, 108 });
        }
        else
        {
            placeKnob(ids[6], { innerX + cellW, cardY + 305, cellW, 108 });
        }
    }

    const int bottomY = 646;
    const int bottomH = 264;
    const int mixX = w - 290 - 18;

    placeKnob("DRY_WET", { mixX + 18, bottomY + 65, 116, 140 });
    placeKnob("OUTPUT_LEVEL", { mixX + 146, bottomY + 65, 116, 140 });

    const int advX = 18;
    const int advW = mixX - advX - 10;

    if (closeAdvanced)
        closeAdvanced->setBounds(mixX - 72, bottomY + 7, 58, 22);

    if (expandedBand >= 0)
    {
        const int rowY = bottomY + 54;
        const int cellW = (advW - 8 * 6) / 7;
        const std::array<juce::String, 7> advIds
        {
            "OTT_LIFT_T", "OTT_LIFT_A", "OTT_LIFT_R", "OTT_LIFT_M",
            "OTT_COMP_T", "OTT_COMP_M", "OTT_LEVEL"
        };
        for (int i = 0; i < 7; ++i)
        {
            const auto idBase = advIds[(size_t) i];
            const auto id = idBase + juce::String(expandedBand + 1);
            placeKnob(id, { advX + 10 + i * (cellW + 8), rowY, cellW, 96 });
        }

        const int row2Y = bottomY + 158;
        const int sharedW = (advW - 8 * 6) / 7;
        const std::array<juce::String, 7> sharedIds
        {
            "OTT_X1", "OTT_X2", "OTT_X3", "OTT_INPUT", "OTT_GATE", "OTT_MIX", "OTT_OUTPUT"
        };
        for (int i = 0; i < 7; ++i)
            placeKnob(sharedIds[(size_t) i],
                      { advX + 10 + i * (sharedW + 8), row2Y, sharedW, 96 });

        if (ottClipper)
            ottClipper->setBounds(advX + 10, bottomY + 222, 86, 28);
    }

    repaint();
}

void VVChainAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    const auto pos = event.position;
    const auto graph = eqGraphBounds();
    if (!graph.contains(pos))
        return;

    float best = 18.f;
    dragBand = -1;

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float x = graphFrequencyToX(graph, parameterValue("EQ" + n + "_FREQ"));
        const float y = eqDbToY(graph, parameterValue("EQ" + n + "_GAIN"));
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
    const auto pos = event.position;
    const auto n = juce::String(dragBand + 1);

    const float hz = juce::jlimit(20.f, 20000.f, graphXToFrequency(graph, pos.x));
    const float db = juce::jlimit(-24.f, 24.f,
                                  18.f - ((pos.y - graph.getY()) / graph.getHeight()) * 36.f);

    setParameter("EQ" + n + "_FREQ", hz);
    setParameter("EQ" + n + "_GAIN", db);
    repaint();
}

void VVChainAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    dragBand = -1;
}
