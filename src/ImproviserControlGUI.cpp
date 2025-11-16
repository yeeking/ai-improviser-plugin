#include "ImproviserControlGUI.h"

// ===============================================================
// ctor / dtor
// ===============================================================

ImproviserControlGUI::ImproviserControlGUI(
    juce::AudioProcessorValueTreeState &apvtState,
    ImproControlListener &_controlListener)
    : controlListener{_controlListener} {
  setGridDimensions(6, 4); // Add this line near start of constructor

  // Chunky toggles
  playingToggle.setClickingTogglesState(true);
  learningToggle.setClickingTogglesState(true);
  leadFollowToggle.setClickingTogglesState(true);

  // Buttons
  loadModelButton.addListener(this);
  saveModelButton.addListener(this);
  resetModelButton.addListener(this); // Add this line

  // Apply custom look and feel to all buttons
  playingToggle.setLookAndFeel(&customLookAndFeel);
  learningToggle.setLookAndFeel(&customLookAndFeel);
  leadFollowToggle.setLookAndFeel(&customLookAndFeel);
  
  loadModelButton.setLookAndFeel(&customLookAndFeel);
  saveModelButton.setLookAndFeel(&customLookAndFeel);
  resetModelButton.setLookAndFeel(&customLookAndFeel);

  // Apply custom look and feel to labels
  quantiseToggle.setLookAndFeel(&customLookAndFeel);
  quantiseToggle.setButtonText("Enable");
  quantiseToggle.setClickingTogglesState(true);
  hostClockToggle.setClickingTogglesState(true);
  hostClockToggle.setLookAndFeel(&customLookAndFeel);
  hostClockToggle.setButtonText("Host clock");
  bpmLabel.setLookAndFeel(&customLookAndFeel);
  midiInLabel.setLookAndFeel(&customLookAndFeel);
  midiOutLabel.setLookAndFeel(&customLookAndFeel);
  midiInLightLabel.setLookAndFeel(&customLookAndFeel);
  midiOutLightLabel.setLookAndFeel(&customLookAndFeel);
  // clockLightLabel.setLookAndFeel(&customLookAndFeel);
  divisionButtonLookAndFeel.setFontSize(12.0f);

  // BPM slider
  bpmSlider.setSliderStyle(juce::Slider::SliderStyle::LinearBar);
  bpmSlider.setRange(60.0, 240.0, 0.5);
  bpmSlider.setNumDecimalPlacesToDisplay(2);
    
  bpmSlider.addListener(this);
  bpmSlider.setValue(120.0);

  bpmLabel.setJustificationType(juce::Justification::centredLeft);

  // Division combo
  divisionCombo.addListener(this);
  divisionCombo.addItem("1", 1);    // 1.0
  divisionCombo.addItem("3", 3);     // ~0.3333
  divisionCombo.addItem("4", 4); // 0.25
  divisionCombo.addItem("8", 8);     // 0.125
  divisionCombo.addItem("12", 12);   // ~0.08333
  divisionCombo.addItem("16", 16);   // 0.0625
  divisionCombo.setSelectedId(1, juce::dontSendNotification);
  divisionCombo.setVisible(false);
  addChildComponent(divisionCombo);

  // Probability slider
  probabilitySlider.setRange(0.0, 1.0, 0.01);
  probabilitySlider.setSliderStyle(juce::Slider::LinearBar);
  probabilitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 24);
  probabilitySlider.addListener(this);
  probabilitySlider.setValue(0.5);

  // Indicator labels
  midiInLightLabel.setJustificationType(juce::Justification::centredLeft);
  midiOutLightLabel.setJustificationType(juce::Justification::centredLeft);
  // clockLightLabel.setJustificationType(juce::Justification::centredLeft);

  // MIDI combos
  midiInCombo.addListener(this);
  midiInCombo.addItem("All", 100); // -> channel 0
  for (int ch = 1; ch <= 16; ++ch)
    midiInCombo.addItem("Ch " + juce::String(ch), 100 + ch);
  midiInCombo.setSelectedId(100, juce::dontSendNotification);

  midiOutCombo.addListener(this);
  for (int ch = 1; ch <= 16; ++ch)
    midiOutCombo.addItem("Ch " + juce::String(ch), 200 + ch);
  midiOutCombo.setSelectedId(201, juce::dontSendNotification); // default Ch 1

  // Group titles
  quantGroup.setTextLabelPosition(juce::Justification::centredLeft);
  probGroup.setTextLabelPosition(juce::Justification::centredLeft);
  midiGroup.setTextLabelPosition(juce::Justification::centredLeft);

  // Add & make visible
  addAndMakeVisible(playingToggle);
  addAndMakeVisible(learningToggle);
  addAndMakeVisible(leadFollowToggle);
  
  addAndMakeVisible(loadModelButton);
  addAndMakeVisible(saveModelButton);
  addAndMakeVisible(resetModelButton); // Add this line

  addAndMakeVisible(quantGroup);
  addAndMakeVisible(quantiseToggle);
  addAndMakeVisible(hostClockToggle);
  addAndMakeVisible(bpmSlider);
  addAndMakeVisible(bpmLabel);

  addAndMakeVisible(probGroup);
  addAndMakeVisible(probabilitySlider);
  addAndMakeVisible(midiInLightLabel);
  addAndMakeVisible(midiOutLightLabel);
  // addAndMakeVisible(clockLightLabel);
  addAndMakeVisible(noteInIndicator);
  addAndMakeVisible(noteOutIndicator);
  addAndMakeVisible(clockIndicator);

  addAndMakeVisible(midiGroup);
  addAndMakeVisible(midiInCombo);
  addAndMakeVisible(midiInLabel);
  addAndMakeVisible(midiOutCombo);
  addAndMakeVisible(midiOutLabel);

  // Toggle listeners last (avoid initial fire)
  playingToggle.addListener(this);
  learningToggle.addListener(this);

  configureChunkyControls();

  // Optional: tune indicators (defaults already reasonable)
  noteInIndicator.setFrameRateHz(30);
  noteOutIndicator.setFrameRateHz(30);
  clockIndicator.setFrameRateHz(30);
  noteInIndicator.setDecaySeconds(1.0f);
  noteOutIndicator.setDecaySeconds(1.0f);
  clockIndicator.setDecaySeconds(0.4f);

  // now do the APVTS hookups
  playingButtonAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvtState, "playing", playingToggle);

  learningButtonAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvtState, "learning", learningToggle);

  probabilitySliderAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          apvtState, "playProbability", probabilitySlider);

  quantiseButtonAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvtState, "quantise", quantiseToggle);
  hostClockButtonAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          apvtState, "quantUseHostClock", hostClockToggle);

  bpmSliderAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          apvtState, "quantBPM", bpmSlider);

  divisionComboAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          apvtState, "quantDivision", divisionCombo);

  midiInComboAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          apvtState, "midiInChannel", midiInCombo);

  midiOutComboAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          apvtState, "midiOutChannel", midiOutCombo);

  hostClockToggle.onStateChange = [this] { updateHostClockToggleText(); };
  updateHostClockToggleText();

  createDivisionButtons();
  updateDivisionButtonsFromCombo();
}

ImproviserControlGUI::~ImproviserControlGUI() {
  playingToggle.removeListener(this);
  learningToggle.removeListener(this);
  loadModelButton.removeListener(this);
  saveModelButton.removeListener(this);
  resetModelButton.removeListener(this); // Add this line
  bpmSlider.removeListener(this);
  probabilitySlider.removeListener(this);
  divisionCombo.removeListener(this);
  midiInCombo.removeListener(this);
  midiOutCombo.removeListener(this);
  for (auto& btn : divisionButtons)
    btn->removeListener(this);
}

// ===============================================================
// Public API
// ===============================================================

// void
// ImproviserControlGUI::addImproviserControlListener(ImproviserControlListener*
// l)
// {
//     listener = l;
// }

void ImproviserControlGUI::setGridDimensions(int columns, int rows) {
  gridColumns = juce::jmax(1, columns);
  gridRows = juce::jmax(1, rows);
  resized();
}

void ImproviserControlGUI::setIndicatorFrameRateHz(int hz) {
  noteInIndicator.setFrameRateHz(hz);
  noteOutIndicator.setFrameRateHz(hz);
  clockIndicator.setFrameRateHz(hz);
}

void ImproviserControlGUI::setIndicatorDecaySeconds(float seconds) {
  noteInIndicator.setDecaySeconds(seconds);
  noteOutIndicator.setDecaySeconds(seconds);
  clockIndicator.setDecaySeconds(seconds);
}

void ImproviserControlGUI::setExternalBpmDisplay(float bpm, bool hostControlled)
{
  externalBpmDisplayValue = bpm;
  displayingHostBpm = hostControlled;
  updateHostClockToggleText();
}

void ImproviserControlGUI::midiReceived(const juce::MidiMessage &msg) {
  // if (!msg.isNoteOnOrOff()) return;
  if (!msg.isNoteOn())
    return;

  const int note = msg.getNoteNumber();
  const float vel = msg.getFloatVelocity();

  noteInIndicator.setNote(note, vel);
}

void ImproviserControlGUI::midiSent(const juce::MidiMessage &msg) {
  if (!msg.isNoteOn())
    return;

  const int note = msg.getNoteNumber();
  const float vel = msg.getFloatVelocity();

  noteOutIndicator.setNote(note, vel);
}

void ImproviserControlGUI::clockTicked() {
  // Arbitrary note/velocity just to trigger the throbbing visual
  // clockIndicator.setNote(60, 1.0f);
  clockIndicator.setString("[]");
}

// ===============================================================
// Painting / layout
// ===============================================================

void ImproviserControlGUI::paint(juce::Graphics &g) {
  g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

juce::Rectangle<int>
ImproviserControlGUI::cellBounds(int cx, int cy, int wCells, int hCells) const {
  auto r = getLocalBounds().reduced(gridGapPx);
  const int cellW = juce::jmax(
      1, (r.getWidth() - gridGapPx * (gridColumns - 1)) / gridColumns);
  const int cellH =
      juce::jmax(1, (r.getHeight() - gridGapPx * (gridRows - 1)) / gridRows);

  const int x = r.getX() + cx * (cellW + gridGapPx);
  const int y = r.getY() + cy * (cellH + gridGapPx);
  const int w = cellW * wCells + gridGapPx * (wCells - 1);
  const int h = cellH * hCells + gridGapPx * (hCells - 1);
  return {x, y, w, h};
}

void ImproviserControlGUI::resized() {
  // Row 0
  playingToggle.setBounds(cellBounds(0, 0));
  learningToggle.setBounds(cellBounds(1, 0));
  leadFollowToggle.setBounds(cellBounds(2, 0));
  
  loadModelButton.setBounds(cellBounds(3, 0));
  saveModelButton.setBounds(cellBounds(4, 0));
  resetModelButton.setBounds(cellBounds(5, 0));

  quantGroup.setBounds(cellBounds(0, 1, 2, 2).reduced(4));
  auto quantArea = quantGroup.getBounds().reduced(10);

  const int rowGap = 6;
  const int rowHeight = (quantArea.getHeight() - rowGap * 2) / 3;

  auto row1 = quantArea.removeFromTop(rowHeight).reduced(4);
  quantArea.removeFromTop(rowGap);
  auto row2 = quantArea.removeFromTop(rowHeight).reduced(4);
  quantArea.removeFromTop(rowGap);
  auto row3 = quantArea.reduced(4);

  auto clockColumnWidth = juce::jmin(row1.getHeight(), 80);
  auto clockArea = row1.removeFromLeft(clockColumnWidth);
  
  // auto clockLabelArea = clockArea.removeFromTop(20);
  // clockLightLabel.setBounds(clockLabelArea);
  
  clockIndicator.setBounds(clockArea);
  row1.removeFromLeft(8);

  auto bpmLabelArea = row1.removeFromLeft(60);
  bpmLabel.setBounds(bpmLabelArea);

  auto sliderArea = row1.reduced(2);
  bpmSlider.setBounds(sliderArea);

  const int buttonCount = static_cast<int>(divisionButtons.size());
  if (buttonCount > 0)
  {
      const int buttonGap = 6;
      const int totalGap = buttonGap * (buttonCount);
      // const int buttonWidth = juce::jmax(40, (row2.getWidth() - totalGap) / buttonCount+1);
      const int buttonWidth = (row2.getWidth() - totalGap) / buttonCount;
      auto buttonRow = row2;
      for (int i = 0; i < buttonCount; ++i)
      {
          auto bounds = buttonRow.removeFromLeft(buttonWidth);
        // DBG("Drawing button " << i << " label " << divisionButtons[i]->getButtonText() << " x " << bounds.getX());

          divisionButtons[i]->setBounds(bounds);
          if (i < buttonCount - 1)
              buttonRow.removeFromLeft(buttonGap);
      }
  }

  const int toggleHeight = 36;
  auto hostArea = row3.removeFromLeft(row3.getWidth() / 2);
  hostClockToggle.setBounds(hostArea.withSizeKeepingCentre(hostArea.getWidth(), toggleHeight));
  quantiseToggle.setBounds(row3.withSizeKeepingCentre(row3.getWidth(), toggleHeight));

  // Row 1-2: Probability (2x2) + indicators stacked
  probGroup.setBounds(cellBounds(2, 1, 3, 2).reduced(4));
  auto probArea = probGroup.getBounds().reduced(10);

  // Add some vertical padding and increase height
  probArea.removeFromTop(8); // Add padding at top
  probabilitySlider.setBounds(
      probArea.removeFromTop(48)); // Increased from 32 to 48

  // Split remaining prob area into two rows for In/Out indicators
  auto topHalf = probArea.removeFromTop(probArea.getHeight() / 2).reduced(4);
  auto bottomHalf = probArea.reduced(4);

  const int labelW = 80;
  auto topLabelArea = topHalf.removeFromLeft(labelW);
  midiInLightLabel.setBounds(topLabelArea);
  noteInIndicator.setBounds(topHalf);

  auto bottomLabelArea = bottomHalf.removeFromLeft(labelW);
  midiOutLightLabel.setBounds(bottomLabelArea);
  noteOutIndicator.setBounds(bottomHalf);

  midiGroup.setBounds(cellBounds(0, 3, gridColumns, 1).reduced(4));
  auto midiArea = midiGroup.getBounds().reduced(10);

  auto midiLeft = midiArea.removeFromLeft(midiArea.getWidth() / 2).reduced(6);
  auto midiRight = midiArea.reduced(6);

  const int midiLabelHeight = 24;
  const int midiComboHeight = 28;

  // Stack label above combo (MIDI In)
  auto midiInBounds =
      midiLeft.removeFromTop(midiLabelHeight + midiComboHeight + 4); // 4px spacing
  midiInLabel.setBounds(midiInBounds.removeFromTop(midiLabelHeight));
  midiInCombo.setBounds(midiInBounds.removeFromTop(midiComboHeight));

  // Stack label above combo (MIDI Out)
  auto midiOutBounds = midiRight.removeFromTop(midiLabelHeight + midiComboHeight + 4);
  midiOutLabel.setBounds(midiOutBounds.removeFromTop(midiLabelHeight));
  midiOutCombo.setBounds(midiOutBounds.removeFromTop(midiComboHeight));
}

// ===============================================================
// Helpers
// ===============================================================

void ImproviserControlGUI::configureChunkyControls() {

  // Chunky toggles
  playingToggle.setSize(140, 40);
  learningToggle.setSize(140, 40);
  leadFollowToggle.setSize(140, 40);


  playingToggle.setToggleState(true, juce::dontSendNotification);
  learningToggle.setToggleState(true, juce::dontSendNotification);
  leadFollowToggle.setToggleState(true, juce::dontSendNotification);

  // Tooltips
  playingToggle.setTooltip("Toggle AI playback on/off");
  learningToggle.setTooltip("Toggle AI learning on/off");
  leadFollowToggle.setTooltip("Toggle AI lead/ follow");

  loadModelButton.setTooltip("Load a trained model from disk");
  saveModelButton.setTooltip("Save the current model to disk");
  resetModelButton.setTooltip(
      "Reset the model to initial state"); // Add tooltip
  quantiseToggle.setTooltip("Toggle quatisation on model output");
  hostClockToggle.setTooltip("Sync quantisation to the host transport clock");
  bpmSlider.setTooltip("Beats per minute (60-240)");
  divisionCombo.setTooltip("Quantisation division (fraction of a beat)");
  probabilitySlider.setTooltip("Probability of AI playing (0.0-1.0)");
  midiInCombo.setTooltip("Select MIDI Input channel (All or 1-16)");
  midiOutCombo.setTooltip("Select MIDI Output channel (1-16)");
}

float ImproviserControlGUI::divisionIdToValue(int itemId) {
  switch (itemId) {
  case 1:
    return 1.0f; // beat
  case 2:
    return 1.0f / 3.0f; // 1/3
  case 3:
    return 0.25f; // quarter
  case 4:
    return 0.125f; // 1/8
  case 5:
    return 1.0f / 12.0f; // 1/12
  case 6:
    return 1.0f / 16.0f; // 1/16
  default:
    return 1.0f;
  }
}

int ImproviserControlGUI::midiInIdToChannel(int itemId) {
  if (itemId == 100)
    return 0; // All
  if (itemId >= 101 && itemId <= 116)
    return itemId - 100; // 1..16
  return 0;
}

int ImproviserControlGUI::midiOutIdToChannel(int itemId) {
  if (itemId >= 201 && itemId <= 216)
    return itemId - 200; // 1..16
  return 1;              // default
}

// ===============================================================
// Listeners (UI -> external listener)
// ===============================================================

void ImproviserControlGUI::buttonClicked(juce::Button *button) 
{
    if (button == &loadModelButton){
      const auto chooserFlags = juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles;

      // Keep chooser alive while the async dialog is visible.
      loadFileChooser = std::make_unique<juce::FileChooser>(
          "Select a model file…",
          juce::File::getCurrentWorkingDirectory(),
          "*.model");

      loadFileChooser->launchAsync(chooserFlags,
          [this](const juce::FileChooser& fc)
          {
            auto chosenFile = fc.getResult();

            if (chosenFile.existsAsFile())
            {
              controlListener.loadModel(
                  chosenFile.getFullPathName().toStdString());
            }
            else
            {
              DBG("No file selected.");
            }

            loadFileChooser.reset();
          });
      return;
    }

    if (button == &saveModelButton)
    {
      const auto chooserFlags =
          juce::FileBrowserComponent::saveMode
          | juce::FileBrowserComponent::canSelectFiles;

      saveFileChooser = std::make_unique<juce::FileChooser>(
          "Save model as…",
          juce::File::getCurrentWorkingDirectory(),
          "*.model");

      saveFileChooser->launchAsync(chooserFlags,
          [this](const juce::FileChooser& fc)
          {
            auto chosenFile = fc.getResult().withFileExtension(".model");

            if (chosenFile.getFullPathName().isNotEmpty())
            {
              controlListener.saveModel(
                  chosenFile.getFullPathName().toStdString());
            }
            else
            {
              DBG("Save cancelled.");
            }

            saveFileChooser.reset();
          });
      return;
    }




  if (button == &resetModelButton) // Add this block
  {
    controlListener.resetModel();
    return;
  }

  for (size_t i = 0; i < divisionButtons.size(); ++i)
  {
      if (button == divisionButtons[i].get())
      {
      
          divisionCombo.setSelectedId(divisionButtonIds[i], juce::sendNotification);
          updateDivisionButtonsFromCombo();
          return;
      }
  }
  // if (button == &playingToggle)
  // {
  //     if (!listener) return;
  //     if (playingToggle.getToggleState()) listener->playingOn();
  //     else                                 listener->playingOff();
  //     return;
  // }
  // if (button == &learningToggle)
  // {
  //     if (!listener) return;
  //     if (learningToggle.getToggleState()) listener->learningOn();
  //     else                                  listener->learningOff();
  //     return;
  // }
}

void ImproviserControlGUI::sliderValueChanged(juce::Slider *slider) {
  // if (!listener) return;

  // if (slider == &bpmSlider)
  // {
  //     listener->setQuantBPM((float) bpmSlider.getValue());
  //     return;
  // }
  // if (slider == &probabilitySlider)
  // {
  //     listener->setPlayProbability((float) probabilitySlider.getValue());
  //     return;
  // }
}

void ImproviserControlGUI::comboBoxChanged(juce::ComboBox *combo) {
  if (combo == &divisionCombo)
  {
      updateDivisionButtonsFromCombo();
      return;
  }
  // if (combo == &midiInCombo)
  // {
  //     const int ch = midiInIdToChannel(midiInCombo.getSelectedId());
  //     listener->setMIDIInChannel(ch);
  //     return;
  // }
  // if (combo == &midiOutCombo)
  // {
  //     const int ch = midiOutIdToChannel(midiOutCombo.getSelectedId());
  //     listener->setMIDIOutChannel(ch);
  //     return;
  // }
}

void ImproviserControlGUI::updateHostClockToggleText()
{
  const bool useHost = hostClockToggle.getToggleState();
  juce::String text = useHost ? "host" : "internal";
  if (useHost && displayingHostBpm)
  {
      text += " (" + juce::String(externalBpmDisplayValue, 2) + ")";
  }
  hostClockToggle.setButtonText(text);
}

void ImproviserControlGUI::createDivisionButtons()
{
  divisionButtons.clear();
  divisionButtonIds.clear();

  const int radioGroup = 0x2384;
  const int numItems = divisionCombo.getNumItems();
  for (int idx = 0; idx < numItems; ++idx)
  // for (int idx = 1; idx <= numItems; ++idx)
  {
      const int itemId = divisionCombo.getItemId(idx);
      const juce::String label = divisionCombo.getItemText(idx);
      // DBG("Adding button " << label );
      auto button = std::make_unique<juce::ToggleButton>(label);
      button->setClickingTogglesState(true);
      button->setRadioGroupId(radioGroup);
      button->addListener(this);
      button->setTooltip("Quantisation division " + label);
      button->setLookAndFeel(&divisionButtonLookAndFeel);
      addAndMakeVisible(*button);
      divisionButtonIds.push_back(itemId);
      divisionButtons.emplace_back(std::move(button));
  }
}

void ImproviserControlGUI::updateDivisionButtonsFromCombo()
{
  const int selectedId = divisionCombo.getSelectedId();
  for (size_t i = 0; i < divisionButtons.size(); ++i)
  {
      const bool shouldSelect = (divisionButtonIds[i] == selectedId);
      divisionButtons[i]->setToggleState(shouldSelect, juce::dontSendNotification);
  }
}
