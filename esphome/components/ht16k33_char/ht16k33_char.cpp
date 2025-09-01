#include <unordered_map>

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

#include "ht16k33_char.h"

/* To add more device types:
 *    -Add to the HT16K33_DEVICE_TYPES enum in the display.py file
 *    -Add a new .h and .c file that defines a class derived from the `HT16k33CharComponent` class.
 *    -Implement a `uint8_t send_to_display(i2c::I2CDevice *display, uint8_t position)` function in that class.
 *        -display: the i2c device of the current display to use. The code will step through the
 *         defined displays and call the send_to_display() function for each one.
 *        -position: The position in the char buffer to start writing to the display. Starts at 0.
 *        -returns the position in the char buffer for the next character that should be displayed
 *         on the next display.
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
  this->char_buffer_.resize(this->char_buffer_size_, ' ');
  this->fist_char_location_ = 0;

  // Check to see if we need to scroll the display.
  if (!(this->scroll_)) {
    // Scrolling is off.
    this->scroll_state_ = HT16K33_SCROLL_STATE_STATIC;
  } else if (this->continuous_) {
    // If the state is continuous, there is no start and end delay. Go directly into the scrolling.
    this->scroll_state_ = HT16K33_SCROLL_STATE_SCROLLING;
    this->last_scroll_ = millis();
  } else {
    this->scroll_state_ = HT16K33_SCROLL_STATE_FIRST_START;
    this->last_scroll_ = millis();
  }
}

void HT16k33CharComponent::update() {
  // TODO: Look into getting rid of the assumption of fixed length for the message to display.
  //       Resize the buffer when a new message is added. I am not sure of the memory implications of doing that...
  // ESP_LOGD("dbg", "Max Size: %d", this->char_buffer_.max_size());
  //       -Max Size: 2147483647

  ESP_LOGD("dbg", "String is: |%s|", this->char_buffer_.c_str());
  ESP_LOGD("dbg", "Size is: %d", this->char_buffer_.size());

  // This checks if the lambda function is defined. If it is not defined, we don't do anything.
  if (this->writer_.has_value()) {
    // This line is responsible for calling the lambda code.
    (*this->writer_)(*this);

    // The lambda code does not actually update the display directly. It manipulates the char buffer.
    //   - If the display is static (no scrolling), we directly call display() to update the display now.
    //   - If scrolling is happening, we do not update the display in this function. The display will
    //     be updated in the loop() function.
    //   - if we are in the state 'FIRST_START' this means we just started the device. In that state,
    //     the display will not be showing anything yet, and we need to run the update_display()
    //     function to show the initial contents.
    if ((this->scroll_state_ == HT16K33_SCROLL_STATE_STATIC) ||
        (this->scroll_state_ == HT16K33_SCROLL_STATE_FIRST_START)) {
      this->update_display();
    }
  }
}

// Note: Scroll that is not continuous will go to the end of the buffer size, not the end of the message in the buffer.
void HT16k33CharComponent::loop() {
  uint32_t now;
  uint8_t current_buffer_location;

  if (this->scroll_state_ == HT16K33_SCROLL_STATE_STATIC) {
    // Check this first. If the display is static, we don't need to do anything in this function.
    return;
  }

  now = millis();

  if (this->last_scroll_ > now) {
    // This will happen when the millis() function overflows. (approx every 50 days)
    this->last_scroll_ = now;
    return;
  }

  switch (this->scroll_state_) {
    case HT16K33_SCROLL_STATE_START:
    case HT16K33_SCROLL_STATE_FIRST_START:
      if ((now - this->last_scroll_) >= this->scroll_delay_) {
        // Start scrolling
        this->last_scroll_ = now;
        this->fist_char_location_++;
        current_buffer_location = this->update_display();
        if (current_buffer_location >= this->char_buffer_.length()) {
          // We reached the end of the char buffer before we reached the end of the display.
          // Scrolling is not required.
          this->scroll_state_ = HT16K33_SCROLL_STATE_STATIC;
        } else {
          this->scroll_state_ = HT16K33_SCROLL_STATE_SCROLLING;
        }
      }
      break;

    case HT16K33_SCROLL_STATE_SCROLLING:
      if ((now - this->last_scroll_) >= this->scroll_speed_) {
        // Scroll to the next character.
        this->last_scroll_ = now;
        this->fist_char_location_++;
        if (this->fist_char_location_ >= this->char_buffer_.length()) {
          // This only happens in continuous mode.
          this->fist_char_location_ = 0;
        }
        current_buffer_location = this->update_display();

        if (!(this->continuous_) && ((current_buffer_location + 1) >= this->char_buffer_.length())) {
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

  // ESP_LOGCONFIG(TAG, "  Device Type: ");  //TODO: Do I add a string to be able to show the device type?
  ESP_LOGCONFIG(TAG, "  Buffer Length: %d", this->char_buffer_size_);
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
    buffer_location = this->send_to_display(display, buffer_location);
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
 *Write a character string to the display buffer.
 *
 *  start_pos:    The position to place the first character in the string. Position 0 is the start
 *                of the display buffer.
 *
 *  str:          The string to put in the buffer.
 *
 *  clear_buffer: Boolean. Set to true to clear the display buffer before writing the string.
 *
 *  Returns the number of bytes written to the buffer.
 ************************************/
uint8_t HT16k33CharComponent::print(uint8_t start_pos, bool clear_buffer, const char *str) {
  uint8_t top;
  uint8_t j;

  if (start_pos >= this->char_buffer_.length()) {
    // Start position is after the end of the buffer
    return 0;
  }

  if ((start_pos + strlen(str)) <= this->char_buffer_.length()) {
    // The entire string will fit in the buffer
    top = (start_pos + strlen(str)) - 1;
  } else {
    top = this->char_buffer_.length() - 1;
  }

  if (clear_buffer) {
    this->char_buffer_.clear();
    this->char_buffer_.resize(this->char_buffer_size_, ' ');
  }

  j = 0;
  for (uint8_t i = start_pos; i <= top; i++) {
    this->char_buffer_.at(i) = str[j];
    j++;
  }

  return j - 1;
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
uint8_t HT16k33CharComponent::printf(uint8_t start_pos, bool clear_buffer, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[this->char_buffer_size_ + 1];  // Add one for the string terminating character.
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
uint8_t HT16k33CharComponent::strftime(uint8_t start_pos, bool clear_buffer, const char *format, ESPTime time) {
  char buffer[64];  // TODO: This buffer is really big, I should make it smaller.
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0) {
    return this->print(start_pos, clear_buffer, buffer);
  }
  return 0;
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
uint8_t HT16k33CharComponent::clock_display(uint8_t start_pos, bool clear_buffer, bool show_leading_zero, bool use_ampm,
                                            ESPTime time) {
  char buffer[6];
  // TODO: strftime is very memory intensive if all I need is hours and minutes. I could rewrite this to not use
  // strftime and save a bunch of flash

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
