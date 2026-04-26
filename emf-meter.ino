#include <M5Unified.h>

const int ADC_PIN = 3;

const int SAMPLE_COUNT = 256;
const float SAMPLE_RATE = 1000.0;
const int SAMPLE_INTERVAL_US = 1000000 / SAMPLE_RATE;

float smoothSignal = 0;
float smoothRMS = 0;
float smooth50Hz = 0;
float smooth100Hz = 0;

const float alpha = 0.15;

const int MAX_SIGNAL = 2000;
const int MAX_RMS = 800;
const int MAX_HZ = 800;

int peakHold = 0;
unsigned long lastPeakDecay = 0;

int samples[SAMPLE_COUNT];

uint32_t getColor(float xRatio) {
  int r, g;

  if (xRatio < 0.5) {
    r = int(255 * (xRatio * 2));
    g = 255;
  } else {
    r = 255;
    g = int(255 * (1 - (xRatio - 0.5) * 2));
  }

  return (r << 16) | (g << 8);
}

float goertzel(float targetFreq, float sampleRate, int *data, int count, float mean) {
  float k = 0.5 + ((count * targetFreq) / sampleRate);
  float omega = (2.0 * PI * k) / count;
  float coeff = 2.0 * cos(omega);

  float q0 = 0;
  float q1 = 0;
  float q2 = 0;

  for (int i = 0; i < count; i++) {
    float x = data[i] - mean;
    q0 = coeff * q1 - q2 + x;
    q2 = q1;
    q1 = q0;
  }

  float power = q1 * q1 + q2 * q2 - coeff * q1 * q2;
  return sqrt(power) / count;
}

void drawBar(int x, int y, int w, int h, float value, float maxValue, const char *label) {
  int barLength = int((value / maxValue) * w);
  barLength = constrain(barLength, 0, w);

  M5.Display.drawRect(x, y, w, h, WHITE);

  for (int i = 0; i < barLength; i += 3) {
    float ratio = float(i) / float(w);
    uint32_t color = getColor(ratio);
    M5.Display.fillRect(x + i, y, 3, h, color);
  }

  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(x, y - 10);
  M5.Display.printf("%s: %.0f", label, value);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(ADC_PIN, ADC_11db);

  M5.Display.fillScreen(BLACK);
}

void loop() {
  M5.update();

  int screenWidth = M5.Display.width();
  int screenHeight = M5.Display.height();

  int minVal = 4095;
  int maxVal = 0;
  long sum = 0;

  unsigned long nextSample = micros();

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    while (micros() < nextSample) {
      // exakt warten
    }

    int v = analogRead(ADC_PIN);
    samples[i] = v;

    if (v < minVal) minVal = v;
    if (v > maxVal) maxVal = v;

    sum += v;
    nextSample += SAMPLE_INTERVAL_US;
  }

  float mean = float(sum) / SAMPLE_COUNT;

  float sqSum = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    float centered = samples[i] - mean;
    sqSum += centered * centered;
  }

  float rms = sqrt(sqSum / SAMPLE_COUNT);
  int peakToPeak = maxVal - minVal;

  float hz50 = goertzel(50.0, SAMPLE_RATE, samples, SAMPLE_COUNT, mean);
  float hz100 = goertzel(100.0, SAMPLE_RATE, samples, SAMPLE_COUNT, mean);

  smoothSignal = alpha * peakToPeak + (1.0 - alpha) * smoothSignal;
  smoothRMS = alpha * rms + (1.0 - alpha) * smoothRMS;
  smooth50Hz = alpha * hz50 + (1.0 - alpha) * smooth50Hz;
  smooth100Hz = alpha * hz100 + (1.0 - alpha) * smooth100Hz;

  if (smoothSignal > peakHold) {
    peakHold = smoothSignal;
  }

  if (millis() - lastPeakDecay > 120) {
    if (peakHold > 0) peakHold -= 5;
    lastPeakDecay = millis();
  }

  M5.Display.fillScreen(BLACK);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(WHITE, BLACK);

  M5.Display.setCursor(5, 5);
  M5.Display.printf("ADC pin: %d", ADC_PIN);

  M5.Display.setCursor(5, 18);
  M5.Display.printf("Mean/DC: %.1f", mean);

  M5.Display.setCursor(5, 31);
  M5.Display.printf("Min:%d Max:%d P2P:%d", minVal, maxVal, peakToPeak);

  int barW = screenWidth - 10;
  int x = 5;

  drawBar(x, 55,  barW, 12, smoothSignal, MAX_SIGNAL, "Signal/P2P");
  drawBar(x, 85,  barW, 12, smoothRMS,    MAX_RMS,    "RMS");
  drawBar(x, 115, barW, 12, smooth50Hz,   MAX_HZ,     "50Hz");
  drawBar(x, 145, barW, 12, smooth100Hz,  MAX_HZ,     "100Hz");
  drawBar(x, 175, barW, 12, peakHold,     MAX_SIGNAL, "Peak Hold");

  M5.Display.setCursor(5, screenHeight - 15);
  M5.Display.printf("Serial: P2P,RMS,50Hz,100Hz,Mean");

  Serial.print(smoothSignal);
  Serial.print(",");
  Serial.print(smoothRMS);
  Serial.print(",");
  Serial.print(smooth50Hz);
  Serial.print(",");
  Serial.print(smooth100Hz);
  Serial.print(",");
  Serial.println(mean);
}
