// POV LED poi sketch.  Uses the following Adafruit parts (X2 for two poi):
//
// - Trinket 5V or 3V (adafruit.com/product/1501 or 1500) (NOT Pro Trinket)
// - 150 mAh LiPoly battery (1317)
// - LiPoly backpack (2124)
// - Medium vibration sensor switch (2384)
//
// See comments in code re: vibration switch and other optional parts.
// Needs Adafruit_DotStar library: github.com/adafruit/Adafruit_DotStar
//
// Use 'soda bottle preform' for enclosure w/5.25" (133 mm) inside depth.
// 3D-printable cap and insert can be downloaded from Thingiverse:
// (add link here)
// Also needs leash - e.g. paracord, or fancy ones avail from flowtoys.com

// TO DO: Change this to use hardware-assist SPI: Pin 1 = data, 2 = clock,
// vibration switch on pin 0.

#include <Arduino.h>
#include <Adafruit_DotStar.h>
#include <avr/power.h>
#include <avr/sleep.h>
// #include <SPI.h> // Enable this line on Pro Trinket

#include "graphics.h" // Graphics data is contained in this header file.
// It's generated using the 'convert.py' Python script.  Various image
// formats are supported, trading off color fidelity for PROGMEM space
// (particularly limited on Trinket).  Handles 1-, 4- and 8-bit-per-pixel
// palette-based images, plus 24-bit truecolor.  1- and 4-bit palettes can
// be altered in RAM while running to provide additional colors, but be
// mindful of peak & average current draw if you do that!  Power limiting
// is normally done in convert.py (keeps this code relatively small & fast).

#define LED_DATA_PIN  0
#define LED_CLOCK_PIN 1

// The vibration switch (aligned perpendicular to leash) is used as a
// poor man's accelerometer -- poi lights only when spinning, saving some
// power.  This requires a certain minimum spin rate, and if you're doing
// very mellow spins, it might not be enough.  There's a 'fast' vibration
// switch you could try, but it's quite twitchy...might be better in this
// case just to comment out this line to have the poi run always-on:
#define MOTION_PIN 2

// Experimental: powering down DotStar strip should conserve much more power
// when idle.  Use PNP transistor (w/1K resistor) as a 'high side' switch.
// Very little space, requires creative free-wiring.  Needs MOTION_PIN also.
// #define POWER_PIN 3

// Optional: image select using tactile button.
#define SELECT_PIN 4

#define SLEEP_TIME 2000 // Not-spinning time before sleep, in milliseconds

Adafruit_DotStar strip = Adafruit_DotStar(NUM_LEDS, // Defined in graphics.h
  LED_DATA_PIN, LED_CLOCK_PIN, DOTSTAR_GBR);

// -------------------------------------------------------------------------

void setup() {
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1);   // Enable 16 MHz on Trinket
#endif

  // Set all pins to input w/pullup by default
  for(uint8_t i=0; i<20; i++) pinMode(i, INPUT_PULLUP);

#ifdef POWER_PIN
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);      // Power-on LED strip
#endif
  strip.begin();

  imageInit();

#ifdef MOTION_PIN
  sleep();                           // Jump right into sleep mode
#endif
}

// GLOBAL STATE STUFF ------------------------------------------------------

#ifdef SELECT_PIN
uint8_t  debounce    = 0;  // Debounce counter for image select pin
#endif
uint32_t prev        = 0L; // Used for time measurement
uint8_t  imageNumber = 0;  // Current image being displayed
uint8_t  imageType;        // Image type: PALETTE[1,4,8] or TRUECOLOR
uint8_t *imagePalette;     // -> palette data in PROGMEM
uint8_t *imagePixels;      // -> pixel data in PROGMEM
uint16_t imageLines;       // Number of lines in active image
uint16_t imageLine;        // Current line number in image
uint8_t  palette[16][3];   // RAM-based color table for 1- or 4-bit images

void imageInit() { // Initialize global image state for current imageNumber
  imageType    = pgm_read_byte(&images[imageNumber].type);
  imageLines   = pgm_read_word(&images[imageNumber].lines);
  imageLine    = 0;
  imagePalette = (uint8_t *)pgm_read_word(&images[imageNumber].palette);
  imagePixels  = (uint8_t *)pgm_read_word(&images[imageNumber].pixels);
  // 1- and 4-bit images have their color palette loaded into RAM both for
  // faster access and to allow dynamic color changing.  Not done w/8-bit
  // because that would require inordinate RAM.
  if(imageType == PALETTE1)      memcpy_P(palette, imagePalette,  2 * 3);
  else if(imageType == PALETTE4) memcpy_P(palette, imagePalette, 16 * 3);
}

// MAIN LOOP ---------------------------------------------------------------

void loop() {
#ifdef MOTION_PIN
  // Tried to do this with watchdog timer but encountered gas pains, so...
  uint32_t t = millis();               // Current time, milliseconds
  if(!digitalRead(MOTION_PIN)) {       // Vibration switch pulled down?
    prev = t;                          // Yes, reset timer
  } else if((t - prev) > SLEEP_TIME) { // No, SLEEP_TIME elapsed w/no switch?
    sleep();                           // Power down
    prev = t;                          // Reset timer on wake
  }
#endif

#ifdef SELECT_PIN
  if(!digitalRead(SELECT_PIN)) {       // Image select pressed?
    if(debounce++ >= 10) {
      if(++imageNumber >= NUM_IMAGES) imageNumber = 0;
      imageInit();                     // Switch to next image
      while(!digitalRead(SELECT_PIN)); // Wait for release
    }
  } else debounce = 0;
#endif

  // Transfer one scanline from pixel data to LED strip

  switch(imageType) {

    case PALETTE1: { // 1-bit (2 color) palette-based image
      uint8_t  pixelNum = 0, byteNum, bitNum, pixels, idx,
              *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS / 8];
      for(byteNum = NUM_LEDS/8; byteNum--; ) { // Always padded to next byte
        pixels = pgm_read_byte(ptr++);  // 8 pixels of data (pixel 0 = LSB)
        for(bitNum = 8; bitNum--; pixels >>= 1) {
          idx = pixels & 1; // Color table index for pixel (0 or 1)
          strip.setPixelColor(pixelNum++,
            palette[idx][0], palette[idx][1], palette[idx][2]);
        }
      }
      break;
    }

    case PALETTE4: { // 4-bit (16 color) palette-based image
      uint8_t  pixelNum, p1, p2,
              *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS / 2];
      for(pixelNum = 0; pixelNum < NUM_LEDS; ) {
        p2  = pgm_read_byte(ptr++); // Data for two pixels...
        p1  = p2 >> 4;              // Shift down 4 bits for first pixel
        p2 &= 0x0F;                 // Mask out low 4 bits for second pixel
        strip.setPixelColor(pixelNum++,
          palette[p1][0], palette[p1][1], palette[p1][2]);
        strip.setPixelColor(pixelNum++,
          palette[p2][0], palette[p2][1], palette[p2][2]);
      }
      break;
    }

    case PALETTE8: { // 8-bit (256 color) PROGMEM-palette-based image
      uint16_t  o;
      uint8_t   pixelNum,
               *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS];
      for(pixelNum = 0; pixelNum < NUM_LEDS; pixelNum++) {
        o = pgm_read_byte(ptr++) * 3; // Offset into imagePalette
        strip.setPixelColor(pixelNum,
          pgm_read_byte(&imagePalette[o++]),
          pgm_read_byte(&imagePalette[o++]),
          pgm_read_byte(&imagePalette[o]));
      }
      break;
    }

    case TRUECOLOR: { // 24-bit ('truecolor') image (no palette)
      uint8_t  pixelNum,
              *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS * 3];
      for(pixelNum = 0; pixelNum < NUM_LEDS; pixelNum++) {
        strip.setPixelColor(pixelNum,
          pgm_read_byte(ptr++),
          pgm_read_byte(ptr++),
          pgm_read_byte(ptr++));
      }
      break;
    }
  }

  strip.show();

  if(++imageLine >= imageLines) imageLine = 0; // Next scanline, wrap around
}

// POWER-SAVING STUFF -- Relentlessly non-portable -------------------------

#ifdef MOTION_PIN
void sleep() {
  // Turn off LEDs one of two ways...
#ifdef POWER_PIN
  digitalWrite(POWER_PIN, HIGH);             // Cut power
#else                                        // or
  strip.clear();                             // Issue '0' data
  strip.show();
#endif

  // Disable peripherals to maximize battery in sleep state
#ifdef __AVR_ATtiny85__
  DIDR0 = _BV(AIN1D) | _BV(AIN0D);           // Disable digital input buffers
#else
  DIDR0 = 0x3F;
  DIDR1 = 0x03;
#endif
  power_all_disable();

  // Enable pin-change interrupt on motion pin
#ifdef __AVR_ATtiny85__
  PCMSK = _BV(MOTION_PIN);                   // Pin mask
  GIMSK = _BV(PCIE);                         // Interrupt enable
#else
  volatile uint8_t *p = portInputRegister(digitalPinToPort(MOTION_PIN));
  if(p == &PIND) {        // Pins 0-7 = PCINT16-23
    PCMSK2 = _BV(MOTION_PIN);
    PCICR  = _BV(PCIE2);
  } else if(p == &PINB) { // Pins 8-13 = PCINT0-5
    PCMSK0 = _BV(MOTION_PIN- 8);
    PCICR  = _BV(PCIE0);
  } else if(p == &PINC) { // Pins 14-20 = PCINT8-14
    PCMSK1 = _BV(MOTION_PIN-14);
    PCICR  = _BV(PCIE1);
  }
#endif

  // If select pin is enabled, that wakes too!
#ifdef SELECT_PIN
  debounce = 0;
#ifdef __AVR_ATtiny85__
  PCMSK |= _BV(SELECT_PIN);                  // Add'l pin mask
#else
  volatile uint8_t *p = portInputRegister(digitalPinToPort(SELECT_PIN));
  if(p == &PIND) {        // Pins 0-7 = PCINT16-23
    PCMSK2 = _BV(SELECT_PIN);
    PCICR  = _BV(PCIE2);
  } else if(p == &PINB) { // Pins 8-13 = PCINT0-5
    PCMSK0 = _BV(SELECT_PIN- 8);
    PCICR  = _BV(PCIE0);
  } else if(p == &PINC) { // Pins 14-20 = PCINT8-14
    PCMSK1 = _BV(SELECT_PIN-14);
    PCICR  = _BV(PCIE1);
  }
#endif
#endif

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);       // Deepest sleep mode
  sleep_enable();
  interrupts();
  sleep_mode();                              // Power down

  // Resumes here on wake

  // Clear pin change settings so interrupt won't fire again
#ifdef __AVR_ATtiny85__
  GIMSK = PCMSK = 0;
#else
  PCICR = PCMSK0 = PCMSK1 = PCMSK2 = 0;
#endif
  power_timer0_enable();                     // Used by millis()
#ifdef POWER_PIN
  digitalWrite(POWER_PIN, LOW);              // Power-up LEDs
#endif
}

EMPTY_INTERRUPT(PCINT0_vect); // Pin change (does nothing, but required)
#ifndef __AVR_ATtiny85__
ISR(PCINT1_vect, ISR_ALIASOF(PCINT0_vect));
ISR(PCINT2_vect, ISR_ALIASOF(PCINT0_vect));
#endif

#endif // MOTION_PIN
