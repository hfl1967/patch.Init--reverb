// reverb.cpp
// A simple stereo reverb for the Electrosmith patch.Init()
// Uses DaisySP's built-in ReverbSc algorithm.
//
// CONTROLS:
//   Knob 1 + CV 1: Reverb time (0 = short, 1 = long/infinite)
//   Knob 2 + CV 2: Damping / high-frequency rolloff
//   Knob 3 + CV 3: Wet/dry mix (0 = dry, 1 = 100% wet)
//   Knob 4 + CV 4: Input gain (useful for driving the reverb harder)
//
// AUDIO BEHAVIOR:
//   - If only the LEFT input is patched, runs in MONO (copies input to both channels)
//   - If both inputs are patched, runs in TRUE STEREO
//   (Note: true mono detection requires a normalization circuit;
//    for now we always process stereo but sum inputs if you only patch one jack)

#include "daisy_patch_sm.h"
#include "daisysp.h"

// These "using namespace" lines let us write DaisyPatchSM instead of
// daisy::patch_sm::DaisyPatchSM — just shorter names for things we use a lot.
using namespace daisy;
using namespace patch_sm;
using namespace daisysp;

// hw is our main hardware object — it controls everything on the module.
DaisyPatchSM hw;

// ReverbSc is a high-quality Schroeder/Keith Barr style reverb from DaisySP.
// It processes two channels (left and right) simultaneously.
ReverbSc reverb;

// --- Helper: read a knob and clamp it to [0, 1] ---
// ADC_9 through ADC_12 are the four knobs on the patch.Init().
// GetAdcValue() returns a float between 0.0 and 1.0.
float ReadKnob(int adcIndex)
{
    return fclamp(hw.GetAdcValue(adcIndex), 0.0f, 1.0f);
}

// --- Helper: read a CV input, scaled and offset to [0, 1] ---
// CV inputs CV_1 through CV_4 accept -5V to +5V.
// GetAdcValue() returns roughly 0.0 at -5V and 1.0 at +5V.
// We clamp the result so out-of-range voltages don't break the DSP.
float ReadCV(int cvIndex)
{
    return fclamp(hw.GetAdcValue(cvIndex), 0.0f, 1.0f);
}

// --- Helper: combine knob + CV, clamped to [0, 1] ---
// The knob sets a base value; CV offsets it.
// We subtract 0.5 from the CV so a 0V CV input = no offset.
// This means: negative CV reduces the value, positive CV raises it.
float ReadKnobWithCV(int knobAdc, int cvAdc)
{
    float knob = ReadKnob(knobAdc);
    float cv   = ReadCV(cvAdc) - 0.5f; // center CV around zero
    return fclamp(knob + cv, 0.0f, 1.0f);
}

// --- The audio callback ---
// This function is called by the hardware at audio rate (96kHz) in blocks.
// in[][]  = input audio samples  [channel][sample]
// out[][] = output audio samples [channel][sample]
// size    = number of samples in this block
void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    // ProcessAllControls() updates knob and CV readings.
    // Call this once per audio callback, before reading any controls.
    hw.ProcessAllControls();

    // Read our four knob+CV pairs
    // Reverb time: map 0-1 to a useful range (0.1s to 10s for ReverbSc)
    float reverbTime = 0.1f + ReadKnobWithCV(ADC_9,  CV_1) * 9.9f;

    // Damping: 0 = bright (no damping), 1 = dark (heavy damping)
    // ReverbSc expects damping as a frequency in Hz; we map to 1kHz–18kHz
    float damping    = 1000.f + ReadKnobWithCV(ADC_10, CV_2) * 17000.f;

    // Wet/dry: 0 = fully dry, 1 = fully wet
    float wetDry     = ReadKnobWithCV(ADC_11, CV_3);

    // Input gain: 0.0 to 2.0 (center position = unity gain)
    float inputGain  = ReadKnobWithCV(ADC_12, CV_4) * 2.0f;

    // Update the reverb parameters (safe to do inside the callback)
    reverb.SetFeedback(reverbTime / 10.0f); // ReverbSc feedback 0..1
    reverb.SetLpFreq(damping);

    // Process each sample in the block
    for(size_t i = 0; i < size; i++)
    {
        // Read stereo inputs
        // IN_L = channel 0, IN_R = channel 1
        // If you only patch the left input, right will be silent (0V).
        // We sum both and halve, then use for both channels (pseudo-mono).
        float inL = in[0][i] * inputGain;
        float inR = in[1][i] * inputGain;

        // If right input is near-silent, treat as mono and copy left to right.
        // This is a simple approximation; a real normalization jack would be
        // handled in hardware. For now this gives reasonable behavior.
        if(fabsf(in[1][i]) < 0.001f)
            inR = inL;

        // Run the reverb — it writes its wet output into sendL and sendR
        float sendL, sendR;
        reverb.Process(inL, inR, &sendL, &sendR);

        // Mix wet and dry signals
        out[0][i] = inL * (1.0f - wetDry) + sendL * wetDry;
        out[1][i] = inR * (1.0f - wetDry) + sendR * wetDry;
    }
}

int main(void)
{
    // Initialize all hardware (ADC, audio codec, clocks, etc.)
    hw.Init();

    // Set audio sample rate to 48kHz.
    // The patch.Init() supports 96kHz, but reverb is less CPU-hungry at 48kHz,
    // and audio quality is still excellent. Easy to change later.
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    // Initialize the reverb with our sample rate
    float sampleRate = hw.AudioSampleRate();
    reverb.Init(sampleRate);

    // Set some pleasant starting defaults
    reverb.SetFeedback(0.7f);   // medium-long reverb time
    reverb.SetLpFreq(8000.f);   // gentle damping

    // Start the audio — from here on, AudioCallback() runs automatically.
    hw.StartAudio(AudioCallback);

    // The main loop doesn't need to do anything for a simple reverb.
    // In the future, this is where you'd update a display, handle MIDI, etc.
    while(true)
    {
        // Turn on the LED to show the firmware is running
        hw.SetLed(true);
    }
}