#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int subBlockSize = 32; // filter coefficients update every 32 samples

    // Transparent below the threshold, smooth saturation above it.
    inline float softClip (float x) noexcept
    {
        constexpr float t = 0.89f;
        const float a = std::abs (x);
        if (a <= t)
            return x;
        const float y = t + (1.0f - t) * std::tanh ((a - t) / (1.0f - t));
        return std::copysign (y, x);
    }

    inline float expMap (float minV, float maxV, float amount) noexcept
    {
        return minV * std::pow (maxV / minV, juce::jlimit (0.0f, 1.0f, amount));
    }
}

TensionAudioProcessor::TensionAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pBuild  = apvts.getRawParameterValue (ParamIDs::build);
    pReverb = apvts.getRawParameterValue (ParamIDs::reverb);
    pSize   = apvts.getRawParameterValue (ParamIDs::size);
    pNoise  = apvts.getRawParameterValue (ParamIDs::noise);
    pTone   = apvts.getRawParameterValue (ParamIDs::tone);
    pLowCut = apvts.getRawParameterValue (ParamIDs::lowCut);
    pWidth  = apvts.getRawParameterValue (ParamIDs::width);
    pCurve  = apvts.getRawParameterValue (ParamIDs::curve);
    pOutput = apvts.getRawParameterValue (ParamIDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout TensionAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = AudioParameterFloatAttributes()
                       .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::build, 1 }, "Build",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::reverb, 1 }, "Reverb",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::size, 1 }, "Size",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.8f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::noise, 1 }, "Noise",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.4f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::tone, 1 }, "Noise Tone",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent));

    NormalisableRange<float> cutRange (40.0f, 1500.0f);
    cutRange.setSkewForCentre (300.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::lowCut, 1 }, "Low Cut",
                                                       cutRange, 400.0f,
                                                       AudioParameterFloatAttributes().withLabel ("Hz")
                                                           .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::width, 1 }, "Width",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::curve, 1 }, "Curve",
                                                       NormalisableRange<float> (0.0f, 1.0f), 0.35f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")
                                                           .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    return layout;
}

bool TensionAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void TensionAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const auto numCh = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, numCh };

    hpf.prepare (spec);
    hpf.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpf.setCutoffFrequency (20.0f);
    hpf.setResonance (0.707f);

    noiseBpf.prepare (spec);
    noiseBpf.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
    noiseBpf.setResonance (1.1f);

    noiseHpf.prepare (spec);
    noiseHpf.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    noiseHpf.setCutoffFrequency (150.0f);

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    wetBuffer.setSize ((int) numCh, samplesPerBlock);
    noiseBuffer.setSize ((int) numCh, samplesPerBlock);

    smoothBuild.reset (sampleRate, 0.05);
    smoothBuild.setCurrentAndTargetValue (pBuild->load());
    smoothOut.reset (sampleRate, 0.05);
    smoothOut.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
}

float TensionAudioProcessor::shapeBuild (float raw, float curve) const noexcept
{
    // curve 0 -> exponent 4 (stays low, then shoots up), 0.5 -> linear, 1 -> exponent 0.25
    const float exponent = std::pow (4.0f, (0.5f - curve) * 2.0f);
    return std::pow (juce::jlimit (0.0f, 1.0f, raw), exponent);
}

void TensionAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numCh  = getTotalNumOutputChannels();
    const int numSmp = buffer.getNumSamples();

    for (int ch = numIn; ch < numCh; ++ch)
        buffer.clear (ch, 0, numSmp);

    if (wetBuffer.getNumSamples() < numSmp)
    {
        wetBuffer.setSize (numCh, numSmp, false, false, true);
        noiseBuffer.setSize (numCh, numSmp, false, false, true);
    }

    const float reverbAmt = pReverb->load();
    const float noiseAmt  = pNoise->load();
    const float noiseTop  = expMap (3000.0f, 16000.0f, pTone->load());
    const float lowCutMax = pLowCut->load();
    const float widthAmt  = pWidth->load();
    const float curve     = pCurve->load();

    smoothBuild.setTargetValue (pBuild->load());
    smoothOut.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    juce::Reverb::Parameters rp;
    rp.roomSize   = juce::jmap (pSize->load(), 0.3f, 0.98f);
    rp.damping    = 0.35f;
    rp.wetLevel   = 1.0f;
    rp.dryLevel   = 0.0f;
    rp.width      = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    for (int start = 0; start < numSmp; start += subBlockSize)
    {
        const int n = juce::jmin (subBlockSize, numSmp - start);

        const float s0 = shapeBuild (smoothBuild.getCurrentValue(), curve);
        smoothBuild.skip (n);
        const float s1 = shapeBuild (smoothBuild.getCurrentValue(), curve);

        // --- 1) Low cut that rises with the build (with a small resonant bump for tension)
        const float cutoff = expMap (20.0f, lowCutMax, s1);
        hpf.setCutoffFrequency (cutoff);
        hpf.setResonance (0.707f + 0.9f * s1);
        noiseBpf.setCutoffFrequency (expMap (180.0f, noiseTop, s1));

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* x = buffer.getWritePointer (ch, start);
            auto* nz = noiseBuffer.getWritePointer (ch, start);
            for (int i = 0; i < n; ++i)
            {
                x[i] = hpf.processSample (ch, x[i]);

                // --- 2) Noise riser: white noise -> HPF -> sweeping band-pass
                const float white = random.nextFloat() * 2.0f - 1.0f;
                nz[i] = noiseBpf.processSample (ch, noiseHpf.processSample (ch, white));
            }
        }

        // Reverb send = filtered signal + some noise (makes the wash grow with the riser)
        const float nG0 = noiseAmt * s0 * s0 * 0.6f, nG1 = noiseAmt * s1 * s1 * 0.6f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* w = wetBuffer.getWritePointer (ch, start);
            const auto* x = buffer.getReadPointer (ch, start);
            const auto* nz = noiseBuffer.getReadPointer (ch, start);
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) n;
                w[i] = x[i] + nz[i] * (nG0 + (nG1 - nG0) * t) * 0.5f;
            }
        }

        // --- 3) Reverb
        if (numCh >= 2)
            reverb.processStereo (wetBuffer.getWritePointer (0, start), wetBuffer.getWritePointer (1, start), n);
        else
            reverb.processMono (wetBuffer.getWritePointer (0, start), n);

        // --- 4) Mix: dry gets washed out while reverb comes up
        const float wet0 = reverbAmt * s0 * 0.35f, wet1 = reverbAmt * s1 * 0.35f;
        const float dry0 = 1.0f - 0.55f * reverbAmt * s0, dry1 = 1.0f - 0.55f * reverbAmt * s1;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* x = buffer.getWritePointer (ch, start);
            const auto* w = wetBuffer.getReadPointer (ch, start);
            const auto* nz = noiseBuffer.getReadPointer (ch, start);
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) n;
                const float dry = dry0 + (dry1 - dry0) * t;
                const float wet = wet0 + (wet1 - wet0) * t;
                const float ng  = nG0 + (nG1 - nG0) * t;
                x[i] = x[i] * dry + w[i] * wet + nz[i] * ng;
            }
        }

        // --- 5) Stereo widening (mid/side) + output
        const float side0 = 1.0f + widthAmt * s0, side1 = 1.0f + widthAmt * s1;
        if (numCh >= 2)
        {
            auto* l = buffer.getWritePointer (0, start);
            auto* r = buffer.getWritePointer (1, start);
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) n;
                const float sg = side0 + (side1 - side0) * t;
                const float m = 0.5f * (l[i] + r[i]);
                const float sd = 0.5f * (l[i] - r[i]) * sg;
                const float g = smoothOut.getNextValue();
                l[i] = softClip ((m + sd) * g);
                r[i] = softClip ((m - sd) * g);
            }
        }
        else
        {
            auto* x = buffer.getWritePointer (0, start);
            for (int i = 0; i < n; ++i)
                x[i] = softClip (x[i] * smoothOut.getNextValue());
        }

        displayBuild.store (s1);
        displayCutoff.store (cutoff);
    }
}

void TensionAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TensionAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* TensionAudioProcessor::createEditor()
{
    return new TensionAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TensionAudioProcessor();
}
