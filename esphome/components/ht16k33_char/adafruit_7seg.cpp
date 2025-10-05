#include <unordered_map>
#include "esphome/core/log.h"
#include "adafruit_7seg.h"

/******************************************************************************
 *Adafruit 7 segment .56" displays
 *  Product Link: https://www.adafruit.com/product/878
 *
 *  Device specific functions for the Adafruit 7 segment, .56" high displays. Both right-side-up
 *  and upside-down orientations are supported. To use these in your device, use device type
 *  'ADAFRUIT_7SEGMENT_.56IN' or 'ADAFRUIT_7SEGMENT_.56IN_FLIPPED'. These devices are mostly only
 *  able to display numbers. There are a few letters in the font set that display reasonably well
 *  on the 7 segment display. If you ask it to print an unsupported character, that digit on the
 *  display will be left blank.
 *
 *  Schematic: https://learn.adafruit.com/assets/108790
 *  Display Datasheet: https://cdn-shop.adafruit.com/datasheets/865datasheet.pdf
 *
 *  Note: You can't use both the period and the colon after the second digit at the same time.
 *        In theory, the device would allow this. But the way I wrote the code will treat the
 *        second character as an invalid character in the third location I can't think of a
 *        scenario where we would need to use both the colon and the period, so I did not
 *        implement it.
 *****************************************************************************/

namespace esphome {
namespace ht16k33_char {

static const char *const TAG = "ht16k33_char";

// Position is the position in the character buffer. position 0 is the begining of the buffer
// Returns the index of the first character to display in the buffer (what we would give as `position` to the next call
// to this function).
uint16_t Adafruit7Seg::send_to_display_(i2c::I2CDevice *display, uint16_t position) {
  return this->send_to_display_common_(display, position);
}

void Adafruit7Seg::write_to_buffer_(uint16_t char_to_write, uint8_t char_position) {
  this->buffer_[this->digit_map_[char_position]] |= (uint8_t) ((char_to_write) &0xFF);
  this->buffer_[this->digit_map_[char_position] + 1] = 0;  // The higher byte is always 0 for the 7-segment displays
}

uint8_t Adafruit7Seg::handle_special_char_(char char_to_find, uint8_t position) {
  if (position > 4) {
    // This should never happen.
    return SPECIAL_CHAR_NOT_FOUND;
  }

  if ((char_to_find == ':') && (position == 2)) {
    // We want a colon between digit 2 and 3
    this->buffer_[5] = this->buffer_[5] | 0b00000010;
    return SPECIAL_CHAR_FOUND;
  }

  if (char_to_find == '.') {
    if (position > 0) {
      // We can't put a period before the first digit.
      // For periods on this device, the period at a location is wired to the digit to its left, hence the [position-1]
      // here.
      this->buffer_[this->digit_map_[position - 1]] |= 0x80;
      return SPECIAL_CHAR_FOUND;
    } else {
      // If there is a decimal point in the first location in the char buffer, skip over it.
      return SPECIAL_CHAR_FOUND_ADVANCE;
    }
  }
  return SPECIAL_CHAR_NOT_FOUND;
}

// Position is the position in the character buffer. position 0 is the begining of the buffer
// Returns the index of the first character to display in the buffer (what we would give as `position` to the next call
// to this function).
uint16_t Adafruit7SegFlip::send_to_display_(i2c::I2CDevice *display, uint16_t position) {
  return this->send_to_display_common_(display, position);
}

void Adafruit7SegFlip::write_to_buffer_(uint16_t char_to_write, uint8_t char_position) {
  this->buffer_[this->digit_map_[char_position]] |= (uint8_t) ((char_to_write) &0xFF);
  this->buffer_[this->digit_map_[char_position] + 1] = 0;  // The higher byte is always 0 for the 7-segment displays
}

uint8_t Adafruit7SegFlip::handle_special_char_(char char_to_find, uint8_t position) {
  if (position > 4) {
    // This should never happen.
    return SPECIAL_CHAR_NOT_FOUND;
  }

  if ((char_to_find == ':') && (position == 2)) {
    // We want a colon between digit 2 and 3
    this->buffer_[5] = this->buffer_[5] | 0b00000010;
    return SPECIAL_CHAR_FOUND;
  }

  if (char_to_find == '\'' || char_to_find == '`') {
    if (position < 4) {
      this->buffer_[this->digit_map_[position]] |= 0x80;
      if (position == 0) {
        // If the char is at the first position on the first display, we need to advance the first char pointer an extra
        // time to keep the display scrolling steady.
        return SPECIAL_CHAR_FOUND_ADVANCE;
      } else {
        return SPECIAL_CHAR_FOUND;
      }
    }
  }

  return SPECIAL_CHAR_NOT_FOUND;
}

}  // namespace ht16k33_char
}  // namespace esphome
