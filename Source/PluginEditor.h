#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace Palette
{
    const juce::Colour night  { 0xff24182f };  // background
    const juce::Colour panel  { 0xff32223f };  // scope + knob bodies
    const juce::Colour raised { 0xff402d50 };
    const juce::Colour ink    { 0xffecdff5 };  // text, clean audio
    const juce::Colour dusk   { 0xff8d7ba0 };  // secondary
    const juce::Colour pop    { 0xffff4f9a };  // reserved for pops / removed audio
}

struct Fonts
{
    static juce::Font bold (float h);
    static juce::Font medium (float h);
};

// ------------------------------------------------------------------ Look and feel
class PlosivoreLook : public juce::LookAndFeel_V4
{
public:
    PlosivoreLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
};

// ------------------------------------------------------------------ Knob with name + value
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& name,
          const juce::String& tooltip, bool hero = false);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::String name;
    bool hero;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ------------------------------------------------------------------ Scrolling pop scope
class PopScope : public juce::Component
{
public:
    void push (const VisFrame& f);
    void advance (double nowMs) { now = nowMs; }
    void paint (juce::Graphics&) override;

private:
    static constexpr int historySize = 1 << 14;
    static constexpr int framesPerPixel = 3;
    std::vector<VisFrame> history = std::vector<VisFrame> (historySize);
    juce::int64 total = 0;

    struct Bubble { juce::int64 frame; double bornMs; float seed; };
    std::vector<Bubble> bubbles;
    double now = 0;

    const VisFrame* at (juce::int64 idx) const
    {
        return (idx >= 0 && idx < total && total - idx < historySize) ? &history[(size_t) (idx & (historySize - 1))] : nullptr;
    }
};

// ------------------------------------------------------------------ Pop counter
class PopCounter : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PopCounter (PlosivoreProcessor& p) : proc (p) { setTooltip ("Click to reset the count"); }
    void update();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { proc.resetRequested = true; shown = 0; flash = 0; repaint(); }

private:
    PlosivoreProcessor& proc;
    int shown = 0;
    float flash = 0;
};

// ------------------------------------------------------------------ Editor
class PlosivoreEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PlosivoreEditor (PlosivoreProcessor&);
    ~PlosivoreEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 800, baseH = 500;

private:
    void timerCallback() override;

    PlosivoreProcessor& proc;
    PlosivoreLook look;
    juce::TooltipWindow tooltips { this, 500 };

    struct Content : public juce::Component
    {
        void paint (juce::Graphics&) override;
    } content;

    PopScope scope;
    PopCounter counter;
    Knob sensitivity, frequency, depth, release, output;
    juce::ToggleButton listenButton { "Listen to removed" }, lookaheadButton { "Lookahead 3 ms" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> listenAtt, lookAtt;

    std::vector<VisFrame> readBuffer = std::vector<VisFrame> (PlosivoreProcessor::fifoSize);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlosivoreEditor)
};
