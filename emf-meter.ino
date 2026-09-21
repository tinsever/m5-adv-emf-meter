#include <M5Cardputer.h>
#include "meter_math.h"

constexpr int ADC_PIN = 3;
constexpr float alpha = 0.15f;
constexpr int ROW_COUNT = 5;
constexpr int ROW_Y = 35;
constexpr int ROW_STEP = 16;
constexpr int BAR_X = 100;
constexpr int BAR_Y_OFFSET = 1;
constexpr int BAR_W = 135;
constexpr int BAR_H = 11;
constexpr int VALUE_X = 59;
constexpr int VALUE_W = 36;

int mainsHz = 50;
bool audioEnabled = false;
bool audioFailed = false;
bool previous50 = false, previous60 = false, previousAudio = false;
float smoothSignal = 0, smoothRMS = 0, smoothFundamental = 0, smoothHarmonic = 0;
int peakHold = 0;
uint32_t lastPeakDecay = 0, lastCue = 0;
int samples[meter::sampleCount];

// Small sprites replace each changed text field in one transfer; the complete
// display is never cleared during normal measurements.
M5Canvas textLine(&M5.Display);
M5Canvas valueText(&M5.Display);
M5Canvas labelText(&M5.Display);
char previousValues[ROW_COUNT][12] = {};
char previousStats[48] = {};
char previousHeader[48] = {};
int previousBarPixels[ROW_COUNT] = {};

uint32_t getColor(float position) {
  const float ratio = meter::ratio(position, 1.0f);
  const int r = ratio < 0.5f ? int(510 * ratio) : 255;
  const int g = ratio < 0.5f ? 255 : int(510 * (1 - ratio));
  return (r << 16) | (g << 8);
}

void drawLine(int y, const char* text, char* previous) {
  if (strcmp(text, previous) == 0) return;
  textLine.fillSprite(BLACK);
  textLine.setCursor(0, 1);
  textLine.print(text);
  textLine.pushSprite(5, y);
  strcpy(previous, text); // Both callers use buffers of the same size (48).
}

void drawHeader() {
  char text[48];
  snprintf(text, sizeof(text), "G3 | %d/%dHz | Sound:%s", mainsHz, mainsHz * 2,
           audioFailed ? "ERR" : (audioEnabled ? "ON" : "OFF"));
  drawLine(4, text, previousHeader);
}

void drawFrequencyLabels() {
  for (int i = 0; i < 2; ++i) {
    labelText.fillSprite(BLACK);
    labelText.setCursor(0, 1);
    labelText.printf("%dHz", mainsHz * (i + 1));
    labelText.pushSprite(5, ROW_Y + (i + 2) * ROW_STEP);
  }
}

void updateBar(int row, float value, float maximum) {
  char text[12];
  snprintf(text, sizeof(text), "%.0f", value);
  const int y = ROW_Y + row * ROW_STEP;
  if (strcmp(text, previousValues[row]) != 0) {
    valueText.fillSprite(BLACK);
    valueText.setCursor(VALUE_W - strlen(text) * 6, 1);
    valueText.print(text);
    valueText.pushSprite(VALUE_X, y);
    strcpy(previousValues[row], text);
  }
  const int innerW = BAR_W - 2;
  const int length = meter::barPixels(value, maximum, innerW);
  const int previous = previousBarPixels[row];
  if (length == previous) return;
  M5.Display.startWrite();
  if (length < previous) {
    M5.Display.fillRect(BAR_X + 1 + length, y + BAR_Y_OFFSET + 1,
                        previous - length, BAR_H - 2, BLACK);
  } else {
    for (int i = previous; i < length; ++i) {
      M5.Display.drawFastVLine(BAR_X + 1 + i, y + BAR_Y_OFFSET + 1,
                              BAR_H - 2, getColor(float(i) / (innerW - 1)));
    }
  }
  M5.Display.endWrite();
  previousBarPixels[row] = length;
}

void printSerialHeader() {
  // Keep the original five numeric CSV fields; identify them after mode changes.
  Serial.printf("# P2P,RMS,%dHz,%dHz,Mean\n", mainsHz, mainsHz * 2);
}

void selectMains(int frequency) {
  if (mainsHz == frequency) return;
  mainsHz = frequency;
  // Old-frequency amplitudes must not be carried into the new filter's display.
  smoothFundamental = smoothHarmonic = 0;
  updateBar(2, 0, meter::maxHz);
  updateBar(3, 0, meter::maxHz);
  drawFrequencyLabels();
  drawHeader();
  printSerialHeader();
}

void pollControls() {
  M5Cardputer.update();
  const bool key50 = M5Cardputer.Keyboard.isKeyPressed('5');
  const bool key60 = M5Cardputer.Keyboard.isKeyPressed('6');
  const bool keyAudio = M5Cardputer.Keyboard.isKeyPressed('a') || M5Cardputer.Keyboard.isKeyPressed('A');
  if (key50 && !previous50) selectMains(50);
  if (key60 && !previous60) selectMains(60);
  if (keyAudio && !previousAudio) {
    audioEnabled = !audioEnabled;
    audioFailed = false;
    drawHeader();
  }
  previous50 = key50;
  previous60 = key60;
  previousAudio = keyAudio;
}

void serviceAudio() {
  const auto cue = meter::audioCue(smoothRMS);
  if (!audioEnabled || !cue.audible || millis() - lastCue < cue.intervalMs) return;
  lastCue = millis();
  if (!M5.Speaker.begin() || !M5.Speaker.tone(cue.frequency, 25)) {
    audioEnabled = false;
    audioFailed = true;
    M5.Speaker.end();
    drawHeader();
    return;
  }
  const uint32_t started = millis();
  while (audioEnabled && M5.Speaker.isPlaying() && millis() - started < 150) {
    pollControls();
    delay(1);
  }
  // Stop I2S playback before acquiring the next block. A floating ADC
  // antenna can otherwise pick up the device's own speaker and clock signals.
  M5.Speaker.end();
  delay(10);
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_spk = true;
  cfg.internal_mic = false;
  M5Cardputer.begin(cfg, true);
  M5.Speaker.setVolume(48);
  M5.Speaker.end();
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(ADC_PIN, ADC_11db);
  M5.Display.setRotation(1);
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextWrap(false);
  M5.Display.fillScreen(BLACK);

  textLine.setColorDepth(16);
  valueText.setColorDepth(16);
  labelText.setColorDepth(16);
  if (!textLine.createSprite(230, 12) || !valueText.createSprite(VALUE_W, 12)
      || !labelText.createSprite(52, 12)) {
    M5.Display.setCursor(5, 5);
    M5.Display.print("Display buffer allocation failed");
    while (true) delay(1000);
  }
  for (M5Canvas* canvas : {&textLine, &valueText, &labelText}) {
    canvas->setFont(&fonts::Font0);
    canvas->setTextSize(1);
    canvas->setTextColor(WHITE, BLACK);
    canvas->setTextWrap(false);
  }
  const char* labels[] = {"P2P", "RMS", "", "", "Peak"};
  for (int row = 0; row < ROW_COUNT; ++row) {
    const int y = ROW_Y + row * ROW_STEP;
    M5.Display.setCursor(5, y + 1);
    M5.Display.print(labels[row]);
    M5.Display.drawRect(BAR_X, y + BAR_Y_OFFSET, BAR_W, BAR_H, WHITE);
  }
  M5.Display.setCursor(5, 123);
  M5.Display.print("5:50Hz  6:60Hz  A:sound");
  drawFrequencyLabels();
  drawHeader();
  printSerialHeader();
  lastPeakDecay = millis();
}

void loop() {
  // Drain queued key events outside acquisition, including quick press/release
  // pairs collected during the previous 200 ms block.
  for (int i = 0; i < 12; ++i) {
    pollControls();
    delay(1);
  }

  uint32_t nextSample = micros();
  bool timingValid = true;
  for (int i = 0; i < meter::sampleCount; ++i) {
    while (!meter::deadlineReached(micros(), nextSample)) {}
    if (micros() - nextSample >= meter::sampleIntervalUs) timingValid = false;
    samples[i] = analogRead(ADC_PIN);
    nextSample += meter::sampleIntervalUs;
  }
  // Do not report frequencies from a window with missed sample deadlines.
  if (!timingValid) return;
  const auto reading = meter::analyze(samples, meter::sampleCount, meter::sampleRate, mainsHz);
  smoothSignal += alpha * (reading.peakToPeak - smoothSignal);
  smoothRMS += alpha * (reading.rms - smoothRMS);
  smoothFundamental += alpha * (reading.fundamental - smoothFundamental);
  smoothHarmonic += alpha * (reading.harmonic - smoothHarmonic);

  const uint32_t decaySteps = (millis() - lastPeakDecay) / 120;
  if (decaySteps) {
    const uint32_t decay = decaySteps * 5;
    peakHold = decay >= uint32_t(peakHold) ? 0 : peakHold - decay;
    lastPeakDecay += decaySteps * 120;
  }
  if (smoothSignal > peakHold) peakHold = int(smoothSignal);

  char text[48];
  snprintf(text, sizeof(text), "DC:%.1f Min:%d Max:%d", reading.mean, reading.minimum, reading.maximum);
  drawLine(19, text, previousStats);
  updateBar(0, smoothSignal, meter::maxSignal);
  updateBar(1, smoothRMS, meter::maxRms);
  updateBar(2, smoothFundamental, meter::maxHz);
  updateBar(3, smoothHarmonic, meter::maxHz);
  updateBar(4, peakHold, meter::maxSignal);
  Serial.printf("%.2f,%.2f,%.2f,%.2f,%.2f\n", smoothSignal, smoothRMS,
                smoothFundamental, smoothHarmonic, reading.mean);
  serviceAudio();
}
