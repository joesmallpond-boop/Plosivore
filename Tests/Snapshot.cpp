// Dev tool: plays a raw stereo float32 file through the real processor at real-time pace
// while the editor is live, then writes a PNG of the editor.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: PlosivoreSnapshot in.f32 out.png [startSec] [lookahead 0|1] [scale]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI gui;
    const double fs = 48000.0; const int block = 768; // 16 ms
    const double startSec = argc > 3 ? std::atof (argv[3]) : 0.0;
    const float scale = argc > 5 ? (float) std::atof (argv[5]) : 1.0f;

    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    juce::MemoryBlock mb; f.loadFileAsData (mb);
    const int n = (int) (mb.getSize() / 8); auto* il = (const float*) mb.getData();

    PlosivoreProcessor proc;
    if (argc > 4) proc.apvts.getParameter (ParamID::lookahead)->setValueNotifyingHost (std::atoi (argv[4]) ? 1.0f : 0.0f);
    proc.setPlayConfigDetails (2, 2, fs, block);
    proc.prepareToPlay (fs, block);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (PlosivoreEditor::baseW * scale), (int) (PlosivoreEditor::baseH * scale));

    juce::AudioBuffer<float> buf (2, block); juce::MidiBuffer midi;
    const int startSample = (int) (startSec * fs);
    for (int pos = 0; pos + block <= n; pos += block)
    {
        for (int i = 0; i < block; ++i) { buf.setSample (0, i, il[2 * (pos + i)]); buf.setSample (1, i, il[2 * (pos + i) + 1]); }
        proc.processBlock (buf, midi);
        if (pos >= startSample) juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
        else if ((pos / block) % 8 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    out.deleteFile();
    juce::FileOutputStream os (out); juce::PNGImageFormat png; png.writeImageToStream (img, os);
    std::printf ("wrote %s, pops counted: %d, latency %d\n", argv[2], proc.popCount.load(), proc.getLatencySamples());
    ed.reset();
    return 0;
}
