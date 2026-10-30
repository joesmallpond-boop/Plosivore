#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "PlosiveEngine.h"

// One column of the scope: sent from the audio thread to the UI through a lock-free FIFO.
struct VisFrame
{
    float inPeak = 0, outPeak = 0, remPeak = 0, grDb = 0;
    bool  popStart = false;
};

namespace ParamID
{
    inline constexpr const char* sensitivity = "sensitivity";
    inline constexpr const char* frequency   = "frequency";
    inline constexpr const char* depth       = "depth";
    inline constexpr const char* release     = "release";
    inline constexpr const char* lookahead   = "lookahead";
    inline constexpr const char* listen      = "listen";
    inline constexpr const char* output      = "output";
}

class PlosivoreProcessor : public juce::AudioProcessor
{
public:
    PlosivoreProcessor();
    ~PlosivoreProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // ---- UI feed (single producer: audio thread, single consumer: message thread)
    static constexpr int fifoSize = 8192;
    juce::AbstractFifo visFifo { fifoSize };
    std::array<VisFrame, fifoSize> visData;
    int readVisFrames (VisFrame* dest, int maxFrames);
    double getVisFrameRate() const { return visFrameRate.load(); }

    std::atomic<int> popCount { 0 };
    std::atomic<bool> resetRequested { false };

private:
    plosivore::Engine engine;
    std::atomic<double> visFrameRate { 750.0 };
    bool popArmed = true;
    int lastLatency = -1;

    std::atomic<float>* pSens = nullptr; std::atomic<float>* pFreq = nullptr;
    std::atomic<float>* pDepth = nullptr; std::atomic<float>* pRel = nullptr;
    std::atomic<float>* pLook = nullptr; std::atomic<float>* pListen = nullptr;
    std::atomic<float>* pOut = nullptr;

    plosivore::Params readParams() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlosivoreProcessor)
};
