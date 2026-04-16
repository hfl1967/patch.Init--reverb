// reverb.cpp
// Stereo reverb for Electrosmith patch.Init()
// Uses ReverbSc from DaisySP-LGPL
//
// CONTROLS:
//   Knob 1 + CV 5: Reverb time
//   Knob 2 + CV 6: Tone (CCW = dark, CW = bright)
//   Knob 3 + CV 7: Wet/dry mix
//   Knob 4 + CV 8: Pre-delay (0-100ms)

#include "daisy_patch_sm.h"
#include "daisysp.h"
#include "../DaisySP/DaisySP-LGPL/Source/Effects/reverbsc.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

DaisyPatchSM hw;
ReverbSc     reverb;

static const int kPreDelayMaxSamples = 4800; // 100ms at 48kHz
float preDelayBufL[kPreDelayMaxSamples];
float preDelayBufR[kPreDelayMaxSamples];
int   preDelayWrite = 0;

float toneFilterStateL = 0.0f;
float toneFilterStateR = 0.0f;

volatile float ledVoltage = 0.0f;

float ReadKnob(int adcIndex)
{
    return fclamp(hw.GetAdcValue(adcIndex), 0.0f, 1.0f);
}

float ReadCV(int cvIndex)
{
    return fclamp(hw.GetAdcValue(cvIndex), 0.0f, 1.0f);
}

float ReadKnobWithCV(int knobAdc, int cvAdc)
{
    float knob = ReadKnob(knobAdc);
    float cv   = ReadCV(cvAdc) - 0.5f;
    return fclamp(knob + cv, 0.0f, 1.0f);
}

// Reads CV_5-8 jacks as bipolar offset (-1.0 to +1.0)
// Returns 0.0 when nothing is patched (signal below deadband threshold)
float ReadCVJack(int cvIndex)
{
    float raw = hw.GetAdcValue(cvIndex);
    if(raw < 0.05f)
        return 0.0f;
    return (raw - 0.5f) * 2.0f;
}

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    hw.ProcessAllControls();

    // Knob 1 + CV_5: Reverb time (power curve for musical taper)
    float knob1    = fclamp(ReadKnobWithCV(ADC_9, CV_1) + ReadCVJack(CV_5), 0.0f, 1.0f);
    float feedback = 0.3f + powf(knob1, 0.3f) * 0.699f;

    // Knob 2 + CV_6: Tone (one-pole LP on wet signal)
    float knob2     = fclamp(ReadKnobWithCV(ADC_10, CV_2) + ReadCVJack(CV_6), 0.0f, 1.0f);
    float toneCoeff = 0.9799f - powf(knob2, 2.0f) * 0.9599f;

    // Knob 3 + CV_7: Wet/dry mix
    // CV_3 floating value compensated via raw ADC read
    // kWetOnset sets the point where reverb begins blending in
    float knob3raw  = ReadKnob(ADC_11);
    float cv3       = hw.GetAdcValue(CV_3);
    float cv7       = ReadCVJack(CV_7);
    float raw       = fclamp(knob3raw + cv3 + cv7, 0.0f, 1.0f);
    static const float kWetOnset = 0.01f;
    float wetPos    = fmaxf(0.0f, (raw - kWetOnset) / (1.0f - kWetOnset));
    wetPos          = powf(wetPos, 3.2f);
    float dryMix    = cosf(wetPos * (float(M_PI) / 2.0f));
    float wetMix    = sinf(wetPos * (float(M_PI) / 2.0f));

    // Knob 4 + CV_8: Pre-delay (0-100ms)
    float knob4           = fclamp(ReadKnobWithCV(ADC_12, CV_4) + ReadCVJack(CV_8), 0.0f, 1.0f);
    int   preDelaySamples = (int)(knob4 * kPreDelayMaxSamples);

    reverb.SetFeedback(feedback);
    reverb.SetLpFreq(18000.f);

    static float rightEnvelope = 0.0f;
    static float ledBrightness = 0.0f;
    float peakLevel = 0.0f;

    for(size_t i = 0; i < size; i++)
    {
        // Envelope follower for mono detection — avoids zero-crossing artifacts
        rightEnvelope = 0.9999f * rightEnvelope + 0.0001f * fabsf(in[1][i]);

        float inL = in[0][i];
        float inR = (rightEnvelope < 0.001f) ? inL : in[1][i];

        float blockPeak = fmaxf(fabsf(inL), fabsf(inR));
        if(blockPeak > peakLevel)
            peakLevel = blockPeak;

        preDelayBufL[preDelayWrite] = inL;
        preDelayBufR[preDelayWrite] = inR;

        int readPos    = (preDelayWrite - preDelaySamples + kPreDelayMaxSamples)
                         % kPreDelayMaxSamples;
        float delayedL = preDelayBufL[readPos];
        float delayedR = preDelayBufR[readPos];

        preDelayWrite = (preDelayWrite + 1) % kPreDelayMaxSamples;

        // Sum to mono before reverb — ReverbSc designed for mono input
        float monoIn = (delayedL + delayedR) * 0.5f;
        float sendL, sendR;
        reverb.Process(monoIn, monoIn, &sendL, &sendR);

        // One-pole LP tone filter on wet signal
        toneFilterStateL = toneCoeff * toneFilterStateL + (1.f - toneCoeff) * sendL;
        toneFilterStateR = toneCoeff * toneFilterStateR + (1.f - toneCoeff) * sendR;

        out[0][i] = (inL * dryMix) + (toneFilterStateL * wetMix);
        out[1][i] = (inR * dryMix) + (toneFilterStateR * wetMix);
    }

    // LED: instant attack, ~400ms decay, scaled to 0-5V for CV_OUT_2
    if(peakLevel > ledBrightness)
        ledBrightness = peakLevel;
    ledBrightness *= 0.9997f;
    ledVoltage = ledBrightness * 5.0f;
}

int main(void)
{
    hw.Init();

    for(int i = 0; i < kPreDelayMaxSamples; i++)
    {
        preDelayBufL[i] = 0.0f;
        preDelayBufR[i] = 0.0f;
    }

    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(4);

    float sampleRate = hw.AudioSampleRate();
    reverb.Init(sampleRate);

    reverb.SetFeedback(0.85f);
    reverb.SetLpFreq(18000.f);

    hw.StartAudio(AudioCallback);

    while(true)
    {
        hw.WriteCvOut(CV_OUT_2, ledVoltage);
        System::Delay(1);
    }
}