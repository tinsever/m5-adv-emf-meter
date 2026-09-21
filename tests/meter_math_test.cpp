#include "../meter_math.h"
#include <cassert>
#include <iostream>

int main() {
  int data[meter::sampleCount];
  for (int& sample : data) sample = 2048;
  for (int mains : {50, 60}) {
    auto result = meter::analyze(data, meter::sampleCount, meter::sampleRate, mains);
    assert(result.mean == 2048 && result.rms == 0 && result.peakToPeak == 0);
    assert(result.fundamental == 0 && result.harmonic == 0);
  }
  // Both regions, with DC offset, primary and second harmonic. Goertzel keeps
  // the existing magnitude convention: a sine of amplitude A reports A/2.
  for (int mains : {50, 60}) {
    for (int i = 0; i < meter::sampleCount; ++i) {
      double t = double(i) / meter::sampleRate;
      data[i] = std::lround(2048 + 600 * std::sin(2 * 3.141592653589793 * mains * t)
                                 + 200 * std::sin(4 * 3.141592653589793 * mains * t));
    }
    const auto result = meter::analyze(data, meter::sampleCount, meter::sampleRate, mains);
    assert(std::fabs(result.mean - 2048) < 0.1f);
    assert(std::fabs(result.rms - std::sqrt(200000.0f)) < 1.0f);
    assert(std::fabs(result.fundamental - 300) < 1.0f);
    assert(std::fabs(result.harmonic - 100) < 1.0f);
    const auto other = meter::analyze(data, meter::sampleCount, meter::sampleRate, mains == 50 ? 60 : 50);
    assert(other.fundamental < 1.0f && other.harmonic < 1.0f);
  }
  // A non-bin-centred signal catches the former +0.5 frequency offset.
  const int count = 256;
  int offBin[count];
  for (int i = 0; i < count; ++i)
    offBin[i] = std::lround(2048 + 600 * std::sin(2 * 3.141592653589793 * 60 * i / 1000));
  assert(std::fabs(meter::goertzel(60, 1000, offBin, count, 2048) - 300) < 3);
  assert(!meter::deadlineReached(0xFFFFFFF0u, 0x10u));
  assert(meter::deadlineReached(0x10u, 0x10u));
  assert(meter::deadlineReached(0x20u, 0xFFFFFFF0u));
  assert(meter::barPixels(-1, 800, 133) == 0);
  assert(meter::barPixels(400, 800, 133) == 66);
  assert(meter::barPixels(4095, 800, 133) == 133);
  assert(!meter::audioCue(0).audible && !meter::audioCue(19).audible);
  assert(meter::audioCue(20).audible);
  assert(meter::audioCue(800).intervalMs < meter::audioCue(200).intervalMs);
  assert(meter::audioCue(800).frequency > meter::audioCue(200).frequency);
  assert(meter::audioCue(4000).intervalMs == meter::audioCue(800).intervalMs);
  std::cout << "PASS: 50/100 and 60/120 Hz, DC, RMS, leakage, timing, bars, audio\n";
}
