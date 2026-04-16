// reverb.cpp
// Stereo reverb for Electrosmith patch.Init()
// Uses ReverbSc from DaisySP-LGPL

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
    float cv   = ReadCV(cvAdc) - 0.5f; // center CV so 0V = no offset
    return fclamp(knob + cv, 0.0f, 1.0f);
}

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    hw.ProcessAllControls();

    // Knob 1: Reverb time (feedback 0.3–0.999, power curve)
    float knob1    = ReadKnobWithCV(ADC_9, CV_1);
    float feedback = 0.3f + powf(knob1, 0.3f) * 0.699f;

    // Knob 2: Tone (one-pole LP on wet signal, squared curve)
    float knob2     = ReadKnobWithCV(ADC_10, CV_2);
    float toneCoeff = 0.9799f - powf(knob2, 2.0f) * 0.9599f;

    // Knob 3: Custom read for knob 3 — CV offset tuned to compensate for floating jack
    float knob3raw = ReadKnob(ADC_11);
    float cv3 = hw.GetAdcValue(CV_3);
    float knob3 = fclamp(knob3raw + cv3 - 0.1f, 0.0f, 1.0f);
    float wetPos = powf(knob3, 2.0f);
    float dryMix = cosf(wetPos * (float(M_PI) / 2.0f));
    float wetMix = sinf(wetPos * (float(M_PI) / 2.0f));

    // Knob 4: Pre-delay (0–100ms)
    float knob4           = ReadKnobWithCV(ADC_12, CV_4);
    int   preDelaySamples = (int)(knob4 * kPreDelayMaxSamples); 

    reverb.SetFeedback(feedback);
    reverb.SetLpFreq(18000.f);

    // Envelope follower for mono detection — avoids zero-crossing artifacts
    // that occur with per-sample amplitude checks on live audio signals
    static float rightEnvelope = 0.0f;
    static float ledBrightness = 0.0f;

    float peakLevel = 0.0f;

    for(size_t i = 0; i < size; i++)
    {
        rightEnvelope = 0.9999f * rightEnvelope + 0.0001f * fabsf(in[1][i]);

        float inL = in[0][i];
        float inR = (rightEnvelope < 0.001f) ? inL : in[1][i];

        float blockPeak = fmaxf(fabsf(inL), fabsf(inR));
        if(blockPeak > peakLevel)
            peakLevel = blockPeak;

        preDelayBufL[preDelayWrite] = inL;
        preDelayBufR[preDelayWrite] = inR;

        int readPos = (preDelayWrite - preDelaySamples + kPreDelayMaxSamples)
                      % kPreDelayMaxSamples;
        float delayedL = preDelayBufL[readPos];
        float delayedR = preDelayBufR[readPos];

        preDelayWrite = (preDelayWrite + 1) % kPreDelayMaxSamples;

        // ReverbSc takes mono input — sum stereo to avoid cross-channel artifacts
        float monoIn = (delayedL + delayedR) * 0.5f;
        float sendL, sendR;
        reverb.Process(monoIn, monoIn, &sendL, &sendR);

        toneFilterStateL = toneCoeff * toneFilterStateL + (1.f - toneCoeff) * sendL;
        toneFilterStateR = toneCoeff * toneFilterStateR + (1.f - toneCoeff) * sendR;

        out[0][i] = (inL * dryMix) + (toneFilterStateL * wetMix);
        out[1][i] = (inR * dryMix) + (toneFilterStateR * wetMix);
    }

    // LED: instant attack, ~400ms decay, scaled to 0–5V for CV_OUT_2
    if (peakLevel > ledBrightness)
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