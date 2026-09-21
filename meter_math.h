#pragma once
#include <cmath>
#include <cstdint>

namespace meter {
constexpr int sampleCount = 200;
constexpr float sampleRate = 1000.0f;
constexpr uint32_t sampleIntervalUs = 1000;
constexpr float maxSignal = 2000.0f;
constexpr float maxRms = 800.0f;
constexpr float maxHz = 800.0f;

// Signed subtraction remains valid when the 32-bit micros() counter wraps.
inline bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

inline float ratio(float value, float maximum) {
  if (value <= 0.0f) return 0.0f;
  return value >= maximum ? 1.0f : value / maximum;
}

inline int barPixels(float value, float maximum, int width) {
  return static_cast<int>(ratio(value, maximum) * width);
}

inline float goertzel(float frequency, float rate, const int* data, int count, float mean) {
  // Evaluate the requested frequency exactly. Adding 0.5 to a floating-point
  // bin index shifts the filter instead of rounding it to a DFT bin.
  const float omega = 2.0f * 3.14159265358979323846f * frequency / rate;
  const float coeff = 2.0f * std::cos(omega);
  float q1 = 0.0f, q2 = 0.0f;
  for (int i = 0; i < count; ++i) {
    const float q0 = coeff * q1 - q2 + (data[i] - mean);
    q2 = q1;
    q1 = q0;
  }
  const float power = q1 * q1 + q2 * q2 - coeff * q1 * q2;
  return std::sqrt(power > 0.0f ? power : 0.0f) / count;
}

struct Reading {
  int minimum, maximum, peakToPeak;
  float mean, rms, fundamental, harmonic;
};

inline Reading analyze(const int* data, int count, float rate, int mainsHz) {
  Reading result = {};
  result.minimum = result.maximum = data[0];
  int32_t sum = 0;
  for (int i = 0; i < count; ++i) {
    if (data[i] < result.minimum) result.minimum = data[i];
    if (data[i] > result.maximum) result.maximum = data[i];
    sum += data[i];
  }
  result.mean = static_cast<float>(sum) / count;
  float squares = 0.0f;
  for (int i = 0; i < count; ++i) {
    const float centered = data[i] - result.mean;
    squares += centered * centered;
  }
  result.rms = std::sqrt(squares / count);
  result.peakToPeak = result.maximum - result.minimum;
  result.fundamental = goertzel(mainsHz, rate, data, count, result.mean);
  result.harmonic = goertzel(2 * mainsHz, rate, data, count, result.mean);
  return result;
}

struct AudioCue { bool audible; uint32_t intervalMs; float frequency; };
inline AudioCue audioCue(float rms) {
  const float strength = ratio(rms, maxRms);
  return {rms >= 20.0f, static_cast<uint32_t>(1200 - 950 * strength), 700 + 1500 * strength};
}
}
