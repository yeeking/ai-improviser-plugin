#include "StatusWidgets.h"
#include <cmath>

ThrobbingOrderCircle::ThrobbingOrderCircle()
{
    // startTimerHz(30);
}

void ThrobbingOrderCircle::setOrder(int order)
{
    const float ord = static_cast<float>(juce::jmax(0, order));
    targetOrder = ord;
    if (ord > maxOrderSeen)
        maxOrderSeen = ord;
    if (maxOrderSeen < 1.0f)
        maxOrderSeen = 1.0f;
}

void ThrobbingOrderCircle::update()

// void ThrobbingOrderCircle::timerCallback()
{
    constexpr float smoothing = 0.15f;
    constexpr float decay = 0.9995f; // gentle leak so max falls over time
    const float delta = targetOrder - currentOrder;
    currentOrder += delta * smoothing;

    maxOrderSeen = juce::jmax(1.0f, juce::jmax(currentOrder, maxOrderSeen * decay));
    if (targetOrder > maxOrderSeen)
        maxOrderSeen = targetOrder;

    phase += 0.08f;
    if (phase > juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;

    const float normalised = juce::jlimit(0.0f, 1.0f, (maxOrderSeen > 0.0f) ? currentOrder / maxOrderSeen : 0.0f);
    history.push_back(normalised);
    const size_t maxHistory = 400;
    while (history.size() > maxHistory)
        history.pop_front();

    // repaint();
}

void ThrobbingOrderCircle::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.0f));

    const auto bounds = getLocalBounds().toFloat();
    const float size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const float pad = size * 0.08f;

    const float normalised = juce::jlimit(0.0f, 1.0f, currentOrder / maxOrderSeen);
    const float baseRadius = (size * 0.5f - pad) * normalised;

    const float throb = 1.0f + 0.05f * std::sin(phase * 2.0f);
    const float radius = juce::jmax(4.0f, baseRadius * throb);

    auto centre = bounds.getCentre();
    juce::Colour fill = juce::Colours::deepskyblue.withMultipliedBrightness(0.8f + 0.2f * normalised);
    g.setColour(fill);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawFittedText("Order " + juce::String(static_cast<int>(std::round(targetOrder))),
                     getLocalBounds(), juce::Justification::centred, 1);

    if (history.size() > 1)
    {
        juce::Path trail;
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();
        const float spacing = history.size() > 1 ? w / static_cast<float>(history.size() - 1) : w;
        for (size_t i = 0; i < history.size(); ++i)
        {
            const float x = w - static_cast<float>(i) * spacing;
            const float y = h - juce::jlimit(0.0f, 1.0f, history[history.size() - 1 - i]) * h;
            if (i == 0)
                trail.startNewSubPath(x, y);
            else
                trail.lineTo(x, y);
        }
        g.setColour(juce::Colours::deepskyblue.withAlpha(0.7f));
        g.strokePath(trail, juce::PathStrokeType(2.0f));
    }
}

CallResponseMeter::CallResponseMeter()
{
    // startTimerHz(30);
}

void CallResponseMeter::setEnergy(float energy01)
{
    targetEnergy = juce::jlimit(0.0f, 1.0f, energy01);
}

void CallResponseMeter::setState(bool isEnabled, bool isInResponse)
{
    enabled = isEnabled;
    inResponse = isInResponse;
}

void CallResponseMeter::update()

// void CallResponseMeter::timerCallback()
{
    constexpr float smoothing = 0.2f;
    const float delta = targetEnergy - currentEnergy;
    currentEnergy += delta * smoothing;

    pulsePhase += (inResponse ? 0.18f : 0.08f);
    if (pulsePhase > juce::MathConstants<float>::twoPi)
        pulsePhase -= juce::MathConstants<float>::twoPi;

    // repaint();
}

void CallResponseMeter::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.0f));

    auto area = getLocalBounds().toFloat();
    const float radius = 8.0f;
    juce::Path bg;
    bg.addRoundedRectangle(area, radius);

    g.setColour(juce::Colours::darkgrey.brighter(0.1f));
    g.fillPath(bg);

    const float fillWidth = area.getWidth() * currentEnergy;
    juce::Rectangle<float> fillRect = area.withWidth(fillWidth);
    juce::Colour start = juce::Colours::chartreuse;
    juce::Colour end = juce::Colours::red;
    juce::ColourGradient grad(start, fillRect.getX(), fillRect.getCentreY(),
                              end, fillRect.getRight(), fillRect.getCentreY(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(fillRect, radius);

    const float pulse = inResponse ? (0.8f + 0.2f * std::sin(pulsePhase * 2.0f)) : 1.0f;
    if (enabled)
    {
        juce::Colour overlay = inResponse ? juce::Colours::orangered.withAlpha(0.25f * pulse)
                                          : juce::Colours::deepskyblue.withAlpha(0.18f);
        g.setColour(overlay);
        g.fillRoundedRectangle(area, radius);
    }

    juce::String text;
    if (!enabled)
        text = "Call/Response: off";
    else
        text = inResponse ? "Call/Response: response" : "Call/Response: call";

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawFittedText(text, area.toNearestInt(), juce::Justification::centred, 1);
}
