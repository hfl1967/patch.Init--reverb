# patch.Init--reverb

A stereo reverb module for the [Electrosmith patch.Init()](https://electro-smith.com/products/patch-init), built in C++ using [libDaisy](https://github.com/electro-smith/libDaisy) and [DaisySP](https://github.com/electro-smith/DaisySP).

## Features

- Four-parameter reverb
- Automatic mono/stereo detection via envelope follower
- Pre-delay up to 100ms
- Tone control via independent one-pole filter on wet signal
- Input level indicator via front panel LED (CV_OUT_2)

## Controls

| Control | Function | Range |
|---|---|---|
| Knob 1 + CV 1 | Reverb time | CCW = short, CW = long |
| Knob 2 + CV 2 | Tone | CCW = dark, CW = bright |
| Knob 3 + CV 3 | Wet/dry mix | CCW = dry, CW = wet |
| Knob 4 + CV 4 | Pre-delay | CCW = 0ms, CW = 100ms |

CV inputs offset their respective knob. 0V = no offset, positive voltage increases the value, negative decreases it.

## Audio

- Patch LEFT input only for mono operation
- Patch both inputs for stereo — right channel is detected automatically via envelope follower

## Status

Work in progress. Tested and working:
- All four knobs
- Mono/stereo auto-detection
- Front panel LED input indicator

Not yet implemented:
- CV modulation for all controls
- Gate inputs and outputs
- Button operaiton 
- Switch operation

## Building

Clone this repo alongside `libDaisy` and `DaisySP` in the same parent directory:

Projects/
├── libDaisy/
├── DaisySP/
└── init_reverb/

Build and flash (requires [Daisy bootloader](https://github.com/electro-smith/DaisyBootloader)):
```bash
make
make program-dfu
```

## Flashing without building

A prebuilt binary is available in `bin/`. Flash using dfu-util:
```bash
dfu-util -a 0 -s 0x90040000:leave -D bin/reverb.bin -d ,0483:df11
```

Or use the [Electrosmith web programmer](https://electro-smith.github.io/Programmer/).

## License

This project is MIT licensed.

The `ReverbSc` algorithm is from [DaisySP-LGPL](https://github.com/electro-smith/DaisySP) and licensed under [LGPL v2.1](https://opensource.org/license/lgpl-2-1/). Since this project's full source is public, the LGPL distribution requirement is satisfied.