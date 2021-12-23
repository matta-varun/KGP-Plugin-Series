/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

enum Slope {
    slope12,
    slope24,
    slope36,
    slope48
};

struct ChainSettings {
    float peakFreq{ 0 }, peakGainInDB{ 0 }, peakQuality{ 1.f };
    float lowCutFrequency{ 0 }, highCutFrequency{ 0 };
    Slope lowCutSlope{ Slope::slope12 }, highCutSlope{ Slope::slope12 };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class KGP_EQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    KGP_EQAudioProcessor();
    ~KGP_EQAudioProcessor() override;

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

    static juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();
    
    juce::AudioProcessorValueTreeState apvts{
        *this, nullptr, "Parameters", createParameterLayout()
    };

private:

    using Filter = juce::dsp::IIR::Filter<float>;

    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;

    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

    MonoChain lChain, rChain;

    enum ChainPositions{
        LowCut,
        Peak,
        HighCut
    };

    using Coefficients = Filter::CoefficientsPtr;

    void updatePeakFilter(const ChainSettings& chainSettings);
    static void updateCoefficients(Coefficients& old, Coefficients& new_);

    template<int index, typename ChainType, typename CoefficientType>
    void update(ChainType& chain, const CoefficientType& coefficients)
    {
        updateCoefficients(chain.template get<index>().coefficients, coefficients[index]);
        chain.template setBypassed<index>(false);
    }

    template<typename ChainType, typename CoefficientType>
    void UpdateCutFilter(ChainType& leftLowCut, const CoefficientType& cutCoefficients, const Slope& lowCutSlope) 
    {
        leftLowCut.template setBypassed<0>(true);
        leftLowCut.template setBypassed<1>(true);
        leftLowCut.template setBypassed<2>(true);
        leftLowCut.template setBypassed<3>(true);

        switch (lowCutSlope) {
        case slope48:
            update<3>(leftLowCut, cutCoefficients);
        case slope36:
            update<2>(leftLowCut, cutCoefficients);
        case slope24:
            update<1>(leftLowCut, cutCoefficients);
        case slope12:
            update<0>(leftLowCut, cutCoefficients);
        }
    }
    void updateLowCutFilter(const ChainSettings& chainSettings);
    void updateHighCutFilter(const ChainSettings& chainSettings);
    void updateFilters();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KGP_EQAudioProcessor)
};
