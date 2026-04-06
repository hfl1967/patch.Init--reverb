# patch.Init--reverb

A stereo reverb module for the [Electrosmith patch.Init()](https://electro-smith.com/products/patch-init), built in C++ using [libDaisy](https://github.com/electro-smith/libDaisy) and [DaisySP](https://github.com/electro-smith/DaisySP).

## Features

- Four-parameter reverb with CV modulation on all controls
- Automatic mono/stereo detection via envelope follower
- Pre-delay up to 100ms
- Tone control via independent one-pole filter on wet signal
- Input level indicator via front panel LED (CV_OUT_2)

## Controls

| Control | Function | Range |
|---|---|---|
| Knob 1 + CV 1 | Reverb time | CCW = short, CW = long/infinite |
| Knob 2 + CV 2 | Tone | CCW = dark, CW = bright |
| Knob 3 + CV 3 | Wet/dry mix | CCW = dry, CW = wet |
| Knob 4 + CV 4 | Pre-delay | CCW = 0ms, CW = 100ms |

CV inputs offset their respective knob. 0V = no offset, positive voltage increases the value, negative decreases it.

## Audio

- Patch LEFT input only for mono operation
- Patch both inputs for stereo — right channel is detected automatically via envelope follower

## Status

Work in progress. Tested and working:
- All four knobs and their CV inputs
- Mono/stereo auto-detection
- Front panel LED input indicator

Not yet implemented:
- Gate inputs and outputs
- Button (planned: freeze)
- Switch (planned: reverb type selection)

## Building

Clone this repo alongside `libDaisy` and `DaisySP` in the same parent directory: