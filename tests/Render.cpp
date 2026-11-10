// Offline test: renders a synthetic 120 BPM loop through TENSION with the BUILD
// knob automated 0 -> 100 % over 4 bars, then a 2-bar drop with BUILD at 0.
// Also writes GUI screenshots. Usage: TensionRender <outDir>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr,
                                                                    (unsigned) b.getNumChannels(), 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

// RMS of content below ~150 Hz (one-pole low-pass) and total RMS for a range
static void measure (const juce::AudioBuffer<float>& b, int s0, int s1, double sr, float& low, float& total)
{
    const float a = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 150.0 / sr);
    float z = 0, sl = 0, st = 0;
    for (int i = s0; i < s1; ++i)
    {
        const float x = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
        z = x + a * (z - x);
        sl += z * z; st += x * x;
    }
    low = std::sqrt (sl / (float) (s1 - s0));
    total = std::sqrt (st / (float) (s1 - s0));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());

    const double sr = 48000.0;
    const int block = 512;
    const double beat = 0.5; // 120 BPM
    const int buildBeats = 16, dropBeats = 8;
    const int total = (int) (sr * beat * (buildBeats + dropBeats));
    const int buildEnd = (int) (sr * beat * buildBeats);

    // --- synthetic source: kick + offbeat bass + saw chord
    juce::AudioBuffer<float> src (2, total);
    src.clear();
    const double chord[] = { 220.0, 261.63, 329.63, 440.0 };
    for (int i = 0; i < total; ++i)
    {
        const double t = i / sr;
        const double tb = std::fmod (t, beat);
        const double kEnv = std::exp (-tb * 9.0);
        const double kFreq = 45.0 + 110.0 * std::exp (-tb * 30.0);
        static double kPh = 0; kPh += kFreq / sr;
        double s = 0.8 * kEnv * std::sin (2.0 * juce::MathConstants<double>::pi * kPh);

        const double off = std::fmod (t + beat * 0.5, beat);
        s += 0.35 * std::exp (-off * 6.0) * (std::fmod (t * 55.0, 1.0) * 2.0 - 1.0);

        double pad = 0;
        for (double f : chord) pad += std::fmod (t * f, 1.0) * 2.0 - 1.0;
        const double gate = std::fmod (t, beat * 0.5) < beat * 0.35 ? 1.0 : 0.3;
        src.setSample (0, i, (float) (s + 0.06 * pad * gate));
        src.setSample (1, i, (float) (s + 0.06 * pad * gate * 0.9));
    }

    TensionAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);
    auto* buildParam = proc.apvts.getParameter (ParamIDs::build);

    juce::AudioBuffer<float> out (src);
    juce::MidiBuffer midi;
    for (int start = 0; start < total; start += block)
    {
        const int n = juce::jmin (block, total - start);
        const float b = start < buildEnd ? (float) start / (float) buildEnd : 0.0f;
        buildParam->setValueNotifyingHost (b);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, start, n);
        proc.processBlock (view, midi);
    }

    // peak + NaN check
    float peak = 0; bool bad = false;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < total; ++i)
        {
            const float v = out.getSample (c, i);
            if (! std::isfinite (v)) bad = true;
            peak = juce::jmax (peak, std::abs (v));
        }

    const int bar = (int) (sr * beat * 4);
    float lowA, totA, lowB, totB, lowSrcB, totSrcB, lowD, totD;
    measure (out, 0, bar, sr, lowA, totA);                 // bar 1 (build ~0-25 %)
    measure (out, buildEnd - bar, buildEnd, sr, lowB, totB); // bar 4 (build 75-100 %)
    measure (src, buildEnd - bar, buildEnd, sr, lowSrcB, totSrcB);
    measure (out, buildEnd + bar / 8, buildEnd + bar, sr, lowD, totD); // drop

    std::printf ("finite=%s peak=%.3f\n", bad ? "NO" : "yes", peak);
    std::printf ("bar1  low=%.3f total=%.3f\n", lowA, totA);
    std::printf ("bar4  low=%.3f total=%.3f  (source bar4 low=%.3f)\n", lowB, totB, lowSrcB);
    std::printf ("drop  low=%.3f total=%.3f\n", lowD, totD);

    juce::AudioBuffer<float> both (2, total * 2);
    for (int c = 0; c < 2; ++c)
    {
        both.copyFrom (c, 0, src, c, 0, total);
        both.copyFrom (c, total, out, c, 0, total);
    }
    writeWav (outDir.getChildFile ("TENSION_demo.wav"), out, sr);
    writeWav (outDir.getChildFile ("TENSION_antes_y_despues.wav"), both, sr);

    // --- GUI screenshots at two build levels
    for (float b : { 0.0f, 0.8f })
    {
        buildParam->setValueNotifyingHost (b);
        juce::AudioBuffer<float> tmp (2, block); tmp.clear();
        for (int k = 0; k < 20; ++k) proc.processBlock (tmp, midi);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        for (int k = 0; k < 90; ++k)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::PNGImageFormat png;
        juce::FileOutputStream os (outDir.getChildFile ("TENSION_gui_" + juce::String (juce::roundToInt (b * 100)) + ".png"));
        os.setPosition (0); os.truncate();
        png.writeImageToStream (img, os);
    }
    return bad ? 1 : 0;
}
