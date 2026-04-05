# patch.Init--reverb

A stereo reverb for the [Electrosmith patch.Init()](https://electro-smith.com/products/patch-init) Eurorack module, built with [libDaisy](https://github.com/electro-smith/libDaisy) and [DaisySP](https://github.com/electro-smith/DaisySP).

## Status

Work in progress. The following have been tested and are working:

- Knob 1: Reverb time
- Knob 2: Tone (dark to bright)
- Knob 3: Wet/dry mix
- Knob 4: Pre-delay (0–100ms)

The following are **not yet implemented or tested**:

- CV inputs (CV 1–4)
- CV outputs
- Gate inputs and outputs
- Button (planned: freeze)
- Switch (planned: reverb type)
- Front panel LED (in progress — see [open issue](#))

## Controls

| Control | Function |
|---|---|
| Knob 1 | Reverb time — CCW = short, CW = long/infinite |
| Knob 2 | Tone — CCW = dark, CW = bright |
| Knob 3 | Wet/dry mix — CCW = dry, CW = wet |
| Knob 4 | Pre-delay — CCW = 0ms, CW = 100ms |

## Audio

- Patch LEFT input only for mono (right channel mirrors left automatically)
- Patch both inputs for true stereo

## Building

This project uses [libDaisy](https://github.com/electro-smith/libDaisy) and [DaisySP](https://github.com/electro-smith/DaisySP), including the LGPL-licensed `ReverbSc` algorithm from `DaisySP-LGPL`.

Clone this repo alongside `libDaisy` and `DaisySP` in the same parent folder:

Projects/
├── libDaisy/
├── DaisySP/
└── init_reverb/   ← this repo

Build and flash (requires [Daisy bootloader](https://github.com/electro-smith/DaisyBootloader) installed on hardware):
```bash
make
make program-dfu
```

## License

This project is MIT licensed.

The `ReverbSc` algorithm used here is from [DaisySP-LGPL](https://github.com/electro-smith/DaisySP) and is licensed under the [LGPL v2.1](https://opensource.org/license/lgpl-2-1/). This means if you distribute a binary built with this code, you must make the LGPL source available. Since this project's full source is public, that requirement is already satisfied.