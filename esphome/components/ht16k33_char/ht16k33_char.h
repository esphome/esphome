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

// Return codes from handle_special_char_()
static const uint8_t SPECIAL_CHAR_NOT_FOUND = 0x00;      // Not a special char.
static const uint8_t SPECIAL_CHAR_FOUND = 0x01;          // Special char found and handled
static const uint8_t SPECIAL_CHAR_FOUND_ADVANCE = 0x02;  // Special char found and handled, advance display if the
                                                         // special char was in the first position of the first display.

class HT16k33CharComponent;

// We can have up to 7 chips. Chip addresses are 0b1110xxx. Default is 0b1110000 (0x70).
// For 7 segment displays the 28 pin package could address up to 8 digits.
// So an absolute maximum number of chars is 8*7=56.

// defines a type `ht16k33_char_writer_t` that is a pointer to a function of the type defined.
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

  void add_char(const char *char_to_add, uint16_t char_code);
  void remove_char(const char *char_to_remove) { this->char_map_.erase(char_to_remove); };

  void set_brightness(uint8_t brightness) { this->brightness_ = brightness - 1; };
  void set_buffer_max_size(uint16_t size_to_set) { this->char_buffer_max_size_ = size_to_set; };

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
  uint8_t printf(uint16_t start_pos, bool clear_buffer, const char *format, ...) __attribute__((format(printf, 4, 5)));

  // Print `str` at the given position.
  uint8_t print(uint16_t start_pos, bool clear_buffer, const char *str);

  // Print `str` at position 0.
  uint8_t print(bool clear_buffer, const char *str);

  // Evaluate the strftime-format and print the result at the given position.
  uint8_t strftime(uint16_t start_pos, bool clear_buffer, const char *format, ESPTime time)
      __attribute__((format(strftime, 4, 0)));

  uint8_t clock_display(uint16_t start_pos, bool clear_buffer, bool show_leading_zero, bool use_ampm, ESPTime time);

  void blank();

  /// Evaluate the strftime-format and print the result at position 0.
  uint8_t strftime(const char *format, ESPTime time) __attribute__((format(strftime, 2, 0)));

 protected:
  std::unordered_map<std::string, uint16_t> char_map_ = {{" ", 0b0000000000000000}};

  // These two functions are overridden by device specific versions in the subclasses.
  virtual uint8_t handle_special_char(char char_to_find, uint8_t position) { return 0; };
  virtual void write_to_buffer(uint16_t char_to_write, uint8_t char_position){};

  uint8_t get_next_char_(uint16_t start_position, std::string *next_char);
  void clear_buffer_();
  uint8_t char_len_(char char_to_test);
  uint16_t send_to_display_common_(i2c::I2CDevice *display, uint16_t position);

  uint8_t scroll_state_;
  uint8_t num_chars_per_display_{0};  // The number of characters per display. This should be set by the derived class.

  std::vector<i2c::I2CDevice *> displays_{this};

  uint8_t fist_char_location_;

  bool scroll_{false};
  bool continuous_{false};
  uint32_t scroll_speed_{250};
  uint32_t scroll_dwell_{2000};
  uint32_t scroll_delay_{750};
  uint32_t last_scroll_{0};

  uint8_t brightness_{15};  // Brightness of the display from 0 (off) to 15 (brightest)

  std::string message_buffer_;  // This buffer holds the entire character message to display.
  uint8_t buffer_[20];          // This buffer is used to send the raw bytes to the HT16k33 device.
  uint16_t
      char_buffer_max_size_;  // This is the maximum allowable length of the char buffer. This should be set to some
                              //  reasonable number greater than the expected length of the strings that will be
                              //  displayed. Note that this is not a hard limit, as there is a null terminating
                              //  character that may be added into this length. This is also a limit in bytes, not
                              //  characters. This means that if multi-byte characters are used, the number of displayed
                              //  characters will be less than the number defined here.

  optional<ht16k33_char_writer_t> writer_{};
};

}  // namespace ht16k33_char
}  // namespace esphome
