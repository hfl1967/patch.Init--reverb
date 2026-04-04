# reverb

A simple stereo reverb for the [Electrosmith patch.Init()](https://electro-smith.com/products/patch-init) Eurorack module, built with [libDaisy](https://github.com/electro-smith/libDaisy) and [DaisySP](https://github.com/electro-smith/DaisySP).

## Controls

| Control | Function |
|---|---|
| Knob 1 + CV 1 | Reverb time |
| Knob 2 + CV 2 | Damping (tone) |
| Knob 3 + CV 3 | Wet/Dry mix |
| Knob 4 + CV 4 | Input gain |

CV inputs offset their respective knob. Center (0V) = no offset. Positive CV raises the value, negative lowers it.

## Audio

- Patch only LEFT input for mono operation (right channel mirrors left automatically)
- Patch both inputs for stereo

## Building

Clone this repo alongside `libDaisy` and `DaisySP`: