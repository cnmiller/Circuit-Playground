// Audio level visualizer for Adafruit Circuit Playground: uses the
// built-in microphone, 10x NeoPixels for display.  Like the FFT example,
// the real work is done in the Circuit Playground library via the 'mic'
// object; this code is almost entirely just dressing up the output with
// a lot of averaging and scaling math and colors.

#include <Adafruit_CircuitPlayground.h>
#include <Wire.h>
#include <SPI.h>

// GLOBAL STUFF ------------------------------------------------------------

// To keep the display 'lively,' an upper and lower range of volume
// levels are dynamically adjusted based on recent audio history, and
// the graph is fit into this range.
#define  FRAMES 8
uint16_t lvl[FRAMES],           // Audio level for the prior #FRAMES frames
         avgLo  = 6,            // Audio volume lower end of range
         avgHi  = 512,          // Audio volume upper end of range
         sum    = 256 * FRAMES; // Sum of lvl[] array
uint8_t  lvlIdx = 0;            // Counter into lvl[] array
int16_t  peak   = 0;            // Falling dot shows recent max
int8_t   peakV  = 0;            // Velocity of peak dot

// SETUP FUNCTION - runs once ----------------------------------------------

void setup() {
  CircuitPlayground.begin();
  CircuitPlayground.setBrightness(128);
  CircuitPlayground.clearPixels();

  for(uint8_t i=0; i<FRAMES; i++) lvl[i] = 256;
  }

const uint8_t PROGMEM
  reds[]   = { 0x9A, 0x75, 0x00, 0x00, 0x00, 0x65, 0x84, 0x9A, 0xAD, 0xAD },
  greens[] = { 0x00, 0x00, 0x00, 0x87, 0xB1, 0x9E, 0x87, 0x66, 0x00, 0x00 },
  blues[]  = { 0x95, 0xD5, 0xFF, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
  gamma8[] = { // Gamma correction improves the appearance of midrange colors
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x06,
    0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09,
    0x0A, 0x0A, 0x0A, 0x0B, 0x0B, 0x0B, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0E,
    0x0E, 0x0F, 0x0F, 0x10, 0x10, 0x11, 0x11, 0x12, 0x12, 0x13, 0x13, 0x14,
    0x14, 0x15, 0x15, 0x16, 0x16, 0x17, 0x18, 0x18, 0x19, 0x19, 0x1A, 0x1B,
    0x1B, 0x1C, 0x1D, 0x1D, 0x1E, 0x1F, 0x1F, 0x20, 0x21, 0x22, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2A, 0x2B, 0x2C, 0x2D,
    0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x44, 0x45, 0x46,
    0x47, 0x48, 0x49, 0x4B, 0x4C, 0x4D, 0x4E, 0x50, 0x51, 0x52, 0x54, 0x55,
    0x56, 0x58, 0x59, 0x5A, 0x5C, 0x5D, 0x5E, 0x60, 0x61, 0x63, 0x64, 0x66,
    0x67, 0x69, 0x6A, 0x6C, 0x6D, 0x6F, 0x70, 0x72, 0x73, 0x75, 0x77, 0x78,
    0x7A, 0x7C, 0x7D, 0x7F, 0x81, 0x82, 0x84, 0x86, 0x88, 0x89, 0x8B, 0x8D,
    0x8F, 0x91, 0x92, 0x94, 0x96, 0x98, 0x9A, 0x9C, 0x9E, 0xA0, 0xA2, 0xA4,
    0xA6, 0xA8, 0xAA, 0xAC, 0xAE, 0xB0, 0xB2, 0xB4, 0xB6, 0xB8, 0xBA, 0xBC,
    0xBF, 0xC1, 0xC3, 0xC5, 0xC7, 0xCA, 0xCC, 0xCE, 0xD1, 0xD3, 0xD5, 0xD7,
    0xDA, 0xDC, 0xDF, 0xE1, 0xE3, 0xE6, 0xE8, 0xEB, 0xED, 0xF0, 0xF2, 0xF5,
    0xF7, 0xFA, 0xFC, 0xFF };

// LOOP FUNCTION - runs over and over - does animation ---------------------

void loop() {
uint8_t  i, r, g, b;
  uint16_t minLvl, maxLvl, a, scaled;
  int16_t  p;

  p           = CircuitPlayground.mic.soundPressureLevel(10); // 10 ms
  p           = map(p, 56, 140, 0, 350); // Scale to 0-350 (may overflow)
  a           = constrain(p, 0, 350);    // Clip to 0-350 range
  sum        -= lvl[lvlIdx];
  lvl[lvlIdx] = a;
  sum        += a;                              // Sum of lvl[] array
  minLvl = maxLvl = lvl[0];                     // Calc min, max of lvl[]...
  for(i=1; i<FRAMES; i++) {
    if(lvl[i] < minLvl)      minLvl = lvl[i];
    else if(lvl[i] > maxLvl) maxLvl = lvl[i];
  }

  // Keep some minimum distance between min & max levels,
  // else the display gets "jumpy."
  if((maxLvl - minLvl) < 40) {
    maxLvl = (minLvl < (512-40)) ? minLvl + 40 : 512;
  }
  avgLo = (avgLo * 7 + minLvl + 2) / 8; // Dampen min/max levels
  avgHi = (maxLvl >= avgHi) ?           // (fake rolling averages)
    (avgHi *  3 + maxLvl + 1) /  4 :    // Fast rise
    (avgHi * 31 + maxLvl + 8) / 32;     // Slow decay

  a = sum / FRAMES; // Average of lvl[] array
  if(a <= avgLo) {  // Below min?
    scaled = 0;     // Bargraph = zero
  } else {          // Else scale to fixed-point coordspace 0-2560
    scaled = 2560L * (a - avgLo) / (avgHi - avgLo);
    if(scaled > 2560) scaled = 2560;
  }
  if(scaled >= peak) {            // New peak
    peakV = (scaled - peak) / 4;  // Give it an upward nudge
    peak  = scaled;
  }

  uint8_t  whole  = scaled / 256,    // Full-brightness pixels (0-10)
           frac   = scaled & 255;    // Brightness of fractional pixel
  int      whole2 = peak / 256,      // Index of peak pixel
           frac2  = peak & 255;      // Between-pixels position of peak
  uint16_t a1, a2;                   // Scaling factors for color blending

  for(i=0; i<10; i++) {              // For each NeoPixel...
    if(i <= whole) {                 // In currently-lit area?
      r = pgm_read_byte(&reds[i]),   // Look up pixel color
      g = pgm_read_byte(&greens[i]),
      b = pgm_read_byte(&blues[i]);
      if(i == whole) {               // Fraction pixel at top of range?
        a1 = (uint16_t)frac + 1;     // Fade toward black
        r  = (r * a1) >> 8;
        g  = (g * a1) >> 8;
        b  = (b * a1) >> 8;
      }
    } else {
      r = g = b = 0;                 // In unlit area
    }
    // Composite the peak pixel atop whatever else is happening...
    if(i == whole2) {                // Peak pixel?
      a1 = 256 - frac2;              // Existing pixel blend factor 1-256
      a2 = frac2 + 1;                // Peak pixel blend factor 1-256
      r  = ((r * a1) + (0x84 * a2)) >> 8; // Will
      g  = ((g * a1) + (0x87 * a2)) >> 8; // it
      b  = ((b * a1) + (0xC3 * a2)) >> 8; // blend?
    } else if(i == (whole2-1)) {     // Just below peak pixel
      a1 = frac2 + 1;                // Opposite blend ratios to above,
      a2 = 256 - frac2;              // but same idea
      r  = ((r * a1) + (0x84 * a2)) >> 8;
      g  = ((g * a1) + (0x87 * a2)) >> 8;
      b  = ((b * a1) + (0xC3 * a2)) >> 8;
    }
    CircuitPlayground.strip.setPixelColor(i,
      pgm_read_byte(&gamma8[r]),
      pgm_read_byte(&gamma8[g]),
      pgm_read_byte(&gamma8[b]));
  }
  CircuitPlayground.strip.show();

  peak += peakV;
  if(peak <= 0) {
    peak  = 0;
    peakV = 0;
  } else if(peakV >= -126) {
    peakV -= 2;
  }

  if(++lvlIdx >= FRAMES) lvlIdx = 0;
  if(CircuitPlayground.rightButton()){
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(880, 154.168098592);
      CircuitPlayground.playTone(1174, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(987, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(987, 154.168098592);
      CircuitPlayground.playTone(932, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(554, 154.168098592);
      CircuitPlayground.playTone(493, 154.168098592);
      CircuitPlayground.playTone(466, 154.168098592);
      CircuitPlayground.playTone(440, 205.557464789);
      CircuitPlayground.playTone(440, 205.557464789);
      CircuitPlayground.playTone(493, 205.557464789);
      CircuitPlayground.playTone(493, 205.557464789);
      CircuitPlayground.playTone(440, 411.114929577);
      CircuitPlayground.playTone(830, 154.168098592);
      CircuitPlayground.playTone(880, 154.168098592);
      CircuitPlayground.playTone(1318, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1760, 154.168098592);
      CircuitPlayground.playTone(2637, 154.168098592);
      CircuitPlayground.playTone(293, 411.114929577);
      CircuitPlayground.playTone(293, 205.557464789);
      CircuitPlayground.playTone(293, 205.557464789);
      CircuitPlayground.playTone(293, 205.557464789);
      CircuitPlayground.playTone(349, 1644.45971831);
      CircuitPlayground.playTone(587, 205.557464789);
      CircuitPlayground.playTone(587, 205.557464789);
      CircuitPlayground.playTone(493, 205.557464789);
      CircuitPlayground.playTone(493, 205.557464789);
      CircuitPlayground.playTone(554, 411.114929577);
      CircuitPlayground.playTone(554, 411.114929577);
      CircuitPlayground.playTone(554, 411.114929577);
      CircuitPlayground.playTone(554, 205.557464789);
      CircuitPlayground.playTone(659, 205.557464789);
      CircuitPlayground.playTone(739, 1438.90225352);
      CircuitPlayground.playTone(739, 411.114929577);
      CircuitPlayground.playTone(698, 205.557464789);
      CircuitPlayground.playTone(698, 205.557464789);
      CircuitPlayground.playTone(739, 205.557464789);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(880, 154.168098592);
      CircuitPlayground.playTone(1174, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(987, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(987, 154.168098592);
      CircuitPlayground.playTone(932, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(554, 154.168098592);
      CircuitPlayground.playTone(493, 154.168098592);
      CircuitPlayground.playTone(466, 154.168098592);
      CircuitPlayground.playTone(440, 205.557464789);
      CircuitPlayground.playTone(440, 205.557464789);
      CircuitPlayground.playTone(493, 205.557464789);
      CircuitPlayground.playTone(493, 205.557464789);
      CircuitPlayground.playTone(440, 411.114929577);
      CircuitPlayground.playTone(830, 154.168098592);
      CircuitPlayground.playTone(880, 154.168098592);
      CircuitPlayground.playTone(1318, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1760, 154.168098592);
      CircuitPlayground.playTone(2637, 154.168098592);
      CircuitPlayground.playTone(293, 411.114929577);
      CircuitPlayground.playTone(293, 205.557464789);
      CircuitPlayground.playTone(293, 205.557464789);
      CircuitPlayground.playTone(293, 205.557464789);
      CircuitPlayground.playTone(349, 1644.45971831);
      CircuitPlayground.playTone(587, 205.557464789);
      CircuitPlayground.playTone(587, 205.557464789);
      CircuitPlayground.playTone(493, 205.557464789);
      CircuitPlayground.playTone(493, 205.557464789);
      CircuitPlayground.playTone(554, 411.114929577);
      CircuitPlayground.playTone(554, 411.114929577);
      CircuitPlayground.playTone(554, 411.114929577);
      CircuitPlayground.playTone(554, 205.557464789);
      CircuitPlayground.playTone(659, 205.557464789);
      CircuitPlayground.playTone(739, 1438.90225352);
      CircuitPlayground.playTone(739, 411.114929577);
      CircuitPlayground.playTone(698, 205.557464789);
      CircuitPlayground.playTone(698, 205.557464789);
      CircuitPlayground.playTone(739, 205.557464789);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(739, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1975, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1108, 154.168098592);
      CircuitPlayground.playTone(1864, 154.168098592);
      CircuitPlayground.playTone(1661, 154.168098592);
      CircuitPlayground.playTone(1479, 154.168098592);
  }
  if(CircuitPlayground.leftButton()){
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(138, 175.5950625);
      CircuitPlayground.playTone(138, 175.5950625);
      CircuitPlayground.playTone(138, 175.5950625);
      CircuitPlayground.playTone(138, 175.5950625);
      CircuitPlayground.playTone(65, 175.5950625);
      CircuitPlayground.playTone(138, 175.5950625);
      CircuitPlayground.playTone(138, 175.5950625);
      CircuitPlayground.playTone(138, 175.5950625);
      CircuitPlayground.playTone(138, 175.5950625);
      CircuitPlayground.playTone(261, 175.5950625);
      CircuitPlayground.playTone(739, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(130, 178.57125);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(622, 175.5950625);
      CircuitPlayground.playTone(622, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(51, 175.5950625);
      CircuitPlayground.playTone(1108, 175.5950625);
      CircuitPlayground.playTone(987, 175.5950625);
      CircuitPlayground.playTone(830, 175.5950625);
      CircuitPlayground.playTone(739, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(554, 175.5950625);
      CircuitPlayground.playTone(554, 175.5950625);
      CircuitPlayground.playTone(493, 175.5950625);
      CircuitPlayground.playTone(493, 175.5950625);
      CircuitPlayground.playTone(51, 175.5950625);
      CircuitPlayground.playTone(1108, 175.5950625);
      CircuitPlayground.playTone(987, 175.5950625);
      CircuitPlayground.playTone(830, 175.5950625);
      CircuitPlayground.playTone(739, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(622, 175.5950625);
      CircuitPlayground.playTone(622, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(51, 175.5950625);
      CircuitPlayground.playTone(1108, 175.5950625);
      CircuitPlayground.playTone(987, 175.5950625);
      CircuitPlayground.playTone(830, 175.5950625);
      CircuitPlayground.playTone(739, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(554, 175.5950625);
      CircuitPlayground.playTone(554, 175.5950625);
      CircuitPlayground.playTone(493, 175.5950625);
      CircuitPlayground.playTone(493, 175.5950625);
      CircuitPlayground.playTone(51, 175.5950625);
      CircuitPlayground.playTone(1108, 175.5950625);
      CircuitPlayground.playTone(987, 175.5950625);
      CircuitPlayground.playTone(830, 175.5950625);
      CircuitPlayground.playTone(739, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(622, 175.5950625);
      CircuitPlayground.playTone(622, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(65, 86.3094375);
      CircuitPlayground.playTone(466, 86.3094375);
      CircuitPlayground.playTone(1108, 89.285625);
      CircuitPlayground.playTone(987, 175.5950625);
      CircuitPlayground.playTone(830, 175.5950625);
      CircuitPlayground.playTone(739, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(554, 175.5950625);
      CircuitPlayground.playTone(554, 175.5950625);
      CircuitPlayground.playTone(493, 175.5950625);
      CircuitPlayground.playTone(493, 175.5950625);
      CircuitPlayground.playTone(38, 38.6904375);
      CircuitPlayground.playTone(38, 44.6428125);
      CircuitPlayground.playTone(65, 41.666625);
      CircuitPlayground.playTone(38, 62.4999375);
      CircuitPlayground.playTone(466, 23.8095);
      CircuitPlayground.playTone(1108, 89.285625);
      CircuitPlayground.playTone(77, 92.2618125);
      CircuitPlayground.playTone(987, 83.33325);
      CircuitPlayground.playTone(830, 175.5950625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(739, 89.285625);
      CircuitPlayground.playTone(932, 23.8095);
      CircuitPlayground.playTone(92, 62.4999375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 26.7856875);
      CircuitPlayground.playTone(92, 59.52375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 35.71425);
      CircuitPlayground.playTone(92, 50.5951875);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 38.6904375);
      CircuitPlayground.playTone(92, 47.619);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(622, 89.285625);
      CircuitPlayground.playTone(932, 38.6904375);
      CircuitPlayground.playTone(92, 47.619);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(65, 86.3094375);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(92, 56.5475625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(987, 89.285625);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(92, 56.5475625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(739, 89.285625);
      CircuitPlayground.playTone(932, 35.71425);
      CircuitPlayground.playTone(92, 50.5951875);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 38.6904375);
      CircuitPlayground.playTone(92, 47.619);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 35.71425);
      CircuitPlayground.playTone(92, 50.5951875);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(92, 56.5475625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(554, 89.285625);
      CircuitPlayground.playTone(932, 32.7380625);
      CircuitPlayground.playTone(92, 53.571375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(493, 89.285625);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(92, 56.5475625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(65, 86.3094375);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(92, 56.5475625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(987, 89.285625);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(92, 56.5475625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(739, 89.285625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 26.7856875);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 127.9760625);
      CircuitPlayground.playTone(659, 53.571375);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 157.7379375);
      CircuitPlayground.playTone(622, 83.33325);
      CircuitPlayground.playTone(622, 5.952375);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 110.1189375);
      CircuitPlayground.playTone(51, 35.71425);
      CircuitPlayground.playTone(1108, 175.5950625);
      CircuitPlayground.playTone(987, 139.8808125);
      CircuitPlayground.playTone(830, 62.4999375);
      CircuitPlayground.playTone(739, 175.5950625);
      CircuitPlayground.playTone(659, 166.6665);
      CircuitPlayground.playTone(659, 92.2618125);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 119.0475);
      CircuitPlayground.playTone(659, 44.6428125);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(554, 148.809375);
      CircuitPlayground.playTone(554, 74.4046875);
      CircuitPlayground.playTone(493, 175.5950625);
      CircuitPlayground.playTone(493, 175.5950625);
      CircuitPlayground.playTone(51, 101.190375);
      CircuitPlayground.playTone(1108, 26.7856875);
      CircuitPlayground.playTone(987, 175.5950625);
      CircuitPlayground.playTone(830, 130.95225);
      CircuitPlayground.playTone(739, 53.571375);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 157.7379375);
      CircuitPlayground.playTone(659, 8.9285625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 110.1189375);
      CircuitPlayground.playTone(659, 35.71425);
      CircuitPlayground.playTone(622, 175.5950625);
      CircuitPlayground.playTone(622, 139.8808125);
      CircuitPlayground.playTone(659, 65.476125);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(51, 166.6665);
      CircuitPlayground.playTone(1108, 92.2618125);
      CircuitPlayground.playTone(987, 17.857125);
      CircuitPlayground.playTone(830, 175.5950625);
      CircuitPlayground.playTone(739, 122.0236875);
      CircuitPlayground.playTone(659, 44.6428125);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 74.4046875);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 175.5950625);
      CircuitPlayground.playTone(659, 101.190375);
      CircuitPlayground.playTone(554, 26.7856875);
      CircuitPlayground.playTone(554, 175.5950625);
      CircuitPlayground.playTone(493, 130.95225);
      CircuitPlayground.playTone(493, 56.5475625);
      CircuitPlayground.playTone(51, 175.5950625);
      CircuitPlayground.playTone(1108, 157.7379375);
      CircuitPlayground.playTone(987, 83.33325);
      CircuitPlayground.playTone(830, 8.9285625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(739, 89.285625);
      CircuitPlayground.playTone(932, 23.8095);
      CircuitPlayground.playTone(92, 62.4999375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 26.7856875);
      CircuitPlayground.playTone(92, 59.52375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 35.71425);
      CircuitPlayground.playTone(92, 50.5951875);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 38.6904375);
      CircuitPlayground.playTone(92, 47.619);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(622, 89.285625);
      CircuitPlayground.playTone(932, 38.6904375);
      CircuitPlayground.playTone(92, 47.619);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(65, 86.3094375);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(92, 56.5475625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(987, 89.285625);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(92, 56.5475625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(739, 89.285625);
      CircuitPlayground.playTone(932, 23.8095);
      CircuitPlayground.playTone(92, 62.4999375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 26.7856875);
      CircuitPlayground.playTone(92, 59.52375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 35.71425);
      CircuitPlayground.playTone(92, 50.5951875);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(659, 89.285625);
      CircuitPlayground.playTone(932, 38.6904375);
      CircuitPlayground.playTone(92, 47.619);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(554, 89.285625);
      CircuitPlayground.playTone(932, 38.6904375);
      CircuitPlayground.playTone(92, 47.619);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(493, 89.285625);
      CircuitPlayground.playTone(932, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(65, 86.3094375);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(92, 56.5475625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(987, 89.285625);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(92, 56.5475625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(277, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(138, 41.666625);
      CircuitPlayground.playTone(739, 44.6428125);
      CircuitPlayground.playTone(932, 23.8095);
      CircuitPlayground.playTone(277, 20.8333125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(415, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(207, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(277, 44.6428125);
      CircuitPlayground.playTone(659, 41.666625);
      CircuitPlayground.playTone(932, 26.7856875);
      CircuitPlayground.playTone(415, 17.857125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(554, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(415, 41.666625);
      CircuitPlayground.playTone(659, 44.6428125);
      CircuitPlayground.playTone(932, 35.71425);
      CircuitPlayground.playTone(554, 5.952375);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(554, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(415, 86.3094375);
      CircuitPlayground.playTone(932, 38.6904375);
      CircuitPlayground.playTone(554, 5.952375);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(329, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(277, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(138, 41.666625);
      CircuitPlayground.playTone(622, 44.6428125);
      CircuitPlayground.playTone(932, 38.6904375);
      CircuitPlayground.playTone(277, 5.952375);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(415, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(207, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(277, 44.6428125);
      CircuitPlayground.playTone(659, 41.666625);
      CircuitPlayground.playTone(932, 41.666625);
      CircuitPlayground.playTone(415, 2.9761875);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(554, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(415, 41.666625);
      CircuitPlayground.playTone(65, 44.6428125);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(554, 11.90475);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(554, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(415, 86.3094375);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(554, 14.8809375);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(329, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(277, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(138, 41.666625);
      CircuitPlayground.playTone(739, 44.6428125);
      CircuitPlayground.playTone(932, 35.71425);
      CircuitPlayground.playTone(277, 8.9285625);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(415, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(207, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(277, 44.6428125);
      CircuitPlayground.playTone(659, 41.666625);
      CircuitPlayground.playTone(932, 38.6904375);
      CircuitPlayground.playTone(415, 5.952375);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(554, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(415, 41.666625);
      CircuitPlayground.playTone(659, 44.6428125);
      CircuitPlayground.playTone(932, 35.71425);
      CircuitPlayground.playTone(554, 5.952375);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(554, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(415, 86.3094375);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(554, 14.8809375);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(329, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(277, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(138, 41.666625);
      CircuitPlayground.playTone(554, 44.6428125);
      CircuitPlayground.playTone(932, 32.7380625);
      CircuitPlayground.playTone(277, 11.90475);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(415, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(207, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(277, 44.6428125);
      CircuitPlayground.playTone(493, 41.666625);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(415, 14.8809375);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(554, 41.666625);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(415, 41.666625);
      CircuitPlayground.playTone(65, 44.6428125);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(554, 11.90475);
      CircuitPlayground.playTone(92, 44.6428125);
      CircuitPlayground.playTone(92, 86.3094375);
      CircuitPlayground.playTone(554, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(415, 86.3094375);
      CircuitPlayground.playTone(932, 29.761875);
      CircuitPlayground.playTone(554, 14.8809375);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(329, 44.6428125);
      CircuitPlayground.playTone(92, 41.666625);
      CircuitPlayground.playTone(277, 44.6428125);
      CircuitPlayground.playTone(138, 41.666625);
      CircuitPlayground.playTone(739, 44.6428125);
      CircuitPlayground.playTone(277, 44.6428125);
      CircuitPlayground.playTone(415, 41.666625);
      CircuitPlayground.playTone(659, 44.6428125);
      CircuitPlayground.playTone(207, 41.666625);
      CircuitPlayground.playTone(69, 44.6428125);
      CircuitPlayground.playTone(277, 44.6428125);
      CircuitPlayground.playTone(659, 41.666625);
      CircuitPlayground.playTone(415, 44.6428125);
      CircuitPlayground.playTone(554, 86.3094375);
      CircuitPlayground.playTone(554, 41.666625);
      CircuitPlayground.playTone(415, 41.666625);
      CircuitPlayground.playTone(659, 44.6428125);
      CircuitPlayground.playTone(554, 41.666625);
      CircuitPlayground.playTone(880, 86.3094375);
      CircuitPlayground.playTone(554, 44.6428125);
      CircuitPlayground.playTone(69, 41.666625);
      CircuitPlayground.playTone(415, 86.3094375);
      CircuitPlayground.playTone(554, 44.6428125);
  }
}


