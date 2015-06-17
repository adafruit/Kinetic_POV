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
//
// This code is written specifically for the Trinket microcontroller and
// will not compile as-is on Pro Trinket or others, though it could be
// adapted with some knowledge and a datasheet.

// TO DO: Change this to use hardware-assist SPI: Pin 1 = data, 2 = clock,
// vibration switch on pin 0.

#include <Adafruit_DotStar.h>
#include <avr/power.h>
#include <avr/sleep.h>

#include "graphics.h" // Graphics data is contained in this header file.
// It's generated using the 'convert.py' Python script from a 16-color,
// 16-pixel-high GIF image.  Limiting to 16 colors allows a decent-sized
// POV message: at 8 bytes (16 pixels @ 4 bits) per line, with about 2.6K
// available on the Trinket, that's nearly 320 lines max.  The 16-color
// palette CAN be changed while running to provide more colors, but be
// mindful of peak & average current draw if you do that!  Power limiting
// is normally done in convert.py (keeps this code small & fast).

#define LED_DATA_PIN  0
#define LED_CLOCK_PIN 1

// The vibration switch (aligned perpendicular to leash) is used as a
// poor man's accelerometer -- poi lights only when spinning, saving some
// power.  This requires a certain minimum spin rate, and if you're doing
// very mellow spins, it might not be enough.  There's a 'fast' vibration
// switch you could try, but it's quite twitchy...might be better in this
// case just to comment out this line to have the poi run always-on:
#define MOTION_PIN 2

// Experimental: powering down the DotStar strip should conserve much more
// power when idle.  Use NPN transistor + 1K resistor.  Very little space,
// requires creative free-wiring.  Useless without MOTION_PIN also.
// #define POWER_PIN 3

#define SLEEP_TIME 2000 // Not-spinning time before sleep, in milliseconds

Adafruit_DotStar strip = Adafruit_DotStar(NUM_LEDS, // Defined in graphics.h
  LED_DATA_PIN, LED_CLOCK_PIN, DOTSTAR_GBR);

// -------------------------------------------------------------------------

void setup() {
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1);   // Enable 16 MHz on Trinket
#endif
#ifdef POWER_PIN
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);     // Power-on LED strip
#endif
  strip.begin();

#ifdef MOTION_PIN
  pinMode(MOTION_PIN, INPUT_PULLUP); // Pull-up on vibration switch
  sleep();                           // Jump right into sleep mode
#endif
}

uint16_t line = 0;  // Current scanline in bitmap
uint32_t prev = 0L; // Used for time measurement

void loop() {
#ifdef MOTION_PIN
  // Tried to do this with watchdog timer but encountered gas pains, so...
  uint32_t t = millis();               // Current time, milliseconds
  if(!(PINB & _BV(2))) {               // Vibration switch pulled down?
    prev = t;                          // Yes, reset timer
  } else if((t - prev) > SLEEP_TIME) { // No, SLEEP_TIME elapsed w/no switch?
    sleep();                           // Power down
    prev = t;                          // Reset timer on wake
  }
#endif

  // Transfer one scanline from pixels[] (through palette[]) to LED strip
  uint8_t i, p1, p2, *ptr = (uint8_t *)&pixels[line * 8];
  for(i=0; i<16; ) {
    p2  = pgm_read_byte(ptr++); // Data for two pixels...
    p1  = p2 >> 4;              // Shift down 4 bits for first pixel
    p2 &= 0x0F;                 // Mask out low 4 bits for second pixel
    strip.setPixelColor(i++, palette[p1][0], palette[p1][1], palette[p1][2]);
    strip.setPixelColor(i++, palette[p2][0], palette[p2][1], palette[p2][2]);
  }

  strip.show();

  if(++line >= LINES) line = 0; // Advance scanline, wrap around
}

// -------------------------------------------------------------------------

#ifdef MOTION_PIN
void sleep() {
  // Turn off LEDs one of two ways...
#ifdef POWER_PIN
  digitalWrite(POWER_PIN, LOW); // Cut power
#else
  strip.clear();                // Issue '0' data
  strip.show();
#endif

  // Disable peripherals to maximize battery in sleep state
  DIDR0 = _BV(AIN1D) | _BV(AIN0D); // Disable digital input buffers
  power_all_disable();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Deepest sleep mode
  sleep_enable();
  PCMSK = _BV(PCINT2);                 // Pin change mask = pin 2
  GIMSK = _BV(PCIE);                   // Pin change interrupt
  interrupts();
  sleep_mode();                        // Power down

  // Resumes here on wake

  GIMSK = 0;             // Clear pin change settings so
  PCMSK = 0;             // interrupt won't fire again.
  power_timer0_enable(); // Used by millis()
#ifdef POWER_PIN
  digitalWrite(POWER_PIN, HIGH); // Power-up LEDs
#endif
}

ISR(PCINT0_vect) { } // Pin change interrupt (does nothing, but required)

#endif // MOTION_PIN
