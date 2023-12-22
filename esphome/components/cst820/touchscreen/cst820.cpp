#include "cst820.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"

#include <algorithm>
#include <cinttypes>

namespace esphome {
namespace cst820 {

static const char *const TAG = "cst820";


void CST820Touchscreen::setup() {
  static const uint8_t power[] = {0xFF};
  write_register(REG_CST820_AUTOSLEEP, power, sizeof(power));  // Disable automatic entry into low power mode

  // Read chip ID
  uint8_t chip_id;
  uint8_t err = read_register(REG_CST820_CHIPID, &chip_id, 1);

  // Read chip firmware version
  uint8_t firmware_version;
  err = read_register(REG_CST820_FIRMWAREVERSION, &firmware_version, 1);
  
  ESP_LOGI("cst820", "Found chip id: %d, firmware version: %d", chip_id, firmware_version);

  uint8_t irq;
  err = read_register(REG_CST820_IRQCONTROL, &irq, 1);
  ESP_LOGI("cst820", "Interupt setting: %d", irq);
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

void CST820Touchscreen::update_touches() {
  uint32_t now = millis();
  uint8_t fingerindex;
  uint8_t gesture;
  uint8_t err;
  err = read_register(REG_CST820_FINGERINDEX, &fingerindex, 1);
  if (err) {
    ESP_LOGD("TAG", "Reg 0x2 Error: %i", err);
    return;
  }
  if (fingerindex == 0) 
    return;
  
  // ESP_LOGD(TAG, "Fingerindex: %i", fingerindex);

  uint8_t data[4];
  err = read_register(REG_CST820_XPOSH, data, 4);
  if (err) {
    ESP_LOGI(TAG, "Failed to read register 0x3");
    return;
  };

  u_int16_t x, y;
  x = (((data[0] & 0x0f) << 8) | data[1]);
  y = (((data[2] & 0x0f) << 8) | data[3]);

  set_raw_touch_position_(0, x, y);
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
