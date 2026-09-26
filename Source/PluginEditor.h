#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include <tuple>
#include "PluginProcessor.h"
#include "SettingsPanel.h"

class VVChainAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                  private juce::Timer
{
public:
    explicit VVChainAudioProcessorEditor(VVChainAudioProcessor&);
    ~VVChainAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BoolAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class GlobalGraphMouseListener final : public juce::MouseListener
    {
    public:
        explicit GlobalGraphMouseListener(VVChainAudioProcessorEditor* ownerIn)
            : owner(ownerIn) {}

        void mouseMove(const juce::MouseEvent& e) override
        {
            if (owner != nullptr)
                owner->updateFloatingValueBoxAt(
                    owner->getLocalPoint(nullptr, e.getScreenPosition().toFloat()));
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            if (owner != nullptr)
                owner->updateFloatingValueBoxAt(
                    owner->getLocalPoint(nullptr, e.getScreenPosition().toFloat()));
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (owner != nullptr)
                owner->updateFloatingValueBoxAt(
                    owner->getLocalPoint(nullptr, e.getScreenPosition().toFloat()));
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (owner != nullptr)
                owner->updateFloatingValueBoxAt(
                    owner->getLocalPoint(nullptr, e.getScreenPosition().toFloat()));
        }

    private:
        VVChainAudioProcessorEditor* owner = nullptr;
    };

    class FloatingValueBox final : public juce::Component
    {
    public:
        using CommitHandler =
            std::function<void(int, const juce::String&)>;

        FloatingValueBox()
        {
            setInterceptsMouseClicks(true, true);
            setMouseCursor(juce::MouseCursor::NormalCursor);
            setVisible(false);

            auto setup = [this](juce::Label& label, int line)
            {
                addAndMakeVisible(label);
                label.setEditable(true, true, false);
                label.setJustificationType(juce::Justification::centredLeft);
                label.setFont(
                    juce::FontOptions(10.5f).withStyle("Bold"));
                label.setColour(
                    juce::Label::textColourId, juce::Colours::white);
                label.setColour(
                    juce::Label::backgroundColourId,
                    juce::Colours::transparentBlack);
                label.setColour(
                    juce::Label::outlineColourId,
                    juce::Colours::transparentBlack);
                label.setColour(
                    juce::Label::textWhenEditingColourId,
                    juce::Colours::white);
                label.setColour(
                    juce::Label::backgroundWhenEditingColourId,
                    juce::Colour(0xff171a1f));
                label.setColour(
                    juce::Label::outlineWhenEditingColourId,
                    juce::Colour(0xff7f8790));
                label.setMouseCursor(
                    juce::MouseCursor::IBeamCursor);
                auto* labelPtr = &label;
                label.onTextChange = [this, labelPtr, line]
                {
                    if (m_updatingText)
                        return;
                    if (m_commitHandler != nullptr)
                        m_commitHandler(line, labelPtr->getText());
                };
            };

            setup(m_line1, 0);
            setup(m_line2, 1);
            setup(m_line3, 2);
        }

        void setCommitHandler(CommitHandler handler)
        {
            m_commitHandler = std::move(handler);
        }

        bool isEditing() const noexcept
        {
            return m_line1.isBeingEdited()
                || m_line2.isBeingEdited()
                || m_line3.isBeingEdited();
        }

        bool isWithinInteractionZone(
            juce::Point<int> parentPoint) const noexcept
        {
            if (!isVisible())
                return false;

            const auto box = getBounds();

            // Small forgiveness around the actual editable box.
            if (box.expanded(12, 10).contains(parentPoint))
                return true;

            if (m_anchorPos.x < 0 || m_anchorPos.y < 0)
                return false;

            // Keep a small area around the source EQ/Dynamic node alive.
            if (parentPoint.getDistanceFrom(m_anchorPos) <= 18.0f)
                return true;

            // Directional "mouse tunnel" from the source node to the nearest
            // edge of the floating box.  Moving toward the box stays alive;
            // moving sideways/outside this corridor closes it immediately.
            const auto anchor = m_anchorPos.toFloat();
            const auto p = parentPoint.toFloat();

            const float targetX = juce::jlimit(
                static_cast<float>(box.getX() + 8),
                static_cast<float>(box.getRight() - 8),
                anchor.x);

            float targetY = static_cast<float>(box.getCentreY());
            if (anchor.y < static_cast<float>(box.getY()))
                targetY = static_cast<float>(box.getY());
            else if (anchor.y > static_cast<float>(box.getBottom()))
                targetY = static_cast<float>(box.getBottom());

            const juce::Point<float> target { targetX, targetY };
            const auto segment = target - anchor;
            const float lengthSquared =
                segment.x * segment.x + segment.y * segment.y;

            if (lengthSquared <= 0.0001f)
                return false;

            const auto fromAnchor = p - anchor;
            const float projection = juce::jlimit(
                0.0f, 1.0f,
                (fromAnchor.x * segment.x
                 + fromAnchor.y * segment.y)
                    / lengthSquared);

            const auto closest = anchor + segment * projection;
            return p.getDistanceFrom(closest) <= 26.0f;
        }

        void updateInfo(const juce::String& line1Text,
                        const juce::String& line2Text,
                        const juce::String& line3Text,
                        juce::Point<int> anchorPos,
                        juce::Rectangle<int> parentBounds)
        {
            const bool textChanged =
                m_line1.getText() != line1Text
                || m_line2.getText() != line2Text
                || m_line3.getText() != line3Text;

            if (!textChanged
                && m_lastPos == anchorPos
                && isVisible())
                return;

            m_updatingText = true;
            if (!m_line1.isBeingEdited())
                m_line1.setText(
                    line1Text, juce::dontSendNotification);
            if (!m_line2.isBeingEdited())
                m_line2.setText(
                    line2Text, juce::dontSendNotification);
            if (!m_line3.isBeingEdited())
                m_line3.setText(
                    line3Text, juce::dontSendNotification);
            m_updatingText = false;
            m_lastPos = anchorPos;
            m_anchorPos = anchorPos;

            constexpr int boxHeight = 66;
            constexpr int boxWidth = 150;
            int targetX = anchorPos.x - boxWidth / 2;
            int targetY = anchorPos.y - boxHeight - 14;

            if (targetX + boxWidth > parentBounds.getRight())
                targetX = parentBounds.getRight() - boxWidth - 4;
            if (targetX < parentBounds.getX())
                targetX = parentBounds.getX() + 4;

            if (targetY < parentBounds.getY())
                targetY = anchorPos.y + 14;
            if (targetY + boxHeight > parentBounds.getBottom())
                targetY = parentBounds.getBottom() - boxHeight - 4;

            setBounds(targetX, targetY, boxWidth, boxHeight);
            if (!isVisible())
                setVisible(true);
            repaint();
        }

        void hideInstantly()
        {
            if (isEditing())
                return;

            if (isVisible())
                setVisible(false);
            m_lastPos = { -1, -1 };
            m_anchorPos = { -1, -1 };
        }

        void resized() override
        {
            m_line1.setBounds(6, 3, getWidth() - 12, 19);
            m_line2.setBounds(6, 23, getWidth() - 12, 19);
            m_line3.setBounds(6, 43, getWidth() - 12, 19);
        }

        void paint(juce::Graphics& g) override
        {
            g.setColour(juce::Colour(0xE6111111));
            g.fillRoundedRectangle(
                getLocalBounds().toFloat(), 4.0f);
            g.setColour(juce::Colour(0x55FFFFFF));
            g.drawRoundedRectangle(
                getLocalBounds().toFloat(), 4.0f, 1.0f);
        }

    private:
        juce::Label m_line1;
        juce::Label m_line2;
        juce::Label m_line3;
        CommitHandler m_commitHandler;
        juce::Point<int> m_lastPos { -1, -1 };
        juce::Point<int> m_anchorPos { -1, -1 };
        bool m_updatingText = false;
    };

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

        void setWheelSingleStepPerEvent(bool enabled)
        {
            wheelSingleStepPerEvent = enabled;
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

        void refreshDisplayedText()
        {
            updateText();
            repaint();
        }

        void setDragSensitivity(int normalSensitivity, int fineSensitivity)
        {
            dragSensitivity = std::max(1, normalSensitivity);
            fineDragSensitivity = std::max(
                dragSensitivity + 1, fineSensitivity);
            if (!fineDragging)
                setMouseDragSensitivity(dragSensitivity);
        }

        void setDiscreteArc(int positions, float startAngle, float endAngle)
        {
            discreteArcEnabled = positions >= 2
                && endAngle > startAngle;
            discretePositions = std::max(2, positions);
            discreteScreenStartAngle = startAngle;
            discreteScreenEndAngle = endAngle;
        }

        // Dedicated detector-blend interaction:
        // vertical drag while the visual remains a horizontal PEAK/ONSETS bar.
        // Up = higher value (toward ONSETS/right), down = lower value
        // (toward PEAK/left).
        void setVerticalValueDrag(bool enabled, double pixelsForFullRange = 133.0)
        {
            verticalValueDrag = enabled;
            verticalPixelsForFullRange =
                std::max(20.0, pixelsForFullRange);
            if (enabled)
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            fineDragging = e.mods.isShiftDown();

            if (discreteArcEnabled)
            {
                setDiscreteValueFromPoint(e.position);
                return;
            }

            if (verticalValueDrag)
            {
                verticalDragStartY = e.position.y;
                verticalDragStartValue = getValue();
                juce::Slider::mouseDown(e);
                return;
            }

            setMouseDragSensitivity(
                fineDragging ? fineDragSensitivity : dragSensitivity);
            juce::Slider::mouseDown(e);
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (discreteArcEnabled)
            {
                setDiscreteValueFromPoint(e.position);
                return;
            }

            if (verticalValueDrag)
            {
                const double range =
                    std::max(0.000001, getMaximum() - getMinimum());
                const double fineScale =
                    e.mods.isShiftDown() ? 0.10 : 1.0;
                const double deltaY =
                    static_cast<double>(e.position.y - verticalDragStartY);

                // Requested mapping:
                // moving UP increases value -> visual blend moves RIGHT/ONSETS.
                // moving DOWN decreases value -> visual blend moves LEFT/PEAK.
                const double next =
                    verticalDragStartValue
                    - deltaY / verticalPixelsForFullRange
                        * range * fineScale;

                setValue(
                    juce::jlimit(getMinimum(), getMaximum(), next),
                    juce::sendNotificationSync);
                return;
            }

            juce::Slider::mouseDrag(e);
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (!discreteArcEnabled)
                juce::Slider::mouseUp(e);

            fineDragging = false;
            setMouseDragSensitivity(dragSensitivity);
        }

        void mouseWheelMove(const juce::MouseEvent& e,
                            const juce::MouseWheelDetails& wheel) override
        {
            if (!isEnabled() || !isScrollWheelEnabled()
                || std::abs(wheel.deltaY) < 0.000001f)
                return;

            if (wheelSingleStepPerEvent)
            {
                // OCT selector: one incoming wheel event = exactly one slope
                // position, independent of wheel/touchpad delta magnitude.
                const double direction = wheel.deltaY > 0.0f ? 1.0 : -1.0;
                const double next = juce::jlimit(
                    getMinimum(), getMaximum(),
                    getValue() + direction * wheelStep);
                setValue(next, juce::sendNotificationSync);
                return;
            }

            // Continuous controls keep fractional-wheel accumulation.
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
        void setDiscreteValueFromPoint(juce::Point<float> point)
        {
            if (!discreteArcEnabled)
                return;

            const auto area = getLocalBounds().toFloat();
            const float cx = area.getCentreX();
            const float cy = area.getCentreY() - 3.0f;

            float angle = std::atan2(point.y - cy, point.x - cx);
            if (angle < 0.0f)
                angle += juce::MathConstants<float>::twoPi;

            const float start = discreteScreenStartAngle;
            const float end = discreteScreenEndAngle;

            if (angle < start)
                angle += juce::MathConstants<float>::twoPi;

            angle = juce::jlimit(start, end, angle);

            const float norm =
                (angle - start) / juce::jmax(0.0001f, end - start);
            const int index = juce::jlimit(
                0, discretePositions - 1,
                juce::roundToInt(
                    norm * static_cast<float>(discretePositions - 1)));

            const double next =
                getMinimum()
                + static_cast<double>(index)
                    * static_cast<double>(getInterval());

            setValue(
                juce::jlimit(getMinimum(), getMaximum(), next),
                juce::sendNotificationSync);
        }

        double wheelStep = 1.0;
        double wheelRemainder = 0.0;
        int dragSensitivity = 180;
        int fineDragSensitivity = 1800;
        bool wheelLogarithmic = false;
        bool wheelSingleStepPerEvent = false;
        bool fineDragging = false;
        bool discreteArcEnabled = false;
        int discretePositions = 4;
        bool verticalValueDrag = false;
        double verticalPixelsForFullRange = 133.0;
        float verticalDragStartY = 0.0f;
        double verticalDragStartValue = 0.0;
        float discreteScreenStartAngle = 7.0f * juce::MathConstants<float>::pi / 6.0f;
        float discreteScreenEndAngle = 11.0f * juce::MathConstants<float>::pi / 6.0f;
        bool graphControlActive = false;
        bool graphControlMoving = false;
    };

    class MetalLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        MetalLookAndFeel();
        bool monochrome = false;
        bool ivoryTheme = true;
        bool isMasterRuntimeActive() const noexcept;

        void drawPanelSurface(juce::Graphics&, juce::Rectangle<float>,
                              bool localMuted = false) const;
        void drawModuleSurface(juce::Graphics&, juce::Rectangle<float>,
                               bool localMuted = false) const;
        void drawGraphSurface(juce::Graphics&, juce::Rectangle<float>) const;
        void drawScrew(juce::Graphics&, juce::Rectangle<float>,
                       bool localMuted = false) const;

        void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider&) override;
        void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPosProportional, float sliderAsymmetry,
                              float sliderStart,
                              juce::Slider::SliderStyle, juce::Slider&) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
        void drawButtonBackground(juce::Graphics&, juce::Button&,
                                  const juce::Colour& backgroundColour,
                                  bool shouldDrawButtonAsHighlighted,
                                  bool shouldDrawButtonAsDown) override;
        void drawButtonText(juce::Graphics&, juce::TextButton&,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;

    private:
        struct HardwareAssets
        {
            juce::Image panel;
            juce::Image module;
            juce::Image graph;
            juce::Image knobStrip;
            juce::Image buttonOff, buttonOn, buttonPressed;
            juce::Image ledOff, ledOn;
            juce::Image powerOff, powerOn;
            juce::Image screw;
            juce::Image sliderTrack;
            juce::Image sliderThumb;
        };

        HardwareAssets studioAssets, ivoryAssets, mutedAssets;
        juce::Image disabledButton;
        juce::Image bypassLedRed;
#if VVCHAIN_HAS_MASTER_LOCK_UI
        juce::Image knobGold, knobBlue, knobGreen, knobRed;
        juce::Image knobBlack, knobSilver, knobPlatinumMaster;
        juce::Image blackFullPanel, ivoryFullPanel;
#endif

        const HardwareAssets& assets(bool localMuted = false) const noexcept;
        const juce::Image& selectKnobStrip(const juce::Slider&,
                                           const HardwareAssets&) const noexcept;
        static juce::Image loadImage(const void* data, int size);
        static void drawImage(juce::Graphics&, const juce::Image&,
                              juce::Rectangle<float>, float opacity = 1.0f);
        static void drawKnobFrame(juce::Graphics&, const juce::Image& strip,
                                  int frameIndex, juce::Rectangle<float>,
                                  float opacity = 1.0f);
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
        bool slopeMode = false;
    };

    void addKnob(const juce::String& id, const juce::String& title,
                 double min, double max, double step, double defaultValue,
                 const juce::String& suffix, int band, int slot,
                 juce::Colour accent, bool tapeDisplayDb = false,
                 const juce::String& attachmentId = {});

    void addBypass(int index, const juce::String& parameterId,
                   const juce::String& tooltip, juce::Colour accent);
    void setSettingsPanelVisible(bool visible);
    void setIvoryTheme(bool ivory);
    void setAnalyzerEnabled(bool enabled);
    void updateAnalyzer();

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
    void pulseGraphControlMovement(const juce::StringArray& ids);
    bool isMasterBypassed() const noexcept;
    juce::Rectangle<float> dynamicMsPopupBounds(int band) const;
    juce::Point<float> dynamicTargetPoint(int band) const;
    bool pointNearDynamicNode(juce::Point<float>, int& band) const;
    void updateFloatingValueBoxAt(juce::Point<float> position);
    void showFloatingValueBoxForBand(
        int band, bool dynamicReadout, float displayedGain,
        juce::Point<float> position);
    void commitFloatingValueEdit(
        int line, const juce::String& text);
    void showEqTypeMenu(int band, juce::Point<float> position);
    void beginRightSolo(
        int band, juce::Point<float> position, bool dynamicTarget);
    float dynamicAverageGainChangeDb(int band) const;
    float dynamicMidGainChangeDb(int band) const;
    float dynamicSideGainChangeDb(int band) const;
    float eqYToDb(const juce::Rectangle<float>& graph, float y) const noexcept;
    float qFromWheel(float q, float deltaY, bool fine) const noexcept;

    FloatingValueBox floatingValueBox;
    int floatingValueBand = -1;
    bool floatingValueDynamic = false;
    std::array<bool, 4> udmbcUiTouched { false, false, false, false };
    std::array<bool, 4> analogUiTouched { false, false, false, false };
    std::array<bool, 4> tapeUiTouched { false, false, false, false };
    VVChainAudioProcessor& audioProcessor;
    MetalLookAndFeel metalLook;
    GlobalGraphMouseListener globalGraphMouseListener;

    std::vector<Knob> knobs;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> bypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 4> bypassAttachments;
    std::unique_ptr<juce::ToggleButton> masterBypassButton;
    std::unique_ptr<BoolAttachment> masterBypassAttachment;
    std::unique_ptr<juce::ToggleButton> deltaMonitorButton;
    std::unique_ptr<BoolAttachment> deltaMonitorAttachment;
    std::array<std::unique_ptr<juce::TextButton>, 4> advancedButtons;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> udmbcBandBypassButtons;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> analogModeButtons;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> analogX2Buttons;
    std::array<std::unique_ptr<BoolAttachment>, 4> analogX2Attachments;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> analogBypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 4> analogBypassAttachments;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> soloButtons;
    std::array<std::unique_ptr<juce::Slider>, 4> dynDetectSliders;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> dynTriggerButtons;
    std::array<std::unique_ptr<Attachment>, 4> dynDetectAttachments;
    std::array<std::unique_ptr<BoolAttachment>, 4> dynTriggerAttachments;
    std::unique_ptr<juce::ToggleButton> soloModeButton;
    std::unique_ptr<SettingsDismissOverlay> settingsDismissOverlay;
    std::unique_ptr<SettingsGearButton> settingsButton;
    std::unique_ptr<juce::TextButton> uiThemeButton;
    std::unique_ptr<SettingsPanel> settingsPanel;
    bool settingsPanelVisible = false;
    bool ivoryTheme = false;
    bool analyzerEnabled = true;

    static constexpr int analyzerFftOrder = 12;
    static constexpr int analyzerFftSize = 1 << analyzerFftOrder;
    static constexpr int analyzerHopSize = analyzerFftSize / 2;
    static constexpr int analyzerDisplayPoints = 256;
    juce::dsp::FFT analyzerFft { analyzerFftOrder };
    juce::dsp::WindowingFunction<float> analyzerWindow {
        analyzerFftSize, juce::dsp::WindowingFunction<float>::hann, false
    };
    std::array<float, analyzerFftSize> analyzerInput {};
    std::array<float, analyzerFftSize * 2> analyzerFftData {};
    std::array<double, analyzerFftSize / 2 + 1> analyzerPower {};
    std::array<double, analyzerFftSize / 2 + 2> analyzerPowerPrefix {};
    std::array<float, analyzerDisplayPoints> analyzerDb {};
    int analyzerInputCount = 0;
    juce::Path analyzerPath;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> tapeBandBypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 4> udmbcBandBypassAttachments;
    std::array<std::unique_ptr<BoolAttachment>, 4> analogModeAttachments;
    std::array<std::unique_ptr<BoolAttachment>, 4> tapeBandBypassAttachments;

    std::unique_ptr<juce::ToggleButton> udmbcClipper;
    std::unique_ptr<BoolAttachment> udmbcClipperAttachment;
    std::unique_ptr<juce::TextButton> closeAdvanced;

    int expandedBand = -1;
    int expandedDynamicBand = -1;
    int dragBand = -1;
    int dragOffsetBand = -1;
    int dragXover = -1;
    int dragOverlapXover = -1;
    int dragDynamicMsBand = -1;
    int dragDynamicHandleBand = -1;
    int rightSoloBand = -1;
    bool rightSoloDynamic = false;
    juce::Point<float> rightSoloPosition {};
    int pendingRightClickBand = -1;
    bool pendingRightClickDynamic = false;
    juce::Point<float> pendingRightClickPosition {};
    bool pendingRightClickDragged = false;
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
    juce::Point<float> graphLastDragPosition { -1.0f, -1.0f };
    int graphMovementPulseGeneration = 0;
    float dynamicDragStartDynamics = 0.0f;
    bool lastMasterBypassUi = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VVChainAudioProcessorEditor)
};
