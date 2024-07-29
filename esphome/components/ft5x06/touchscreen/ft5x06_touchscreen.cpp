#include "ft5x06_touchscreen.h"

#include "esphome/core/log.h"

namespace esphome {
namespace ft5x06 {

static const char *const TAG = "ft5x06.touchscreen";

void FT5x06Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FT5x06 Touchscreen...");
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  // wait 200ms after reset.
  this->set_timeout(200, [this] { this->continue_setup_(); });
}

void FT5x06Touchscreen::continue_setup_() {
  uint8_t data[4];
  if (!this->set_mode_(FT5X06_OP_MODE))
    return;

  if (!this->err_check_(this->read_register(FT5X06_VENDOR_ID_REG, data, 1), "Read Vendor ID"))
    return;
  switch (data[0]) {
    case FT5X06_ID_1:
    case FT5X06_ID_2:
    case FT5X06_ID_3:
      this->vendor_id_ = (VendorId) data[0];
      ESP_LOGD(TAG, "Read vendor ID 0x%X", data[0]);
      break;

    default:
      ESP_LOGE(TAG, "Unknown vendor ID 0x%X", data[0]);
      this->mark_failed();
      return;
  }
  // reading the chip registers to get max x/y does not seem to work.
  if (this->display_ != nullptr) {
    if (this->x_raw_max_ == this->x_raw_min_) {
      this->x_raw_max_ = this->display_->get_native_width();
    }
    if (this->y_raw_max_ == this->y_raw_min_) {
      this->y_raw_max_ = this->display_->get_native_height();
    }
  }
  ESP_LOGCONFIG(TAG, "FT5x06 Touchscreen setup complete");
}

void FT5x06Touchscreen::update_touches() {
  uint8_t touch_cnt;
  uint8_t data[MAX_TOUCHES][6];

  if (!this->read_byte(FT5X06_TD_STATUS, &touch_cnt) || touch_cnt > MAX_TOUCHES) {
    ESP_LOGW(TAG, "Failed to read status");
    return;
  }
  if (touch_cnt == 0)
    return;

  if (!this->read_bytes(FT5X06_TOUCH_DATA, (uint8_t *) data, touch_cnt * 6)) {
    ESP_LOGW(TAG, "Failed to read touch data");
    return;
  }
  for (uint8_t i = 0; i != touch_cnt; i++) {
    uint8_t status = data[i][0] >> 6;
    uint8_t id = data[i][2] >> 3;
    uint16_t x = encode_uint16(data[i][0] & 0x0F, data[i][1]);
    uint16_t y = encode_uint16(data[i][2] & 0xF, data[i][3]);

    ESP_LOGD(TAG, "Read %X status, id: %d, pos %d/%d", status, id, x, y);
    if (status == 0 || status == 2) {
      this->add_raw_touch_position_(id, x, y);
    }
  }
}

void FT5x06Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "FT5x06 Touchscreen:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  ESP_LOGCONFIG(TAG, "  Vendor ID: 0x%X", (int) this->vendor_id_);
}

bool FT5x06Touchscreen::err_check_(i2c::ErrorCode err, const char *msg) {
  if (err != i2c::ERROR_OK) {
    this->mark_failed();
    ESP_LOGE(TAG, "%s failed - err 0x%X", msg, err);
    return false;
  }
  return true;
}
bool FT5x06Touchscreen::set_mode_(FTMode mode) {
  return this->err_check_(this->write_register(FT5X06_MODE_REG, (uint8_t *) &mode, 1), "Set mode");
}

}  // namespace ft5x06
}  // namespace esphome
