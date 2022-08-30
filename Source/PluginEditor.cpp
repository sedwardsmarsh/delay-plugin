/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NewProjectAudioProcessorEditor::NewProjectAudioProcessorEditor (NewProjectAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // setup the main gain slider
    // we don't need to specify parameter limits (max and min) because those are specified in the PluginProcessor.cpp
    gainSlider.setSliderStyle (juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 100, 50);
    addAndMakeVisible (gainSlider); // make this slider a child component of the main component
    
    // instantiate the gain slider attachment unique pointer
    // audioProcessor.apvts -> accessing the apvts from the audioProcessor that created this class instance
    // "GAIN" -> parameter ID, specified in PluginProcessor.cpp
    // gainSlider -> slider object
    gainSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "GAIN", gainSlider);
    
    // setup the main gain label
    gainLabel.setText("main gain", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    gainLabel.attachToComponent(&gainSlider, false);
    addAndMakeVisible(gainLabel);
    
    // instantiate wet gain slider
    wetGainSlider.setSliderStyle (juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    wetGainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 100, 50);
    addAndMakeVisible (wetGainSlider);
    wetGainSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "WET_GAIN", wetGainSlider);
    
    // setup the wet gain label
    wetGainLabel.setText("wet gain", juce::dontSendNotification);
    wetGainLabel.setJustificationType(juce::Justification::centred);
    wetGainLabel.attachToComponent(&wetGainSlider, false);
    addAndMakeVisible(wetGainLabel);
    
    // setup the buffer clear button
    clearBufferButton.setButtonText("clear buffer");
    clearBufferButton.onClick = [this]() { audioProcessor.clearBufferFlag = true; };
    addAndMakeVisible(clearBufferButton);
    
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
}

NewProjectAudioProcessorEditor::~NewProjectAudioProcessorEditor()
{
}

//==============================================================================
void NewProjectAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

//    g.setColour (juce::Colours::white);
//    g.setFont (15.0f);
//    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void NewProjectAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    gainSlider.setBounds (getWidth() * 3/4 - 100, getHeight()/2 - 50, 200, 100);
    wetGainSlider.setBounds (getWidth() * 1/4 - 100, getHeight()/2 - 75, 200, 100);
    clearBufferButton.setBounds(getWidth() * 1/4 - 50, getHeight()/2 + 25, 100, 100);
}
