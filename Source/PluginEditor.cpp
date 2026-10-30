#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

// ================================================================== Fonts
static Typeface::Ptr boldFace()
{
    static auto t = Typeface::createSystemTypefaceFor (BinaryData::BricolageExtraBold_ttf, BinaryData::BricolageExtraBold_ttfSize);
    return t;
}
static Typeface::Ptr mediumFace()
{
    static auto t = Typeface::createSystemTypefaceFor (BinaryData::BricolageMedium_ttf, BinaryData::BricolageMedium_ttfSize);
    return t;
}
Font Fonts::bold (float h)   { return Font (FontOptions (boldFace()).withHeight (h)); }
Font Fonts::medium (float h) { return Font (FontOptions (mediumFace()).withHeight (h)); }

static const String minusSign = String::fromUTF8 ("\xe2\x88\x92");
static String formatDb (float db)
{
    if (db > -0.05f) return "0.0 dB";
    return minusSign + String (-db, 1) + " dB";
}

// ================================================================== Look and feel
PlosivoreLook::PlosivoreLook()
{
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (TooltipWindow::backgroundColourId, Palette::raised);
    setColour (TooltipWindow::textColourId, Palette::ink);
}

void PlosivoreLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, Slider& s)
{
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float thick = jmax (4.0f, r * 0.13f);
    const float arcR = r - thick * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool active = s.isMouseOverOrDragging();

    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Palette::dusk.withAlpha (0.28f));
    g.strokePath (track, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));

    Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (Palette::ink.withAlpha (active ? 1.0f : 0.88f));
    g.strokePath (value, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));

    const float bodyR = r * 0.66f;
    g.setColour (active ? Palette::raised.brighter (0.08f) : Palette::raised);
    g.fillEllipse (Rectangle<float> (bodyR * 2, bodyR * 2).withCentre (c));

    const float dotDist = bodyR * 0.68f;
    const auto dot = c.getPointOnCircumference (dotDist, angle);
    const float dotR = jmax (2.5f, r * 0.075f);
    g.setColour (Palette::ink);
    g.fillEllipse (Rectangle<float> (dotR * 2, dotR * 2).withCentre (dot));

    if (s.hasKeyboardFocus (false))
    {
        g.setColour (Palette::ink.withAlpha (0.5f));
        g.drawEllipse (Rectangle<float> (r * 2 + 2, r * 2 + 2).withCentre (c), 1.0f);
    }
}

void PlosivoreLook::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const float rad = r.getHeight() * 0.5f;
    const bool on = b.getToggleState();
    const auto onColour = b.findColour (ToggleButton::tickColourId);

    if (on)
    {
        g.setColour (onColour);
        g.fillRoundedRectangle (r, rad);
    }
    else
    {
        g.setColour (Palette::panel.brighter (highlighted ? 0.08f : 0.0f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Palette::dusk.withAlpha (highlighted ? 0.8f : 0.5f));
        g.drawRoundedRectangle (r, rad, 1.2f);
    }

    // State dot so on/off doesn't rely on colour alone
    const float d = 8.0f;
    auto dotArea = Rectangle<float> (d, d).withCentre ({ r.getX() + rad + 2.0f, r.getCentreY() });
    g.setColour (on ? Palette::night : Palette::dusk);
    if (on) g.fillEllipse (dotArea); else g.drawEllipse (dotArea, 1.4f);

    g.setColour (on ? Palette::night : Palette::ink);
    g.setFont (Fonts::medium (14.5f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (rad + 12.0f).withTrimmedRight (10.0f), Justification::centredLeft);

    if (b.hasKeyboardFocus (false))
    {
        g.setColour (Palette::ink.withAlpha (0.6f));
        g.drawRoundedRectangle (r.expanded (2.0f), rad + 2.0f, 1.0f);
    }
}

void PlosivoreLook::drawCornerResizer (Graphics& g, int w, int h, bool over, bool dragging)
{
    // Triangle of six small dots in the corner
    g.setColour (Palette::dusk.withAlpha (over || dragging ? 0.9f : 0.45f));
    const float step = jmin ((float) w, (float) h) / 4.0f, d = 2.6f;
    for (int row = 0; row < 3; ++row)
        for (int col = 2 - row; col < 3; ++col)
            g.fillEllipse ((float) w - step * (float) (3 - col) + step * 0.5f - d * 0.5f,
                           (float) h - step * (float) (3 - row) + step * 0.5f - d * 0.5f, d, d);
}

Rectangle<int> PlosivoreLook::getTooltipBounds (const String& text, Point<int> pos, Rectangle<int> parent)
{
    const int w = jmin (300, (int) GlyphArrangement::getStringWidth (Fonts::medium (13.5f), text) + 24);
    const int h = 30;
    return Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                           pos.y > parent.getCentreY() ? pos.y - (h + 10) : pos.y + 14, w, h)
               .constrainedWithin (parent);
}

void PlosivoreLook::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    g.setColour (Palette::raised);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 8.0f);
    g.setColour (Palette::ink);
    g.setFont (Fonts::medium (13.5f));
    g.drawText (text, Rectangle<int> (w, h).reduced (12, 0), Justification::centredLeft);
}

// ================================================================== Knob
Knob::Knob (AudioProcessorValueTreeState& state, const String& paramId, const String& n,
            const String& tip, bool isHero)
    : name (n), hero (isHero)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.22f, MathConstants<float>::pi * 2.78f, true);
    slider.setMouseDragSensitivity (hero ? 220 : 180);
    slider.setTooltip (tip);
    slider.setWantsKeyboardFocus (true);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (hero ? 44 : 40);
    slider.setBounds (r);
}

void Knob::paint (Graphics& g)
{
    auto r = getLocalBounds().removeFromBottom (hero ? 44 : 40);
    g.setColour (Palette::ink);
    g.setFont (Fonts::bold (hero ? 17.0f : 15.0f));
    g.drawText (name, r.removeFromTop (hero ? 22 : 20), Justification::centred);

    String value = slider.getTextFromValue (slider.getValue());
    if (value.startsWithChar ('-')) value = minusSign + value.substring (1);
    g.setColour (slider.isMouseOverOrDragging() ? Palette::ink : Palette::dusk);
    g.setFont (Fonts::medium (13.5f));
    g.drawText (value, r, Justification::centredTop);
}

// ================================================================== Scope
void PopScope::push (const VisFrame& f)
{
    history[(size_t) (total & (historySize - 1))] = f;
    if (f.popStart)
        bubbles.push_back ({ total, now, Random::getSystemRandom().nextFloat() });
    ++total;
}

static float ampToNorm (float a)
{
    // Gentle power curve: quiet detail stays visible, but loud pops still tower over the voice.
    return jlimit (0.0f, 1.0f, std::pow (jmax (a, 0.0f), 0.6f));
}

void PopScope::paint (Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, 14.0f);

    auto inner = bounds.reduced (16.0f, 12.0f);
    auto lane = inner.removeFromBottom (24.0f);
    auto caption = inner.removeFromBottom (20.0f);
    inner.removeFromBottom (4.0f);
    auto bubbleRow = inner.removeFromTop (34.0f);
    auto wave = inner;

    const int W = (int) wave.getWidth();
    const float cy = wave.getCentreY();
    const float halfH = wave.getHeight() * 0.5f;
    const float right = wave.getRight();

    // ---- gather columns (newest at the right)
    std::vector<float> hin ((size_t) W, 0.0f), hout ((size_t) W, 0.0f), gr ((size_t) W, 0.0f);
    float recentMax = 0.0f;
    for (int px = 0; px < W; ++px)
    {
        float in = 0, out = 0, red = 0;
        bool any = false;
        for (int k = 0; k < framesPerPixel; ++k)
            if (auto* f = at (total - 1 - (juce::int64) (px * framesPerPixel + k)))
            {
                in = jmax (in, f->inPeak); out = jmax (out, f->outPeak); red = jmin (red, f->grDb); any = true;
            }
        if (! any) continue;
        const size_t i = (size_t) (W - 1 - px);
        hin[i]  = ampToNorm (in)  * halfH * 0.94f;
        hout[i] = ampToNorm (out) * halfH * 0.94f;
        gr[i]   = red;
        if (px < 250) recentMax = jmax (recentMax, in);
    }

    auto envelope = [&] (const std::vector<float>& h)
    {
        Path p;
        p.startNewSubPath (wave.getX(), cy);
        for (int i = 0; i < W; ++i) p.lineTo (wave.getX() + (float) i, cy - h[(size_t) i]);
        for (int i = W - 1; i >= 0; --i) p.lineTo (wave.getX() + (float) i, cy + h[(size_t) i]);
        p.closeSubPath();
        return p;
    };

    // centre line
    g.setColour (Palette::dusk.withAlpha (0.25f));
    g.fillRect (wave.getX(), cy - 0.5f, wave.getWidth(), 1.0f);

    // Input envelope in pink sits behind the cleaned output: whatever pink shows is what was removed.
    g.setColour (Palette::pop);
    g.fillPath (envelope (hin));
    g.setColour (Palette::ink.withAlpha (0.92f));
    g.fillPath (envelope (hout));

    // ---- reduction lane
    g.setColour (Palette::night.withAlpha (0.55f));
    g.fillRoundedRectangle (lane, 8.0f);
    const float laneTop = lane.getY() + 4.0f, laneH = lane.getHeight() - 8.0f;
    {
        Path p;
        p.startNewSubPath (lane.getX(), laneTop);
        for (int i = 0; i < W && i < (int) lane.getWidth(); ++i)
            p.lineTo (lane.getX() + (float) i, laneTop + laneH * jlimit (0.0f, 1.0f, -gr[(size_t) i] / 30.0f));
        p.lineTo (lane.getX() + jmin ((float) W, lane.getWidth()), laneTop);
        p.closeSubPath();
        g.setColour (Palette::pop.withAlpha (0.9f));
        g.fillPath (p);
    }
    g.setFont (Fonts::medium (12.5f));
    g.setColour (Palette::dusk);
    g.drawText ("Reduction", caption, Justification::centredLeft);
    const float nowGr = W > 0 ? gr[(size_t) (W - 1)] : 0.0f;
    g.setColour (nowGr < -1.0f ? Palette::pop : Palette::dusk);
    g.drawText (formatDb (nowGr), caption, Justification::centredRight);

    // ---- bubbles: each caught pop inflates, bursts, then leaves a dot that scrolls away
    const float rowY = bubbleRow.getCentreY();
    for (auto& b : bubbles)
    {
        const float x = right - (float) (total - 1 - b.frame) / (float) framesPerPixel;
        if (x < wave.getX() - 30.0f) continue;

        float depth = 0.0f;  // deepest reduction during this pop
        for (int k = 0; k < 90; ++k)
            if (auto* f = at (b.frame + k)) depth = jmin (depth, f->grDb);
        const float R = 5.0f + jlimit (0.0f, 30.0f, -depth) * 0.3f;
        const float y = rowY + (b.seed - 0.5f) * 6.0f;
        const float age = (float) ((now - b.bornMs) * 0.001);

        if (age < 0.14f)
        {
            const float t = age / 0.14f;
            const float rr = R * (0.35f + 0.65f * (1.0f - (1.0f - t) * (1.0f - t)));
            g.setColour (Palette::pop.withAlpha (0.9f));
            g.fillEllipse (Rectangle<float> (rr * 2, rr * 2).withCentre ({ x, y }));
            g.setColour (Palette::ink.withAlpha (0.55f));
            g.fillEllipse (Rectangle<float> (rr * 0.5f, rr * 0.5f).withCentre ({ x - rr * 0.35f, y - rr * 0.35f }));
        }
        else if (age < 0.42f)
        {
            const float t = (age - 0.14f) / 0.28f;
            const float alpha = 1.0f - t;
            g.setColour (Palette::pop.withAlpha (alpha));
            const float rr = R * (1.0f + 0.9f * t);
            g.drawEllipse (Rectangle<float> (rr * 2, rr * 2).withCentre ({ x, y }), 2.0f * (1.0f - t) + 0.5f);
            for (int k = 0; k < 7; ++k)  // droplets
            {
                const float a = (float) k / 7.0f * MathConstants<float>::twoPi + b.seed * 3.0f;
                const auto p = Point<float> (x, y).getPointOnCircumference (R * (1.1f + 0.9f * t), a);
                const float dr = 2.6f * (1.0f - t) + 0.6f;
                g.fillEllipse (Rectangle<float> (dr * 2, dr * 2).withCentre (p));
            }
        }
        else
        {
            const float d = 4.0f + R * 0.35f;  // the dot's size still tells you how big the pop was
            g.setColour (Palette::pop.withAlpha (0.8f));
            g.fillEllipse (Rectangle<float> (d, d).withCentre ({ x, y }));
        }
    }

    // ---- empty state
    if (recentMax < 0.0015f)
    {
        g.setColour (Palette::dusk);
        g.setFont (Fonts::medium (15.0f));
        g.drawText ("Play a vocal. Every pop that gets caught shows up here in pink.", wave, Justification::centred);
    }
}

// ================================================================== Counter
void PopCounter::update()
{
    const int n = proc.popCount.load();
    if (n > shown) flash = 1.0f;
    shown = n;
    if (flash > 0.0f) { flash = jmax (0.0f, flash - 0.06f); repaint(); }
}

void PopCounter::paint (Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (Palette::ink.interpolatedWith (Palette::pop, flash));
    g.setFont (Fonts::bold (40.0f));
    g.drawText (String (shown), r.removeFromTop (44), Justification::centredRight);
    g.setColour (Palette::dusk);
    g.setFont (Fonts::medium (13.5f));
    g.drawText (shown == 1 ? "pop caught" : "pops caught", r, Justification::topRight);
}

// ================================================================== Editor
PlosivoreEditor::PlosivoreEditor (PlosivoreProcessor& p)
    : AudioProcessorEditor (p), proc (p), counter (p),
      sensitivity (p.apvts, ParamID::sensitivity, "Sensitivity", "How readily a low thump counts as a pop", true),
      frequency   (p.apvts, ParamID::frequency,   "Frequency",   "Pops below this frequency are removed"),
      depth       (p.apvts, ParamID::depth,       "Depth",       "The most a pop can be turned down"),
      release     (p.apvts, ParamID::release,     "Release",     "How quickly the low end returns after a pop"),
      output      (p.apvts, ParamID::output,      "Output",      "Output level")
{
    setLookAndFeel (&look);
    tooltips.setLookAndFeel (&look);

    addAndMakeVisible (content);
    for (Component* c : std::initializer_list<Component*> { &scope, &counter, &sensitivity, &frequency, &depth,
                                                             &release, &output, &listenButton, &lookaheadButton })
        content.addAndMakeVisible (c);

    listenButton.setColour (ToggleButton::tickColourId, Palette::pop);
    listenButton.setTooltip ("Hear only what is being removed");
    lookaheadButton.setColour (ToggleButton::tickColourId, Palette::ink);
    lookaheadButton.setTooltip ("Catches the very start of each pop. Adds 3 ms of latency.");
    listenAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamID::listen, listenButton);
    lookAtt   = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ParamID::lookahead, lookaheadButton);

    // Discard frames queued while the window was closed
    while (proc.readVisFrames (readBuffer.data(), (int) readBuffer.size()) > 0) {}

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 2, baseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / baseH);
    setSize (baseW, baseH);
    startTimerHz (60);
}

PlosivoreEditor::~PlosivoreEditor()
{
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void PlosivoreEditor::timerCallback()
{
    scope.advance (Time::getMillisecondCounterHiRes());
    const int n = proc.readVisFrames (readBuffer.data(), (int) readBuffer.size());
    for (int i = 0; i < n; ++i) scope.push (readBuffer[(size_t) i]);
    scope.repaint();
    counter.update();
}

void PlosivoreEditor::paint (Graphics& g) { g.fillAll (Palette::night); }

void PlosivoreEditor::resized()
{
    const float s = (float) getWidth() / (float) baseW;
    content.setBounds (0, 0, baseW, baseH);
    content.setTransform (AffineTransform::scale (s));

    auto r = Rectangle<int> (baseW, baseH).reduced (26, 20);
    auto header = r.removeFromTop (62);
    counter.setBounds (header.removeFromRight (220));
    r.removeFromTop (8);
    scope.setBounds (r.removeFromTop (238));
    r.removeFromTop (18);

    auto controls = r;
    auto toggles = controls.removeFromRight (190);
    toggles = toggles.withSizeKeepingCentre (190, 96);
    listenButton.setBounds (toggles.removeFromTop (42));
    toggles.removeFromTop (12);
    lookaheadButton.setBounds (toggles.removeFromTop (42));

    controls.removeFromRight (16);
    sensitivity.setBounds (controls.removeFromLeft (142));
    controls.removeFromLeft (14);
    const int kw = controls.getWidth() / 4;
    for (auto* k : { &frequency, &depth, &release, &output })
    {
        auto cell = controls.removeFromLeft (kw);
        k->setBounds (cell.withTrimmedTop (jmax (0, cell.getHeight() - 126)));   // bottom-aligned with the hero knob
    }
}

void PlosivoreEditor::Content::paint (Graphics& g)
{
    g.fillAll (Palette::night);
    auto r = getLocalBounds().reduced (26, 20).removeFromTop (62);
    g.setColour (Palette::ink);
    g.setFont (Fonts::bold (40.0f));
    g.drawText ("plosivore", r.removeFromTop (44), Justification::centredLeft);
    g.setColour (Palette::dusk);
    g.setFont (Fonts::medium (13.5f));
    g.drawText ("Eats plosives and mic pops, leaves the voice alone", r, Justification::topLeft);
}
