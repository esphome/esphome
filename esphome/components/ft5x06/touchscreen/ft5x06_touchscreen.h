#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ft5x06 {

struct Store {
 public:
  volatile bool available;
  static void IRAM_ATTR HOT gpio_intr(Store *store) { store->available = true; }
};

static const char *const TAG = "ft5x06.touchscreen";

enum VendorId {
  FT5X06_ID_UNKNOWN = 0,
  FT5X06_ID_1 = 0x51,
  FT5X06_ID_2 = 0x11,
  FT5X06_ID_3 = 0xCD,
};

enum FTCmd {
  FT5X06_VENDOR_ID_REG = 0xA8,
  FT5X06_TD_STATUS = 0x02,
  FT5X06_TOUCH_DATA = 0x03,
  FT5X06_I_MODE = 0xA4,
};

static const size_t MAX_TOUCHES = 5;  // max number of possible touches reported

class FT5x06ButtonListener {
 public:
  virtual void update_button(uint8_t index, bool state) = 0;
};

class FT5x06Touchscreen : public touchscreen::Touchscreen, public Component, public i2c::I2CDevice {
 public:
  void set_rotation(touchscreen::TouchRotation rotation) { this->rotation_ = rotation; }
  void set_interrupt_pin(GPIOPin *pin) { this->interrupt_pin_ = pin; }
  void register_button_listener(FT5x06ButtonListener *listener) { this->button_listeners_.push_back(listener); }

  void setup() override {
    i2c::ErrorCode err;
    ESP_LOGCONFIG(TAG, "Setting up FT5x06 Touchscreen...");
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT);
    this->interrupt_pin_->setup();
    // wait 200ms after reset.
    this->set_timeout(200, [this] { this->continue_setup_(); });
  }

  void continue_setup_(void) {
    i2c::ErrorCode err;
    uint8_t data;

    if (!this->read_byte(FT5X06_VENDOR_ID_REG, &data)) {
      esph_log_e(TAG, "Failed to read vendor ID");
      this->mark_failed();
      return;
    }
    switch (data) {
      case FT5X06_ID_1:
      case FT5X06_ID_2:
      case FT5X06_ID_3:
        this->vendor_id_ = (VendorId) data;
        esph_log_d(TAG, "Read vendor ID 0x%X", data);
        break;

      default:
        esph_log_e(TAG, "Unknown vendor ID 0x%X", data);
        this->mark_failed();
        return;
    }

    if (this->interrupt_pin_->is_internal()) {
      this->is_int_internal_ = true;
      static_cast<InternalGPIOPin *>(this->interrupt_pin_)
          ->attach_interrupt(Store::gpio_intr, &this->store_, gpio::INTERRUPT_FALLING_EDGE);
      this->write_byte(FT5X06_I_MODE, 1);
    } else {
      this->write_byte(FT5X06_I_MODE, 0);
    }
    ESP_LOGCONFIG(TAG, "FT5x06 Touchscreen setup complete");
  }

  void loop() override {
    touchscreen::TouchPoint tp;
    uint8_t touch_cnt;
    uint8_t data[MAX_TOUCHES][6];  // 8 bytes each for each point, plus extra space for the key byte

    if (this->is_int_internal_) {
      if (!this->store_.available)
        return;
      this->store_.available = false;
    } else if (!this->was_down_) {
      if (this->interrupt_pin_->digital_read())
        return;
    }
    if (!this->read_byte(FT5X06_TD_STATUS, &touch_cnt) || touch_cnt > MAX_TOUCHES) {
      esph_log_w(TAG, "Failed to read status");
      return;
    }
    if (touch_cnt == 0) {
      this->send_release_();
      this->was_down_ = false;
      esph_log_d(TAG, "Send release");
      return;
    }
    if (!this->read_bytes(FT5X06_TOUCH_DATA, (uint8_t *) data, touch_cnt * 6)) {
      esph_log_w(TAG, "Failed to read touch data");
      return;
    }
    this->was_down_ = true;

    for (uint8_t i = 0; i != touch_cnt; i++) {
      uint8_t status = data[i][0] >> 6;
      tp.id = data[i][2] >> 3;
      uint16_t x = encode_uint16(data[i][0] & 0x0F, data[i][1]);
      uint16_t y = encode_uint16(data[i][2] & 0xF, data[i][3]);

      switch (this->rotation_) {
        case touchscreen::ROTATE_0_DEGREES:
          tp.x = x;
          tp.y = y;
          break;
        case touchscreen::ROTATE_90_DEGREES:
          tp.x = y;
          tp.y = this->display_width_ - x;
          break;
        case touchscreen::ROTATE_180_DEGREES:
          tp.x = this->display_width_ - x;
          tp.y = this->display_height_ - y;
          break;
        case touchscreen::ROTATE_270_DEGREES:
          tp.x = this->display_height_ - y;
          tp.y = x;
          break;
      }
      esph_log_d(TAG, "Read %X status, id: %d, pos %d/%d", status, tp.id, tp.x, tp.y);
      if (status == 0 || status == 2) {
        this->defer([this, tp]() { this->send_touch_(tp); });
      }
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "FT5x06 Touchscreen:");
    LOG_I2C_DEVICE(this);
    LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
    ESP_LOGCONFIG(TAG, "  Rotation: %d", (int) this->rotation_);
    ESP_LOGCONFIG(TAG, "  Vendor ID: 0x%X", (int) this->vendor_id_);
  }

 protected:
  GPIOPin *interrupt_pin_;
  bool is_int_internal_{};
  bool was_down_{};
  Store store_;
  std::vector<FT5x06ButtonListener *> button_listeners_;
  VendorId vendor_id_{FT5X06_ID_UNKNOWN};
};

}  // namespace ft5x06
}  // namespace esphome
