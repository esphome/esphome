#pragma once
#include "esphome/core/color.h"

namespace esphome {
namespace display {

/** TextAlign is used to tell the display class how to position a piece of text. By default
 * the coordinates you enter for the print*() functions take the upper left corner of the text
 * as the "anchor" point. You can customize this behavior to, for example, make the coordinates
 * refer to the *center* of the text.
 *
 * All text alignments consist of an X and Y-coordinate alignment. For the alignment along the X-axis
 * these options are allowed:
 *
 * - LEFT (x-coordinate of anchor point is on left)
 * - CENTER_HORIZONTAL (x-coordinate of anchor point is in the horizontal center of the text)
 * - RIGHT (x-coordinate of anchor point is on right)
 *
 * For the Y-Axis alignment these options are allowed:
 *
 * - TOP (y-coordinate of anchor is on the top of the text)
 * - CENTER_VERTICAL (y-coordinate of anchor is in the vertical center of the text)
 * - BASELINE (y-coordinate of anchor is on the baseline of the text)
 * - BOTTOM (y-coordinate of anchor is on the bottom of the text)
 *
 * These options are then combined to create combined TextAlignment options like:
 * - TOP_LEFT (default)
 * - CENTER (anchor point is in the middle of the text bounds)
 * - ...
 */
enum class TextAlign {
  TOP = 0x00,
  CENTER_VERTICAL = 0x01,
  BASELINE = 0x02,
  BOTTOM = 0x04,

  LEFT = 0x00,
  CENTER_HORIZONTAL = 0x08,
  RIGHT = 0x10,

  TOP_LEFT = TOP | LEFT,
  TOP_CENTER = TOP | CENTER_HORIZONTAL,
  TOP_RIGHT = TOP | RIGHT,

  CENTER_LEFT = CENTER_VERTICAL | LEFT,
  CENTER = CENTER_VERTICAL | CENTER_HORIZONTAL,
  CENTER_RIGHT = CENTER_VERTICAL | RIGHT,

  BASELINE_LEFT = BASELINE | LEFT,
  BASELINE_CENTER = BASELINE | CENTER_HORIZONTAL,
  BASELINE_RIGHT = BASELINE | RIGHT,

  BOTTOM_LEFT = BOTTOM | LEFT,
  BOTTOM_CENTER = BOTTOM | CENTER_HORIZONTAL,
  BOTTOM_RIGHT = BOTTOM | RIGHT,
};

/// Turn the pixel OFF.
extern const Color COLOR_OFF;
/// Turn the pixel ON.
extern const Color COLOR_ON;

enum ImageType { IMAGE_TYPE_BINARY = 0, IMAGE_TYPE_GRAYSCALE = 1, IMAGE_TYPE_RGB24 = 2 };

/// Bit pattern defines the line-type
enum LineType {
  LINE_TYPE_SOLID = 0b1111,
  LINE_TYPE_DOTTED = 0b0101,
  LINE_TYPE_DASHED = 0b0111,
  // Following defines number of bits used to define line pattern
  PATTERN_LENGTH = 4
};

enum DisplayRotation {
  DISPLAY_ROTATION_0_DEGREES = 0,
  DISPLAY_ROTATION_90_DEGREES = 90,
  DISPLAY_ROTATION_180_DEGREES = 180,
  DISPLAY_ROTATION_270_DEGREES = 270,
};

}  // namespace display
}  // namespace esphome
