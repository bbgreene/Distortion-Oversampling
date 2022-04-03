/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
DistortionOversamplingAudioProcessor::DistortionOversamplingAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), treeState(*this, nullptr, "PARAMETERS", createParameterLayout()), oversamplingModule(2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR)
#endif
{
    treeState.addParameterListener("oversample", this);
    treeState.addParameterListener("input", this);
}

DistortionOversamplingAudioProcessor::~DistortionOversamplingAudioProcessor()
{
    treeState.removeParameterListener("oversample", this);
    treeState.removeParameterListener("input", this);
}

juce::AudioProcessorValueTreeState::ParameterLayout DistortionOversamplingAudioProcessor::createParameterLayout()
{
    std::vector <std::unique_ptr<juce::RangedAudioParameter>> params;
    
    //make sure to update number of reservations after adding params
    auto pOSToggle = std::make_unique<juce::AudioParameterBool>("oversample", "Oversample", false);
    auto pInput = std::make_unique<juce::AudioParameterFloat>("input", "Input", 0.0, 24.0, 0.0);
    
    params.push_back(std::move(pOSToggle));
    params.push_back(std::move(pInput));
    
    return { params.begin(), params.end() };
}

void DistortionOversamplingAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    if (parameterID == "oversample")
    {
        osToggle = newValue;
        std::cout << osToggle << std::endl;
    }
    if (parameterID == "input")
    {
        dBInput = newValue;
        rawInput = juce::Decibels::decibelsToGain(newValue);
        DBG(rawInput);
    }
}

//==============================================================================
const juce::String DistortionOversamplingAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DistortionOversamplingAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DistortionOversamplingAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DistortionOversamplingAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DistortionOversamplingAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DistortionOversamplingAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DistortionOversamplingAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DistortionOversamplingAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String DistortionOversamplingAudioProcessor::getProgramName (int index)
{
    return {};
}

void DistortionOversamplingAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void DistortionOversamplingAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumInputChannels();
    
    osToggle = treeState.getRawParameterValue("oversample")->load();
    rawInput = juce::Decibels::decibelsToGain(static_cast<float>(*treeState.getRawParameterValue("input")));
    oversamplingModule.initProcessing(samplesPerBlock);
}

void DistortionOversamplingAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DistortionOversamplingAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void DistortionOversamplingAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::AudioBlock<float> upSampledBlock (buffer);
    
    // If statement of oversampling
    if(osToggle)
    {
        // if on, increase sample rate
        upSampledBlock = oversamplingModule.processSamplesUp(block);
        
        // soft clip distortion
        for (int sample = 0; sample < upSampledBlock.getNumSamples(); ++sample)
        {
            for (int ch = 0; ch < upSampledBlock.getNumChannels(); ++ch)
            {
                float* data = upSampledBlock.getChannelPointer(ch);
            
                data[sample] = halfWaveData(data[sample]);
            }
        }
        //decrease sample rate
        oversamplingModule.processSamplesDown(block);
    }
    
    // if oversampling is off
    else
    {
        // soft clip distortion
        for (int sample = 0; sample < block.getNumSamples(); ++sample)
        {
            for (int ch = 0; ch < block.getNumChannels(); ++ch)
            {
                float* data = block.getChannelPointer(ch);
            
                data[sample] = halfWaveData(data[sample]);
            }
        }
    }
        
}
// softclip algorithim (rounded)
float DistortionOversamplingAudioProcessor::softClipData(float samples)
{
    samples *= rawInput;
    
    return piDivisor * std::atan(samples );
}

// hardclip algorithim (any sample above 1 or -1 will be squared)
float DistortionOversamplingAudioProcessor::hardClipData(float samples)
{
    samples *= rawInput;
    
    // will equate to true if the values is 1 or -1, because using absolute value
    if (std::abs(samples) > 1.0)
    {
        // if true then this will output 1 (or -1)
        samples *= 1.0 / std::abs(samples);
    }
    
    return samples;
}

// halfwave rectification algorithim (all negative values assigned to zero)
float DistortionOversamplingAudioProcessor::halfWaveData(float samples)
{
    samples *= rawInput;
    samples += 0.25;
    
    // values below zero are softclipped
    if (samples < 0.0)
    {
        samples = softClipData(samples);
    }
    // values above zero are hardclipped
    else
    {
        samples = hardClipData(samples);
    }
    
    return samples - 0.25;
}

//==============================================================================
bool DistortionOversamplingAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DistortionOversamplingAudioProcessor::createEditor()
{
//    return new DistortionOversamplingAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void DistortionOversamplingAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save params
    juce::MemoryOutputStream stream(destData, false);
    treeState.state.writeToStream(stream);
}

void DistortionOversamplingAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Recall params
    auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));
    
    if(tree.isValid())
    {
        treeState.state = tree;
        osToggle = *treeState.getRawParameterValue("oversample");
        rawInput = juce::Decibels::decibelsToGain(static_cast<float>(*treeState.getRawParameterValue("input")));
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DistortionOversamplingAudioProcessor();
}
