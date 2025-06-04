#include <avr/pgmspace.h>
#include <Wire.h>

// Precomputed 8-bit sine wave table (0-255)
const uint8_t sineTable[256] PROGMEM = {
  127, 130, 133, 136, 139, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 173,
  176, 179, 182, 184, 187, 190, 193, 195, 198, 200, 203, 205, 208, 210, 213, 215,
  217, 219, 221, 224, 226, 228, 229, 231, 233, 235, 236, 238, 239, 241, 242, 244,
  245, 246, 247, 248, 249, 250, 251, 251, 252, 253, 253, 254, 254, 254, 254, 254,
  255, 254, 254, 254, 254, 254, 253, 253, 252, 251, 251, 250, 249, 248, 247, 246,
  245, 244, 242, 241, 239, 238, 236, 235, 233, 231, 229, 228, 226, 224, 221, 219,
  217, 215, 213, 210, 208, 205, 203, 200, 198, 195, 193, 190, 187, 184, 182, 179,
  176, 173, 170, 167, 164, 161, 158, 155, 152, 149, 146, 143, 139, 136, 133, 130,
  127, 124, 121, 118, 115, 111, 108, 105, 102,  99,  96,  93,  90,  87,  84,  81,
   78,  75,  72,  70,  67,  64,  61,  59,  56,  54,  51,  49,  46,  44,  41,  39,
   37,  35,  33,  30,  28,  26,  25,  23,  21,  19,  18,  16,  15,  13,  12,  10,
    9,   8,   7,   6,   5,   4,   3,   3,   2,   1,   1,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   1,   1,   2,   3,   3,   4,   5,   6,   7,   8,
    9,  10,  12,  13,  15,  16,  18,  19,  21,  23,  25,  26,  28,  30,  33,  35,
   37,  39,  41,  44,  46,  49,  51,  54,  56,  59,  61,  64,  67,  70,  72,  75,
   78,  81,  84,  87,  90,  93,  96,  99, 102, 105, 108, 111, 115, 118, 121, 124
};

// I2C address of the DAC (e.g. MCP4725)
const uint8_t dacAddr = 0x60;

// Sample rate derived from Timer1 (16 MHz / (1 * (OCR1A + 1)))
const uint32_t sampleRate = 62500; // with OCR1A = 255 and prescaler = 1

// Desired output frequency in Hz
const float outputFreq = 1000.0;

volatile uint16_t phaseAcc = 0;
uint16_t phaseIncrement = (uint16_t)(outputFreq * 65536.0 / sampleRate);

ISR(TIMER1_COMPA_vect) {
  uint8_t index = phaseAcc >> 8; // use upper 8 bits as table index
  uint16_t value = ((uint16_t)pgm_read_byte(&sineTable[index])) << 4; // 12-bit
  Wire.beginTransmission(dacAddr);
  Wire.write(0x40);                // write DAC register
  Wire.write(value >> 8);          // high byte
  Wire.write(value & 0xFF);        // low byte
  Wire.endTransmission();
  phaseAcc += phaseIncrement;
}

void setup() {
  // Initialize I2C for the DAC
  Wire.begin();

  // Setup Timer1 to trigger ISR at sampleRate
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS10); // CTC mode, no prescaler
  OCR1A = 255;
  TIMSK1 = _BV(OCIE1A);
  interrupts();
}

void loop() {
  // nothing needed here; waveform is generated in the interrupt
}
