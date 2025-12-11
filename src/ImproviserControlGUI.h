#pragma once

#include <JuceHeader.h>
#include "NoteIndicatorComponent.h"

class CustomButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setFontSize(float size) { fontSize = size; }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                       bool isMouseOverButton, bool isButtonDown) override
    {
                g.setColour(juce::Colours::white);
        auto font = juce::Font(fontSize);
        g.setFont(font);
        g.drawText(button.getButtonText(), button.getLocalBounds(),
                  juce::Justification::centred, false);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto cornerSize = 6.0f;
        
        // Draw background
        g.setColour(button.getToggleState() ? juce::Colours::green 
                                           : juce::Colours::darkgrey);
        g.fillRoundedRectangle(bounds, cornerSize);
        
        // Draw outline
        g.setColour(button.getToggleState() ? juce::Colours::blue 
                                           : juce::Colours::grey);
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        
        // Draw text
        g.setColour(juce::Colours::white);
        auto font = juce::Font(fontSize);
        g.setFont(font);
        g.drawText(button.getButtonText(), bounds,
                  juce::Justification::centred, false);
    }
    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        g.setColour(juce::Colours::white);
        auto font = juce::Font(fontSize);
        g.setFont(font);
        g.drawText(label.getText(), label.getLocalBounds(),
                  label.getJustificationType(), false);
    }

    private:
    float fontSize{24.0f};
};

class RoundToggleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setLabelHeight(float h) { labelHeight = h; }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto circleArea = bounds.withTrimmedBottom(labelHeight).reduced(4.0f);
        const float diameter = juce::jmin(circleArea.getWidth(), circleArea.getHeight());
        auto circle = circleArea.withSizeKeepingCentre(diameter, diameter);

        juce::Colour offColour = juce::Colours::darkgrey;
        juce::Colour onColour  = juce::Colours::limegreen;

        if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
            offColour = offColour.brighter(0.2f);

        g.setColour(button.getToggleState() ? onColour : offColour);
        g.fillEllipse(circle);

        g.setColour(button.getToggleState() ? onColour.brighter(0.3f)
                                            : juce::Colours::grey);
        g.drawEllipse(circle, 2.0f);

        auto labelArea = bounds.removeFromBottom(labelHeight);
        auto labelIntArea = labelArea.toNearestInt();
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawFittedText(button.getButtonText(), labelIntArea,
                         juce::Justification::centred, 1);
    }

private:
    float labelHeight { 18.0f };
};

enum class ModelIoState
{
    Idle = 0,
    Loading = 1,
    Saving = 2
};

/**
 * @brief  Abstract listener for improvement‑control objects.
 *
 * Implementers receive callbacks when the user requests to load,
 * save, or reset a configuration/state.
 */
class ImproControlListener
{
public:
    virtual ~ImproControlListener() = default;

    /** Load data from persistent storage. */
    virtual bool loadModel(std::string filename) = 0;

    /** Persist current data. */
    virtual bool saveModel(std::string filename) = 0;

    /** Reset to defaults or clear state. */
    virtual void resetModel()  = 0;
};

class ImproviserControlGUI : public juce::Component,
                             private juce::Button::Listener,
                             private juce::Slider::Listener,
                             private juce::ComboBox::Listener
{
public:
    ImproviserControlGUI(juce::AudioProcessorValueTreeState& apvtStateIn, ImproControlListener& improControlListener);
    ~ImproviserControlGUI() override;

    // // Attach a listener; stored internally (no ownership taken).
    // void addImproviserControlListener(ImproviserControlListener* listener);

    // Grid sizing for resized()
    void setGridDimensions(int columns, int rows);

    // Forwarders to tune the indicators (optional)
    void setIndicatorFrameRateHz(int hz);
    void setIndicatorDecaySeconds(float seconds);
    void setExternalBpmDisplay(float bpm, bool hostControlled);
    void setAvoidTransposition(int semitoneOffset);
    void setSlowMoScalar(float scalar);
    void setOverpolyExtra(int extraCount);
    void setCallResponseEnergy(float energy01);
    void setCallResponsePhase(bool enabled, bool inResponse);
    void setModelStatus(int pitchSize, int pitchOrder,
                        int ioiSize, int ioiOrder,
                        int durSize, int durOrder);
    void setModelIoStatus(ModelIoState state, const std::string& stageText);

    // Feed incoming / outgoing MIDI for the indicators
    void midiReceived(const juce::MidiMessage& msg);
    void midiSent(const juce::MidiMessage& msg);
    void clockTicked();

    // JUCE overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    /** only repaint things that are animated */
    void smartRepaint();
    
    // maps 
    static float divisionIdToValue(int itemId);

    

private:
    class CallResponseEnergyBar : public juce::Component
    {
    public:
        void setEnergy(float value);
        void paint(juce::Graphics& g) override;

    private:
        float energy { 0.0f };
    };

    void updateLeadFollowStatusLabel();
    // === UI Controls ===
    juce::ToggleButton playingToggle { "AI playing" };
    juce::ToggleButton learningToggle { "AI learning" };
    juce::ToggleButton leadFollowToggle { "Lead/ follow" };

    juce::TextButton loadModelButton { "load model" };
    juce::TextButton saveModelButton { "save model" };
    juce::TextButton resetModelButton { "reset model" }; // Add this line

    juce::GroupComponent quantGroup { {}, "Quantisation" };
    juce::ToggleButton quantiseToggle { "Quantise" };
    juce::ToggleButton hostClockToggle { "Host clock" };
    juce::Slider bpmSlider;        // 60..240 BPM
    juce::Label  bpmLabel { {}, "BPM" };

    juce::ComboBox divisionCombo;  // beat, 1/3, quarter, 1/8, 1/12, 1/16

    juce::GroupComponent behaviourGroup { {}, "Behaviour" };
    juce::ToggleButton avoidToggle { "Avoid" };
    juce::ToggleButton slowMoToggle { "SlowMo" };
    juce::ToggleButton overpolyToggle { "Overpoly" };
    juce::ToggleButton callResponseToggle { "Call/resp" };
    juce::Label leadFollowStatusLabel { {}, "Lead/follow" };
    CallResponseEnergyBar callResponseEnergyBar;
    juce::Label avoidTranspositionLabel { {}, "Avoid 0" };
    juce::Label slowMoStatusLabel { {}, "SlowMo" };
    juce::Label overpolyStatusLabel { {}, "Overpoly" };
    juce::Label callResponseStatusLabel { {}, "Call/resp" };
    juce::Slider callRespGainSlider;
    juce::Slider callRespSilenceSlider;
    juce::Slider callRespDrainSlider;
    juce::Label callRespGainLabel { {}, "sens" };
    juce::Label callRespSilenceLabel { {}, "wait" };
    juce::Label callRespDrainLabel { {}, "decay" };
    juce::Label modelPitchLabel { {}, "Pitch: -" };
    juce::Label modelIoILabel { {}, "IOI: -" };
    juce::Label modelDurLabel { {}, "Dur: -" };

    juce::GroupComponent probGroup { {}, "Play Probability" };
    juce::Slider probabilitySlider;  // 0..1
    /**  */
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playingButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> learningButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> leadFollowButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> avoidButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> slowMoButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> overpolyButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> callResponseButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> callRespGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> callRespSilenceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> callRespDrainAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> probabilitySliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> quantiseButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hostClockButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmSliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionComboAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiInComboAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiOutComboAttachment;

    std::vector<std::unique_ptr<juce::ToggleButton>> divisionButtons;
    std::vector<int> divisionButtonIds;

    // New: two note indicators + labels
    NoteIndicatorComponent noteInIndicator;
    NoteIndicatorComponent noteOutIndicator;
    NoteIndicatorComponent clockIndicator;
    juce::Label midiInLightLabel  { {}, "to AI"  };
    juce::Label midiOutLightLabel { {}, "from AI" };
    juce::Label clockLightLabel   { {}, "clock" };

    juce::GroupComponent midiGroup { {}, "MIDI Routing" };
    juce::ComboBox midiInCombo;      // All, 1..16   (All => 0)
    juce::Label    midiInLabel { {}, "MIDI In" };
    juce::ComboBox midiOutCombo;     // 1..16
    juce::Label    midiOutLabel { {}, "MIDI Out" };
    bool displayingHostBpm = false;
    float externalBpmDisplayValue = 0.0f;
    ModelIoState currentModelIoState { ModelIoState::Idle };
    bool modelIoFlashOn { false };
    double lastModelIoFlashMs { 0.0 };
    juce::Colour defaultLoadButtonColour;
    juce::Colour defaultSaveButtonColour;

    // Layout settings
    int gridColumns = 4;
    int gridRows    = 4;
    int gridGapPx   = 8;

    // Helpers
    juce::Rectangle<int> cellBounds(int cx, int cy, int wCells = 1, int hCells = 1) const;
    void configureChunkyControls();
    void createDivisionButtons();
    void updateDivisionButtonsFromCombo();
    void updateHostClockToggleText();

    static int midiInIdToChannel(int itemId);
    static int midiOutIdToChannel(int itemId);

    // Event handlers
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    // ImproviserControlListener* listener = nullptr;
    juce::AudioProcessorValueTreeState& apvtState;
    ImproControlListener& controlListener; 
    CustomButtonLookAndFeel customLookAndFeel;  // Add this member variable
    CustomButtonLookAndFeel divisionButtonLookAndFeel;
    RoundToggleLookAndFeel behaviourButtonLookAndFeel;
    std::unique_ptr<juce::FileChooser> loadFileChooser;
    std::unique_ptr<juce::FileChooser> saveFileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImproviserControlGUI)
};
