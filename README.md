# Cardputer ADV EMF Meter

<img src="https://github.com/tinsever/m5-adv-emf-meter/blob/main/demo.jpeg?raw=true" width="500">

*Photo of the original version; the updated layout fits all five bars on screen.*

An EMF (electromagnetic field) scanner built with a Cardputer ADV and a single jumper wire as an antenna.
It detects ambient electrical fields, visualizes signal strength, and identifies mains frequency components in real time: **50/100 Hz** or **60/120 Hz** for regions such as the US.

# Quickstart
a) Using Launcher: Download .bin from [Releases](https://github.com/tinsever/m5-adv-emf-meter/releases) to your Cardputer ADV

b) Arduino IDE: put `emf-meter.ino` and `meter_math.h` in an `emf-meter` sketch folder. Install M5Cardputer 1.1.1, M5Unified 0.2.22 and M5GFX 0.2.29, then select the Cardputer board and compile/flash.

c) PlatformIO: run `pio run -e cardputer-adv`. The Launcher-ready application binary is `.pio/build/cardputer-adv/firmware.bin`. Install it through Launcher, rather than flashing this application-only file at address `0x0`. The tested configuration uses PlatformIO Core 6.2.0 and Arduino-ESP32 2.0.17; exact board/library versions are pinned in `platformio.ini`.

Then put the jumper wire into G3.

## Controls

| Key | Action |
| --- | --- |
| **5** | Select 50 Hz mains and the 100 Hz harmonic |
| **6** | Select 60 Hz mains and the 120 Hz harmonic |
| **A** | Toggle audio cues |

The app starts with **50/100 Hz and sound OFF**. Settings apply to the current session and reset on restart. The header shows the selected frequencies and sound status. Changing frequency clears the two frequency measurements so old values are not relabeled as measurements of the new frequencies.

Audio uses short, quiet beeps: stronger smoothed RMS produces a higher pitch and shorter interval. It stays silent below 20 RMS ADC counts. These thresholds are relative signal levels, not calibrated field-strength limits. If speaker initialization/playback fails, the header shows `Sound:ERR`; press A to retry.

Beeps run **between** acquisition windows. Speaker playback and its I2S driver stop before the next window, followed by a short settling delay. Real-device testing is still needed to evaluate any remaining electrical coupling from the speaker hardware.

# What it does

- Uses a floating ADC pin as an antenna
- Detects nearby electrical fields (e.g. live wires, sockets, devices)
- Displays:
  - Peak-to-peak signal
  - RMS (signal energy)
  - Selected mains component: 50 Hz or 60 Hz
  - Second harmonic: 100 Hz or 120 Hz
  - DC offset (mean)
  - Peak hold
- All five bars fit on the Cardputer-ADV's 240 × 135 landscape display
- Reduced display flicker: static labels/borders stay in place, changed numbers are replaced using small sprites, and only the changed portions of bars are drawn or erased
- Optional audio cues controlled with A
- Serial output for logging / plotting

# How it works
The jumper wire acts as a high-impedance antenna, picking up electric fields via capacitive coupling.

The ESP32 ADC samples the signal and extracts features:

- Peak-to-Peak: overall signal variation
- RMS: signal energy
- Goertzel algorithm: evaluates the selected frequencies exactly (50/100 Hz or 60/120 Hz)

Each measurement uses 200 samples at 1 kHz. A 200 ms window contains an integer number of cycles for all four frequencies, improving separation between the two mains profiles. DC is removed before RMS and frequency analysis. Frequency magnitude retains the original scaling convention: a pure sine with peak amplitude A gives approximately A/2. Values are relative ADC counts, not calibrated magnetic or electric field units.

ADC acquisition does not perform display updates, keyboard I/O or audio playback. Queued keyboard events are processed between windows. Windows that miss a complete 1 ms sampling interval are discarded rather than reporting incorrect frequency values. Deadline comparisons also handle the `micros()` counter wrapping.

Serial output remains five numeric CSV columns: `P2P,RMS,fundamental,harmonic,Mean`. At startup and after a frequency change, a comment identifies the active columns, e.g. `# P2P,RMS,60Hz,120Hz,Mean`. Logging tools should ignore lines beginning with `#`.

## Validation

Compile the firmware with `pio run -e cardputer-adv`. Run the hardware-independent signal tests with:

```sh
c++ -std=c++11 -Wall -Wextra -Werror tests/meter_math_test.cpp -o /tmp/emf-meter-test
/tmp/emf-meter-test
```

The tests cover 50/100 Hz and 60/120 Hz mixtures, DC rejection, RMS and magnitude scaling, cross-profile rejection, non-bin-centred frequency evaluation, timer wraparound, bar limits, and audio cadence.

Hardware check after installation: confirm all five bars are visible; press 5/6 to switch frequencies and labels; check shrinking bars and shorter numbers leave no stale pixels; toggle A and compare readings with sound off/on. Compilation and host tests do not establish real-world measurement accuracy or eliminate the need for this device check.

# Hardware
- M5 Cardputer ADV (this layout and keyboard controls target the ADV)
- 1x jumper wire as antenna

## Could be nice but I don't have either:
- longer wire: stronger signal
- metal probe: more stable readings
