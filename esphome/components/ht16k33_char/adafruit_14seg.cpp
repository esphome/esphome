#include <unordered_map>
#include "esphome/core/log.h"
#include "adafruit_14seg.h"

/******************************************************************************
 *Adafruit 14 segment .56" displays
 *  Product Link: https://www.adafruit.com/product/1911
 *
 *  Device specific functions for the Adafruit 14 segment displays. Both right-side-up and
 *  upside-down orientations are supported. To use these in your device, use device type
 *  'ADAFRUIT_14_SEG' or 'ADAFRUIT_14_SEG_FLIPPED'. These devices can display pretty much all
 *  of the ASCII characters. If you ask it to print an unsupported character, that digit on the
 *  display will be left blank.
 *
 *  Schematic: https://learn.adafruit.com/assets/114463
 *  Display Datasheet: https://cdn-shop.adafruit.com/datasheets/CID2379.pdf
 *
 *****************************************************************************/

namespace esphome {
namespace ht16k33_char {

void Adafruit14Seg::write_to_buffer(uint16_t char_to_write, uint8_t char_position) {
  this->buffer_[this->digit_map_[char_position]] |= (uint8_t) ((char_to_write) &0xFF);
  this->buffer_[this->digit_map_[char_position] + 1] |= (uint8_t) ((char_to_write >> 8) & 0x3F);
}

uint8_t Adafruit14Seg::handle_special_char(char char_to_find, uint8_t position) {
  if (position > 4) {
    // This should never happen.
    return SPECIAL_CHAR_NOT_FOUND;
  }

  if (char_to_find == '.') {
    if (position > 0) {
      // We can't put a period before the first digit.
      this->buffer_[this->digit_map_[position - 1] + 1] |= 0x40;
      return SPECIAL_CHAR_FOUND;
    } else {
      // If the peroid is in the first position on the display, we need to advance the first char pointer an extra time
      // to make the scrolling smooth
      return SPECIAL_CHAR_FOUND_ADVANCE;
    }
  }
  return SPECIAL_CHAR_NOT_FOUND;
}

void Adafruit14SegFlip::write_to_buffer(uint16_t char_to_write, uint8_t char_position) {
  this->buffer_[this->digit_map_[char_position]] |= (uint8_t) ((char_to_write) &0xFF);
  this->buffer_[this->digit_map_[char_position] + 1] |= (uint8_t) ((char_to_write >> 8) & 0x3F);
}

uint8_t Adafruit14SegFlip::handle_special_char(char char_to_find, uint8_t position) {
  if (position > 4) {
    // This should never happen.
    return SPECIAL_CHAR_NOT_FOUND;
  }

  if (char_to_find == '\'' || char_to_find == '`') {
    if (position < 4) {
      this->buffer_[this->digit_map_[position] + 1] |= 0x40;
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
