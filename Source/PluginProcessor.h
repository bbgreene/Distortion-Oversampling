/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class DistortionOversamplingAudioProcessor  : public juce::AudioProcessor, juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    DistortionOversamplingAudioProcessor();
    ~DistortionOversamplingAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    juce::AudioProcessorValueTreeState treeState;

private:
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    
    // oversampling bool initialisation
    bool osToggle {false};
    
    // oversampling dsp module initialisation
    juce::dsp::Oversampling<float> oversamplingModule;
    
    //variables
    float dBInput {0.0};
    float rawInput {1.0};
    float bias {0.0};
    // distortion models enum selection
    enum class DisModels
    {
        kSoft,
        kHard,
        kTube,
        kHalfWave,
        kFullWave
    };
    
    DisModels disModel = DisModels::kSoft;
    
    //distortion
    float softClipData(float samples);
    float hardClipData(float samples);
    float tubeData(float samples);
    float halfWaveData(float samples);
    float fullWaveData(float samples);
    
    // softclip divisor. Creating this constexpr is more efficient than doing 2/pi for every sample in the audio block, because calculated at initialisation
    static constexpr float piDivisor = 2.0 / juce::MathConstants<float>::pi;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionOversamplingAudioProcessor)
};
