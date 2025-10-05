#include <unordered_map>
#include "esphome/core/log.h"
#include "adafruit_7seg_large.h"

/******************************************************************************
 *Adafruit 7 segment 1.2" displays
 *  Product Link: https://www.adafruit.com/product/1270
 *
 *  Device specific functions for the Adafruit 7 segment, 1.2" high displays. Both right-side-up
 *  and upside-down orientations are supported. To use these in your device, use device type
 *  'ADAFRUIT_7SEGMENT_1.2IN' or 'ADAFRUIT_7SEGMENT_1.2IN_FLIPPED'. These devices are mostly only
 *  able to display numbers. There are a few letters in the font set that display reasonably well
 *  on the 7 segment display. If you ask it to print an unsupported character, that digit on the
 *  display will be left blank.
 *
 *  Schematic: https://learn.adafruit.com/assets/122068
 *  Display Datasheet: https://cdn-shop.adafruit.com/datasheets/1264datasheet.pdf
 *
 *  Note: As of this writing (3/2025) the schematic for this device linked above is wrong. The
 *        LEDs segments are connected to the HT16K33 the same as the .56" devices.
 *
 *  Note: This display doesnt have a decimal point after each digit. In addition to the four
 *        7-segment digits, it has four other dots controlled by bits in DisplayBuffer[5]:
 *          0b00000010 - A colon between digits 2 and 3.
 *          0b00000100 - The upper part of the colon at the left edge of the display.
 *          0b00001000 - The lower part of the colon at the left edge of the display.
 *          0b00010000 - A dot between digits 3 and 4 at the top. Could be a decimal point if the
 *                       display was flipped upside-down. (DisplayBuffer[5] = 0b00000010)
 *****************************************************************************/

namespace esphome {
namespace ht16k33_char {

static const char *const TAG = "ht16k33_char";

// Position is the position in the character buffer. position 0 is the begining of the buffer
// Returns the index of the first character to display in the buffer (what we would give as `position` to the next call
// to this function).
uint16_t Adafruit7SegLarge::send_to_display_(i2c::I2CDevice *display, uint16_t position) {
  return this->send_to_display_common_(display, position);
}

void Adafruit7SegLarge::write_to_buffer_(uint16_t char_to_write, uint8_t char_position) {
  this->buffer_[this->digit_map_[char_position]] |= (uint8_t) ((char_to_write) & 0xFF);
  this->buffer_[this->digit_map_[char_position] + 1] = 0; //The higher byte is always 0 for the 7-segment displays
}

uint8_t Adafruit7SegLarge::handle_special_char_(char char_to_find, uint8_t position) {
  if (position > 4) {
    //This should never happen.
    return SPECIAL_CHAR_NOT_FOUND;
  }

  if (char_to_find == ':') {
    if (position == 0) {
      // We want a colon before the first digit
      this->buffer_[5] |= 0b00001100;
    } else if (position == 2) {
      // We want a colon between digit 2 and 3
      this->buffer_[5] = this->buffer_[5] | 0b00000010;
    }
    return SPECIAL_CHAR_FOUND;
  } else if (char_to_find == '\'' || char_to_find == '`') {
    if (position == 0) {
      // We want an apostrophe before the first digit
      this->buffer_[5] = this->buffer_[5] | 0b00000100;
    } else if (position == 3) {
      // We want an apostrophe before the fourth digit
      this->buffer_[5] = this->buffer_[5] | 0b00010000;
    }
    return SPECIAL_CHAR_FOUND;
  } else if (char_to_find == '.') {
    if (position == 0) {
      // We want an period before the first digit
      this->buffer_[5] = this->buffer_[5] | 0b00001000;
      return SPECIAL_CHAR_FOUND;
    }
  }
  return SPECIAL_CHAR_NOT_FOUND;
}

// Position is the position in the character buffer. position 0 is the begining of the buffer
// Returns the index of the next character to display in the buffer (what we would give as `position` to the next call
// to this function).
uint16_t Adafruit7SegLargeFlip::send_to_display_(i2c::I2CDevice *display, uint16_t position) {
  return this->send_to_display_common_(display, position);
}

void Adafruit7SegLargeFlip::write_to_buffer_(uint16_t char_to_write, uint8_t char_position) {
  this->buffer_[this->digit_map_[char_position]] |= (uint8_t) ((char_to_write) & 0xFF);
  this->buffer_[this->digit_map_[char_position] + 1] = 0; //The higher byte is always 0 for the 7-segment displays
}

uint8_t Adafruit7SegLargeFlip::handle_special_char_(char char_to_find, uint8_t position) {
  if (position > 4) {
    //This should never happen.
    return SPECIAL_CHAR_NOT_FOUND;
  }

  if (char_to_find == ':') {
    if (position == 2) {
      // We want a colon between digit 2 and 3
      this->buffer_[5] = this->buffer_[5] | 0b00000010;
      return SPECIAL_CHAR_FOUND;
    } else if (position == 4) {
      // We want a colon after digit 4
      this->buffer_[5] = this->buffer_[5] | 0b00001100;
      return SPECIAL_CHAR_FOUND;
    }
  } else if (char_to_find == '.') {
    if (position == 1) {
      // We want an period before the second digit
      this->buffer_[5] = this->buffer_[5] | 0b00010000;
      return SPECIAL_CHAR_FOUND;
    } else if (position == 4) {
      // We want an period after the 4th digit
      this->buffer_[5] = this->buffer_[5] | 0b00000100;
      return SPECIAL_CHAR_FOUND;
    }
  } else if (((char_to_find == '\'' || char_to_find == '`')) && (position == 4)) {
    this->buffer_[5] = this->buffer_[5] | 0b00001000;
    return SPECIAL_CHAR_FOUND;
  }
  return SPECIAL_CHAR_NOT_FOUND;
}

}  // namespace ht16k33_char
}  // namespace esphome
