#pragma once

#include <cstdint>
#include <algorithm>
#include "esphome/components/display/display_color_utils.h"
#include "esphome/core/color.h"

/* Utility for converting internal \a Color RGB representation to supported IC hardware color keys
 *
 * Focus in driver layer is on efficiency.
 * For optimum output quality on RGB inputs consider offline color keying/dithering.
 * Also see e.g. Image component.
 */

namespace esphome::epaper_spi {

// The Rec.601 luma helpers and the RGB565 mono lookup live in the shared display utilities, so other
// drivers (e.g. it8951 in its 1 bit per pixel mode) can use them too.
using display::color_to_mono;
using display::rec601_luma;
using display::Rgb565ToMono;

/** Delta for when to regard as gray */
static constexpr uint8_t COLORCONV_GRAY_THRESHOLD = 50;

/** Native color keys for black/white/red panels, for use with color_to_bwr */
enum class BwrColor : uint8_t {
  BWR_COLOR_BLACK,
  BWR_COLOR_WHITE,
  BWR_COLOR_RED,
};

/** Map RGB color to one of 4 discrete gray levels (2 bits per pixel)
 *
 * @param color RGB color to convert from
 * @return      Gray level: 0 = black, 3 = white
 */
constexpr uint8_t color_to_gray4(Color color) {
  const uint8_t level = (uint8_t) ((rec601_luma(color) + 32u) >> 6);  // quantize 0..255 to 0..3, rounded
  return level > 3 ? 3 : level;
}

/** Map RGB color to discrete BWYR hex 4 color key
 *
 * @tparam NATIVE_COLOR  Type of native hardware color values
 * @param color     RGB color to convert from
 * @param hw_black  Native value for black
 * @param hw_white  Native value for white
 * @param hw_yellow Native value for yellow
 * @param hw_red    Native value for red
 * @return          Converted native hardware color value
 */
template<typename NATIVE_COLOR>
constexpr NATIVE_COLOR color_to_bwyr(Color color, NATIVE_COLOR hw_black, NATIVE_COLOR hw_white, NATIVE_COLOR hw_yellow,
                                     NATIVE_COLOR hw_red) {
  // --- Step 1: Check for Grayscale (Black or White) ---
  // We define "grayscale" as a color where the min and max components
  // are close to each other.

  const auto [min_rgb, max_rgb] = std::minmax({color.r, color.g, color.b});

  if ((max_rgb - min_rgb) < COLORCONV_GRAY_THRESHOLD) {
    // It's a shade of gray. Map to BLACK or WHITE.
    return color_to_mono(color) ? hw_white : hw_black;
  }

  // --- Step 2: Check for Primary/Secondary Colors ---
  // If it's not gray, it's a color. We check which components are
  // "on" (over 128) vs "off". This divides the RGB cube into 8 corners.
  const bool r_on = (color.r > 128);
  const bool g_on = (color.g > 128);
  const bool b_on = (color.b > 128);

  if (r_on) {
    if (!b_on) {
      return g_on ? hw_yellow : hw_red;
    }

    // At least red+blue high (but not gray) -> White
    return hw_white;
  } else {
    return (b_on && g_on) ? hw_white : hw_black;
  }
}

/** Map RGB color to discrete BWR (black/white/red) 3 color key
 *
 * Grays map to black or white by luma; other colors map to whichever of black, white or red is
 * nearest in RGB space, so e.g. orange renders as red rather than being folded into white.
 *
 * @tparam NATIVE_COLOR  Type of native hardware color values
 * @param color     RGB color to convert from
 * @param hw_black  Native value for black
 * @param hw_white  Native value for white
 * @param hw_red    Native value for red
 * @return          Converted native hardware color value
 */
template<typename NATIVE_COLOR>
constexpr NATIVE_COLOR color_to_bwr(Color color, NATIVE_COLOR hw_black, NATIVE_COLOR hw_white, NATIVE_COLOR hw_red) {
  const auto [min_rgb, max_rgb] = std::minmax({color.r, color.g, color.b});

  if ((max_rgb - min_rgb) < COLORCONV_GRAY_THRESHOLD) {
    return color_to_mono(color) ? hw_white : hw_black;
  }

  const int r = color.r;
  const int g = color.g;
  const int b = color.b;
  const int d_black = r * r + g * g + b * b;
  const int d_white = (255 - r) * (255 - r) + (255 - g) * (255 - g) + (255 - b) * (255 - b);
  const int d_red = (255 - r) * (255 - r) + g * g + b * b;

  if (d_red < d_white && d_red < d_black) {
    return hw_red;
  }
  // Ties (e.g. yellow) go to white
  return d_white <= d_black ? hw_white : hw_black;
}

/** Map RGB color to discrete BWYRGB hex 6 color key
 *
 * Divides the RGB cube into 8 corners by which components are "on" (over 128), same as
 * color_to_bwyr, but also resolves the green and blue corners instead of folding them into
 * white/black.
 *
 * @tparam NATIVE_COLOR  Type of native hardware color values
 * @param color     RGB color to convert from
 * @param hw_black  Native value for black
 * @param hw_white  Native value for white
 * @param hw_yellow Native value for yellow
 * @param hw_red    Native value for red
 * @param hw_green  Native value for green
 * @param hw_blue   Native value for blue
 * @return          Converted native hardware color value
 */
template<typename NATIVE_COLOR>
constexpr NATIVE_COLOR color_to_bwyrgb(Color color, NATIVE_COLOR hw_black, NATIVE_COLOR hw_white,
                                       NATIVE_COLOR hw_yellow, NATIVE_COLOR hw_red, NATIVE_COLOR hw_green,
                                       NATIVE_COLOR hw_blue) {
  const auto [min_rgb, max_rgb] = std::minmax({color.r, color.g, color.b});

  if ((max_rgb - min_rgb) < COLORCONV_GRAY_THRESHOLD) {
    return color_to_mono(color) ? hw_white : hw_black;
  }

  const bool r_on = (color.r > 128);
  const bool g_on = (color.g > 128);
  const bool b_on = (color.b > 128);

  if (r_on && g_on && !b_on) {
    return hw_yellow;
  }
  if (r_on && !g_on && !b_on) {
    return hw_red;
  }
  if (!r_on && g_on && !b_on) {
    return hw_green;
  }
  if (!r_on && !g_on && b_on) {
    return hw_blue;
  }
  // Handle "impure" colors (cyan, magenta) by folding into the closest primary.
  if (!r_on && g_on && b_on) {
    return hw_green;  // cyan
  }
  if (r_on && !g_on) {
    return hw_red;  // magenta
  }
  if (r_on) {
    // All high (but not gray) -> white
    return hw_white;
  }
  // !r_on && !g_on && !b_on
  // All low (but not gray) -> black
  return hw_black;
}

/** Map RGB color to discrete BWYRGBO hex 7 color key
 *
 * Same corner logic as color_to_bwyrgb, except the red/yellow corner is split three ways
 * instead of two, for panels with a dedicated orange ink.
 *
 * @tparam NATIVE_COLOR  Type of native hardware color values
 * @param color     RGB color to convert from
 * @param hw_black  Native value for black
 * @param hw_white  Native value for white
 * @param hw_yellow Native value for yellow
 * @param hw_red    Native value for red
 * @param hw_green  Native value for green
 * @param hw_blue   Native value for blue
 * @param hw_orange Native value for orange
 * @return          Converted native hardware color value
 */
template<typename NATIVE_COLOR>
constexpr NATIVE_COLOR color_to_bwyrgbo(Color color, NATIVE_COLOR hw_black, NATIVE_COLOR hw_white,
                                        NATIVE_COLOR hw_yellow, NATIVE_COLOR hw_red, NATIVE_COLOR hw_green,
                                        NATIVE_COLOR hw_blue, NATIVE_COLOR hw_orange) {
  const auto [min_rgb, max_rgb] = std::minmax({color.r, color.g, color.b});

  if ((max_rgb - min_rgb) < COLORCONV_GRAY_THRESHOLD) {
    return color_to_mono(color) ? hw_white : hw_black;
  }

  const bool r_on = (color.r > 128);
  const bool g_on = (color.g > 128);
  const bool b_on = (color.b > 128);

  if (r_on && !b_on) {
    // Between red and yellow: split the gradient in three instead of two, since this panel has a
    // dedicated orange ink. Named orange (e.g. 0xFFA500) has g close to the midpoint, so the
    // plain g_on (>128) threshold used by color_to_bwyrgb can't tell it apart from yellow.
    if (color.g > 170) {
      return hw_yellow;
    }
    if (color.g > 85) {
      return hw_orange;
    }
    return hw_red;
  }
  if (!r_on && g_on && !b_on) {
    return hw_green;
  }
  if (!r_on && !g_on && b_on) {
    return hw_blue;
  }
  // Handle "impure" colors (cyan, magenta) by folding into the closest primary.
  if (!r_on && g_on && b_on) {
    return hw_green;  // cyan
  }
  if (r_on && !g_on) {
    return hw_red;  // magenta
  }
  if (r_on) {
    // All high (but not gray) -> white
    return hw_white;
  }
  // !r_on && !g_on && !b_on
  // All low (but not gray) -> black
  return hw_black;
}

/// Native color codes of E Ink Spectra 6 controllers, 4 bits per pixel.
enum Spectra6Color : uint8_t {
  SPECTRA6_BLACK = 0,
  SPECTRA6_WHITE = 1,
  SPECTRA6_YELLOW = 2,
  SPECTRA6_RED = 3,
  SPECTRA6_BLUE = 5,
  SPECTRA6_GREEN = 6,
};

/// Map RGB color to the native Spectra 6 color code.
constexpr uint8_t color_to_spectra6(Color color) {
  return color_to_bwyrgb<uint8_t>(color, SPECTRA6_BLACK, SPECTRA6_WHITE, SPECTRA6_YELLOW, SPECTRA6_RED, SPECTRA6_GREEN,
                                  SPECTRA6_BLUE);
}

}  // namespace esphome::epaper_spi
