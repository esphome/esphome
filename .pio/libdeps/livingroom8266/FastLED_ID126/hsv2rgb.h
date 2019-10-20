#ifndef __INC_HSV2RGB_H
#define __INC_HSV2RGB_H

#include "FastLED.h"

#include "pixeltypes.h"

FASTLED_NAMESPACE_BEGIN

// hsv2rgb_rainbow - convert a hue, saturation, and value to RGB
//                   using a visually balanced rainbow (vs a straight
//                   mathematical spectrum).
//                   This 'rainbow' yields better yellow and orange
//                   than a straight 'spectrum'.
//
//                   NOTE: here hue is 0-255, not just 0-191

void hsv2rgb_rainbow( const struct CHSV& hsv, struct CRGB& rgb);
void hsv2rgb_rainbow( const struct CHSV* phsv, struct CRGB * prgb, int numLeds);
#define HUE_MAX_RAINBOW 255


// hsv2rgb_spectrum - convert a hue, saturation, and value to RGB
//                    using a mathematically straight spectrum (vs
//                    a visually balanced rainbow).
//                    This 'spectrum' will have more green & blue
//                    than a 'rainbow', and less yellow and orange.
//
//                    NOTE: here hue is 0-255, not just 0-191

void hsv2rgb_spectrum( const struct CHSV& hsv, struct CRGB& rgb);
void hsv2rgb_spectrum( const struct CHSV* phsv, struct CRGB * prgb, int numLeds);
#define HUE_MAX_SPECTRUM 255


// hsv2rgb_raw - convert hue, saturation, and value to RGB.
//               This 'spectrum' conversion will be more green & blue
//               than a real 'rainbow', and the hue is specified just
//               in the range 0-191.  Together, these result in a
//               slightly faster conversion speed, at the expense of
//               color balance.
//
//               NOTE: Hue is 0-191 only!
//               Saturation & value are 0-255 each.
//

void hsv2rgb_raw(const struct CHSV& hsv, struct CRGB & rgb);
void hsv2rgb_raw(const struct CHSV* phsv, struct CRGB * prgb, int numLeds);
#define HUE_MAX 191


// rgb2hsv_approximate - recover _approximate_ HSV values from RGB.
//
//   NOTE 1: This function is a long-term work in process; expect
//   results to change slightly over time as this function is
//   refined and improved.
//
//   NOTE 2: This function is most accurate when the input is an
//   RGB color that came from a fully-saturated HSV color to start
//   with.  E.g. CHSV( hue, 255, 255) -> CRGB -> CHSV will give
//   best results.
//
//   NOTE 3: This function is not nearly as fast as HSV-to-RGB.
//   It is provided for those situations when the need for this
//   function cannot be avoided, or when extremely high performance
//   is not needed.
//
//   NOTE 4: Why is this 'only' an "approximation"?
//   Not all RGB colors have HSV equivalents!  For example, there
//   is no HSV value that will ever convert to RGB(255,255,0) using
//   the code provided in this library.   So if you try to
//   convert RGB(255,255,0) 'back' to HSV, you'll necessarily get
//   only an approximation.  Emphasis has been placed on getting
//   the 'hue' as close as usefully possible, but even that's a bit
//   of a challenge.  The 8-bit HSV and 8-bit RGB color spaces
//   are not a "bijection".
//
//   Nevertheless, this function does a pretty good job, particularly
//   at recovering the 'hue' from fully saturated RGB colors that
//   originally came from HSV rainbow colors.  So if you start
//   with CHSV(hue_in,255,255), and convert that to RGB, and then
//   convert it back to HSV using this function, the resulting output
//   hue will either exactly the same, or very close (+/-1).
//   The more desaturated the original RGB color is, the rougher the
//   approximation, and the less accurate the results.
//
CHSV rgb2hsv_approximate( const CRGB& rgb);

FASTLED_NAMESPACE_END

#endif
