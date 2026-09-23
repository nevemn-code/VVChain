#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>
#include <tuple>
#include "PluginProcessor.h"

class VVChainAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                  private juce::Timer
{
public:
    explicit VVChainAudioProcessorEditor(VVChainAudioProcessor&);
    ~VVChainAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void timerCallback() override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BoolAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class WheelSlider final : public juce::Slider
    {
    public:
        WheelSlider()
        {
            // JUCE enables wheel interaction by default; make it explicit so
            // look-and-feel / host changes cannot silently disable it.
            setScrollWheelEnabled(true);
            setMouseDragSensitivity(180);
        }

        void setWheelBehaviour(double step, bool logarithmic = false)
        {
            wheelStep = std::max(0.000001, step);
            wheelLogarithmic = logarithmic;
            wheelRemainder = 0.0;
        }

        void setGraphControlState(bool active, bool moving)
        {
            graphControlActive = active;
            graphControlMoving = moving;
            repaint();
        }

        bool isGraphControlActive() const noexcept { return graphControlActive; }
        bool isGraphControlMoving() const noexcept { return graphControlMoving; }

        void mouseDown(const juce::MouseEvent& e) override
        {
            fineDragging = e.mods.isShiftDown();
            setMouseDragSensitivity(fineDragging ? 1800 : 180);
            juce::Slider::mouseDown(e);
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            juce::Slider::mouseUp(e);
            fineDragging = false;
            setMouseDragSensitivity(180);
        }

        void mouseWheelMove(const juce::MouseEvent& e,
                            const juce::MouseWheelDetails& wheel) override
        {
            if (!isEnabled() || !isScrollWheelEnabled()
                || std::abs(wheel.deltaY) < 0.000001f)
                return;

            // Mouse wheels can arrive as full notches or fractional touch-pad
            // deltas. Accumulate the latter so both devices feel identical.
            wheelRemainder += juce::jlimit(-4.0, 4.0,
                                           static_cast<double>(wheel.deltaY));

            const int ticks = static_cast<int>(std::trunc(wheelRemainder));
            if (ticks == 0)
                return;

            wheelRemainder -= static_cast<double>(ticks);

            double next = getValue();

            if (e.mods.isShiftDown())
            {
                // Shift-wheel is always one host/APVTS parameter increment.
                // This is the finest deterministic adjustment this control
                // can make, regardless of the normal wheel step.
                const double fineStep =
                    std::max(0.000001, static_cast<double>(getInterval()));
                next += static_cast<double>(ticks) * fineStep;
            }
            else if (wheelLogarithmic)
            {
                // About one semitone per wheel tick: familiar studio-style
                // frequency adjustment without huge jumps at the top end.
                next *= std::pow(2.0,
                                 static_cast<double>(ticks) / 24.0);
            }
            else
            {
                next += static_cast<double>(ticks) * wheelStep;
            }

            next = juce::jlimit(getMinimum(), getMaximum(), next);
            setValue(next, juce::sendNotificationSync);
        }

    private:
        double wheelStep = 1.0;
        double wheelRemainder = 0.0;
        bool wheelLogarithmic = false;
        bool fineDragging = false;
        bool graphControlActive = false;
        bool graphControlMoving = false;
    };

    class MetalLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        bool monochrome = false;

        void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider&) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    };

    struct Knob
    {
        juce::String id;
        juce::String title;
        juce::String suffix;
        juce::Colour accent;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<Attachment> attachment;
        int band = -1;
        int slot = -1;
        bool tapeDisplayDb = false;
    };

    void addKnob(const juce::String& id, const juce::String& title,
                 double min, double max, double step, double defaultValue,
                 const juce::String& suffix, int band, int slot,
                 juce::Colour accent, bool tapeDisplayDb = false,
                 const juce::String& attachmentId = {});

    void addBypass(int index, const juce::String& parameterId,
                   const juce::String& tooltip, juce::Colour accent);

    Knob* findKnob(const juce::String& id);
    void placeKnob(const juce::String& id, juce::Rectangle<int> area);
    void setExpandedBand(int band);

    float parameterValue(const juce::String& id) const;
    void setParameter(const juce::String& id, float value);

    juce::Rectangle<float> eqGraphBounds() const;
    float graphFrequencyToX(const juce::Rectangle<float>&, float hz) const;
    float constrainXoverFrequency(int index, float hz) const;
    float graphXToFrequency(const juce::Rectangle<float>&, float x) const;
    float eqDbToY(const juce::Rectangle<float>&, float db) const;
    float dynamicThresholdFromDynamics(float dynamics) const;
    float dynamicEffectiveTargetGain(int band) const;
    void drawEqGraph(juce::Graphics&, juce::Rectangle<float>);
    void drawCard(juce::Graphics&, juce::Rectangle<float>, juce::Colour,
                  const juce::String&, const juce::String&);
    void drawPanel(juce::Graphics&, juce::Rectangle<float>, const juce::String&,
                   const juce::String&, juce::Colour);
    void drawModuleLeds(juce::Graphics&);
    void drawGraphDragHint(juce::Graphics&, juce::Rectangle<float>);
    juce::String formatGraphFrequency(float hz) const;
    juce::Colour uiColour(juce::Colour) const noexcept;
    void updateBypassVisuals();
    void setGraphControlState(const juce::StringArray& ids, bool moving);
    void clearGraphControlState();
    void setGraphControlMoving(bool moving);
    bool isMasterBypassed() const noexcept;
    juce::Rectangle<float> dynamicMsPopupBounds(int band) const;
    juce::Point<float> dynamicTargetPoint(int band) const;
    bool pointNearDynamicNode(juce::Point<float>, int& band) const;
    float dynamicAverageGainChangeDb(int band) const;
    float dynamicMidGainChangeDb(int band) const;
    float dynamicSideGainChangeDb(int band) const;

    VVChainAudioProcessor& audioProcessor;
    MetalLookAndFeel metalLook;

    std::vector<Knob> knobs;
    std::array<std::unique_ptr<juce::ToggleButton>, 5> bypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 5> bypassAttachments;
    std::unique_ptr<juce::ToggleButton> masterBypassButton;
    std::unique_ptr<BoolAttachment> masterBypassAttachment;
    std::unique_ptr<juce::ToggleButton> deessBypassButton;
    std::unique_ptr<BoolAttachment> deessBypassAttachment;
    std::unique_ptr<juce::ToggleButton> deessLocalBypassButton;
    std::unique_ptr<BoolAttachment> deessLocalBypassAttachment;
    std::unique_ptr<juce::ToggleButton> deltaMonitorButton;
    std::unique_ptr<BoolAttachment> deltaMonitorAttachment;
    std::array<std::unique_ptr<juce::TextButton>, 4> advancedButtons;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> ottBandBypassButtons;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> analogModeButtons;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> analogBypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 4> analogBypassAttachments;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> soloButtons;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> dynDetectButtons;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> dynTriggerButtons;
    std::array<std::unique_ptr<BoolAttachment>, 4> dynDetectAttachments;
    std::array<std::unique_ptr<BoolAttachment>, 4> dynTriggerAttachments;
    std::unique_ptr<juce::ToggleButton> soloModeButton;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> atypeBandBypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 4> ottBandBypassAttachments;
    std::array<std::unique_ptr<BoolAttachment>, 4> analogModeAttachments;
    std::array<std::unique_ptr<BoolAttachment>, 4> atypeBandBypassAttachments;

    std::unique_ptr<juce::ToggleButton> ottClipper;
    std::unique_ptr<BoolAttachment> ottClipperAttachment;
    std::unique_ptr<juce::TextButton> closeAdvanced;

    int expandedBand = -1;
    int expandedDynamicBand = -1;
    int dragBand = -1;
    int dragOffsetBand = -1;
    int dragXover = -1;
    int dragOverlapXover = -1;
    int dragDynamicMsBand = -1;
    int dragDynamicHandleBand = -1;
    int hoverDynamicBand = -1;
    int hoverXover = -1;
    float dynamicGainDragStartY = 0.0f;
    float dynamicHandleDragStartY = 0.0f;
    float dynamicHandleDragStartValue = 0.0f;
    float dynamicGainDragStartOffset = 0.0f;
    float dynamicTargetDragStartY = 0.0f;
    float dynamicTargetDragStartValue = 0.0f;
    float graphFreqDragStartHz = 0.0f;
    float graphFreqDragStartX = 0.0f;
    float graphFreqDragGrabOffsetX = 0.0f;
    float overlapDragStartY = 0.0f;
    float overlapDragStartValue = 50.0f;
    bool showGraphDragHint = false;
    juce::String graphDragHint;
    juce::Point<float> graphDragHintPosition {};
    int graphHintBand = -1;
    int graphHintActiveMask = 0; // 1=FREQ, 2=GAIN, 4=DYNAMICS, 8=Q
    juce::uint32 graphHintAutoHideAt = 0;

    // Graph gestures are deliberately separated:
    // Static EQ node = latched single-axis control (frequency OR gain)
    // Dynamic range handle = vertical only (DYNAMICS parameter)
    // X-overs = horizontal only
    enum class GraphEqDragAxis
    {
        Undetermined,
        Frequency,
        Gain
    };

    GraphEqDragAxis graphEqDragAxis = GraphEqDragAxis::Undetermined;
    float dynamicDragStartDynamics = 0.0f;
    bool lastMasterBypassUi = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VVChainAudioProcessorEditor)
};