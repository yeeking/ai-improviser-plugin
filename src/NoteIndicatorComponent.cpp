#include "NoteIndicatorComponent.h"

NoteIndicatorComponent::NoteIndicatorComponent()
{
    updateFrameTimer();
}

NoteIndicatorComponent::~NoteIndicatorComponent()
{
    stopTimer();
}

void NoteIndicatorComponent::setNote(int noteNumber, float velocity01)
{
    const auto doSet = [this, noteNumber, velocity01]
    {
        setNoteOnMessageThread(noteNumber, velocity01);
    };

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        doSet();
    else
        juce::MessageManager::callAsync(doSet);
}

void NoteIndicatorComponent::setNoteOnMessageThread(int noteNumber, float velocity01)
{
    lastNote.store(juce::jlimit(-1, 127, noteNumber), std::memory_order_relaxed);
    brightness.store(juce::jlimit(0.0f, 1.0f, velocity01), std::memory_order_relaxed);

    if (brightness.load(std::memory_order_relaxed) > redrawThresh)
        repaint();
}

void NoteIndicatorComponent::setFrameRateHz(int hz)
{
    frameRateHz = juce::jlimit(1, 240, hz);
    updateFrameTimer();
}

void NoteIndicatorComponent::setDecaySeconds(float seconds)
{
    decaySeconds = juce::jlimit(0.05f, 5.0f, seconds);
}

void NoteIndicatorComponent::setRedrawThreshold(float threshold01)
{
    redrawThresh = juce::jlimit(0.0f, 1.0f, threshold01);
}

void NoteIndicatorComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Base backplate
    auto outlineColour = findColour(juce::GroupComponent::outlineColourId);
    auto fillColour    = findColour(juce::TextButton::buttonColourId).withMultipliedAlpha(0.12f);

    g.setColour(fillColour);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(outlineColour);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    // Bright overlay
    const float b = brightness.load(std::memory_order_relaxed);
    if (b > 0.0f)
    {
        auto activeColour = findColour(juce::TextButton::buttonOnColourId).withAlpha(juce::jlimit(0.0f, 1.0f, b));
        g.setColour(activeColour);
        g.fillRoundedRectangle(bounds, 6.0f);
    }

    // Note number text
    juce::String labelText = "-";
    const int n = lastNote.load(std::memory_order_relaxed);
    if (n >= 0 && n <= 127)
        labelText = juce::String(n);

    const float h = bounds.getHeight();
    const float w = bounds.getWidth();
    float fontSize = juce::jmin(h * 0.70f, w * 0.45f);

    g.setFont(juce::Font(fontSize, juce::Font::bold));

    // Contrast-aware text
    juce::Colour textColour = (b > 0.4f) ? juce::Colours::black : findColour(juce::Label::textColourId);
    g.setColour(textColour);

    g.drawFittedText(labelText, getLocalBounds(), juce::Justification::centred, 1);
}

void NoteIndicatorComponent::resized()
{
    // No children to layout
}

void NoteIndicatorComponent::timerCallback()
{
    const float prev = brightness.load(std::memory_order_relaxed);
    if (prev <= 0.0f || decaySeconds <= 0.0f || frameRateHz <= 0)
        return;

    const float decayPerTick = 1.0f / (decaySeconds * (float) frameRateHz);
    const float next = juce::jmax(0.0f, prev - decayPerTick);
    brightness.store(next, std::memory_order_relaxed);

    const bool needRepaint = (prev > redrawThresh) || (next > redrawThresh);
    if (needRepaint)
        repaint();
}

void NoteIndicatorComponent::updateFrameTimer()
{
    stopTimer();
    if (frameRateHz > 0)
        startTimerHz(frameRateHz);
}
