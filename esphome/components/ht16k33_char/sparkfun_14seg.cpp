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

// Write a character at position 'char_position' to the memory buffer.
void Sparkfun14Seg::write_to_buffer(uint16_t char_to_write, uint8_t char_position) {
  // char_position should be 0-3
  if ((char_position >= 0) && (char_position <= 3)) {
    for (uint8_t i = 0; i < 8; i++) {
      // i counts through the com positions
      this->buffer_[i * 2 + 1] |= ((char_to_write >> i) & 0x01) << (char_position);
      this->buffer_[i * 2 + 1] |= ((char_to_write >> (i + 8)) & 0x01) << (char_position + 4);
    }
  }
}

uint8_t Sparkfun14Seg::handle_special_char(char char_to_find, uint8_t position) {
  if (position > 4) {
    // This should never happen.
    return SPECIAL_CHAR_NOT_FOUND;
  }

  if ((char_to_find == ':') && (position == 2)) {
    // Colon at position 3
    this->buffer_[2] |= 0x01;
    return SPECIAL_CHAR_FOUND;
  } else if ((char_to_find == '.') && (position == 3)) {
    // Period at position 4
    this->buffer_[4] |= 0x01;
    return SPECIAL_CHAR_FOUND;
  }

  return SPECIAL_CHAR_NOT_FOUND;
}

// Write a character at position 'char_position' to the memory buffer.
//  Note that for this flipped device, char_position is the logical position of the character.
//  For example, char_position = 0 is the left most character on the display. char_position is
//  converted in this function to correctly place the digits on the flipped display.
void Sparkfun14SegFlip::write_to_buffer(uint16_t char_to_write, uint8_t char_position) {
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

uint8_t Sparkfun14SegFlip::handle_special_char(char char_to_find, uint8_t position) {
  if (position > 4) {
    // This should never happen.
    return SPECIAL_CHAR_NOT_FOUND;
  }

  // Look for special characters. For this flipped display, there is a colon between digit one
  //  and two, and a period at the top of the display between digit zero and one. The display
  //  will try (badly) to display a colon if it is placed in any other location. This causes
  //  scrolling with a colon to look wierd. This seems like a very edge case, so I did not try
  //  to fix it. The period at the top of the display is not implemented.
  if ((char_to_find == ':') && (position == 2)) {
    // Colon at position 3
    this->buffer_[2] |= 0x01;
    return SPECIAL_CHAR_FOUND;
  }

  return SPECIAL_CHAR_NOT_FOUND;
}

}  // namespace ht16k33_char
}  // namespace esphome
