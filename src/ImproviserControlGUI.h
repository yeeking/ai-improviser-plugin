#pragma once

#include <JuceHeader.h>
#include <functional>
#include "NoteIndicatorComponent.h"

class CustomButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
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
                             private juce::ComboBox::Listener,
                             private juce::Label::Listener
{
public:
    ImproviserControlGUI(juce::AudioProcessorValueTreeState& apvtState, ImproControlListener& improControlListener);
    ~ImproviserControlGUI() override;

    // // Attach a listener; stored internally (no ownership taken).
    // void addImproviserControlListener(ImproviserControlListener* listener);

    // Grid sizing for resized()
    void setGridDimensions(int columns, int rows);

    // Forwarders to tune the indicators (optional)
    void setIndicatorFrameRateHz(int hz);
    void setIndicatorDecaySeconds(float seconds);
    void setBpmAdjustCallback(std::function<void(int)> cb);

    // Feed incoming / outgoing MIDI for the indicators
    void midiReceived(const juce::MidiMessage& msg);
    void midiSent(const juce::MidiMessage& msg);
    void clockTicked();

    // JUCE overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    // maps 
    static float divisionIdToValue(int itemId);

private:
    // === UI Controls ===
    juce::ToggleButton playingToggle { "AI playing" };
    juce::ToggleButton learningToggle { "AI learning" };
    juce::ToggleButton leadFollowToggle { "AI leading" };

    juce::TextButton loadModelButton { "load model" };
    juce::TextButton saveModelButton { "save model" };
    juce::TextButton resetModelButton { "reset model" }; // Add this line

    juce::GroupComponent quantGroup { {}, "Quantisation" };
    juce::ToggleButton quantiseToggle { "Quantise" };
    juce::ToggleButton hostClockToggle { "Host clock" };
    juce::Slider bpmSlider;        // 60..240 BPM (hidden, used for APVTS)
    juce::Label  bpmLabel { {}, "BPM" };
    juce::Label  bpmValueLabel { {}, "120.00" };
    juce::TextButton bpmUpButton { "+" };
    juce::TextButton bpmDownButton { "-" };

    juce::ComboBox divisionCombo;  // beat, 1/3, quarter, 1/8, 1/12, 1/16

    juce::GroupComponent probGroup { {}, "Play Probability" };
    juce::Slider probabilitySlider;  // 0..1
    /**  */
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playingButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> learningButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> leadFollowButtonAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> probabilitySliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> quantiseButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hostClockButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmSliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionComboAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiInComboAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiOutComboAttachment;

    std::vector<std::unique_ptr<juce::ToggleButton>> divisionButtons;
    std::vector<int> divisionButtonIds;
    std::function<void(int)> bpmAdjustCallback;

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

    // Layout settings
    int gridColumns = 4;
    int gridRows    = 4;
    int gridGapPx   = 8;

    // Helpers
    juce::Rectangle<int> cellBounds(int cx, int cy, int wCells = 1, int hCells = 1) const;
    void configureChunkyControls();
    void createDivisionButtons();
    void updateDivisionButtonsFromCombo();
    void updateBpmDisplay();
    void adjustBpm(double delta);
    void updateHostClockToggleText();

    static int midiInIdToChannel(int itemId);
    static int midiOutIdToChannel(int itemId);

    // Event handlers
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void labelTextChanged(juce::Label* labelThatHasChanged) override;

    // ImproviserControlListener* listener = nullptr;
    ImproControlListener& controlListener; 
    CustomButtonLookAndFeel customLookAndFeel;  // Add this member variable
    std::unique_ptr<juce::FileChooser> loadFileChooser;
    std::unique_ptr<juce::FileChooser> saveFileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImproviserControlGUI)
};
