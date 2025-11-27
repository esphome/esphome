#include <unordered_map>
#include "esphome/core/log.h"
#include "adafruit_7seg.h"

/******************************************************************************
 *  Implements two devices
 ******************************************************************************
 *Adafruit 7 segment .56" displays
 *  Product Link: https://www.adafruit.com/product/878
 *
 *  Device specific functions for the Adafruit 7 segment, .56" high displays. Both right-side-up
 *  and upside-down orientations are supported. To use these in your device, use device type
 *  'ADAFRUIT_7_SEG_.56IN' or 'ADAFRUIT_7_SEG_.56IN_FLIPPED'. These devices are mostly only
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
 ******************************************************************************
 *Adafruit 7 segment 1.2" displays
 *  Product Link: https://www.adafruit.com/product/1270
 *
 *  Device specific functions for the Adafruit 7 segment, 1.2" high displays. Both right-side-up
 *  and upside-down orientations are supported. To use these in your device, use device type
 *  'ADAFRUIT_7_SEG_1.2IN' or 'ADAFRUIT_7_SEG_1.2IN_FLIPPED'. These devices are mostly only
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
 *                       display was flipped upside-down.
 *****************************************************************************/

namespace esphome {
namespace ht16k33_char {

void Adafruit7Seg::write_to_buffer(uint16_t char_to_write, uint8_t char_position) {
  this->buffer_[this->digit_map_[char_position]] |= (uint8_t) ((char_to_write) &0x7F);
  this->buffer_[this->digit_map_[char_position] + 1] = 0;  // The higher byte is always 0 for the 7-segment displays
}

uint8_t Adafruit7Seg::handle_special_char(char char_to_find, uint8_t position) {
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

void Adafruit7SegFlip::write_to_buffer(uint16_t char_to_write, uint8_t char_position) {
  this->buffer_[this->digit_map_[char_position]] |= (uint8_t) ((char_to_write) &0x7F);
  this->buffer_[this->digit_map_[char_position] + 1] = 0;  // The higher byte is always 0 for the 7-segment displays
}

uint8_t Adafruit7SegFlip::handle_special_char(char char_to_find, uint8_t position) {
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

void Adafruit7SegLarge::write_to_buffer(uint16_t char_to_write, uint8_t char_position) {
  this->buffer_[this->digit_map_[char_position]] |= (uint8_t) ((char_to_write) &0x7F);
  this->buffer_[this->digit_map_[char_position] + 1] = 0;  // The higher byte is always 0 for the 7-segment displays
}

uint8_t Adafruit7SegLarge::handle_special_char(char char_to_find, uint8_t position) {
  if (position > 4) {
    // This should never happen.
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

void Adafruit7SegLargeFlip::write_to_buffer(uint16_t char_to_write, uint8_t char_position) {
  this->buffer_[this->digit_map_[char_position]] |= (uint8_t) ((char_to_write) &0x7F);
  this->buffer_[this->digit_map_[char_position] + 1] = 0;  // The higher byte is always 0 for the 7-segment displays
}

uint8_t Adafruit7SegLargeFlip::handle_special_char(char char_to_find, uint8_t position) {
  if (position > 4) {
    // This should never happen.
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
