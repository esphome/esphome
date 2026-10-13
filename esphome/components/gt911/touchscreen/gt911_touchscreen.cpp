#include "gt911_touchscreen.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/gpio.h"

namespace esphome::gt911 {

ESPHOME_LOG_TAG(TAG, "gt911.touchscreen");

static const uint8_t PRIMARY_ADDRESS = 0x5D;    // default I2C address for GT911
static const uint8_t SECONDARY_ADDRESS = 0x14;  // secondary I2C address for GT911
static const uint8_t GET_TOUCH_STATE[2] = {0x81, 0x4E};
static const uint8_t CLEAR_TOUCH_STATE[3] = {0x81, 0x4E, 0x00};
static const uint8_t GET_TOUCHES[2] = {0x81, 0x4F};
static const uint8_t GET_SWITCHES[2] = {0x80, 0x4D};
static const uint8_t GET_MAX_VALUES[2] = {0x80, 0x48};
static const size_t MAX_TOUCHES = 5;  // max number of possible touches reported
static const size_t MAX_BUTTONS = 4;  // max number of buttons scanned

// Product id, firmware version and config version, logged once after setup
static const uint8_t GET_PRODUCT_ID[2] = {0x81, 0x40};
static const uint8_t GET_FIRMWARE_VERSION[2] = {0x81, 0x44};
static const uint8_t GET_CONFIG_VERSION[2] = {0x80, 0x47};

// Reset sequence (Goodix GT911 datasheet): RST low, INT set for the address, RST high, T3 >= 5 ms with INT held,
// then T4 >= 50 ms before INT turns back into the interrupt input
static constexpr uint32_t RESET_LOW_MS = 10;
static constexpr uint32_t T3_MS = 5;
static constexpr uint32_t T4_MS = 51;

#define ERROR_CHECK(err) \
  if ((err) != i2c::ERROR_OK) { \
    this->status_set_warning(LOG_STR(ESP_LOG_MSG_COMM_FAIL)); \
    return; \
  }

void GT911Touchscreen::setup() {
  if (this->reset_pin_ == nullptr) {
    // No reset, so the chip keeps whatever address the board strapped; setup_internal_ probes both
    this->setup_internal_();
    return;
  }
  this->reset_pin_->setup();
  this->reset_pin_->digital_write(false);
  if (this->interrupt_pin_ != nullptr) {
    // The INT level while reset is released selects the address: low for 0x5D, high for 0x14
    this->interrupt_pin_->setup();
    this->interrupt_pin_->pin_mode(gpio::FLAG_OUTPUT);
    this->interrupt_pin_->digital_write(this->address_ == SECONDARY_ADDRESS);
  }
  this->set_timeout(RESET_LOW_MS, [this] {
    this->reset_pin_->digital_write(true);
    this->set_timeout(T3_MS, [this] {
      // 0x14 needs INT back low once the address is latched
      if (this->interrupt_pin_ != nullptr && this->address_ == SECONDARY_ADDRESS)
        this->interrupt_pin_->digital_write(false);
      this->set_timeout(T4_MS, [this] {
        if (this->interrupt_pin_ != nullptr)
          this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT);
        this->setup_internal_();
      });
    });
  });
}

void GT911Touchscreen::setup_internal_() {
  uint8_t data[4];
  i2c::ErrorCode err = this->write(GET_SWITCHES, sizeof(GET_SWITCHES));
  if (err != i2c::ERROR_OK && (this->reset_pin_ == nullptr || this->interrupt_pin_ == nullptr)) {
    // The driver did not select the address itself, so the chip may sit on the other one
    this->address_ = this->address_ == PRIMARY_ADDRESS ? SECONDARY_ADDRESS : PRIMARY_ADDRESS;
    err = this->write(GET_SWITCHES, sizeof(GET_SWITCHES));
  }
  if (err == i2c::ERROR_OK) {
    err = this->read(data, 1);
    if (err == i2c::ERROR_OK) {
      ESP_LOGD(TAG, "Switches ADDR: 0x%02X DATA: 0x%02X", this->address_, data[0]);

      // data[0] & 1 == 1  =>  controller uses falling edge  =>  active-low
      // data[0] & 1 == 0  =>  controller uses rising  edge  =>  active-high
      bool active_high = !(data[0] & 1);

      if (this->interrupt_pin_ != nullptr) {
        if (this->interrupt_pin_->is_internal()) {
          // Direct MCU pin: attach a hardware interrupt, no polling needed.
          this->attach_interrupt_(static_cast<InternalGPIOPin *>(this->interrupt_pin_),
                                  active_high ? gpio::INTERRUPT_RISING_EDGE : gpio::INTERRUPT_FALLING_EDGE);
          ESP_LOGD(TAG, "Interrupt pin: hardware interrupt, active %s",
                   active_high ? LOG_STR_LITERAL("HIGH") : LOG_STR_LITERAL("LOW"));
        } else {
          // IO expander pin: leave as output for configuration only.
          ESP_LOGD(TAG, "Interrupt pin: IO expander polling mode, active %s",
                   active_high ? LOG_STR_LITERAL("HIGH") : LOG_STR_LITERAL("LOW"));
        }
      }
    }
  }

  if (this->x_raw_max_ == 0 || this->y_raw_max_ == 0) {
    // no calibration? Attempt to read the max values from the touchscreen.
    if (err == i2c::ERROR_OK) {
      err = this->write(GET_MAX_VALUES, sizeof(GET_MAX_VALUES));
      if (err == i2c::ERROR_OK) {
        err = this->read(data, sizeof(data));
        if (err == i2c::ERROR_OK) {
          this->x_raw_max_ = encode_uint16(data[1], data[0]);
          this->y_raw_max_ = encode_uint16(data[3], data[2]);
          if (this->swap_x_y_)
            std::swap(this->x_raw_max_, this->y_raw_max_);
        }
      }
    }
    if (err != i2c::ERROR_OK) {
      this->mark_failed(LOG_STR("Calibration error"));
      return;
    }
  }

  if (err != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->setup_done_ = true;
  this->log_device_info_();
}

void GT911Touchscreen::log_device_info_() {
  uint8_t product[4];
  uint8_t firmware[2];
  uint8_t config;
  if (this->write(GET_PRODUCT_ID, sizeof(GET_PRODUCT_ID)) != i2c::ERROR_OK ||
      this->read(product, sizeof(product)) != i2c::ERROR_OK ||
      this->write(GET_FIRMWARE_VERSION, sizeof(GET_FIRMWARE_VERSION)) != i2c::ERROR_OK ||
      this->read(firmware, sizeof(firmware)) != i2c::ERROR_OK ||
      this->write(GET_CONFIG_VERSION, sizeof(GET_CONFIG_VERSION)) != i2c::ERROR_OK ||
      this->read(&config, 1) != i2c::ERROR_OK) {
    return;
  }
  ESP_LOGD(TAG, "Product %c%c%c%c, firmware 0x%04X, config version 0x%02X", product[0], product[1], product[2],
           product[3], encode_uint16(firmware[1], firmware[0]), config);
}

void GT911Touchscreen::update_touches() {
  this->skip_update_ = true;  // skip send touch events by default, set to false after successful error checks
  if (!this->setup_done_) {
    return;
  }

  i2c::ErrorCode err;
  uint8_t touch_state = 0;
  uint8_t data[MAX_TOUCHES + 1][8];  // 8 bytes each for each point, plus extra space for the key byte

  err = this->write(GET_TOUCH_STATE, sizeof(GET_TOUCH_STATE));
  ERROR_CHECK(err);
  err = this->read(&touch_state, 1);
  ERROR_CHECK(err);
  this->write(CLEAR_TOUCH_STATE, sizeof(CLEAR_TOUCH_STATE));
  uint8_t num_of_touches = touch_state & 0x07;

  if ((touch_state & 0x80) == 0 || num_of_touches > MAX_TOUCHES) {
    return;
  }

  err = this->write(GET_TOUCHES, sizeof(GET_TOUCHES));
  ERROR_CHECK(err);
  // num_of_touches is guaranteed to be 0..5. Also read the key data
  err = this->read(data[0], sizeof(data[0]) * num_of_touches + 1);
  ERROR_CHECK(err);

  this->skip_update_ = false;  // All error checks passed, send touch events
  for (uint8_t i = 0; i != num_of_touches; i++) {
    uint16_t id = data[i][0];
    uint16_t x = encode_uint16(data[i][2], data[i][1]);
    uint16_t y = encode_uint16(data[i][4], data[i][3]);
    this->add_raw_touch_position_(id, x, y);
  }
  auto keys = data[num_of_touches][0] & ((1 << MAX_BUTTONS) - 1);
  if (keys != this->button_state_) {
    this->button_state_ = keys;
    for (size_t i = 0; i != MAX_BUTTONS; i++) {
      for (auto *listener : this->button_listeners_)
        listener->update_button(i, (keys & (1 << i)) != 0);
    }
  }
}

void GT911Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "GT911 Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

}  // namespace esphome::gt911
