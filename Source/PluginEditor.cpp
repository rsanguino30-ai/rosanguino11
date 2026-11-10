#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour bg      { 0xff0d0d12 };
    const juce::Colour panel   { 0xff17171f };
    const juce::Colour track   { 0xff2a2a35 };
    const juce::Colour text    { 0xffe8e8ee };
    const juce::Colour dim     { 0xff8a8a99 };
    const juce::Colour cool    { 0xff4fc3ff }; // build = 0
    const juce::Colour hot     { 0xffff3d5a }; // build = 1

    inline juce::Colour accent (float heat) { return cool.interpolatedWith (hot, juce::jlimit (0.0f, 1.0f, heat)); }
}

//==============================================================================
TensionLookAndFeel::TensionLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Colours::dim);
}

juce::Label* TensionLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (13.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

void TensionLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider& s)
{
    const bool big = s.getProperties()["big"];
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (big ? 14.0f : 6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float thickness = big ? 12.0f : 5.0f;
    const float arcR = radius - thickness * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto accent = big ? Colours::accent (heat) : Colours::accent (heat * 0.6f);

    // Track
    juce::Path trackArc;
    trackArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::track);
    g.strokePath (trackArc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc (+ glow on the big knob)
    if (pos > 0.0f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        if (big)
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (accent.withAlpha (0.08f * (float) i * (0.4f + heat)));
                g.strokePath (valueArc, juce::PathStrokeType (thickness + (float) i * 7.0f,
                                                              juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Body
    const float bodyR = arcR - thickness * (big ? 1.4f : 1.6f);
    g.setGradientFill (juce::ColourGradient (Colours::panel.brighter (0.25f), centre.x, centre.y - bodyR,
                                             Colours::panel.darker (0.4f), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // Pointer
    const float p0 = bodyR * (big ? 0.70f : 0.45f), p1 = bodyR * 0.90f;
    const juce::Point<float> a (centre.x + p0 * std::sin (angle), centre.y - p0 * std::cos (angle));
    const juce::Point<float> b (centre.x + p1 * std::sin (angle), centre.y - p1 * std::cos (angle));
    g.setColour (Colours::text);
    g.drawLine ({ a, b }, big ? 4.0f : 2.5f);

    if (big)
    {
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (bodyR * 0.36f, juce::Font::bold));
        g.drawText (juce::String (juce::roundToInt (pos * 100.0f)) + "%",
                    juce::Rectangle<float> (bodyR * 2.0f, bodyR * 0.6f).withCentre (centre),
                    juce::Justification::centred);
    }
}

//==============================================================================
TensionAudioProcessorEditor::TensionAudioProcessorEditor (TensionAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    setupKnob (build,  ParamIDs::build,  "BUILD", true);
    setupKnob (reverb, ParamIDs::reverb, "REVERB");
    setupKnob (size,   ParamIDs::size,   "SIZE");
    setupKnob (noise,  ParamIDs::noise,  "NOISE");
    setupKnob (tone,   ParamIDs::tone,   "NOISE TONE");
    setupKnob (lowCut, ParamIDs::lowCut, "LOW CUT");
    setupKnob (width,  ParamIDs::width,  "WIDTH");
    setupKnob (curve,  ParamIDs::curve,  "CURVE");
    setupKnob (output, ParamIDs::output, "OUTPUT");

    for (int i = 0; i < 40; ++i)
        streaks.push_back ({ rng.nextFloat(), rng.nextFloat(), 0.03f + rng.nextFloat() * 0.08f, 0.4f + rng.nextFloat() });

    setSize (780, 440);
    startTimerHz (60);
}

TensionAudioProcessorEditor::~TensionAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void TensionAudioProcessorEditor::setupKnob (Knob& k, const juce::String& paramID, const juce::String& name, bool big)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (big ? juce::Slider::NoTextBox : juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.getProperties().set ("big", big);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, Colours::text);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (big ? 18.0f : 12.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, big ? Colours::text : Colours::dim);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramID, k.slider);
}

void TensionAudioProcessorEditor::timerCallback()
{
    heat   = processor.displayBuild.load();
    cutoff = processor.displayCutoff.load();
    lnf.heat = heat;

    const float dt = 1.0f / 60.0f;
    for (auto& s : streaks)
    {
        s.y -= dt * s.speed * (0.05f + heat * 1.6f);
        if (s.y + s.len < 0.0f)
        {
            s.y = 1.0f + rng.nextFloat() * 0.2f;
            s.x = rng.nextFloat();
        }
    }
    repaint();
}

void TensionAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    const auto accent = Colours::accent (heat);

    g.fillAll (Colours::bg);

    // Glow rising from the bottom with the build
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.05f + 0.35f * heat), area.getCentreX(), area.getBottom(),
                                             juce::Colours::transparentBlack, area.getCentreX(), area.getBottom() - area.getHeight() * (0.3f + 0.7f * heat), false));
    g.fillRect (area);

    // Rising streaks — faster and brighter as tension grows
    for (const auto& s : streaks)
    {
        const float x = area.getWidth() * s.x;
        const float y0 = area.getHeight() * s.y;
        const float y1 = y0 + area.getHeight() * s.len * (1.0f + heat * 2.0f);
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.05f + 0.45f * heat), x, y0,
                                                 juce::Colours::transparentBlack, x, y1, false));
        g.fillRect (x, y0, 1.5f, y1 - y0);
    }

    // Header
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("TENSION", 24, 14, 300, 34, juce::Justification::centredLeft);
    g.setColour (Colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("BUILD-UP FX  |  CHAMBA AUDIO", 24, 44, 300, 16, juce::Justification::centredLeft);

    g.setColour (accent);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("LOW CUT  " + juce::String (juce::roundToInt (cutoff)) + " Hz",
                getWidth() - 224, 22, 200, 20, juce::Justification::centredRight);

    // Panel behind the small knobs
    auto panel = juce::Rectangle<float> (340.0f, 76.0f, area.getWidth() - 364.0f, area.getHeight() - 100.0f);
    g.setColour (Colours::panel.withAlpha (0.85f));
    g.fillRoundedRectangle (panel, 14.0f);
    g.setColour (accent.withAlpha (0.25f));
    g.drawRoundedRectangle (panel, 14.0f, 1.0f);
}

void TensionAudioProcessorEditor::resized()
{
    // Big knob on the left
    auto bigArea = juce::Rectangle<int> (20, 70, 300, 300);
    build.slider.setBounds (bigArea);
    build.label.setBounds (bigArea.getX(), bigArea.getBottom() + 2, bigArea.getWidth(), 26);

    // Small knobs: 2 rows x 4
    Knob* grid[2][4] = { { &reverb, &size, &noise, &tone }, { &lowCut, &width, &curve, &output } };
    const int px = 352, py = 88, cellW = 98, cellH = 164;
    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 4; ++c)
        {
            auto cell = juce::Rectangle<int> (px + c * cellW, py + r * cellH, cellW, cellH);
            grid[r][c]->label.setBounds (cell.removeFromTop (18));
            grid[r][c]->slider.setBounds (cell.reduced (4, 2));
        }
}
