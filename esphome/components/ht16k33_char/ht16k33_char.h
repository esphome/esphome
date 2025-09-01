#pragma once

#include "esphome/core/component.h"
#include "esphome/core/time.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ht16k33_char {

static const uint8_t HT16K33_DISPLAY_DATA_ADDRESS = 0x00;
static const uint8_t HT16K33_SYSTEM_SETUP = 0x20;
static const uint8_t HT16K33_KEY_DATA_ADDRESS = 0x40;
static const uint8_t HT16K33_INT_FLAG_ADDRESS = 0x60;
static const uint8_t HT16K33_DISPLAY_SETUP = 0x80;
static const uint8_t HT16K33_ROW_INT_SET = 0xA0;
static const uint8_t HT16K33_DIMMING_SET = 0xE0;

static const uint8_t HT16K33_DISPLAY_OFF = 0x00;
static const uint8_t HT16K33_DISPLAY_ON = 0x01;
static const uint8_t HT16K33_MODE_STANDBY = 0x00;
static const uint8_t HT16K33_MODE_NORMAL = 0x01;

class HT16k33CharComponent;

// We can have up to 7 chips. Chip addresses are 0b1110xxx. Default is 0b1110000 (0x70).
// For 7 segment displays the 28 pin package could address up to 8 digits.
// So an absolute maximum number of chars is 8*7=56.

// TODO: Explain what this does...
using ht16k33_char_writer_t = std::function<void(HT16k33CharComponent &)>;

class HT16k33CharComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;  // Called at the update interval set during device config
  void loop() override;    // Called at the speed of the main loop (approx every 16ms)
  void dump_config() override;

  void set_writer(ht16k33_char_writer_t &&writer) { this->writer_ = writer; };
  float get_setup_priority() const override;
  uint8_t update_display();

  void add_char(const char *char_to_add, uint16_t char_code) { this->char_map_[char_to_add[0]] = char_code; };
  void remove_char(const char *char_to_remove) { this->char_map_.erase(char_to_remove[0]); };

  // This needs to have the stub or it won't work. This function is replaced by the device specific functions in
  // the subclasses.
  virtual uint8_t send_to_display(i2c::I2CDevice *display, uint8_t position) { return 0; };

  void set_brightness(uint8_t brightness) { this->brightness_ = brightness - 1; };
  void set_buffer_size(uint8_t size_to_set) { this->char_buffer_size_ = size_to_set; };

  // Called automatically during setup to generate a list of I2CDevices that represent the displays.
  // We iterate through the displays_ to address individual displays during runtime.
  void add_secondary_display(i2c::I2CDevice *display) { this->displays_.push_back(display); }

  void set_scroll(bool scroll) { this->scroll_ = scroll; }
  void set_continuous(bool continuous) { this->continuous_ = continuous; }
  void set_scroll_speed(uint32_t scroll_speed) { this->scroll_speed_ = scroll_speed; }
  void set_scroll_dwell(uint32_t scroll_dwell) { this->scroll_dwell_ = scroll_dwell; }
  void set_scroll_delay(uint32_t scroll_delay) { this->scroll_delay_ = scroll_delay; }

  void brightness(uint8_t brightness_to_set);
  void set_blink(uint8_t blink_state);
  void display_off(bool turn_off);
  void display_standby(bool standby);

  // Evaluate the printf-format and print the result at the given position.
  uint8_t printf(uint8_t start_pos, bool clear_buffer, const char *format, ...) __attribute__((format(printf, 4, 5)));

  // Print `str` at the given position.
  uint8_t print(uint8_t start_pos, bool clear_buffer, const char *str);

  // Print `str` at position 0.
  uint8_t print(bool clear_buffer, const char *str);

  // Evaluate the strftime-format and print the result at the given position.
  uint8_t strftime(uint8_t start_pos, bool clear_buffer, const char *format, ESPTime time)
      __attribute__((format(strftime, 4, 0)));

  uint8_t clock_display(uint8_t start_pos, bool clear_buffer, bool show_leading_zero, bool use_ampm, ESPTime time);

  void blank();

  /// Evaluate the strftime-format and print the result at position 0.
  uint8_t strftime(const char *format, ESPTime time) __attribute__((format(strftime, 2, 0)));

 protected:
  std::unordered_map<char, uint16_t> char_map_ = {{' ', 0b0000000000000000}};

  uint8_t scroll_state_;

  std::vector<i2c::I2CDevice *> displays_{this};

  uint8_t fist_char_location_;

  bool scroll_{false};
  bool continuous_{false};
  uint32_t scroll_speed_{250};
  uint32_t scroll_dwell_{2000};
  uint32_t scroll_delay_{750};
  uint32_t last_scroll_{0};

  uint8_t brightness_{15};  // Brightness of the display from 0 (off) to 15 (brightest)

  std::string char_buffer_;   // This buffer holds the entire character message to display.
  uint8_t buffer_[20];        // This buffer is used to send the raw bytes to the HT16k33 device. TODO: Make this 17?
  uint8_t char_buffer_size_;  // This is the length of the character buffer. I need to track this separately instead of
                              // just calling buffer.length(), since when I clear the buffer, it resets the size to 0.
                              // TODO: Maybe a different data type would be better here?

  optional<ht16k33_char_writer_t> writer_{};
};

}  // namespace ht16k33_char
}  // namespace esphome
