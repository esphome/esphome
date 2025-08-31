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

static const char *const TAG = "ht16k33_char";

// Position is the index in the character buffer of the first digit to display. position 0 is the
//  begining of the buffer Returns the index of the first character to display on the next display.
//  (what we would give as `position` to the next call to this function).
uint8_t Adafruit14Seg::send_to_display(i2c::I2CDevice *display, uint8_t position) {
  uint8_t i;
  char char_to_find;
  bool special_character_found;
  const uint8_t digit_map[4] = {1, 3, 5, 7};
  uint8_t char_buffer_location;

  this->buffer_[0] = HT16K33_DISPLAY_DATA_ADDRESS;

  // Clear any old data from the buffer
  for (int i = 1; i < 16; i++) {
    this->buffer_[i] = 0x00;
  }

  char_buffer_location = position;
  i = 0;
  special_character_found = false;

  // In this while loop, `i` represents the digit that will display the character. We count through
  // the four digits in the display and set them to the next four characters in the character buffer.
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
        this->buffer_[digit_map[i]] = (uint8_t) ((it->second) & 0xFF);
        this->buffer_[digit_map[i] + 1] = (uint8_t) ((it->second >> 8) & 0xFF);
        special_character_found = false;
        i++;
      } else {
        // Look for special characters. For this display, the only special character is a period.
        //  Because of how scrolling works, if there is a period in the first location in the char
        //  buffer, the display will skip over it. So '.123' will be displayed as '123'. This is
        //  only true if the period is in exactly the first digit. If you want to display .123 on
        //  the display, you can make the char buffer '0.123' and it will display as expected.
        if (!special_character_found) {
          if (char_to_find == '.') {
            special_character_found = true;
            char_buffer_location++;
            if (i > 0) {
              // We can't put a period before the first digit.
              this->buffer_[digit_map[i - 1] + 1] |= 0x40;
            } else {
              // If there is a decimal point in the first location in the char buffer, skip over it.
              this->fist_char_location_++;
            }
            continue;
          }
        }

        // Digit is not in the map. Blank the digit.
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
      this->buffer_[digit_map[3] + 1] |= 0x40;
      char_buffer_location++;
    }
  }

  display->write(this->buffer_, 16);
  return char_buffer_location;
}

// Position is the position in the character buffer. position 0 is the begining of the buffer
// Returns the index of the first character to display in the buffer (what we would give as `position` to the next call
// to this function).
uint8_t Adafruit14SegFlip::send_to_display(i2c::I2CDevice *display, uint8_t position) {
  uint8_t i;
  char char_to_find;
  const uint8_t digit_map[4] = {7, 5, 3, 1};
  uint8_t char_buffer_location;

  this->buffer_[0] = HT16K33_DISPLAY_DATA_ADDRESS;

  // Clear any old data from the buffer
  for (int i = 1; i < 16; i++) {
    this->buffer_[i] = 0x00;
  }

  char_buffer_location = position;
  i = 0;

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
        this->buffer_[digit_map[i]] = (uint8_t) ((it->second) & 0xFF);
        this->buffer_[digit_map[i] + 1] = (uint8_t) ((it->second >> 8) & 0xFF);
        i++;
      } else {
        // Regarding special characters: With the display flipped, the decimal points are now on the
        //  top. I suppose I could count those as ' or `. For now I don't do that and just ignore
        //  the decimal points on the display. ' and ` can be displayed using the actual 14-segment
        //  digits, so I will leave them there.

        // Digit is not in the map. Blank the digit.
        this->buffer_[digit_map[i]] = 0x00;
        i++;
      }

      char_buffer_location++;
    }
  }

  display->write(this->buffer_, 16);
  return char_buffer_location;
}

}  // namespace ht16k33_char
}  // namespace esphome
