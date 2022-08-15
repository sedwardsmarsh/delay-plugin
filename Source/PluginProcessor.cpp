/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NewProjectAudioProcessor::NewProjectAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       // *this -> audio processor we want to connect to, just use the current instance.
                       // nullptr -> we're not supplying an undo manager.
                       // "Parameters" -> name of the value tree.
                       // createParamters() -> returns our parameterLayout object.
                       ), apvts (*this, nullptr, "Parameters", createParameters())
#endif
{
}

NewProjectAudioProcessor::~NewProjectAudioProcessor()
{
}

//==============================================================================
const juce::String NewProjectAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NewProjectAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool NewProjectAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool NewProjectAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double NewProjectAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int NewProjectAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int NewProjectAudioProcessor::getCurrentProgram()
{
    return 0;
}

void NewProjectAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String NewProjectAudioProcessor::getProgramName (int index)
{
    return {};
}

void NewProjectAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void NewProjectAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
//    std::cout << "Sample Rate = " << getSampleRate() << std::endl << "times two = " << getSampleRate() * 2 << std::endl;
    auto delayBufferSize = sampleRate * 2.0; // 2 seconds of audio @ 44.1kHz is 88200 samples
    delayBuffer.setSize(getTotalNumOutputChannels(), (int)delayBufferSize);
}

void NewProjectAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NewProjectAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void NewProjectAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // TODO: modularize getting parameter values into a function that returns a dictionary of values or something
    
    // get the value from the main gain slider
    auto mainGainPtr = apvts.getRawParameterValue ("GAIN"); // returns a std::atomic<float>* wtf is that?
    // need to dereference and call load() to access the value
    auto mainGain = mainGainPtr->load();
    
    // wet gain
    auto wetGainPtr = apvts.getRawParameterValue ("WET_GAIN");
    auto wetGain = wetGainPtr->load();

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        fillDelayBuffer (buffer, channel);
        readDelayBuffer (buffer, delayBuffer, channel, wetGain);
        fillDelayBuffer (buffer, channel);
        
        buffer.applyGain(mainGain);
    }
    
    updateBufferPositions (buffer, delayBuffer);
}

void NewProjectAudioProcessor::fillDelayBuffer (juce::AudioBuffer<float>& buffer, int channel)
{
    auto bufferSize = buffer.getNumSamples();
    auto delayBufferSize = delayBuffer.getNumSamples();
    
    // Check to see if main buffer copies to delay buffer without needing to wrap...
    if (delayBufferSize > bufferSize + writePosition) {
        
        // copy main buffer samples to delay buffer
        delayBuffer.copyFrom (channel, writePosition, buffer.getWritePointer (channel), bufferSize);
    }
    
    // Needs to wrap
    else
    {
        // Determine how much space is left at the end of the delay buffer
        auto numSamplesToEnd = delayBufferSize - writePosition;
        // Copy that amount of samples to the end
        delayBuffer.copyFrom (channel, writePosition, buffer.getWritePointer (channel), numSamplesToEnd);
        
        // calculate how many samples are remaining to copy
        auto numSamplesAtStart = bufferSize - numSamplesToEnd;
        // copy remaining amount to beginning of delay buffer, from the start
        delayBuffer.copyFrom (channel, 0, buffer.getWritePointer (channel, numSamplesToEnd), numSamplesAtStart);
    }
}

void NewProjectAudioProcessor::readDelayBuffer (juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer, int channel, float wetGain)
{
    auto bufferSize = buffer.getNumSamples();
    auto delayBufferSize = delayBuffer.getNumSamples();
    
    // 1 second of audio from the past (in the delay buffer)
    auto readPosition = writePosition - getSampleRate();
    
    // what if readPosition is negative? Should we increment until becoming positive?
    if (readPosition < 0)
        readPosition += delayBufferSize;

    if (readPosition + bufferSize < delayBufferSize)
    {
        // add bufferSize number of samples starting at readPosition from the delayBuffer to the main buffer
        buffer.addFrom (channel, 0, delayBuffer.getReadPointer (channel, readPosition), bufferSize, wetGain);
    }
    else
    {
        int numSamplesToEnd = delayBufferSize - readPosition;
        buffer.addFrom (channel, 0, delayBuffer.getReadPointer (channel, readPosition), numSamplesToEnd, wetGain);

        int numSamplesAtStart = bufferSize - numSamplesToEnd;
        buffer.addFrom (channel, numSamplesToEnd, delayBuffer.getReadPointer (channel, 0), numSamplesAtStart, wetGain);
    }
}

void NewProjectAudioProcessor::updateBufferPositions (juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer)
{
    auto bufferSize = buffer.getNumSamples();
    auto delayBufferSize = delayBuffer.getNumSamples();

    writePosition += bufferSize;
    writePosition %= delayBufferSize;
}

//==============================================================================
bool NewProjectAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* NewProjectAudioProcessor::createEditor()
{
    return new NewProjectAudioProcessorEditor (*this);
}

//==============================================================================
void NewProjectAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void NewProjectAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NewProjectAudioProcessor();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout NewProjectAudioProcessor::createParameters()
{
    // vector that contains parameter layout information
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Allocate memory for the unique pointers of each parameter.
    // We're allocating pointers of type juce::AudioParameterFloat because AudioParameterFloat inherits from type juce::RangedAudioParameter
    // TODO: Set parameter version hints: only for Logic and Garageband
    
    // main gain
    auto gainParameterID = juce::ParameterID { "GAIN", 1 };
    params.push_back (std::make_unique<juce::AudioParameterFloat>(gainParameterID, "Gain", 0.0f, 1.0f, 1.0f));
    
    // wet gain
    auto wetGainParameterID = juce::ParameterID { "WET_GAIN", 1 };
    params.push_back (std::make_unique<juce::AudioParameterFloat>(wetGainParameterID, "Wet_Gain", 0.0f, 1.0f, 0.5f));
    
    // the return type is a vector
    return { params.begin(), params.end() };
}
