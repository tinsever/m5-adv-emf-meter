# Cardputer ADV EMF Meter

<img src="https://github.com/tinsever/m5-adv-emf-meter/blob/main/demo.jpeg?raw=true" width="500">

An EMF (electromagnetic field) scanner built with an Cardputer ADV and a single jumper wire as an antenna.
It detects ambient electrical fields, visualizes signal strength, and identifies mains frequency components (50 Hz / 100 Hz) in real time.

# Quickstart
a) Using Launcher: Download .bin from [Releases](https://github.com/tinsever/m5-adv-emf-meter/releases) to your Cardputer ADV
b) Flashing: Using Arduino IDE and code in emf-meter.ino

# What it does

- Uses a floating ADC pin as an antenna
- Detects nearby electrical fields (e.g. live wires, sockets, devices)
- Displays:
  - Peak-to-peak signal
  - RMS (signal energy)
  - 50 Hz component (mains detection)
  - 100 Hz harmonic
  - DC offset (mean)
  - Peak hold
- Real-time bar visualization on the M5 display
- Serial output for logging / plotting

# How it works
The jumper wire acts as a high-impedance antenna, picking up electric fields via capacitive coupling.

The ESP32 ADC samples the signal and extracts features:

- Peak-to-Peak: overall signal variation
- RMS: signal energy
- Goertzel algorithm: detects specific frequencies (50 Hz / 100 Hz)

# Hardware
- M5 Cardputer ADV (should work with other M5-ESP devices too..)
- 1x jumper wire as antenna

## Could be nice but I don't have either:
- longer wire: stronger signal
- metal probe: more stable readings
