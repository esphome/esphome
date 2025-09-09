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
uint8_t Adafruit7SegLarge::send_to_display(i2c::I2CDevice *display, uint8_t position) {
  uint8_t i;
  char char_to_find;

  // Note: we use this to detect when there are multiple special characters in a rown in the
  // string. Say something like '..1234'. A period before the first character is valid, so
  // the first character turns on the period LED. Without this flag, because the character
  // location has not moved, the next period is also valid. With this flag, the next period
  // will be treated as an invalid character.
  bool special_character_found;

  const uint8_t digit_map[4] = {1, 3, 7, 9};
  uint8_t char_buffer_location;

  this->buffer_[0] = HT16K33_DISPLAY_DATA_ADDRESS;

  // Clear any old data from the buffer
  for (int i = 1; i < 16; i++) {
    this->buffer_[i] = 0x00;
  }

  char_buffer_location = position;
  i = 0;
  special_character_found = false;

  while (i < 4) {
    if (char_buffer_location >= this->message_buffer_.length()) {
      // char_buffer_location is past the end of the character buffer.
      if (this->continuous_) {
        // We want a continuous display where the message starts over immediately.
        char_buffer_location = 0;
      } else {
        // Blank the digits past the end of the display.
        this->buffer_[digit_map[i]] = 0x00;
        i++;
      }
    }

    else {
      // The character to find is within the bounds of the buffer array.
      char_to_find = this->message_buffer_.at(char_buffer_location);

      auto it = this->char_map_.find(char_to_find);
      if (it != this->char_map_.end()) {
        this->buffer_[digit_map[i]] = it->second;
        special_character_found = false;
        i++;
      } else {
        // Look for special characters. These characters are only valid at certain locations in the display. A special
        // character in an invalid location will be treated the same way as an invalid character. In the case of an
        // invalid character, that location in the display will be left blank. only one special character will be
        // evaulated per location on the display.
        if (!special_character_found) {
          if (char_to_find == ':') {
            if (i == 0) {
              // We want a colon before the first digit
              this->buffer_[5] = this->buffer_[5] | 0b00001100;
              special_character_found = true;
              char_buffer_location++;
              continue;
            } else if (i == 2) {
              // We want a colon between digit 2 and 3
              this->buffer_[5] = this->buffer_[5] | 0b00000010;
              special_character_found = true;
              char_buffer_location++;
              continue;
            }
          } else if (char_to_find == '\'' || char_to_find == '`') {
            if (i == 0) {
              // We want an apostrophe before the first digit
              this->buffer_[5] = this->buffer_[5] | 0b00000100;
              special_character_found = true;
              char_buffer_location++;
              continue;

            } else if (i == 3) {
              // We want an apostrophe before the fourth digit
              this->buffer_[5] = this->buffer_[5] | 0b00010000;
              special_character_found = true;
              char_buffer_location++;
              continue;
            }
          } else if (char_to_find == '.') {
            if (i == 0) {
              // We want an period before the first digit
              this->buffer_[5] = this->buffer_[5] | 0b00001000;
              special_character_found = true;
              char_buffer_location++;
              continue;
            }
          }
        }

        this->buffer_[digit_map[i]] = 0x00;
        special_character_found = false;
        i++;
      }

      char_buffer_location++;
    }
  }

  display->write(this->buffer_, 16);
  return char_buffer_location;
}

// Position is the position in the character buffer. position 0 is the begining of the buffer
// Returns the index of the next character to display in the buffer (what we would give as `position` to the next call
// to this function).
uint8_t Adafruit7SegLargeFlip::send_to_display(i2c::I2CDevice *display, uint8_t position) {
  uint8_t i;
  char char_to_find;
  bool special_character_found;

  const uint8_t digit_map[4] = {9, 7, 3, 1};
  uint8_t char_buffer_location;

  this->buffer_[0] = HT16K33_DISPLAY_DATA_ADDRESS;

  // Clear any old data from the buffer
  for (int i = 1; i < 16; i++) {
    this->buffer_[i] = 0x00;
  }

  char_buffer_location = position;
  i = 0;
  special_character_found = false;

  while (i < 4) {
    if (char_buffer_location >= this->message_buffer_.length()) {
      // char_buffer_location is past the end of the character buffer.
      if (this->continuous_) {
        // We want a continuous display where the message starts over immediately.
        char_buffer_location = 0;
      } else {
        // Blank the digits past the end of the display.
        this->buffer_[digit_map[i]] = 0x00;
        i++;
      }
    } else {
      // The character to find is within the bounds of the buffer array.
      char_to_find = this->message_buffer_.at(char_buffer_location);

      auto it = this->char_map_.find(char_to_find);
      if (it != this->char_map_.end()) {
        this->buffer_[digit_map[i]] = it->second;
        special_character_found = false;
        i++;
      } else {
        // Look for special characters. These characters are only valid at certain locations in the display. A special
        // character in an invalid location will be treated the same way as an invalid character. In the case of an
        // invalid character, that location in the display will be left blank. only one special character will be
        // evaulated per location on the display.
        if (!special_character_found) {
          if (char_to_find == ':') {
            if (i == 2) {
              // We want a colon between digit 2 and 3
              this->buffer_[5] = this->buffer_[5] | 0b00000010;
              special_character_found = true;
              char_buffer_location++;
              continue;
            }
          } else if (char_to_find == '.') {
            if (i == 1) {
              // We want an period before the second digit
              this->buffer_[5] = this->buffer_[5] | 0b00010000;
              special_character_found = true;
              char_buffer_location++;
              continue;
            }
          }
        }

        this->buffer_[digit_map[i]] = 0x00;
        special_character_found = false;
        i++;
      }

      char_buffer_location++;
    }
  }

  // We can have special characters after the last digit.
  if (!(char_buffer_location >= this->message_buffer_.length())) {
    char_to_find = this->message_buffer_.at(char_buffer_location);
    if (char_to_find == '.') {
      this->buffer_[5] = this->buffer_[5] | 0b00000100;
      char_buffer_location++;
    } else if (char_to_find == '\'' || char_to_find == '`') {
      this->buffer_[5] = this->buffer_[5] | 0b00001000;
      char_buffer_location++;
    } else if (char_to_find == ':') {
      this->buffer_[5] = this->buffer_[5] | 0b00001100;
      char_buffer_location++;
    }
  }

  display->write(this->buffer_, 16);
  return char_buffer_location;
}

}  // namespace ht16k33_char
}  // namespace esphome
