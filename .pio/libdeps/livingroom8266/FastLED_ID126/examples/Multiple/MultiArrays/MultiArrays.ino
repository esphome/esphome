// MultiArrays - see https://github.com/FastLED/FastLED/wiki/Multiple-Controller-Examples for more info on
// using multiple controllers.  In this example, we're going to set up three NEOPIXEL strips on three
// different pins, each strip getting its own CRGB array to be played with

#include <FastLED.h>

#define NUM_LEDS_PER_STRIP 60
CRGB redLeds[NUM_LEDS_PER_STRIP];
CRGB greenLeds[NUM_LEDS_PER_STRIP];
CRGB blueLeds[NUM_LEDS_PER_STRIP];

// For mirroring strips, all the "special" stuff happens just in setup.  We
// just addLeds multiple times, once for each strip
void setup() {
  // tell FastLED there's 60 NEOPIXEL leds on pin 10
  FastLED.addLeds<NEOPIXEL, 10>(redLeds, NUM_LEDS_PER_STRIP);

  // tell FastLED there's 60 NEOPIXEL leds on pin 11
  FastLED.addLeds<NEOPIXEL, 11>(greenLeds, NUM_LEDS_PER_STRIP);

  // tell FastLED there's 60 NEOPIXEL leds on pin 12
  FastLED.addLeds<NEOPIXEL, 12>(blueLeds, NUM_LEDS_PER_STRIP);

}

void loop() {
  for(int i = 0; i < NUM_LEDS_PER_STRIP; i++) {
    // set our current dot to red, green, and blue
    redLeds[i] = CRGB::Red;
    greenLeds[i] = CRGB::Green;
    blueLeds[i] = CRGB::Blue;
    FastLED.show();
    // clear our current dot before we move on
    redLeds[i] = CRGB::Black;
    greenLeds[i] = CRGB::Black;
    blueLeds[i] = CRGB::Blue;
    delay(100);
  }

  for(int i = NUM_LEDS_PER_STRIP-1; i >= 0; i--) {
    // set our current dot to red, green, and blue
    redLeds[i] = CRGB::Red;
    greenLeds[i] = CRGB::Green;
    blueLeds[i] = CRGB::Blue;
    FastLED.show();
    // clear our current dot before we move on
    redLeds[i] = CRGB::Black;
    greenLeds[i] = CRGB::Black;
    blueLeds[i] = CRGB::Blue;
    delay(100);
  }
}
