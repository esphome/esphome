#include "cst860.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"

#include <algorithm>
#include <cinttypes>

namespace esphome {
namespace cst860 {

static const char *const TAG = "cst860";

void CST860TouchscreenStore::gpio_intr(CST860TouchscreenStore *store) { store->touch = true; }

void CST860Component::setup() {
  static const uint8_t power[] = { 0xFF };
  write_register(0xFE, power, sizeof(power)); // Disable automatic entry into low power mode
}

void CST860Component::loop() {
  if ((this->irq_pin_ != nullptr) && (this->store_.touch || this->touched)) {
    this->store_.touch = false;
    check_touch_();
  }
}

void CST860Component::update() {
  check_touch_();
}

void CST860Component::check_touch_() {
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
  ESP_LOGD(TAG, "Fingerindex: %i", fingerindex);

  uint8_t data[4];
  err = read_register(0x03, data, 4);
  if (err) {
    ESP_LOGI(TAG, "Failed to read register 0x3");
    return;
  };

  u_int16_t x, y;
  if (this->swap_x_y_) {
    y = (((data[0] & 0x0f) << 8) | data[1]);
    x = (((data[2] & 0x0f) << 8) | data[3]);
  } else {
    x = (((data[0] & 0x0f) << 8) | data[1]);
    y = (((data[2] & 0x0f) << 8) | data[3]);
  }

  TouchPoint touchpoint;
  if (!this->invert_x_)
    touchpoint.x = x;
  else
    touchpoint.x = this->x_raw_max_ - x;
  if (!this->invert_y_)
    touchpoint.y = y;
  else
    touchpoint.y = this->y_raw_max_ - y;

  if (!this->touched || (now - this->last_pos_ms_) >= this->report_millis_) {
    this->defer([this, touchpoint]() { this->send_touch_(touchpoint); });
    this->x = touchpoint.x;
    this->y = touchpoint.y;
    this->touched = true;
    this->last_pos_ms_ = now;
  };

  ESP_LOGD(TAG, "Touching at [%d, %d]", x, y); //, touchpoint.x, touchpoint.y);
}

void CST860Component::set_calibration(int16_t x_max, int16_t y_max, bool invert_x, bool invert_y) {  // NOLINT
  this->x_raw_max_ = x_max;
  this->y_raw_max_ = y_max;
  this->invert_x_ = invert_x;
  this->invert_y_ = invert_y;
}

void CST860Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CST860:");

  LOG_PIN("  IRQ Pin: ", this->irq_pin_);
  ESP_LOGCONFIG(TAG, "  X max: %d", this->x_raw_max_);
  ESP_LOGCONFIG(TAG, "  Y max: %d", this->y_raw_max_);

  ESP_LOGCONFIG(TAG, "  Swap X/Y: %s", YESNO(this->swap_x_y_));

  ESP_LOGCONFIG(TAG, "  Report interval: %" PRIu32, this->report_millis_);

  LOG_UPDATE_INTERVAL(this);
}

float CST860Component::get_setup_priority() const { return setup_priority::DATA; }



}  // namespace cst860
}  // namespace esphome
