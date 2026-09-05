#include <unordered_map>

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

#include "ht16k33_char.h"

/* To add more device types:
 *    -Add to the HT16K33_DEVICE_TYPES enum in the display.py file
 *    -If nessecary, add formatting functions to display.py that convert character codes from the
 *     standard format to the correct format for the new device.
 *    -Add a new .h and .c file that defines a class derived from the `HT16k33CharComponent` class.
 *     This class should:
 *      -Add the character codes to char_map_ during class initialization.
 *      -Implement a `uint8_t handle_special_char(char char_to_find, uint8_t position)` function.
 *      -Implement a `void write_to_buffer(uint16_t char_to_write, uint8_t char_position)'
 */

namespace esphome {
namespace ht16k33_char {

static const char *const TAG = "ht16k33_char";

// States for the scrolling state machine.
static const uint8_t HT16K33_SCROLL_STATE_STATIC = 0;
static const uint8_t HT16K33_SCROLL_STATE_START = 1;
static const uint8_t HT16K33_SCROLL_STATE_SCROLLING = 2;
static const uint8_t HT16K33_SCROLL_STATE_END = 3;
static const uint8_t HT16K33_SCROLL_STATE_FIRST_START = 4;
static const uint8_t HT16K33_SCROLL_STATE_STOPPED = 5;

// Return a setup priority. More info here: https://esphome.io/api/namespaceesphome_1_1setup__priority
float HT16k33CharComponent::get_setup_priority() const { return setup_priority::PROCESSOR; }

void HT16k33CharComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HT16K33...");
  uint8_t setup_buffer;

  for (auto *display : this->displays_) {
    setup_buffer = HT16K33_SYSTEM_SETUP | HT16K33_MODE_NORMAL;
    display->write(&setup_buffer, 1);

    setup_buffer = HT16K33_DISPLAY_SETUP | HT16K33_DISPLAY_ON;
    display->write(&setup_buffer, 1);
  }

  this->brightness(this->brightness_);

  this->blank();
  this->fist_char_location_ = 0;

  // Check to see if we need to scroll the display.
  if (!(this->scroll_)) {
    // Scrolling is off.
    this->scroll_state_ = HT16K33_SCROLL_STATE_STATIC;
  } else if (this->continuous_) {
    // If the state is continuous, there is no start and end delay. Go directly into the scrolling.
    this->scroll_state_ = HT16K33_SCROLL_STATE_SCROLLING;
    this->last_scroll_ = App.get_loop_component_start_time();
  } else {
    this->scroll_state_ = HT16K33_SCROLL_STATE_FIRST_START;
    this->last_scroll_ = App.get_loop_component_start_time();
  }
}

void HT16k33CharComponent::update() {
  uint16_t current_buffer_location;

  // This checks if the lambda function is defined. If it is not defined, we don't do anything.
  if (this->writer_.has_value()) {
    // This line is responsible for calling the lambda code.
    (*this->writer_)(*this);

    // The lambda code does not actually update the display directly. It manipulates the message buffer.
    //   - If the display is static (no scrolling), we directly call display() to update the display now.
    //   - If scrolling is happening, we do not update the display in this function. The display will
    //     be updated in the loop() function.
    //   - if we are in the state 'FIRST_START' this means we just started the device. In that state,
    //     the display will not be showing anything yet, and we need to run the update_display()
    //     function to show the initial contents.
    if ((this->scroll_state_ == HT16K33_SCROLL_STATE_STATIC) ||
        (this->scroll_state_ == HT16K33_SCROLL_STATE_FIRST_START) ||
        (this->scroll_state_ == HT16K33_SCROLL_STATE_STOPPED)) {
      this->last_scroll_ = App.get_loop_component_start_time();
      current_buffer_location = this->update_display();

      if ((this->fist_char_location_ == 0) && (current_buffer_location >= this->message_buffer_.length()) &&
          (this->scroll_state_ == HT16K33_SCROLL_STATE_FIRST_START)) {
        // We reached the end of the char buffer before we reached the end of the display.
        this->scroll_state_ = HT16K33_SCROLL_STATE_STOPPED;
      }
    }
  }
}

void HT16k33CharComponent::loop() {
  uint32_t now;
  uint8_t current_buffer_location;

  if ((this->scroll_state_ == HT16K33_SCROLL_STATE_STATIC) || (this->scroll_state_ == HT16K33_SCROLL_STATE_STOPPED)) {
    // Check this first. If the display is static, we don't need to do anything in this function.
    return;
  }

  now = App.get_loop_component_start_time();

  if (this->last_scroll_ > now) {
    // This will happen when the millis() function overflows. (approx every 50 days)
    //  I don't know if App.get_loop_component_start_time() handles this, but if it doesnt,
    //  this check should keep the code from misbehaving in this instance.
    this->last_scroll_ = now;
    return;
  }

  switch (this->scroll_state_) {
    case HT16K33_SCROLL_STATE_START:
    case HT16K33_SCROLL_STATE_FIRST_START:
      if ((now - this->last_scroll_) >= this->scroll_delay_) {
        // Start scrolling
        this->last_scroll_ = now;
        this->fist_char_location_ += this->char_len_(this->message_buffer_[this->fist_char_location_]);
        current_buffer_location = this->update_display();

        // This handles if there is only a single scroll, it skips directly to STATE_END.
        if (!(this->continuous_) && ((current_buffer_location + 1) > this->message_buffer_.length())) {
          this->scroll_state_ = HT16K33_SCROLL_STATE_END;
        } else {
          this->scroll_state_ = HT16K33_SCROLL_STATE_SCROLLING;
        }
      }
      break;

    case HT16K33_SCROLL_STATE_SCROLLING:
      if ((now - this->last_scroll_) >= this->scroll_speed_) {
        // Scroll to the next character.
        this->last_scroll_ = now;
        this->fist_char_location_ += this->char_len_(this->message_buffer_[this->fist_char_location_]);
        if (this->fist_char_location_ > this->message_buffer_.length()) {
          // This only happens in continuous mode.
          this->fist_char_location_ = 0;
        }
        current_buffer_location = this->update_display();

        if (!(this->continuous_) && ((current_buffer_location + 1) > this->message_buffer_.length())) {
          // We have reached the end of the stuff to display. Go to the end delay.
          // The display does not need to be updated here.
          this->scroll_state_ = HT16K33_SCROLL_STATE_END;
        }
      }
      break;

    case HT16K33_SCROLL_STATE_END:
      if ((now - this->last_scroll_) >= this->scroll_dwell_) {
        // Go back to the begining
        this->last_scroll_ = now;
        this->scroll_state_ = HT16K33_SCROLL_STATE_START;
        this->fist_char_location_ = 0;
        this->update_display();
      }
      break;
  }
}

void HT16k33CharComponent::dump_config() {
  uint8_t i;

  ESP_LOGCONFIG(TAG, "HT16K33 Char:");
  ESP_LOGCONFIG(TAG, "  Max Buffer Length: %d", this->char_buffer_max_size_);
  ESP_LOGCONFIG(TAG, "  Brightness: %d", this->brightness_);

  // Scrolling stuff
  if (this->scroll_) {
    ESP_LOGCONFIG(TAG, "  Scrolling: Enabled");
    if (this->continuous_) {
      ESP_LOGCONFIG(TAG, "    Continuous: Yes");
    } else {
      ESP_LOGCONFIG(TAG, "    Continuous: No");
    }
    ESP_LOGCONFIG(TAG, "    Scroll Speed:       %0.2f sec", this->scroll_speed_ / 1000.);
    ESP_LOGCONFIG(TAG, "    Scroll Start Delay: %0.2f sec", this->scroll_delay_ / 1000.);
    ESP_LOGCONFIG(TAG, "    Scroll End Delay    %0.2f sec", this->scroll_dwell_ / 1000.);
  } else {
    ESP_LOGCONFIG(TAG, "  Scrolling: Disabled");
  }

  // Display device addresses.
  ESP_LOGCONFIG(TAG, "  Number of displays: %d", this->displays_.size());

  ESP_LOGCONFIG(TAG, "  I2C Addresses:");
  i = 0;
  for (auto *display : this->displays_) {
    ESP_LOGCONFIG(TAG, "    Device[%d]: 0x%02X", i, display->get_i2c_address());
    i++;
  }

  if (this->is_failed()) {
    // Nothing in this code actually sets the device to failed, so this should never trigger.
    //  I am leaving this in incase I want to implement a check during init to verify the
    //  correct device is on the bus.
    ESP_LOGCONFIG(TAG, "  I2C Device at 0x70 does not appear to be a HT16K33");
  }
  LOG_UPDATE_INTERVAL(this);
}

/****************************
 *Zeros out all of the display memory on the device. This is not the same as
 * turning it off, but it will have the same effect.
 ****************************/
void HT16k33CharComponent::blank() {
  for (auto *display : this->displays_) {
    this->buffer_[0] = HT16K33_DISPLAY_DATA_ADDRESS;

    // Clear the buffer
    for (int i = 1; i < 16; i++) {
      this->buffer_[i] = 0x00;
    }
    display->write(this->buffer_, 16);
  }
}

/****************************
 *Updates the displays based on the current char_buffer_ and first_char_location_.
 *
 *  -Returns the buffer location of the *next* character after the last one displayed.
 *   This can be used to determine scrolling state.
 ****************************/
uint8_t HT16k33CharComponent::update_display() {
  uint8_t buffer_location = this->fist_char_location_;

  for (auto *display : this->displays_) {
    buffer_location = this->send_to_display_common_(display, buffer_location);
  }

  return buffer_location;
}

/***********************************
 *Sets the brightness of the display
 *
 *  brightness_to_set: The brightness value to set. Valid values are 0-16. Setting brightness to 0
 *  turns off the display. Setting it to 16 is full brightness. Setting an invalid brightness value
 *  will result in the device being set to full brightness.
 ************************************/
void HT16k33CharComponent::brightness(uint8_t brightness_to_set) {
  uint8_t buffer;

  if (brightness_to_set == 0) {
    this->display_off(true);
  } else {
    // Valid brightness values are 0x00 - 0x0F
    if (brightness_to_set >= 16) {
      buffer = HT16K33_DIMMING_SET | 0x0F;
    } else {
      buffer = HT16K33_DIMMING_SET | (brightness_to_set - 1);
    }

    for (auto *display : this->displays_) {
      display->write(&buffer, 1);
    }
  }
}

/***********************************
 *Sets the blink function on the HT16k33. This blinking occurs independent of the CPU.
 *
 *  blink_state: The desired blink state. Valid values are:
 *    *0: No Blinking
 *    *1: Blink rate 2hZ (twice per second)
 *    *2: Blink rate 1hz (once per second)
 *    *3: Blink rate .5hZ (once per 2 seconds)
 *  An invalid blink rate will turn off blinking.
 ************************************/
void HT16k33CharComponent::set_blink(uint8_t blink_state) {
  uint8_t buffer;

  if (blink_state > 0x03) {
    // Valid values for blink are 0-3 anything else turns off blinking.
    buffer = HT16K33_DISPLAY_SETUP | HT16K33_DISPLAY_ON;
  } else {
    buffer = HT16K33_DISPLAY_SETUP | (blink_state << 1) | HT16K33_DISPLAY_ON;
  }

  for (auto *display : this->displays_) {
    display->write(&buffer, 1);
  }
}

/***********************************
 *Turns the displays on or off.
 *
 *  turn_off: Boolean. Set to true to turn off the device.
 *
 *Note: this function will also clear the bink state.
 ************************************/
void HT16k33CharComponent::display_off(bool turn_off) {
  uint8_t buffer;

  if (turn_off) {
    buffer = HT16K33_DISPLAY_SETUP | HT16K33_DISPLAY_OFF;
  } else {
    buffer = HT16K33_DISPLAY_SETUP | HT16K33_DISPLAY_ON;
  }

  for (auto *display : this->displays_) {
    display->write(&buffer, 1);
  }
}

/***********************************
 *Puts the display into standby mode. This is probably a lower power state than just
 *turning it off, but I have not tested that.
 *
 *  standby: Boolean. Set to true to put the device in standby mode. False to turn it back on.
 ************************************/
void HT16k33CharComponent::display_standby(bool standby) {
  uint8_t buffer;

  if (standby) {
    buffer = HT16K33_SYSTEM_SETUP | HT16K33_MODE_STANDBY;
  } else {
    buffer = HT16K33_SYSTEM_SETUP | HT16K33_MODE_NORMAL;
  }

  for (auto *display : this->displays_) {
    display->write(&buffer, 1);
  }
}

/***********************************
 * Gets a string that represents the next character to display.
 *  Assumes UTF-8 encoding.
 *
 *  start_position: The start position in the message buffer array.
 *
 *  *next_char: The address of a std::string to store the next character string.
 *
 * Returns the number of bytes it took to get the next character. (this will usually be 1, but not always.)
 ************************************/
uint8_t HT16k33CharComponent::get_next_char_(uint16_t start_position, std::string *next_char) {
  char first_char = this->message_buffer_[start_position];
  uint8_t next_char_length = this->char_len_(first_char);

  // Clear contents from the string to hold the next character.
  next_char->resize(1);
  next_char->clear();

  // Add all of the chars that represent the character to display.
  for (uint8_t i = 0; i < next_char_length; i++) {
    next_char->push_back(this->message_buffer_.at(start_position + i));
  }

  return next_char_length;
}

/***********************************
 *Given a 8-bit char, determines the number of bytes that are needed to represent the character code.
 *  This will by typically be 1 for normal characters, but for multi-byte characters, it may be up to 4.
 *  Assumes UTF-8 encoding.
 *  NOTE: std::mblen() is supposed to do this too, but it doesnt seem to work here. It always returns 1.
 *
 *  char_to_test: An 8-bit char representing the first byte of the character to check.
 *
 * Returns: The number of bytes needed to represent the character.
 ************************************/
uint8_t HT16k33CharComponent::char_len_(char char_to_test) {
  uint8_t first_char;

  first_char = std::char_traits<char>::to_int_type(char_to_test);
  if (first_char <= 0x7F) {
    // Single byte character
    return 1;
  } else if ((first_char & 0xE0) == 0xC0) {
    // Two byte character
    return 2;
  } else if ((first_char & 0xF0) == 0xE0) {
    // Three byte character
    return 3;
  } else if ((first_char & 0xF0) == 0xF0) {
    // Four byte character
    return 4;
  } else {
    // Invalid character code.
    return 0;
  }
}

/***********************************
 *Clear the contents of the buffer.
 *  This function clears the buffer that is directly sent to the devices.
 *  It should be called before adding new data to the buffer.
 ************************************/
void HT16k33CharComponent::clear_buffer_() {
  for (unsigned char &i : this->buffer_) {
    i = 0x00;
  }
}

/***********************************
 * Write the message characters to the display send buffer and send to the display indicated. This function
 * works for all the devices that I have tested. This function relies on two device specific functions:
 * handle_special_char() and write_to_buffer().
 *
 *  display: the display device to send the buffer to.
 *
 *  position: The position in the message buffer of the first character to display.
 *
 * Returns: the location in the message buffer of the next character. This is the position to
 *          send to the next display if one is present.
 ************************************/
uint16_t HT16k33CharComponent::send_to_display_common_(i2c::I2CDevice *display, uint16_t position) {
  uint8_t char_length;
  uint8_t digit_number;
  uint16_t char_buffer_location;
  bool special_character_found;
  std::string char_to_find;

  // Clear any old data from the buffer.
  this->clear_buffer_();

  this->buffer_[0] = HT16K33_DISPLAY_DATA_ADDRESS;
  char_buffer_location = position;
  digit_number = 0;
  special_character_found = false;

  while (digit_number < this->num_chars_per_display_) {
    if (char_buffer_location >= this->message_buffer_.length()) {
      // char_buffer_location is past the end of the character buffer.
      if (this->continuous_) {
        // We want a continuous display where the message starts over immediately.
        char_buffer_location = 0;
      } else {
        // Blank the digits past the end of the display.
        this->write_to_buffer(0, digit_number);
        digit_number++;
      }
    }

    else {
      // The character to find is within the bounds of the buffer array.
      char_length = this->get_next_char_(char_buffer_location, &char_to_find);
      if (char_length == 0) {
        // I don't think this is possible. If it is, display a blank character.
        char_to_find.resize(1);
        char_to_find.clear();
        char_to_find.push_back(' ');
        char_buffer_location = char_buffer_location + 1;
      } else {
        char_buffer_location = char_buffer_location + char_length;
      }

      auto it = this->char_map_.find(char_to_find);
      if (it != this->char_map_.end()) {
        // We found the character we want to write in the character map. Write that character code to the display buffer
        this->write_to_buffer(it->second, digit_number);
        special_character_found = false;
        digit_number++;
      } else {
        // The character we were looking for was not in the character map. Check if the character is a special
        // character. Special characters such as '.' and ':' have separate LEDs on the display. These
        // characters are only valid at certain locations in the display. A special character in an invalid
        // location will be treated the same way as an invalid character. In the case of an invalid character,
        // that location in the display will be left blank. only one special character will be evaulated per
        // location on the display.
        if (!special_character_found) {
          switch (this->handle_special_char(char_to_find.at(0), digit_number)) {
            case SPECIAL_CHAR_FOUND:
              special_character_found = true;
              continue;
            case SPECIAL_CHAR_FOUND_ADVANCE:
              // This case covers if we are scrolling the display and the first character in the first display is a
              // special character. In this instance, we want to skip over that character, or the scrolling will end
              // up choppy. To do this, we increment the first_char_location_ variable.
              special_character_found = true;
              if ((this->fist_char_location_ == (char_buffer_location - 1)) &&
                  (this->scroll_state_ != HT16K33_SCROLL_STATE_STATIC) &&
                  (this->scroll_state_ != HT16K33_SCROLL_STATE_STOPPED)) {
                this->fist_char_location_++;  // All special characters are single byte only.
              }
              continue;
          }
        }

        // The character we were looking for is not in the character map or a speical character, blank this digit.
        this->write_to_buffer(0, digit_number);
        special_character_found = false;
        digit_number++;
      }
    }
  }

  // We may be able to have special characters after the last digit, Handle that here.
  if (!(char_buffer_location >= this->message_buffer_.length())) {
    this->get_next_char_(char_buffer_location, &char_to_find);
    if (this->handle_special_char(char_to_find.at(0), digit_number) == SPECIAL_CHAR_FOUND) {
      char_buffer_location++;
    }
  }

  display->write(this->buffer_, 16);
  return char_buffer_location;
}

/***********************************
 *Add a character to the character map.
 *  This function should correctly handle UTF-8 mutibyte characters.
 *  If char_to_add is more than one character long, only the first character will be used.
 *
 *  char_to_add: The character to add to the map
 *
 *  char_code: The code that represents which LEDs should be light for this character.
 ************************************/
void HT16k33CharComponent::add_char(const char *char_to_add, uint16_t char_code) {
  std::string lookup_string = char_to_add;

  if (lookup_string.length() > this->char_len_(char_to_add[0])) {
    // If the string contains more than one character, we truncate to the first character only.
    lookup_string.resize(this->char_len_(char_to_add[0]));
  }

  this->char_map_[lookup_string] = char_code;
}

/***********************************
 *Write a character string to the display buffer.
 *
 *  start_pos:    The position to place the first character in the string. Position 0 is the start
 *                of the display buffer.
 *
 *  str:          The string to put in the buffer.
 *
 *  clear_buffer: Boolean. Set to true to clear the display buffer before writing the string.
 *
 *  Returns the number of bytes written to the buffer. Note that the number of bytes in the buffer
 *    is limited by this->char_buffer_max_size_. If str is a longer string, or adding it would make the
 *    total buffer length (in bytes) exceede char_buffer_max_size_, the string is truncated to prevent this.
 ************************************/
uint8_t HT16k33CharComponent::print(uint16_t start_pos, bool clear_buffer, const char *str) {
  size_t old_message_size = this->message_buffer_.length();
  uint16_t len = strlen(str);

  if (clear_buffer) {
    this->message_buffer_.clear();
  }

  if (start_pos >= this->char_buffer_max_size_) {
    // We can't write past the end of the buffer
    return 0;
  }

  // If the string is too short, add blank spaces at the start until we get to start_pos.
  if (start_pos > this->message_buffer_.size()) {
    this->message_buffer_.resize(start_pos, ' ');
  }

  if (start_pos + len > this->char_buffer_max_size_) {
    // Adding the entire string would make us exceede the max allowable string length.
    //  Truncate the string to make the resulting string fit within the size limit.
    len = this->char_buffer_max_size_ - start_pos;
    this->message_buffer_.resize(start_pos);
    this->message_buffer_.insert(start_pos, str, len);
  } else {
    this->message_buffer_.insert(start_pos, str, len);
  }

  if ((this->message_buffer_.size() != old_message_size) && (this->scroll_state_ != HT16K33_SCROLL_STATE_STATIC)) {
    // If the new message is a different size from the old one, we restart the scrolling.
    this->scroll_state_ = HT16K33_SCROLL_STATE_FIRST_START;
    this->fist_char_location_ = 0;
  }

  return len;
}

/***********************************
 *Write a character string to the start of the display buffer.
 *
 *  clear_buffer: Boolean. Set to true to clear the display buffer before writing the string.
 *
 *  str:          The string to put in the buffer.
 *
 *  Returns the number of bytes written to the buffer.
 ************************************/
uint8_t HT16k33CharComponent::print(bool clear_buffer, const char *str) { return this->print(0, clear_buffer, str); }

/***********************************
 *Implements a printf to write a formatted string to the display buffer.
 *
 *  start_pos:    The position to place the first character in the string. Position 0 is the start
 *                of the display buffer.
 *
 *  clear_buffer: Boolean. Set to true to clear the display buffer before writing the string.
 *
 *  The remaining parameters are the normal parameters for printf.
 *
 *  Returns the number of bytes written to the buffer.
 ************************************/
uint8_t HT16k33CharComponent::printf(uint16_t start_pos, bool clear_buffer, const char *format, ...) {
  va_list arg;
  va_start(arg, format);

  // Determine how long of a string we need to hold the output of printf.
  int len = vsnprintf(NULL, 0, format, arg) + 1;  // Add 1 for the null terminator

  // Limit the output of printf to the defined max size.
  if (len > this->char_buffer_max_size_) {
    len = this->char_buffer_max_size_ + 1;  // Add 1 for the null terminator
  }

  char buffer[len];
  vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);

  return this->print(start_pos, clear_buffer, buffer);
}

/***********************************
 *Implements strftime to write a formatted time string to the display buffer.
 *
 *  start_pos:    The position to place the first character in the string. Position 0 is the start
 *                of the display buffer.
 *
 *  clear_buffer: Boolean. Set to true to clear the display buffer before writing the string.
 *
 *  format: The formatting string for strftime.
 *
 *  time: the time object to write.
 *
 *  Returns the number of bytes written to the buffer.
 ************************************/
uint8_t HT16k33CharComponent::strftime(uint16_t start_pos, bool clear_buffer, const char *format, ESPTime time) {
  std::string time_string_buffer;
  time_string_buffer = time.strftime(format);
  return this->print(start_pos, clear_buffer, time_string_buffer.c_str());
}

/***********************************
 *Implements a simplified display function to write the time to the display in the format hours:minutes
 *
 *  start_pos: The position to place the first character in the string. Position 0 is the start
 *             of the display buffer.
 *
 *  clear_buffer: Boolean. Set to true to clear the display buffer before writing the string.
 *
 *  show_leading_zero:  Boolean. Set to true to display the leading zero on hours (ex: 01:30)
 *
 *  use_ampm: Boolean. Set to true to convert the time to 12 hour time for display.
 *
 *  time: the time object to write.
 *
 *  Returns the number of bytes written to the buffer.
 ************************************/
uint8_t HT16k33CharComponent::clock_display(uint16_t start_pos, bool clear_buffer, bool show_leading_zero,
                                            bool use_ampm, ESPTime time) {
  char buffer[6];

  if (use_ampm) {
    time.strftime(buffer, sizeof(buffer), "%I:%M");
  } else {
    time.strftime(buffer, sizeof(buffer), "%H:%M");
  }

  if ((!show_leading_zero) && (buffer[0] == '0')) {
    // Clear leading zero
    buffer[0] = ' ';
  }

  return this->print(start_pos, clear_buffer, buffer);
}

}  // namespace ht16k33_char
}  // namespace esphome
