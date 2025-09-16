#pragma once

#include <JuceHeader.h>

// Listener interface provided by the host application (as per spec).
// (Declared here for convenience; if you already have it elsewhere, include that instead.)
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

    virtual void setMIDIInChannel(int ch)  = 0;   // 0 = All, 1-16 explicit
    virtual void setMIDIOutChannel(int ch) = 0;   // 1-16

    virtual void loadModelDialogue() = 0;
    virtual void saveModelDialogue() = 0;
};

class ImproviserControlGUI : public juce::Component,
                             private juce::Button::Listener,
                             private juce::Slider::Listener,
                             private juce::ComboBox::Listener,
                             private juce::Timer
{
public:
    ImproviserControlGUI();
    ~ImproviserControlGUI() override;

    // Attach a listener; stored internally (no ownership taken).
    void addImproviserControlListener(ImproviserControlListener* listener);

    // Optional: change grid used in resized() for layout.
    // Defaults: 4 columns x 4 rows.
    void setGridDimensions(int columns, int rows);

    // Set animation framerate for the MIDI light (Hz). Default: 30.
    void setFrameRateHz(int hz);

    // Optional: set decay time (seconds) for the MIDI light from full (1.0) to 0.
    // Default: 0.4 seconds.
    void setDecaySeconds(float seconds);

    /** tell ui we received some midi - message thread */
    void midiReceived(const juce::MidiMessage& msg);
    /** tell ui we sent some midi */
    void midiSent(const juce::MidiMessage& msg);
    

    // JUCE overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // override on timer interface
    void timerCallback() override; 

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

    juce::GroupComponent midiGroup { {}, "MIDI Routing" };
    juce::ComboBox midiInCombo;      // All, 1..16   (All => 0)
    juce::Label    midiInLabel { {}, "MIDI In" };
    juce::ComboBox midiOutCombo;     // 1..16
    juce::Label    midiOutLabel { {}, "MIDI Out" };

    // === MIDI light state ===
    // Drawn inside probGroup, below the probability slider.
    juce::Rectangle<int> midiLightBounds; // set in resized()
    std::atomic<float>   noteBrightness { 0.0f }; // 0..1, decays over time
    std::atomic<int>     lastNoteNumber { 0 };
    float                brightnessRedrawThreshold = 0.02f;  // repaint while above this
    int                  frameRateHz = 30;                   // animation rate
    float                decaySeconds = 0.4f;                // time to fade from 1.0 -> 0.0

    // Layout settings
    int gridColumns = 4;
    int gridRows    = 4;
    int gridGapPx   = 8;

    // Helper: return the rectangle for cell (cx,cy) sapanning (wCells x hCells)
    juce::Rectangle<int> cellBounds(int cx, int cy, int wCells = 1, int hCells = 1) const;

    // Helper: set big (“chunky”) look/feel-ish sizing for controls
    void configureChunkyControls();

    // Map combo selection -> division float (fraction of beat)
    static float divisionIdToValue(int itemId);

    // Map “All, 1-16” for MIDI In; we use 0 for All
    static int midiInIdToChannel(int itemId);
    static int midiOutIdToChannel(int itemId);

    // === Event handlers ===
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    // Update or restart timer based on frameRateHz
    void updateFrameTimer();

    // Listener (not owned)
    ImproviserControlListener* listener = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImproviserControlGUI)
};
