#pragma once

#include <JuceHeader.h>
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



class ImproviserControlGUI : public juce::Component,
                             private juce::Button::Listener,
                             private juce::Slider::Listener,
                             private juce::ComboBox::Listener
{
public:
    ImproviserControlGUI(juce::AudioProcessorValueTreeState& apvtState);
    ~ImproviserControlGUI() override;

    // // Attach a listener; stored internally (no ownership taken).
    // void addImproviserControlListener(ImproviserControlListener* listener);

    // Grid sizing for resized()
    void setGridDimensions(int columns, int rows);

    // Forwarders to tune the indicators (optional)
    void setIndicatorFrameRateHz(int hz);
    void setIndicatorDecaySeconds(float seconds);

    // Feed incoming / outgoing MIDI for the indicators
    void midiReceived(const juce::MidiMessage& msg);
    void midiSent(const juce::MidiMessage& msg);

    // JUCE overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // === UI Controls ===
    juce::ToggleButton playingToggle { "AI playing" };
    juce::ToggleButton learningToggle { "AI learning" };

    juce::TextButton loadModelButton { "load model" };
    juce::TextButton saveModelButton { "save model" };
    juce::TextButton resetModelButton { "reset model" }; // Add this line

    juce::GroupComponent quantGroup { {}, "Quantisation" };
    juce::Slider bpmSlider;        // 60..240 BPM
    juce::Label  bpmLabel { {}, "BPM" };

    juce::ComboBox divisionCombo;  // beat, 1/3, quarter, 1/8, 1/12, 1/16
    juce::Label    divisionLabel { {}, "division" };

    juce::GroupComponent probGroup { {}, "Play Probability" };
    juce::Slider probabilitySlider;  // 0..1
    /**  */
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playingButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> learningButtonAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> probabilitySliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmSliderAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionComboAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiInComboAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiOutComboAttachment;


    // New: two note indicators + labels
    NoteIndicatorComponent noteInIndicator;
    NoteIndicatorComponent noteOutIndicator;
    juce::Label midiInLightLabel  { {}, "MIDI In"  };
    juce::Label midiOutLightLabel { {}, "MIDI Out" };

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

    static float divisionIdToValue(int itemId);
    static int midiInIdToChannel(int itemId);
    static int midiOutIdToChannel(int itemId);

    // Event handlers
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    // ImproviserControlListener* listener = nullptr;
    CustomButtonLookAndFeel customLookAndFeel;  // Add this member variable

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImproviserControlGUI)
};

