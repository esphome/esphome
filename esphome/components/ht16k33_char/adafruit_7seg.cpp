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
uint8_t Adafruit7Seg::send_to_display(i2c::I2CDevice *display, uint8_t position) {
  uint8_t i;
  char char_to_find;
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
    if (char_buffer_location >= this->char_buffer_.length()) {
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
      char_to_find = this->char_buffer_.at(char_buffer_location);

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
          }

          if (char_to_find == '.') {
            special_character_found = true;
            char_buffer_location++;
            if (i > 0) {
              // We can't put a period before the first digit.
              this->buffer_[digit_map[i - 1]] |= 0x80;
            } else {
              // If there is a decimal point in the first location in the char buffer, skip over it.
              this->fist_char_location_++;
            }
            continue;
          }
        }

        this->buffer_[digit_map[i]] = 0x00;
        special_character_found = false;
        i++;
      }

      char_buffer_location++;
    }
  }

  // We can have a period after the last digit. Handle that here
  if (!(char_buffer_location >= this->char_buffer_.length())) {
    char_to_find = this->char_buffer_.at(char_buffer_location);
    if (char_to_find == '.') {
      this->buffer_[digit_map[3]] |= 0x80;
      char_buffer_location++;
    }
  }

  display->write(this->buffer_, 16, true);
  return char_buffer_location;
}

// Position is the position in the character buffer. position 0 is the begining of the buffer
// Returns the index of the first character to display in the buffer (what we would give as `position` to the next call
// to this function).
uint8_t Adafruit7SegFlip::send_to_display(i2c::I2CDevice *display, uint8_t position) {
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
    if (char_buffer_location >= this->char_buffer_.length()) {
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
      char_to_find = this->char_buffer_.at(char_buffer_location);

      auto it = this->char_map_.find(char_to_find);
      if (it != this->char_map_.end()) {
        this->buffer_[digit_map[i]] |= it->second;
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
          }
          if (char_to_find == '\'' || char_to_find == '`') {
            this->buffer_[digit_map[i]] |= 0x80;
            special_character_found = true;
            char_buffer_location++;
            if (i == 0) {
              // If there is a ' or ` in the first location, we want to advance
              // the first char location by 2 instead of 1.
              this->fist_char_location_++;
            }
            continue;
          }
        }

        this->buffer_[digit_map[i]] = 0x00;
        special_character_found = false;
        i++;
      }

      char_buffer_location++;
    }
  }

  display->write(this->buffer_, 16, true);
  return char_buffer_location;
}

}  // namespace ht16k33_char
}  // namespace esphome
