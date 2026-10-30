#include "PluginProcessor.h"
#include "PluginEditor.h"

static constexpr float lookaheadOnMs = 3.0f;

PlosivoreProcessor::PlosivoreProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PlosivoreState", createLayout())
{
    pSens   = apvts.getRawParameterValue (ParamID::sensitivity);
    pFreq   = apvts.getRawParameterValue (ParamID::frequency);
    pDepth  = apvts.getRawParameterValue (ParamID::depth);
    pRel    = apvts.getRawParameterValue (ParamID::release);
    pLook   = apvts.getRawParameterValue (ParamID::lookahead);
    pListen = apvts.getRawParameterValue (ParamID::listen);
    pOut    = apvts.getRawParameterValue (ParamID::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout PlosivoreProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = AudioParameterFloatAttributes().withLabel ("%")
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto hz  = AudioParameterFloatAttributes().withLabel ("Hz")
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; });
    auto dB  = AudioParameterFloatAttributes().withLabel ("dB")
                   .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });
    auto ms  = AudioParameterFloatAttributes().withLabel ("ms")
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; });

    NormalisableRange<float> freqRange (60.0f, 300.0f);  freqRange.setSkewForCentre (140.0f);
    NormalisableRange<float> relRange  (5.0f, 250.0f);   relRange.setSkewForCentre (50.0f);

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::sensitivity, 1 }, "Sensitivity",
                                                         NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f, pct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::frequency, 1 }, "Frequency",
                                                         freqRange, 150.0f, hz));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::depth, 1 }, "Depth",
                                                         NormalisableRange<float> (0.0f, 40.0f, 0.1f), 24.0f, dB));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::release, 1 }, "Release",
                                                         relRange, 40.0f, ms));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { ParamID::lookahead, 1 }, "Lookahead", false));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { ParamID::listen, 1 }, "Listen to removed", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 }, "Output",
                                                         NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dB));
    return { p.begin(), p.end() };
}

plosivore::Params PlosivoreProcessor::readParams() const
{
    plosivore::Params p;
    p.sensitivity = pSens->load();
    p.frequency   = pFreq->load();
    p.depthDb     = pDepth->load();
    p.releaseMs   = pRel->load();
    p.lookaheadMs = pLook->load() > 0.5f ? lookaheadOnMs : 0.0f;
    p.listen      = pListen->load() > 0.5f;
    p.outputDb    = pOut->load();
    return p;
}

void PlosivoreProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, juce::jmax (1, getTotalNumInputChannels()));
    engine.setParams (readParams());
    engine.snapCrossover();
    visFrameRate = engine.getFrameRateHz();
    lastLatency = engine.getLatencySamples();
    setLatencySamples (lastLatency);
    popArmed = true;
}

bool PlosivoreProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PlosivoreProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    for (int c = numIn; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    engine.setParams (readParams());
    const int latency = engine.getLatencySamples();
    if (latency != lastLatency)
    {
        lastLatency = latency;
        setLatencySamples (latency);   // host re-aligns tracks (plugin delay compensation)
    }

    if (resetRequested.exchange (false))
        popCount = 0;

    engine.process (buffer.getArrayOfWritePointers(), numIn, buffer.getNumSamples(),
        [this] (const plosivore::Frame& f)
        {
            VisFrame v { f.inPeak, f.outPeak, f.remPeak, f.grDb, false };

            // Count a pop when reduction passes 6 dB; re-arm once it recovers past 2 dB.
            if (popArmed && f.grDb < -6.0f) { popArmed = false; v.popStart = true; popCount.fetch_add (1); }
            else if (! popArmed && f.grDb > -2.0f) popArmed = true;

            const auto scope = visFifo.write (1);   // drops the frame if the UI is not reading
            if (scope.blockSize1 > 0) visData[(size_t) scope.startIndex1] = v;
        });
}

int PlosivoreProcessor::readVisFrames (VisFrame* dest, int maxFrames)
{
    const auto scope = visFifo.read (juce::jmin (maxFrames, visFifo.getNumReady()));
    for (int i = 0; i < scope.blockSize1; ++i) dest[i] = visData[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[scope.blockSize1 + i] = visData[(size_t) (scope.startIndex2 + i)];
    return scope.blockSize1 + scope.blockSize2;
}

void PlosivoreProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PlosivoreProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PlosivoreProcessor::createEditor() { return new PlosivoreEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PlosivoreProcessor(); }
