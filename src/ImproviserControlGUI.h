#pragma once

#include <JuceHeader.h>
#include "NoteIndicatorComponent.h"

// Listener interface provided by the host application (unchanged).
struct ImproviserControlListener
{
    virtual ~ImproviserControlListener() = default;

    virtual void playingOff() = 0;
    virtual void playingOn()  = 0;

    virtual void learningOn()  = 0;
    virtual void learningOff() = 0;

    virtual void setPlayProbability(float prob) = 0;

    virtual void setQuantBPM(float bpm)           = 0;
    virtual void setQuantDivision(float division) = 0;

    virtual void setMIDIInChannel(int ch)  = 0;   // 0 = All, 1-16
    virtual void setMIDIOutChannel(int ch) = 0;   // 1-16

    virtual void loadModelDialogue() = 0;
    virtual void saveModelDialogue() = 0;
};

class ImproviserControlGUI : public juce::Component,
                             private juce::Button::Listener,
                             private juce::Slider::Listener,
                             private juce::ComboBox::Listener
{
public:
    ImproviserControlGUI();
    ~ImproviserControlGUI() override;

    // Attach a listener; stored internally (no ownership taken).
    void addImproviserControlListener(ImproviserControlListener* listener);

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

    juce::GroupComponent quantGroup { {}, "Quantisation" };
    juce::Slider bpmSlider;        // 60..240 BPM
    juce::Label  bpmLabel { {}, "BPM" };

    juce::ComboBox divisionCombo;  // beat, 1/3, quarter, 1/8, 1/12, 1/16
    juce::Label    divisionLabel { {}, "division" };

    juce::GroupComponent probGroup { {}, "Play Probability" };
    juce::Slider probabilitySlider;  // 0..1

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

    ImproviserControlListener* listener = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImproviserControlGUI)
};
