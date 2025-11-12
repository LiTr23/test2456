#include <Arduino.h>
#include <avr/interrupt.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <RotaryEncoder.h>

// ==== Oscillators ====
enum Waveform { SINE, TRIANGLE, SQUARE, SAW, NOISE, WAVE_COUNT };

struct Oscillator {
  Waveform type;
  int octaveIndex;
  int volume;
  float freq;
  uint16_t phase;
  uint16_t phaseInc;
};

Oscillator osc1;
Oscillator osc2;

// ==== Wavetable buffers (256 samples) ====
int8_t waveSine[256];
int8_t waveTri[256];
int8_t waveSaw[256];
int8_t waveSquare[256];

// ==== Audio constants ====
const int PIN_AUDIO_OUT = 9; // PWM output using Timer1
const uint32_t SAMPLE_RATE = 16000;

uint16_t freqToPhaseInc(float freq) {
  return (uint16_t)((freq * 65536.0) / SAMPLE_RATE + 0.5);
}

void generateWavetables() {
  for (int i = 0; i < 256; i++) {
    float ph = (float)i / 256.0;
    waveSine[i] = (int8_t)(127.0 * sinf(2 * M_PI * ph));
    waveTri[i]  = (int8_t)(127.0 * (ph < 0.5 ? (4*ph - 1) : (-4*ph + 3)));
    waveSaw[i]  = (int8_t)(127.0 * (2*ph - 1));
    waveSquare[i] = (ph < 0.5) ? 127 : -127;
  }
}

void updateOscillatorParams(Oscillator &osc) {
  osc.freq = calcFreq(baseFreq, osc.octaveIndex);
  osc.phaseInc = freqToPhaseInc(osc.freq);
}

void setupTimer1() {
  pinMode(PIN_AUDIO_OUT, OUTPUT);
  TCCR1A = _BV(COM1A1) | _BV(WGM11);
  TCCR1B = _BV(WGM12) | _BV(WGM13) | _BV(CS10);
  ICR1 = F_CPU / SAMPLE_RATE - 1;
  OCR1A = 128;
  TIMSK1 = _BV(TOIE1);
}

ISR(TIMER1_OVF_vect) {
  bool drone = (digitalRead(PIN_MODE_SWITCH) == LOW);
  digitalWrite(PIN_GATE_OUT, drone ? HIGH : LOW);
  if (!drone) {
    OCR1A = 128;
    return;
  }

  osc1.phase += osc1.phaseInc;
  osc2.phase += osc2.phaseInc;

  int8_t s1 = 0;
  int8_t s2 = 0;

  if (osc1.type == NOISE) {
    s1 = random(-127, 128);
  } else {
    uint8_t idx = osc1.phase >> 8;
    int8_t *table = getWaveTable(osc1.type);
    s1 = table[idx];
  }

  if (osc2.type == NOISE) {
    s2 = random(-127, 128);
  } else {
    uint8_t idx = osc2.phase >> 8;
    int8_t *table = getWaveTable(osc2.type);
    s2 = table[idx];
  }

  long mix = (long)s1 * osc1.volume + (long)s2 * osc2.volume;
  mix /= (2 * 1023);
  int out = (int)mix + 128;
  OCR1A = constrain(out, 0, 255);
}

// ==== Pins ====
const int PIN_OSC1_VOLUME = A1;
const int PIN_OSC2_VOLUME = A2;
const int PIN_MODE_SWITCH = 10;
const int PIN_GATE_OUT = 11;

// ==== Encoders ====
RotaryEncoder encOsc1Type(2, 3);
RotaryEncoder encOsc1Range(4, 5);
RotaryEncoder encOsc2Type(6, 7);
RotaryEncoder encOsc2Range(8, 9);

// === Rotary encoder constants ===
const int ENCODER_PULSES_PER_REV = 20;   // mechanical pulses per revolution
const int ENCODER_STEPS         = 5;    // desired discrete steps per revolution
const int PULSES_PER_STEP       = ENCODER_PULSES_PER_REV / ENCODER_STEPS; // 4

// Accumulators for 1/4-step logic
int lastPosType1 = 0, stepAccumType1 = 0;
int lastPosRange1 = 0, stepAccumRange1 = 0;
int lastPosType2 = 0, stepAccumType2 = 0;
int lastPosRange2 = 0, stepAccumRange2 = 0;

// ==== Octaves ====
int octaveSteps[5] = { -3, -2, 0, 1, 2 };
float baseFreq = 440.0;

// ==== Returns pointer to oscillator according to type ====
// Map oscillator type to wavetable
int8_t* getWaveTable(Waveform type) {
  switch (type) {
    case SINE:     return waveSine;
    case TRIANGLE: return waveTri;
    case SQUARE:   return waveSquare;
    case SAW:      return waveSaw;
    default:       return waveSine;
  }
}

// ==== Frequency from octave ====
float calcFreq(float base, int octIndex) {
  return base * pow(2, octaveSteps[octIndex]);
}

// ==== Encoder with 20-pulse rotary ====
void updateEncoderDiv4(RotaryEncoder &enc, int &lastPos, int &accum, int &val, int maxVal) {
  int pos = enc.getPosition();
  int diff = pos - lastPos;
  if (diff != 0) {
    accum += diff;
    lastPos = pos;

    if (abs(accum) >= PULSES_PER_STEP) {
      if (accum > 0) val++;
      else val--;
      accum = 0;

      // wrap value into range 0..(maxVal-1)
      if (val < 0) val += maxVal;
      if (val >= maxVal) val -= maxVal;
    }
  }
}

// ==== Setup ====
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("2-oscillator synth without Mozzi");

  pinMode(PIN_MODE_SWITCH, INPUT_PULLUP);
  pinMode(PIN_GATE_OUT, OUTPUT);
  digitalWrite(PIN_GATE_OUT, LOW);

  osc1.type = SINE;  osc1.octaveIndex = 2;  osc1.volume = 1023;  osc1.phase = 0;
  osc2.type = SINE;  osc2.octaveIndex = 2;  osc2.volume = 1023;  osc2.phase = 0;

  generateWavetables();
  updateOscillatorParams(osc1);
  updateOscillatorParams(osc2);

  setupTimer1();
  sei();
}


// ==== Loop ====
void loop() {
  // Call tick on encoders as often as possible to catch all transitions
  encOsc1Type.tick();
  encOsc1Range.tick();
  encOsc2Type.tick();
  encOsc2Range.tick();

  // Update encoder-driven parameters
  updateEncoderDiv4(encOsc1Type, lastPosType1, stepAccumType1, (int&)osc1.type, WAVE_COUNT);
  updateEncoderDiv4(encOsc2Type, lastPosType2, stepAccumType2, (int&)osc2.type, WAVE_COUNT);

  updateEncoderDiv4(encOsc1Range, lastPosRange1, stepAccumRange1, osc1.octaveIndex, 5);
  updateEncoderDiv4(encOsc2Range, lastPosRange2, stepAccumRange2, osc2.octaveIndex, 5);

  updateOscillatorParams(osc1);
  updateOscillatorParams(osc2);

  osc1.volume = analogRead(PIN_OSC1_VOLUME);
  osc2.volume = analogRead(PIN_OSC2_VOLUME);
}


