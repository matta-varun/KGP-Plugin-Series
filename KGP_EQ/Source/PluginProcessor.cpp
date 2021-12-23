/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KGP_EQAudioProcessor::KGP_EQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

KGP_EQAudioProcessor::~KGP_EQAudioProcessor()
{
}

//==============================================================================
const juce::String KGP_EQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool KGP_EQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool KGP_EQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool KGP_EQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double KGP_EQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int KGP_EQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int KGP_EQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void KGP_EQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String KGP_EQAudioProcessor::getProgramName (int index)
{
    return {};
}

void KGP_EQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void KGP_EQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    juce::dsp::ProcessSpec spec;

    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;

    lChain.prepare(spec);
    rChain.prepare(spec);

    updateFilters();
}

void KGP_EQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool KGP_EQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void KGP_EQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateFilters();

    juce::dsp::AudioBlock<float> block(buffer);

    auto lBlock = block.getSingleChannelBlock(0);
    auto rBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(lBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rBlock);

    lChain.process(leftContext);
    rChain.process(rightContext);
}

//==============================================================================
bool KGP_EQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* KGP_EQAudioProcessor::createEditor()
{
//    return new KGP_EQAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void KGP_EQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void KGP_EQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings settings;

    settings.lowCutFrequency = apvts.getRawParameterValue("Low-Cut Frequency")->load();
    settings.highCutFrequency = apvts.getRawParameterValue("High-Cut Frequency")->load();
    settings.lowCutSlope = static_cast<Slope>( apvts.getRawParameterValue("Low-Cut Slope")->load() );
    settings.highCutSlope = static_cast<Slope>( apvts.getRawParameterValue("High-Cut Slope")->load() );
    settings.peakFreq = apvts.getRawParameterValue("Peak Frequency")->load();
    settings.peakGainInDB = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();

    return settings;
}

void KGP_EQAudioProcessor::updatePeakFilter(const ChainSettings& chainSettings) {
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), chainSettings.peakFreq, chainSettings.peakQuality, juce::Decibels::decibelsToGain(chainSettings.peakGainInDB));

    updateCoefficients(lChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void KGP_EQAudioProcessor::updateCoefficients(Coefficients& old, Coefficients& new_) {
    *old = *new_;
}

void KGP_EQAudioProcessor::updateLowCutFilter(const ChainSettings& chainSettings) {
    auto lowCutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFrequency, getSampleRate(), 2 * (chainSettings.lowCutSlope + 1));
    
    auto& leftLowCut = lChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rChain.get<ChainPositions::LowCut>();

    UpdateCutFilter(leftLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
    UpdateCutFilter(rightLowCut, lowCutCoefficients, chainSettings.lowCutSlope);
}

void KGP_EQAudioProcessor::updateHighCutFilter(const ChainSettings& chainSettings) {
    auto highCutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFrequency, getSampleRate(), 2 * (chainSettings.highCutSlope + 1));

    auto& leftHighCut = lChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rChain.get<ChainPositions::HighCut>();

    UpdateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
    UpdateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);
}

void KGP_EQAudioProcessor::updateFilters() {
    auto chainSettings = getChainSettings(apvts);

    updateLowCutFilter(chainSettings);
    updatePeakFilter(chainSettings);
    updateHighCutFilter(chainSettings);
}

juce::AudioProcessorValueTreeState::ParameterLayout
KGP_EQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>("Low-Cut Frequency",
        "Low-Cut Frequency", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.2f),
        20.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("High-Cut Frequency",
        "High-Cut Frequency", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.2f),
        20000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Frequency",
        "Peak Frequency", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.2f),
        750.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain",
        "Peak Gain", juce::NormalisableRange<float>(-30.f, 30.f, 0.5f, 1.f),
        0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Quality",
        "Peak Quality", juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f),
        1.f));

    juce::StringArray strArray;
    for (int i = 0; i < 4; i++) {
        juce::String str;
        str << (12 + i * 12);
        str << " dB/Oct";
        strArray.add(str);
    }

    layout.add(std::make_unique<juce::AudioParameterChoice>("Low-Cut Slope", "Low-Cut Slope", strArray, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("High-Cut Slope", "High-Cut Slope", strArray, 0));

        return layout;
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KGP_EQAudioProcessor();
}
