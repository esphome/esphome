#ifndef __INC_COLOR_H
#define __INC_COLOR_H

#include "FastLED.h"

FASTLED_NAMESPACE_BEGIN

///@file color.h
/// contains definitions for color correction and temperature
///@defgroup ColorEnums Color correction/temperature
/// definitions for color correction and light temperatures
///@{
typedef enum {
   // Color correction starting points

   /// typical values for SMD5050 LEDs
   ///@{
    TypicalSMD5050=0xFFB0F0 /* 255, 176, 240 */,
    TypicalLEDStrip=0xFFB0F0 /* 255, 176, 240 */,
  ///@}

   /// typical values for 8mm "pixels on a string"
   /// also for many through-hole 'T' package LEDs
   ///@{
   Typical8mmPixel=0xFFE08C /* 255, 224, 140 */,
   TypicalPixelString=0xFFE08C /* 255, 224, 140 */,
   ///@}

   /// uncorrected color
   UncorrectedColor=0xFFFFFF

} LEDColorCorrection;


typedef enum {
   /// @name Black-body radiation light sources
   /// Black-body radiation light sources emit a (relatively) continuous
   /// spectrum, and can be described as having a Kelvin 'temperature'
   ///@{
   /// 1900 Kelvin
   Candle=0xFF9329 /* 1900 K, 255, 147, 41 */,
   /// 2600 Kelvin
   Tungsten40W=0xFFC58F /* 2600 K, 255, 197, 143 */,
   /// 2850 Kelvin
   Tungsten100W=0xFFD6AA /* 2850 K, 255, 214, 170 */,
   /// 3200 Kelvin
   Halogen=0xFFF1E0 /* 3200 K, 255, 241, 224 */,
   /// 5200 Kelvin
   CarbonArc=0xFFFAF4 /* 5200 K, 255, 250, 244 */,
   /// 5400 Kelvin
   HighNoonSun=0xFFFFFB /* 5400 K, 255, 255, 251 */,
   /// 6000 Kelvin
   DirectSunlight=0xFFFFFF /* 6000 K, 255, 255, 255 */,
   /// 7000 Kelvin
   OvercastSky=0xC9E2FF /* 7000 K, 201, 226, 255 */,
   /// 20000 Kelvin
   ClearBlueSky=0x409CFF /* 20000 K, 64, 156, 255 */,
   ///@}

   /// @name Gaseous light sources
   /// Gaseous light sources emit discrete spectral bands, and while we can
   /// approximate their aggregate hue with RGB values, they don't actually
   /// have a proper Kelvin temperature.
   ///@{
   WarmFluorescent=0xFFF4E5 /* 0 K, 255, 244, 229 */,
   StandardFluorescent=0xF4FFFA /* 0 K, 244, 255, 250 */,
   CoolWhiteFluorescent=0xD4EBFF /* 0 K, 212, 235, 255 */,
   FullSpectrumFluorescent=0xFFF4F2 /* 0 K, 255, 244, 242 */,
   GrowLightFluorescent=0xFFEFF7 /* 0 K, 255, 239, 247 */,
   BlackLightFluorescent=0xA700FF /* 0 K, 167, 0, 255 */,
   MercuryVapor=0xD8F7FF /* 0 K, 216, 247, 255 */,
   SodiumVapor=0xFFD1B2 /* 0 K, 255, 209, 178 */,
   MetalHalide=0xF2FCFF /* 0 K, 242, 252, 255 */,
   HighPressureSodium=0xFFB74C /* 0 K, 255, 183, 76 */,
   ///@}

   /// Uncorrected temperature 0xFFFFFF
   UncorrectedTemperature=0xFFFFFF
} ColorTemperature;

FASTLED_NAMESPACE_END

///@}
#endif
