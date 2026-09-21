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

juce::Rectangle<float> inset(juce::Rectangle<float> r, float amount)
{
    return r.reduced(amount);
}

void drawCard(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent)
{
    juce::ColourGradient bg(juce::Colour(0xff292c32), r.getX(), r.getY(),
                            juce::Colour(0xff15171b), r.getRight(), r.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(r, 7.f);

    g.setColour(juce::Colours::black.withAlpha(0.95f));
    g.drawRoundedRectangle(r, 7.f, 1.f);

    g.setColour(accent.withAlpha(0.20f));
    g.drawRoundedRectangle(inset(r, 2.f), 5.f, 1.f);
}

void drawSection(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent,
                 const juce::String& title, const juce::String& subtitle)
{
    juce::ColourGradient bg(juce::Colour(0xff25282e), r.getX(), r.getY(),
                            juce::Colour(0xff111317), r.getX(), r.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(r, 9.f);

    g.setColour(juce::Colours::black.withAlpha(0.95f));
    g.drawRoundedRectangle(r, 9.f, 1.f);

    g.setColour(accent.withAlpha(0.85f));
    g.fillRoundedRectangle(r.getX(), r.getY(), 4.f, r.getHeight(), 2.f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15.f).withStyle("Bold"));
    g.drawText(title, (int)r.getX() + 18, (int)r.getY() + 9, 220, 22,
               juce::Justification::left);

    g.setColour(juce::Colour(0xff8f949e));
    g.setFont(juce::FontOptions(9.f));
    g.drawText(subtitle, (int)r.getX() + 18, (int)r.getY() + 31,
               (int)r.getWidth() - 36, 16, juce::Justification::left);
}
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider)
{
    const auto area = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height)
        .reduced(5.f);
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f - 4.f;
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);

    const float angle = juce::jmap(sliderPosProportional, rotaryStartAngle, rotaryEndAngle);

    g.setColour(juce::Colours::black.withAlpha(0.95f));
    g.fillEllipse(cx - radius - 3.f, cy - radius - 3.f,
                  2.f * (radius + 3.f), 2.f * (radius + 3.f));

    juce::ColourGradient rim(juce::Colour(0xff666a72), cx, cy - radius,
                             juce::Colour(0xff1b1d21), cx, cy + radius, false);
    g.setGradientFill(rim);
    g.fillEllipse(cx - radius, cy - radius, 2.f * radius, 2.f * radius);

    juce::ColourGradient face(juce::Colour(0xff454952), cx, cy - radius * .8f,
                              juce::Colour(0xff191b20), cx, cy + radius, false);
    g.setGradientFill(face);
    g.fillEllipse(cx - radius + 4.f, cy - radius + 4.f,
                  2.f * (radius - 4.f), 2.f * (radius - 4.f));

    juce::Path arcBg;
    arcBg.addCentredArc(cx, cy, radius + 5.f, radius + 5.f, 0.f,
                        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colours::black.withAlpha(0.95f));
    g.strokePath(arcBg, juce::PathStrokeType(2.5f));

    juce::Path arc;
    arc.addCentredArc(cx, cy, radius + 5.f, radius + 5.f, 0.f,
                      rotaryStartAngle, angle, true);
    g.setColour(accent.withAlpha(0.92f));
    g.strokePath(arc, juce::PathStrokeType(2.7f));

    const float tx = cx + std::cos(angle - juce::MathConstants<float>::halfPi) * (radius * .68f);
    const float ty = cy + std::sin(angle - juce::MathConstants<float>::halfPi) * (radius * .68f);
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(cx - 1.6f, cy - radius + 8.f, 3.2f, radius * .32f, 1.5f);
    g.setColour(accent);
    g.fillEllipse(tx - 2.f, ty - 2.f, 4.f, 4.f);
}

void VVChainAudioProcessorEditor::MetalLookAndFeel::drawToggleButton(
    juce::Graphics& g, juce::ToggleButton& button,
    bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto r = button.getLocalBounds().toFloat().reduced(1.f);
    const bool on = button.getToggleState();
    const auto accent = button.findColour(juce::ToggleButton::tickColourId);

    juce::ColourGradient bg(on ? juce::Colour(0xff3b4650) : juce::Colour(0xff292c31),
                            r.getX(), r.getY(), juce::Colour(0xff111317),
                            r.getX(), r.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(r, 5.f);

    g.setColour(accent.withAlpha(on ? 0.9f : 0.25f));
    g.drawRoundedRectangle(r, 5.f, on ? 1.4f : 1.f);

    g.setColour(on ? accent : juce::Colour(0xff8c919a));
    g.setFont(juce::FontOptions(10.f).withStyle("Bold"));
    g.drawText(button.getButtonText(), r.toNearestInt(), juce::Justification::centred);

    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

VVChainAudioProcessorEditor::VVChainAudioProcessorEditor(VVChainAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), page(*this)
{
    setLookAndFeel(&metalLook);
    setResizable(true, true);
    setSize(1500, 1020);

    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(11);
    viewport.setViewedComponent(&page, false);
    addAndMakeVisible(viewport);

    const std::array<juce::String, 5> bypassIds
    {
        "EQ_BYPASS", "OTT_BYPASS", "ATYPE_BYPASS", "DEESS_BYPASS", "MIX_BYPASS"
    };
    const std::array<juce::String, 5> bypassNames
    {
        "EQ / ANALOG", "OTT", "TYPE-A", "DE-ESSER", "MIX / OUT"
    };

    for (int i = 0; i < 5; ++i)
        addBypass(i, bypassIds[(size_t)i], bypassNames[(size_t)i]);

    for (int b = 0; b < 4; ++b)
    {
        const int group = 10 + b;
        const auto c = kBandColours[(size_t)b];
        const auto n = juce::String(b + 1);
        addKnob("EQ" + n + "_FREQ", "FREQUENCY", 20, 20000, 1, parameterValue("EQ" + n + "_FREQ"), " Hz", group, 0, c);
        addKnob("EQ" + n + "_GAIN", "GAIN", -24, 24, .1, parameterValue("EQ" + n + "_GAIN"), " dB", group, 1, c);
        addKnob("EQ" + n + "_Q", "Q", .1, 18, .01, parameterValue("EQ" + n + "_Q"), "", group, 2, c);
    }
    addKnob("EQ_COLOR", "ANALOG COLOR", 0, 100, .1, parameterValue("EQ_COLOR"), " %", 15, 0, juce::Colour(0xff60a5fa));
    addKnob("HF_CORNER", "HP / CORNER", 40, 120, 1, parameterValue("HF_CORNER"), " Hz", 15, 1, juce::Colour(0xff60a5fa));

    for (int b = 0; b < 4; ++b)
    {
        const int group = 20 + b;
        const auto c = kBandColours[(size_t)b];
        const auto n = juce::String(b + 1);
        addKnob("OTT_DEGREE" + n, "DEGREE", 0, 100, .1, parameterValue("OTT_DEGREE" + n), " %", group, 0, c);
        addKnob("OTT_LIFT_T" + n, "LIFTER THRESH", -80, 0, .1, parameterValue("OTT_LIFT_T" + n), " dB", group, 1, c);
        addKnob("OTT_LIFT_A" + n, "LIFTER ATT", 1, 500, .1, parameterValue("OTT_LIFT_A" + n), " ms", group, 2, c);
        addKnob("OTT_LIFT_R" + n, "LIFTER REL", 10, 2500, 1, parameterValue("OTT_LIFT_R" + n), " ms", group, 3, c);
        addKnob("OTT_LIFT_M" + n, "LIFTER MIX", 0, 100, .1, parameterValue("OTT_LIFT_M" + n), " %", group, 4, c);
        addKnob("OTT_COMP_T" + n, "COMP THRESH", -24, 0, .1, parameterValue("OTT_COMP_T" + n), " dB", group, 5, c);
        addKnob("OTT_COMP_A" + n, "COMP ATT", .1, 250, .1, parameterValue("OTT_COMP_A" + n), " ms", group, 6, c);
        addKnob("OTT_COMP_R" + n, "COMP REL", 10, 2500, 1, parameterValue("OTT_COMP_R" + n), " ms", group, 7, c);
        addKnob("OTT_COMP_M" + n, "COMP MIX", 0, 100, .1, parameterValue("OTT_COMP_M" + n), " %", group, 8, c);
        addKnob("OTT_LEVEL" + n, "BAND LEVEL", -24, 12, .1, parameterValue("OTT_LEVEL" + n), " dB", group, 9, c);
    }

    addKnob("OTT_X1", "XOVER 1", 80, 600, 1, parameterValue("OTT_X1"), " Hz", 25, 0, juce::Colour(0xfffacc15));
    addKnob("OTT_X2", "XOVER 2", 750, 3000, 1, parameterValue("OTT_X2"), " Hz", 25, 1, juce::Colour(0xfffacc15));
    addKnob("OTT_X3", "XOVER 3", 6000, 12000, 1, parameterValue("OTT_X3"), " Hz", 25, 2, juce::Colour(0xfffacc15));
    addKnob("OTT_INPUT", "INPUT", -24, 24, .1, parameterValue("OTT_INPUT"), " dB", 25, 3, juce::Colour(0xfffacc15));
    addKnob("OTT_GATE", "GATE", -90, 0, .1, parameterValue("OTT_GATE"), " dB", 25, 4, juce::Colour(0xfffacc15));
    addKnob("OTT_MIX", "MASTER MIX", 0, 100, .1, parameterValue("OTT_MIX"), " %", 25, 5, juce::Colour(0xfffacc15));
    addKnob("OTT_OUTPUT", "OUTPUT", -24, 24, .1, parameterValue("OTT_OUTPUT"), " dB", 25, 6, juce::Colour(0xfffacc15));

    ottClipper = std::make_unique<juce::ToggleButton>("CLIP");
    ottClipper->setLookAndFeel(&metalLook);
    ottClipperAttachment = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, "OTT_CLIPPER", *ottClipper);
    page.addAndMakeVisible(*ottClipper);

    for (int b = 0; b < 4; ++b)
    {
        const int group = 30 + b;
        const auto c = kBandColours[(size_t)b];
        const auto n = juce::String(b + 1);
        addKnob("ATYPE_DEGREE" + n, "TYPE-A DEGREE", 0, 100, .1, parameterValue("ATYPE_DEGREE" + n), " %", group, 0, c);
        addKnob("ATYPE_LEVEL" + n, "BAND LEVEL", -6, 6, .1, parameterValue("ATYPE_LEVEL" + n), " dB", group, 1, c);
    }

    addKnob("ATYPE_ATTACK", "ATTACK", 1, 100, .1, parameterValue("ATYPE_ATTACK"), " ms", 35, 0, juce::Colour(0xfff472b6));
    addKnob("ATYPE_RELEASE", "RELEASE", 20, 500, 1, parameterValue("ATYPE_RELEASE"), " ms", 35, 1, juce::Colour(0xfff472b6));
    addKnob("ATYPE_INPUT", "INPUT", -24, 24, .1, parameterValue("ATYPE_INPUT"), " dB", 35, 2, juce::Colour(0xfff472b6));
    addKnob("ATYPE_MIX", "MIX", 0, 100, .1, parameterValue("ATYPE_MIX"), " %", 35, 3, juce::Colour(0xfff472b6));
    addKnob("ATYPE_OUTPUT", "OUTPUT", -24, 24, .1, parameterValue("ATYPE_OUTPUT"), " dB", 35, 4, juce::Colour(0xfff472b6));

    deEssVoice.addItem("Male Vocal · 12.5 kHz", 1);
    deEssVoice.addItem("Female Vocal · 13.5 kHz", 2);
    deEssVoice.setLookAndFeel(&metalLook);
    deEssVoiceAttachment = std::make_unique<ComboAttachment>(
        audioProcessor.apvts, "DEESS_VOICE", deEssVoice);
    page.addAndMakeVisible(deEssVoice);

    addKnob("DEESS_INTENSITY", "INTENSITY", 2, 10, .01, parameterValue("DEESS_INTENSITY"), "", 40, 0, juce::Colour(0xff67d3aa));
    addKnob("DEESS_OFFSET", "OFFSET", -.1, .1, .0001, parameterValue("DEESS_OFFSET"), "", 40, 1, juce::Colour(0xff67d3aa));

    addKnob("DRY_WET", "DRY / WET", 0, 100, .1, parameterValue("DRY_WET"), " %", 50, 0, juce::Colour(0xff38bdf8));
    addKnob("OUTPUT_LEVEL", "OUTPUT", -24, 12, .1, parameterValue("OUTPUT_LEVEL"), " dB", 50, 1, juce::Colour(0xff9ed85c));

    page.setSize(1480, 2050);
}

void VVChainAudioProcessorEditor::addKnob(
    const juce::String& id, const juce::String& title,
    double min, double max, double step, double defaultValue,
    const juce::String& suffix, int group, int slot, juce::Colour accent)
{
    Knob k;
    k.group = group;
    k.slot = slot;
    k.accent = accent;
    k.slider = std::make_unique<juce::Slider>();
    k.label = std::make_unique<juce::Label>();

    k.slider->setLookAndFeel(&metalLook);
    k.slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 18);
    k.slider->setRange(min, max, step);
    k.slider->setDoubleClickReturnValue(true, defaultValue);
    k.slider->setTextValueSuffix(suffix);
    k.slider->setColour(juce::Slider::rotarySliderFillColourId, accent);
    k.slider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff0d0f12));
    k.slider->setNumDecimalPlacesToDisplay(step < .001 ? 4 : step < .01 ? 3 : step < .1 ? 2 : step < 1 ? 1 : 0);

    k.label->setText(title, juce::dontSendNotification);
    k.label->setColour(juce::Label::textColourId, juce::Colour(0xffc9cdd5));
    k.label->setJustificationType(juce::Justification::centred);
    k.label->setFont(juce::FontOptions(9.f).withStyle("Bold"));

    k.attachment = std::make_unique<Attachment>(
        audioProcessor.apvts, id, *k.slider);

    page.addAndMakeVisible(*k.slider);
    page.addAndMakeVisible(*k.label);
    knobs.push_back(std::move(k));
}

void VVChainAudioProcessorEditor::addBypass(int moduleIndex, const juce::String& parameterId,
                                            const juce::String& title)
{
    auto b = std::make_unique<juce::ToggleButton>("BYPASS");
    b->setLookAndFeel(&metalLook);
    b->setColour(juce::ToggleButton::tickColourId, kBandColours[(size_t)moduleIndex % 4]);
    b->setTooltip(title);
    bypassAttachments[(size_t)moduleIndex] = std::make_unique<BoolAttachment>(
        audioProcessor.apvts, parameterId, *b);
    page.addAndMakeVisible(*b);
    bypassButtons[(size_t)moduleIndex] = std::move(b);
}

float VVChainAudioProcessorEditor::parameterValue(const juce::String& id) const
{
    if (auto* p = audioProcessor.apvts.getRawParameterValue(id))
        return p->load();
    return 0.f;
}

void VVChainAudioProcessorEditor::setParameter(const juce::String& id, float value)
{
    if (auto* p = audioProcessor.apvts.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

juce::Rectangle<float> VVChainAudioProcessorEditor::graphBounds() const
{
    return { 24.f, 86.f, (float)page.getWidth() - 48.f, 310.f };
}

float VVChainAudioProcessorEditor::graphFrequencyToX(
    const juce::Rectangle<float>& graph, float hz) const
{
    return graph.getX() + graph.getWidth() * logMap(hz, 20.f, 20000.f);
}

float VVChainAudioProcessorEditor::graphXToFrequency(
    const juce::Rectangle<float>& graph, float x) const
{
    const float t = juce::jlimit(0.f, 1.f, (x - graph.getX()) / graph.getWidth());
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
    juce::ColourGradient bg(juce::Colour(0xff101318), graph.getX(), graph.getY(),
                            juce::Colour(0xff1b1e24), graph.getRight(), graph.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(graph, 8.f);

    g.setColour(juce::Colours::black.withAlpha(0.95f));
    g.drawRoundedRectangle(graph, 8.f, 1.f);

    for (int i = 0; i < 9; ++i)
    {
        const float db = -18.f + i * 4.5f;
        const float y = eqDbToY(graph, db);
        g.setColour(db == 0.f ? juce::Colours::white.withAlpha(0.55f)
                              : juce::Colour(0xff56606b).withAlpha(0.45f));
        g.drawHorizontalLine((int)y, graph.getX(), graph.getRight());
        g.setColour(juce::Colour(0xff9aa0aa));
        g.setFont(juce::FontOptions(9.f));
        g.drawText(juce::String(db, db == 0.f ? 0 : 1) + " dB",
                   (int)graph.getX() + 5, (int)y - 8, 42, 15,
                   juce::Justification::left);
    }

    for (float f : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = graphFrequencyToX(graph, f);
        g.setColour(juce::Colour(0xff56606b).withAlpha(0.5f));
        g.drawVerticalLine((int)x, graph.getY(), graph.getBottom());
        g.setColour(juce::Colour(0xff9aa0aa));
        g.drawText(f >= 1000.f ? juce::String(f / 1000.f, f >= 10000.f ? 0 : 1) + "k"
                               : juce::String((int)f),
                   (int)x + 3, (int)graph.getBottom() - 18, 35, 14,
                   juce::Justification::left);
    }

    juce::Path response;
    for (int i = 0; i <= 300; ++i)
    {
        const float hz = invLogMap(i / 300.f, 20.f, 20000.f);
        float db = 0.f;
        for (int b = 0; b < 4; ++b)
        {
            const auto n = juce::String(b + 1);
            const float f0 = parameterValue("EQ" + n + "_FREQ");
            const float gain = parameterValue("EQ" + n + "_GAIN");
            const float q = juce::jmax(.1f, parameterValue("EQ" + n + "_Q"));
            const float xx = std::log(std::max(hz, 20.f) / std::max(f0, 20.f));
            const float width = juce::jmax(.02f, 1.f / (q * 1.8f));
            db += gain * std::exp(-(xx * xx) / (2.f * width * width));
        }

        const juce::Point<float> pt(graphFrequencyToX(graph, hz), eqDbToY(graph, db));
        if (i == 0) response.startNewSubPath(pt);
        else response.lineTo(pt);
    }

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.strokePath(response, juce::PathStrokeType(2.4f));

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const auto c = kBandColours[(size_t)b];
        const float x = graphFrequencyToX(graph, parameterValue("EQ" + n + "_FREQ"));
        const float y = eqDbToY(graph, parameterValue("EQ" + n + "_GAIN"));

        if (selectedBand == b)
        {
            g.setColour(c.withAlpha(0.15f));
            g.fillEllipse(x - 16.f, y - 16.f, 32.f, 32.f);
        }
        g.setColour(c);
        g.fillEllipse(x - 7.f, y - 7.f, 14.f, 14.f);
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.drawEllipse(x - 7.f, y - 7.f, 14.f, 14.f, 1.f);
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions(9.f).withStyle("Bold"));
        g.drawText(juce::String(b + 1), (int)x - 6, (int)y - 6, 12, 12, juce::Justification::centred);
    }
}

void VVChainAudioProcessorEditor::drawOttOverview(
    juce::Graphics& g, juce::Rectangle<float> r)
{
    const float xs[3] = {
        parameterValue("OTT_X1"), parameterValue("OTT_X2"), parameterValue("OTT_X3")
    };
    for (int i = 0; i < 3; ++i)
    {
        const float x = r.getX() + r.getWidth() * logMap(xs[i], 20.f, 20000.f);
        g.setColour(juce::Colour(0xffffc75a).withAlpha(.55f));
        g.drawVerticalLine((int)x, (int)r.getY() + 3, (int)r.getBottom() - 3);
    }

    const float centres[4] = {
        std::sqrt(20.f * xs[0]),
        std::sqrt(xs[0] * xs[1]),
        std::sqrt(xs[1] * xs[2]),
        std::sqrt(xs[2] * 18000.f)
    };

    for (int b = 0; b < 4; ++b)
    {
        const float degree = parameterValue("OTT_DEGREE" + juce::String(b + 1));
        const float x = r.getX() + r.getWidth() * logMap(centres[b], 20.f, 20000.f);
        const float y = r.getBottom() - 10.f - degree / 100.f * (r.getHeight() - 24.f);
        g.setColour(kBandColours[(size_t)b].withAlpha(.8f));
        g.fillEllipse(x - 5.f, y - 5.f, 10.f, 10.f);
        g.drawLine(x, y, x, r.getBottom() - 12.f, 1.5f);
    }

    g.setColour(juce::Colour(0xffaeb4bd));
    g.setFont(juce::FontOptions(9.f));
    g.drawText("GATE 6:1  →  LIFTER 6:1  →  COMPRESSOR 8:1  →  LIMITER",
               (int)r.getX() + 8, (int)r.getBottom() - 16,
               (int)r.getWidth() - 16, 14, juce::Justification::left);
}

void VVChainAudioProcessorEditor::drawTypeOverview(
    juce::Graphics& g, juce::Rectangle<float> r)
{
    for (float f : { 80.f, 3000.f, 9000.f })
    {
        const float x = r.getX() + r.getWidth() * logMap(f, 20.f, 20000.f);
        g.setColour(juce::Colour(0xff9aa0aa).withAlpha(.45f));
        g.drawVerticalLine((int)x, (int)r.getY() + 3, (int)r.getBottom() - 3);
    }

    const float centres[4] = { 40.f, 700.f, 5200.f, 12000.f };
    for (int b = 0; b < 4; ++b)
    {
        const float degree = parameterValue("ATYPE_DEGREE" + juce::String(b + 1));
        const float x = r.getX() + r.getWidth() * logMap(centres[b], 20.f, 20000.f);
        const float y = r.getBottom() - 8.f - degree / 100.f * (r.getHeight() - 16.f);
        g.setColour(kBandColours[(size_t)b].withAlpha(.82f));
        g.drawLine(x, r.getBottom() - 8.f, x, y, 2.f);
        g.fillEllipse(x - 4.f, y - 4.f, 8.f, 8.f);
    }

    g.setColour(juce::Colour(0xffaeb4bd));
    g.setFont(juce::FontOptions(9.f));
    g.drawText("TYPE-A  •  80 Hz LP  /  80 Hz–3 kHz  /  >3 kHz  /  >9 kHz",
               (int)r.getX() + 8, (int)r.getBottom() - 16,
               (int)r.getWidth() - 16, 14, juce::Justification::left);
}

void VVChainAudioProcessorEditor::drawDeEsserOverview(
    juce::Graphics& g, juce::Rectangle<float> r)
{
    const int voice = parameterValue("DEESS_VOICE") > .5f ? 1 : 0;
    const float ref = voice ? 13500.f : 12500.f;
    const float x = r.getX() + r.getWidth() * logMap(ref, 20.f, 20000.f);

    g.setColour(juce::Colour(0xff67d3aa).withAlpha(.08f));
    g.fillRect(r);
    g.setColour(juce::Colour(0xffffc75a));
    g.drawVerticalLine((int)x, (int)r.getY() + 4, (int)r.getBottom() - 4);

    g.setColour(juce::Colour(0xff67d3aa));
    juce::Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float hz = invLogMap(i / 200.f, 20.f, 20000.f);
        const float intensity = parameterValue("DEESS_INTENSITY");
        float att = 0.f;
        if (hz >= ref)
        {
            const float c = intensity * ref / juce::jmax(hz, 1.f);
            att = -12.f * juce::jlimit(0.f, 1.f, 1.f - 1.f / c);
        }
        const float xx = r.getX() + r.getWidth() * logMap(hz, 20.f, 20000.f);
        const float yy = r.getBottom() - 12.f
            - juce::jlimit(0.f, 1.f, (att + 12.f) / 12.f) * (r.getHeight() - 22.f);
        if (i == 0) p.startNewSubPath(xx, yy);
        else p.lineTo(xx, yy);
    }
    g.strokePath(p, juce::PathStrokeType(2.f));

    g.setColour(juce::Colour(0xffc4c9d2));
    g.setFont(juce::FontOptions(9.f));
    g.drawText(voice ? "FEMALE 13.5 kHz" : "MALE 12.5 kHz",
               (int)r.getX() + 8, (int)r.getY() + 7, 130, 14,
               juce::Justification::left);
}

void VVChainAudioProcessorEditor::drawMixOverview(
    juce::Graphics& g, juce::Rectangle<float> r)
{
    const float dry = parameterValue("DRY_WET") / 100.f;
    const float out = juce::jmap(parameterValue("OUTPUT_LEVEL"), -24.f, 12.f, 0.f, 1.f);

    g.setColour(juce::Colour(0xff38bdf8).withAlpha(.2f));
    g.fillRect(r.getX() + r.getWidth() * .20f,
               r.getY() + (1.f - dry) * r.getHeight(),
               r.getWidth() * .24f, dry * r.getHeight());
    g.setColour(juce::Colour(0xff9ed85c).withAlpha(.2f));
    g.fillRect(r.getX() + r.getWidth() * .56f,
               r.getY() + (1.f - out) * r.getHeight(),
               r.getWidth() * .24f, out * r.getHeight());
}

void VVChainAudioProcessorEditor::paintPage(
    juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ColourGradient bg(juce::Colour(0xff050608), 0.f, 0.f,
                            juce::Colour(0xff111318), 0.f, bounds.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRect(bounds);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(23.f).withStyle("Bold"));
    g.drawText("VVCHAIN", 20, 13, 220, 28, juce::Justification::left);

    g.setColour(juce::Colour(0xff5d6875));
    g.setFont(juce::FontOptions(10.f));
    g.drawText("FREQUALIZER ENGINE  •  EQ / ANALOG  •  OTT  •  TYPE-A  •  REFERENCE DE-ESSER  •  MIX / OUT",
               20, 41, getWidth() - 40, 16, juce::Justification::left);

    drawEqGraph(g, graphBounds());

    const int width = getWidth();

    const juce::Rectangle<float> eqSection(18.f, 410.f, width - 36.f, 300.f);
    drawSection(g, eqSection, juce::Colour(0xff38bdf8),
                "EQ / ANALOG", "4-band parametric EQ • metallic control cards • drag nodes on the spectrum");

    for (int b = 0; b < 4; ++b)
    {
        auto card = juce::Rectangle<int>(
            30 + b * ((width - 76) / 4), 455, (width - 86) / 4, 235);
        drawCard(g, card.toFloat(), kBandColours[(size_t)b]);
        g.setColour(kBandColours[(size_t)b]);
        g.setFont(juce::FontOptions(11.f).withStyle("Bold"));
        g.drawText("BAND " + juce::String(b + 1),
                   card.getX() + 10, card.getY() + 8, 90, 18, juce::Justification::left);
    }

    auto eqShared = juce::Rectangle<float>(30.f, 600.f, 190.f, 90.f);
    g.setColour(juce::Colour(0xffaab3bd));
    g.setFont(juce::FontOptions(9.f));
    g.drawText("GLOBAL", 235, 608, 70, 16, juce::Justification::left);

    const juce::Rectangle<float> ottSection(18.f, 725.f, width - 36.f, 560.f);
    drawSection(g, ottSection, juce::Colour(0xffffc75a),
                "OTT / PUNKOTT-MB", "4-band multiband dynamics • all band controls and shared crossover/master controls visible");
    drawOttOverview(g, { 32.f, 772.f, (float)width - 64.f, 96.f });

    for (int b = 0; b < 4; ++b)
    {
        auto card = juce::Rectangle<int>(
            30 + b * ((width - 86) / 4), 878, (width - 96) / 4, 285);
        drawCard(g, card.toFloat(), kBandColours[(size_t)b]);
        g.setColour(kBandColours[(size_t)b]);
        g.setFont(juce::FontOptions(10.f).withStyle("Bold"));
        g.drawText("BAND " + juce::String(b + 1),
                   card.getX() + 10, card.getY() + 6, 100, 16, juce::Justification::left);
    }

    drawCard(g, { 30.f, 1172.f, 420.f, 92.f }, juce::Colour(0xffffc75a));
    if (ottClipper)
        ottClipper->setBounds(472, 1203, 82, 26);

    const juce::Rectangle<float> typeSection(18.f, 1300.f, width - 36.f, 300.f);
    drawSection(g, typeSection, juce::Colour(0xffff5fa2),
                "TYPE-A", "4-band transient/enhancer stage • all degrees plus attack/release/input/mix/output");
    drawTypeOverview(g, { 32.f, 1345.f, (float)width - 64.f, 80.f });

    for (int b = 0; b < 4; ++b)
    {
        auto card = juce::Rectangle<int>(
            30 + b * ((width - 86) / 4), 1438, (width - 96) / 4, 140);
        drawCard(g, card, kBandColours[(size_t)b]);
    }

    const juce::Rectangle<float> deSection(18.f, 1615.f, width - 36.f, 260.f);
    drawSection(g, deSection, juce::Colour(0xff67d3aa),
                "REFERENCE DE-ESSER", "8192 FFT • paired-sample detector • >10 trigger • Male/Female target frequency");
    drawDeEsserOverview(g, { 32.f, 1665.f, (float)width - 64.f, 86.f });

    const juce::Rectangle<float> mixSection(18.f, 1890.f, width - 36.f, 150.f);
    drawSection(g, mixSection, juce::Colour(0xff9ed85c),
                "MIX / OUT", "Global dry/wet and output trim");
    drawMixOverview(g, { 32.f, 1940.f, (float)width - 64.f, 70.f });

    g.setColour(juce::Colour(0xff737984));
    g.setFont(juce::FontOptions(9.f));
    g.drawText("VVChain • ALL MODULES ON ONE PAGE • DOUBLE CLICK A KNOB TO RESET",
               22, 2027, width - 44, 14, juce::Justification::centred);
}

void VVChainAudioProcessorEditor::Page::paint(juce::Graphics& g)
{
    owner.paintPage(g, getLocalBounds().toFloat());
}

void VVChainAudioProcessorEditorEditor_placeholder() {}

void VVChainAudioProcessorEditor::pageMouseDown(const juce::MouseEvent& e)
{
    const auto graph = graphBounds();
    if (!graph.contains(e.position))
        return;

    dragBand = -1;
    float best = 20.f;

    for (int b = 0; b < 4; ++b)
    {
        const auto n = juce::String(b + 1);
        const float f = parameterValue("EQ" + n + "_FREQ");
        const float gain = parameterValue("EQ" + n + "_GAIN");
        const juce::Point<float> p(
            graphFrequencyToX(graph, f), eqDbToY(graph, gain));

        const float d = p.getDistanceFrom(e.position);
        if (d < best)
        {
            best = d;
            dragBand = b;
            selectedBand = b;
        }
    }
    page.repaint();
}

void VVChainAudioProcessorEditor::pageMouseDrag(const juce::MouseEvent& e)
{
    if (dragBand < 0)
        return;

    const auto graph = graphBounds();
    const auto n = juce::String(dragBand + 1);
    setParameter("EQ" + n + "_FREQ", graphXToFrequency(graph, e.position.x));

    const float gain = juce::jlimit(
        -24.f, 24.f,
        -18.f + (graph.getBottom() - e.position.y) / graph.getHeight() * 36.f);
    setParameter("EQ" + n + "_GAIN", gain);
    page.repaint();
}

void VVChainAudioProcessorEditor::pageMouseUp()
{
    dragBand = -1;
}

void VVChainAudioProcessorEditor::Page::mouseDown(const juce::MouseEvent& e)
{
    owner.pageMouseDown(e);
}

void VVChainAudioProcessorEditor::Page::mouseDrag(const juce::MouseEvent& e)
{
    owner.pageMouseDrag(e);
}

void VVChainAudioProcessorEditor::Page::mouseUp(const juce::MouseEvent&)
{
    owner.pageMouseUp();
}

void VVChainAudioProcessorEditor::placeGroup(
    int group, juce::Rectangle<int> area, int columns,
    int knobWidth, int knobHeight, int gapX, int gapY)
{
    std::vector<Knob*> list;
    for (auto& k : knobs)
        if (k.group == group)
            list.push_back(&k);

    for (size_t i = 0; i < list.size(); ++i)
    {
        auto* k = list[i];
        const int col = (int)(i % (size_t)columns);
        const int row = (int)(i / (size_t)columns);
        const int x = area.getX() + col * (knobWidth + gapX);
        const int y = area.getY() + row * (knobHeight + gapY);

        k->label->setBounds(x, y, knobWidth, 18);
        k->slider->setBounds(x, y + 16, knobWidth, knobHeight);
    }
}

void VVChainAudioProcessorEditor::resized()
{
    viewport.setBounds(getLocalBounds());
    page.setSize(viewport.getWidth() - 12, 2050);

    const int width = page.getWidth();

    for (int b = 0; b < 4; ++b)
    {
        const int cardX = 30 + b * ((width - 86) / 4);
        const int cardW = (width - 96) / 4;
        placeGroup(10 + b, { cardX + 8, 486, cardW - 16, 165 }, 3, 82, 82, 4, 3);
    }

    placeGroup(15, { 235, 585, 190, 100 }, 2, 82, 82, 10, 5);

    for (int b = 0; b < 4; ++b)
    {
        const int cardX = 30 + b * ((width - 86) / 4);
        const int cardW = (width - 96) / 4;
        placeGroup(20 + b, { cardX + 7, 904, cardW - 14, 240 }, 3, 76, 76, 2, 3);
    }
    placeGroup(25, { 40, 1178, 420, 80 }, 7, 55, 62, 3, 2);

    for (int b = 0; b < 4; ++b)
    {
        const int cardX = 30 + b * ((width - 86) / 4);
        const int cardW = (width - 96) / 4;
        placeGroup(30 + b, { cardX + 8, 1464, cardW - 16, 96 }, 2, 82, 82, 4, 3);
    }
    placeGroup(35, { width - 520, 1460, 500, 104 }, 5, 82, 82, 4, 3);

    placeGroup(40, { 50, 1716, 260, 92 }, 2, 82, 82, 5, 2);
    deEssVoice.setBounds(330, 1718, 235, 28);

    placeGroup(50, { 50, 1960, 260, 88 }, 2, 82, 82, 5, 2);

    bypassButtons[0]->setBounds(width - 120, 423, 92, 26);
    bypassButtons[1]->setBounds(width - 120, 738, 92, 26);
    bypassButtons[2]->setBounds(width - 120, 1313, 92, 26);
    bypassButtons[3]->setBounds(width - 120, 1628, 92, 26);
    bypassButtons[4]->setBounds(width - 120, 1903, 92, 26);
    if (ottClipper)
        ottClipper->setBounds(472, 1203, 82, 26);
}
