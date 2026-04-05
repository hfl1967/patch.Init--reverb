// reverb.cpp
// A simple stereo reverb for the Electrosmith patch.Init()
// Uses the ReverbSc algorithm from DaisySP-LGPL.
//
// CONTROLS:
//   Knob 1 + CV 1: Reverb time (power curve — long tails in upper range)
//   Knob 2 + CV 2: Tone — standalone one-pole filter on wet signal
//                  Full CCW = dark/muffled, Full CW = bright/open
//   Knob 3 + CV 3: Wet/dry mix (full CCW = dry, full CW = wet)
//   Knob 4 + CV 4: Pre-delay (0ms to 100ms)
//
// AUDIO BEHAVIOR:
//   - If only the LEFT input is patched, right channel mirrors left (mono mode)
//   - If both inputs are patched, runs in true stereo
//
// LED BEHAVIOR:
//   - Front panel red LED lights when input signal exceeds kPeakThreshold
//   - Stays on for kPeakHoldTicks callbacks then turns off
//   - Tune kPeakThreshold: raise if always on, lower if never lights
//
// BLOCK SIZE:
//   - Set to 4 samples to eliminate zipper/whine noise on knob movement

#include "daisy_patch_sm.h"
#include "daisysp.h"
#include "../DaisySP/DaisySP-LGPL/Source/Effects/reverbsc.h"

using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

DaisyPatchSM hw;
ReverbSc reverb;

// --- Pre-delay circular buffer ---
// 100ms at 48kHz = 4800 samples per channel
static const int kPreDelayMaxSamples = 4800;
float preDelayBufL[kPreDelayMaxSamples];
float preDelayBufR[kPreDelayMaxSamples];
int preDelayWrite = 0;

// --- Tone filter state ---
// One-pole lowpass filter applied to the wet reverb signal.
// This sits outside ReverbSc so we have full, predictable control.
// Two state variables, one per channel, persist between callbacks.
float toneFilterStateL = 0.0f;
float toneFilterStateR = 0.0f;

// --- LED settings ---
volatile float ledVoltage = 0.0f;

// --- Helpers ---
float ReadKnob(int adcIndex)
{
    return fclamp(hw.GetAdcValue(adcIndex), 0.0f, 1.0f);
}

float ReadCV(int cvIndex)
{
    return fclamp(hw.GetAdcValue(cvIndex), 0.0f, 1.0f);
}

// Combines knob + CV. CV is centered so 0V = no offset.
float ReadKnobWithCV(int knobAdc, int cvAdc)
{
    float knob = ReadKnob(knobAdc);
    float cv = ReadCV(cvAdc) - 0.5f;
    return fclamp(knob + cv, 0.0f, 1.0f);
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    hw.ProcessAllControls();

    // Knob 1: Reverb time
    // Power curve: short rooms in lower quarter, near-infinite in upper quarter
    float knob1 = ReadKnobWithCV(ADC_9, CV_1);
    float feedback = 0.3f + powf(knob1, 0.3f) * 0.699f;

    // Knob 2: Tone — one-pole lowpass on the wet signal
    // We keep ReverbSc's internal LP wide open (fixed at 18kHz)
    // and apply our own filter after the reverb for full control.
    // The filter coefficient controls how much signal passes through:
    //   coeff near 0.0 = very dark (heavily filtered)
    //   coeff near 1.0 = bright (filter barely active)
    // We map the knob through a power curve so the bright end is
    // more gradual and the dark end is more dramatic.
    // Full CCW = 0.02 (very dark), Full CW = 0.9999 (fully open/bright)
    float knob2 = ReadKnobWithCV(ADC_10, CV_2);
    // Invert: full CW = bright (low coeff = filter open)
    //         full CCW = dark (high coeff = filter damps highs)
    float toneCoeff = 0.9799f - powf(knob2, 2.0f) * 0.9599f;
    // powf(x, 2.0) = squared curve — most of the knob sweep is open/bright,
    // darkness happens quickly in the lower CCW range, which is more musical

    // Knob 3: Wet/dry — constant power crossfade keeps volume even
    // sin/cos ensures total energy stays consistent across the sweep
    // Full CCW = dry only, Full CW = wet only, noon = equal blend
    float knob3 = ReadKnobWithCV(ADC_11, CV_3);
    // Push curve so reverb appears earlier in the sweep (~8-9 o'clock)
    float wetPos = powf(knob3, 0.5f); // square root = earlier onset
    float dryMix = cosf(wetPos * (float(M_PI) / 2.0f));
    float wetMix = sinf(wetPos * (float(M_PI) / 2.0f));

    // Knob 4: Pre-delay (0ms to 100ms)
    // Creates a gap between dry signal and reverb tail
    // Small amounts (5-20ms) make reverb feel natural and separated
    float knob4 = ReadKnobWithCV(ADC_12, CV_4);
    int preDelaySamples = (int)(knob4 * kPreDelayMaxSamples);

    // Fix ReverbSc internal LP wide open — tone is handled by our filter
    reverb.SetFeedback(feedback);
    reverb.SetLpFreq(18000.f);

    float peakLevel = 0.0f;

    for (size_t i = 0; i < size; i++)
    {
        float inL = in[0][i];
        float inR = in[1][i];

        // Mono mode: mirror left to right if right input is silent
        if (fabsf(inR) < 0.001f)
            inR = inL;

        // Peak detection on input signal for LED
        float blockPeak = fmaxf(fabsf(inL), fabsf(inR));
        if (blockPeak > peakLevel)
            peakLevel = blockPeak;

        // Write to pre-delay buffer
        preDelayBufL[preDelayWrite] = inL;
        preDelayBufR[preDelayWrite] = inR;

        // Read delayed signal — wraps around using modulo
        int readPos = (preDelayWrite - preDelaySamples + kPreDelayMaxSamples) % kPreDelayMaxSamples;
        float delayedL = preDelayBufL[readPos];
        float delayedR = preDelayBufR[readPos];

        preDelayWrite = (preDelayWrite + 1) % kPreDelayMaxSamples;

        // Run reverb on pre-delayed signal
        float sendL, sendR;
        reverb.Process(delayedL, delayedR, &sendL, &sendR);

        // Apply standalone one-pole lowpass to wet signal (tone knob)
        // Formula: output = coeff * output_prev + (1 - coeff) * input
        // Higher coeff = more of the previous output kept = darker/smoother
        // This is a simple but effective and musical-sounding filter
        toneFilterStateL = toneCoeff * toneFilterStateL + (1.f - toneCoeff) * sendL;
        toneFilterStateR = toneCoeff * toneFilterStateR + (1.f - toneCoeff) * sendR;

        // Dry signal bypasses both pre-delay and tone filter
        out[0][i] = (inL * dryMix) + (toneFilterStateL * wetMix);
        out[1][i] = (inR * dryMix) + (toneFilterStateR * wetMix);
    }

    // LED: instant attack, smooth decay
    // Jumps to peak level immediately, decays smoothly each callback
    static float ledBrightness = 0.0f;
    if (peakLevel > ledBrightness)
        ledBrightness = peakLevel;
    ledBrightness *= 0.998f;

    // Scale 0.0-1.0 to 0.0-5.0V for the DAC
    ledVoltage = ledBrightness * 5.0f;
}

int main(void)
{
    hw.Init();

    // Clear pre-delay buffers
    for (int i = 0; i < kPreDelayMaxSamples; i++)
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
    System::Delay(100);
    hw.WriteCvOut(CV_OUT_1, 0.0f); // force DAC channels to known state
    hw.WriteCvOut(CV_OUT_2, 0.0f);

    while (true)
    {
        hw.WriteCvOut(CV_OUT_2, ledVoltage);
        System::Delay(1);
    }
}