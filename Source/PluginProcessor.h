#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace ParamIDs
{
    inline constexpr auto build   = "build";    // macro 0..1
    inline constexpr auto reverb  = "reverb";   // max reverb wet at full build
    inline constexpr auto size    = "size";     // room size
    inline constexpr auto noise   = "noise";    // max noise riser level
    inline constexpr auto tone    = "tone";     // how bright the noise sweep gets
    inline constexpr auto lowCut  = "lowcut";   // HPF cutoff at full build (Hz)
    inline constexpr auto width   = "width";    // stereo widening at full build
    inline constexpr auto curve   = "curve";    // response curve of the macro
    inline constexpr auto output  = "output";   // output gain (dB)
}

class TensionAudioProcessor : public juce::AudioProcessor
{
public:
    TensionAudioProcessor();
    ~TensionAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Shaped (post-curve) build amount, read by the editor for visuals.
    std::atomic<float> displayBuild { 0.0f };
    std::atomic<float> displayCutoff { 20.0f };

private:
    float shapeBuild (float raw, float curve) const noexcept;

    juce::dsp::StateVariableTPTFilter<float> hpf;       // low cut on the signal
    juce::dsp::StateVariableTPTFilter<float> noiseBpf;  // sweeping band on the noise
    juce::dsp::StateVariableTPTFilter<float> noiseHpf;  // keeps noise out of the sub
    juce::Reverb reverb;

    juce::AudioBuffer<float> wetBuffer, noiseBuffer;
    juce::Random random;

    juce::SmoothedValue<float> smoothBuild, smoothOut;

    std::atomic<float>* pBuild  = nullptr;
    std::atomic<float>* pReverb = nullptr;
    std::atomic<float>* pSize   = nullptr;
    std::atomic<float>* pNoise  = nullptr;
    std::atomic<float>* pTone   = nullptr;
    std::atomic<float>* pLowCut = nullptr;
    std::atomic<float>* pWidth  = nullptr;
    std::atomic<float>* pCurve  = nullptr;
    std::atomic<float>* pOutput = nullptr;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TensionAudioProcessor)
};
