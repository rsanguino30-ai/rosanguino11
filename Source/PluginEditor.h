#pragma once

#include "PluginProcessor.h"

class TensionLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TensionLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    float heat = 0.0f; // 0..1, tints the accent colour with the build amount
};

class TensionAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit TensionAudioProcessorEditor (TensionAudioProcessor&);
    ~TensionAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramID, const juce::String& name, bool big = false);

    TensionAudioProcessor& processor;
    TensionLookAndFeel lnf;

    Knob build, reverb, size, noise, tone, lowCut, width, curve, output;

    struct Streak { float x, y, len, speed; };
    std::vector<Streak> streaks;
    juce::Random rng;
    float heat = 0.0f;
    float cutoff = 20.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TensionAudioProcessorEditor)
};
