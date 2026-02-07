#pragma once

#include <cstdint>
#include <algorithm>
#include "esphome/core/color.h"

/* Utility for converting internal \a Color RGB representation to supported IC hardware color keys
 *
 * Focus in driver layer is on efficiency.
 * For optimum output quality on RGB inputs consider offline color keying/dithering.
 * Also see e.g. Image component.
 */

namespace esphome::epaper_spi {

/** Delta for when to regard as gray */
static constexpr uint8_t COLORCONV_GRAY_THRESHOLD = 50;

/** Map RGB color to discrete BWYR hex 4 color key
 *
 * @tparam NATIVE_COLOR  Type of native hardware color values
 * @param color     RGB color to convert from
 * @param hw_black  Native value for black
 * @param hw_white  Native value for white
 * @param hw_yellow Native value for yellow
 * @param hw_red    Native value for red
 * @return          Converted native hardware color value
 * @internal Constexpr. Does not depend on side effects ("pure").
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
    // We split the luminance at the halfway point (382 = (255*3)/2)
    if ((static_cast<int>(color.r) + color.g + color.b) > 382) {
      return hw_white;
    }
    return hw_black;
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

}  // namespace esphome::epaper_spi
