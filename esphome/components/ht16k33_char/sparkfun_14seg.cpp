#include <unordered_map>
#include "esphome/core/log.h"
#include "sparkfun_14seg.h"

/******************************************************************************
 *Sparkfun 14 segment QWIIC displays
 *  Product Link: https://www.sparkfun.com/sparkfun-qwiic-alphanumeric-display-red.html
 *
 *  Device specific functions for the Sparkfun 14 segment displays. Both right-side-up and
 *  upside-down orientations are supported. To use these in your device, use device type
 *  'SPARKFUN_14_SEG' or 'SPARKFUN_14_SEG_FLIPPED'. These devices can display pretty much all
 *  of the ASCII characters. If you ask it to print an unsupported character, that digit on the
 *  display will be left blank.
 *
 *  Schematic: https://cdn.sparkfun.com/assets/c/7/2/8/a/Qwiic_Alphanumeric_Display.pdf
 *  Display Datasheet: https://cdn.sparkfun.com/assets/c/8/7/2/5/VK16K33Datasheet.pdf
 *
 *****************************************************************************/

namespace esphome {
namespace ht16k33_char {

static const char *const TAG = "ht16k33_char";

// Position is the index in the character buffer of the first digit to display. position 0 is the
//  begining of the buffer Returns the index of the first character to display on the next display.
//  (what we would give as `position` to the next call to this function).
uint8_t Sparkfun14Seg::send_to_display(i2c::I2CDevice *display, uint8_t position) {
  uint8_t i;
  char char_to_find;
  bool special_character_found;
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
        this->write_to_buffer_(0x0000, i);
        i++;
      }
    }

    else {
      // The character to find is within the bounds of the buffer array.
      char_to_find = this->char_buffer_.at(char_buffer_location);

      // Look for special characters. For this display, there is a colon between digit one and two,
      //  and a period after digit two. The display will try (badly) to display a colon if it is
      //  placed in any other location. This causes scrolling with a colon to look wierd. This seems
      //  like a very edge case, so I did not try to fix it. A period in any other location will
      //  display as blank.
      if (!special_character_found) {
        if ((char_to_find == ':') && (i == 2)) {
          // Colon at position 3
          special_character_found = true;
          char_buffer_location++;
          this->buffer_[2] |= 0x01;
          continue;
        } else if ((char_to_find == '.') && (i == 3)) {
          // Period at position 4
          special_character_found = true;
          char_buffer_location++;
          this->buffer_[4] |= 0x01;
          continue;
        }
      }

      auto it = this->char_map_.find(char_to_find);
      if (it != this->char_map_.end()) {
        this->write_to_buffer_((it->second), i);
        special_character_found = false;
        i++;
      } else {
        // Digit is not in the map. Blank the digit.
        this->write_to_buffer_(0x0000, i);
        special_character_found = false;
        i++;
      }

      char_buffer_location++;
    }
  }

  display->write(this->buffer_, 16, true);
  return char_buffer_location;
}

// Write a character at position 'char_position' to the memory buffer.
void Sparkfun14Seg::write_to_buffer_(uint16_t char_to_write, uint8_t char_position) {
  // char_position should be 0-3
  if ((char_position >= 0) && (char_position <= 3)) {
    for (uint8_t i = 0; i < 8; i++) {
      // i counts through the com positions
      this->buffer_[i * 2 + 1] |= ((char_to_write >> i) & 0x01) << (char_position);
      this->buffer_[i * 2 + 1] |= ((char_to_write >> (i + 8)) & 0x01) << (char_position + 4);
    }
  }
}

// Position is the position in the character buffer. position 0 is the begining of the buffer
// Returns the index of the first character to display in the buffer (what we would give as `position` to the next call
// to this function).
uint8_t Sparkfun14SegFlip::send_to_display(i2c::I2CDevice *display, uint8_t position) {
  uint8_t i;
  char char_to_find;
  bool special_character_found;
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
        this->write_to_buffer_(0x0000, i);
        i++;
      }
    }

    else {
      // The character to find is within the bounds of the buffer array.
      char_to_find = this->char_buffer_.at(char_buffer_location);

      // Look for special characters. For this flipped display, there is a colon between digit one
      //  and two, and a period at the top of the display between digit zero and one. The display
      //  will try (badly) to display a colon if it is placed in any other location. This causes
      //  scrolling with a colon to look wierd. This seems like a very edge case, so I did not try
      //  to fix it. The period at the top of the display is not implemented.
      if (!special_character_found) {
        if ((char_to_find == ':') && (i == 2)) {
          // Colon at position 3
          special_character_found = true;
          char_buffer_location++;
          this->buffer_[2] |= 0x01;
          continue;
        }
      }

      auto it = this->char_map_.find(char_to_find);
      if (it != this->char_map_.end()) {
        this->write_to_buffer_((it->second), i);
        special_character_found = false;
        i++;
      } else {
        // Digit is not in the map. Blank the digit.
        this->write_to_buffer_(0x0000, i);
        special_character_found = false;
        i++;
      }

      char_buffer_location++;
    }
  }

  display->write(this->buffer_, 16, true);
  return char_buffer_location;
}

// Write a character at position 'char_position' to the memory buffer.
//  Note that for this flipped device, char_position is the logical position of the character.
//  For example, char_position = 0 is the left most character on the display. char_position is
//  converted in this function to correctly place the digits on the flipped display.
void Sparkfun14SegFlip::write_to_buffer_(uint16_t char_to_write, uint8_t char_position) {
  // char_position should be 0-3
  if ((char_position >= 0) && (char_position <= 3)) {
    uint8_t flipped_char_position = 3 - char_position;

    for (uint8_t i = 0; i < 8; i++) {
      // i counts through the com positions
      this->buffer_[i * 2 + 1] |= ((char_to_write >> i) & 0x01) << (flipped_char_position);
      this->buffer_[i * 2 + 1] |= ((char_to_write >> (i + 8)) & 0x01) << (flipped_char_position + 4);
    }
  }
}

}  // namespace ht16k33_char
}  // namespace esphome
