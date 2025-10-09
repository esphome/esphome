#include "cst92xx_touchscreen.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace cst92xx {

void CST92xxTouchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");

  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(5);
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);

    this->set_timeout(30, [this] { this->continue_setup_(); });
  } else {
    this->continue_setup_();
  }
}

void CST92xxTouchscreen::continue_setup_() {
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
  }

  uint8_t buffer[4] = {0xD1, 0x01, 0x00, 0x00};

  ESP_LOGCONFIG(TAG, "Entering command mode");

  if (this->write_register16(CST92XX_CMD_MODE_REG, buffer, sizeof(2)) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Write byte to 0xD101 (enter command mode) failed");
    this->mark_failed();
    return;
  }

  delay(10);

  if (this->read16_(CST92XX_CHECKCODE_REG, buffer, 4)) {
    uint16_t chip_id = buffer[2] + (buffer[3] << 8);
    uint16_t project_id = buffer[0] + (buffer[1] << 8);

    ESP_LOGCONFIG(TAG, "Checkcode: 0x%02X%02X%02X%02X", buffer[0], buffer[1], buffer[2], buffer[3]);
  }

  if (this->x_raw_max_ == 0 || this->y_raw_max_ == 0) {
    if (this->read16_(CST92XX_RESOLUTION_REG, buffer, 4)) {
      this->x_raw_max_ = buffer[0] + (buffer[1] << 8);
      this->y_raw_max_ = buffer[2] + (buffer[3] << 8);

      ESP_LOGCONFIG(TAG, "Got CST92XX size %dx%d", this->x_raw_max_, this->y_raw_max_);
    } else {
      this->x_raw_max_ = this->display_->get_native_width();
      this->y_raw_max_ = this->display_->get_native_height();
    }
  }

  if (this->read16_(CST92XX_PROJECT_ID_REG, buffer, 4)) {
    uint16_t chip_id = buffer[2] + (buffer[3] << 8);
    uint16_t project_id = buffer[0] + (buffer[1] << 8);

    this->chip_id_ = chip_id;

    ESP_LOGCONFIG(TAG, "Chip ID 0x%04X, project ID %x", chip_id, project_id);
  }

  ESP_LOGCONFIG(TAG, "CST92xx Touchscreen setup complete");
}

bool CST92xxTouchscreen::read16_(uint16_t addr, uint8_t *data, size_t len) {
  if (this->read_register16(addr, data, len) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Read data from 0x%04X failed", addr);
    this->mark_failed();
    return false;
  }
  return true;
}

void CST92xxTouchscreen::dump_config() {
  ESP_LOGCONFIG(TAG, "CST92xx Touchscreen:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);

  ESP_LOGCONFIG(TAG,
                "  X Raw Min: %d, X Raw Max: %d\n"
                "  Y Raw Min: %d, Y Raw Max: %d",
                this->x_raw_min_, this->x_raw_max_, this->y_raw_min_, this->y_raw_max_);

  const char *name;
  switch (this->chip_id_) {
    case CST9217_CHIP_ID:
      name = "CST9217";
      break;
    case CST9220_CHIP_ID:
      name = "CST9220";
      break;
    default:
      name = "Unknown";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Chip type: %s", name);
}

void CST92xxTouchscreen::update_touches() {
  uint8_t data[CST92XX_DATA_LENGTH];

  if (!this->read16_(CST92XX_DATA_REG, data, sizeof data)) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

  // for (size_t i = 0; i < sizeof(data); i++)
  //   ESP_LOGD(TAG, "data[%d] 0x%02X", i, data[i]);

  if (data[6] != CST92XX_ACK_VALUE || data[0] == CST92XX_ACK_VALUE || data[5] == 0x80) {
    this->skip_update_ = true;
    return;
  }

  /*uint8_t ack_data[1] = { 0xD1 };
  if(!this->write_register16(CST92XX_DATA_REG, ack_data, sizeof(ack_data))) {
    ESP_LOGE(TAG, "Sending ack failed");
    this->status_set_warning();
    return;
  }*/

  uint8_t num_of_touches = data[5] & 0x7F;
  // ESP_LOGD(TAG, "Got %d touches", num_of_touches);

  for (uint8_t i = 0; i != num_of_touches; i++) {
    uint8_t *p = &data[i * 5 + (i ? 2 : 0)];
    uint8_t status = p[0] & 0x0F;

    if (status == 0x06) {
      int16_t x = ((p[1] << 4) | (p[3] >> 4));
      int16_t y = ((p[2] << 4) | (p[3] & 0x0F));

      // ESP_LOGD(TAG, "Read touch %d: %d %d", i, x, y);

      this->add_raw_touch_position_(i, x, y);
    }
  }
}

}  // namespace cst92xx
}  // namespace esphome
