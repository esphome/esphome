#include "cst820.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"

#include <algorithm>
#include <cinttypes>

namespace esphome {
namespace cst820 {

static const char *const TAG = "cst820";

void CST820TouchscreenStore::gpio_intr(CST820TouchscreenStore *store) { store->touch = true; }

void CST820Touchscreen::setup() {
  static const uint8_t power[] = {0xFF};
  write_register(0xFE, power, sizeof(power));  // Disable automatic entry into low power mode
  // Update display dimensions if they were updated during display setup
  this->x_raw_min_ = 0;
  this->y_raw_min_ = 0;
  if (this->swap_x_y_) {
    this->x_raw_max_ = this->get_height_();
    this->y_raw_max_ = this->get_width_();
  } else {
    this->x_raw_max_ = this->get_width_();
    this->y_raw_max_ = this->get_height_();
  };
}

/*
void CST820Touchscreen::loop() {
  if ((this->irq_pin_ != nullptr) && (this->store_.touch || this->touched)) {
    this->store_.touch = false;
    check_touch_();
  }
}

void CST820Touchscreen::update() { check_touch_(); }
*/

void CST820Touchscreen::update_touches() {
  uint32_t now = millis();
  uint8_t fingerindex;
  uint8_t gesture;
  uint8_t err;
  err = read_register(0x02, &fingerindex, 1);
  if (err) {
    ESP_LOGD("TAG", "Reg 0x2 Error: %i", err);
  }
  if (fingerindex == 0) {
    if (this->touched) {
      this->touched = false;
      for (auto *listener : this->touch_listeners_) {
        listener->release();
      };
    };
    return;
  };
  // ESP_LOGD(TAG, "Fingerindex: %i", fingerindex);

  uint8_t data[4];
  err = read_register(0x03, data, 4);
  if (err) {
    ESP_LOGI(TAG, "Failed to read register 0x3");
    return;
  };

  u_int16_t x, y;
  x = (((data[0] & 0x0f) << 8) | data[1]);
  y = (((data[2] & 0x0f) << 8) | data[3]);

  set_raw_touch_position_(0, x, y);
  /*TouchPoint touchpoint;
  if (!this->invert_x_) {
    touchpoint.x = x;
  } else {
    touchpoint.x = this->x_raw_max_ - x;
  };
  if (!this->invert_y_) {
    touchpoint.y = y;
  } else {
    touchpoint.y = this->y_raw_max_ - y;
  };
  if (!this->touched || (now - this->last_pos_ms_) >= this->report_millis_) {
    this->defer([this, touchpoint]() { this->send_touch_(touchpoint); });
    this->x = touchpoint.x;
    this->y = touchpoint.y;
    this->touched = true;
    this->last_pos_ms_ = now;
  };

  ESP_LOGD(TAG, "Touching at [%d, %d]", x, y); */ //, touchpoint.x, touchpoint.y);
}


void CST820Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CST820:");
  LOG_I2C_DEVICE(this);

  // LOG_PIN("  IRQ Pin: ", this->irq_pin_);


  //#LOG_UPDATE_INTERVAL(this);
}

float CST820Touchscreen::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace cst820
}  // namespace esphome
