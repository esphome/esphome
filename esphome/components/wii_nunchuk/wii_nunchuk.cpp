#include "wii_nunchuk.h"

#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::wii_nunchuk {

static const char *const TAG = "wii_nunchuk";

static constexpr uint8_t WII_NUNCHUK_REGISTER_INIT_1 = 0xF0;
static constexpr uint8_t WII_NUNCHUK_REGISTER_INIT_2 = 0xFB;
static constexpr uint8_t WII_NUNCHUK_REGISTER_IDENT = 0xFA;
static constexpr uint8_t WII_NUNCHUK_REGISTER_STATE = 0x00;

static constexpr uint8_t WII_NUNCHUK_INIT_1_VALUE = 0x55;
static constexpr uint8_t WII_NUNCHUK_INIT_2_VALUE = 0x00;

static constexpr uint32_t WII_NUNCHUK_IDENT_NONE = 0x00000000;
static constexpr uint32_t WII_NUNCHUK_IDENT_NUNCHUK = 0xA4200000;

void WiiNunchukComponent::setup() {
  if (!this->write_bytes(WII_NUNCHUK_REGISTER_INIT_1, &WII_NUNCHUK_INIT_1_VALUE, 1) ||
      !this->write_bytes(WII_NUNCHUK_REGISTER_INIT_2, &WII_NUNCHUK_INIT_2_VALUE, 1)) {
    ESP_LOGE(TAG, "Failed to initialize controller");
    this->mark_failed();
    return;
  }

  uint8_t ident[6];
  if (this->write(&WII_NUNCHUK_REGISTER_IDENT, 1) != i2c::ERROR_OK ||
      this->read(ident, sizeof(ident)) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Could not read extension ident");
  } else {
    const uint32_t decoded_ident = this->decode_ident_(ident);
    ESP_LOGCONFIG(TAG, "  Extension Ident: 0x%08" PRIX32, decoded_ident);
    if (decoded_ident != WII_NUNCHUK_IDENT_NONE && decoded_ident != WII_NUNCHUK_IDENT_NUNCHUK) {
      ESP_LOGW(TAG, "Detected an unsupported Wii extension type");
    }
  }

  this->setup_complete_ = true;
}

void WiiNunchukComponent::loop() {
  if (!this->setup_complete_ || this->is_failed()) {
    return;
  }

  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_update_ms_ < this->update_interval_ms_) {
    return;
  }
  this->last_update_ms_ = now;

  if (!this->read_state_()) {
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();
  this->publish_state_();
}

void WiiNunchukComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Wii Nunchuk:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  ESP_LOGCONFIG(TAG, "  Update Interval: %lu ms", this->update_interval_ms_);

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Joystick X", this->joystick_x_sensor_);
  LOG_SENSOR("  ", "Joystick Y", this->joystick_y_sensor_);
  LOG_SENSOR("  ", "Acceleration X", this->acceleration_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->acceleration_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->acceleration_z_sensor_);
#endif

#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "C Button", this->c_button_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Z Button", this->z_button_binary_sensor_);
#endif
}

bool WiiNunchukComponent::read_state_() {
  if (this->write(&WII_NUNCHUK_REGISTER_STATE, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to request state");
    return false;
  }

  delay(3);

  uint8_t data[6];
  if (this->read(data, sizeof(data)) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to read state");
    return false;
  }

  this->joystick_x_ = static_cast<int16_t>(data[0]) - (1 << 7);
  this->joystick_y_ = static_cast<int16_t>(data[1]) - (1 << 7);
  this->acceleration_x_ = static_cast<int16_t>((data[2] << 2) | ((data[5] & 0x0C) >> 2)) - (1 << 9);
  this->acceleration_y_ = static_cast<int16_t>((data[3] << 2) | ((data[5] & 0x30) >> 4)) - (1 << 9);
  this->acceleration_z_ = static_cast<int16_t>((data[4] << 2) | ((data[5] & 0xC0) >> 6)) - (1 << 9);
  this->button_c_ = (data[5] & 0x02) == 0;
  this->button_z_ = (data[5] & 0x01) == 0;

  return true;
}

void WiiNunchukComponent::publish_state_() const {
#ifdef USE_SENSOR
  if (this->joystick_x_sensor_ != nullptr) {
    this->joystick_x_sensor_->publish_state(this->joystick_x_);
  }
  if (this->joystick_y_sensor_ != nullptr) {
    this->joystick_y_sensor_->publish_state(this->joystick_y_);
  }
  if (this->acceleration_x_sensor_ != nullptr) {
    this->acceleration_x_sensor_->publish_state(this->acceleration_x_);
  }
  if (this->acceleration_y_sensor_ != nullptr) {
    this->acceleration_y_sensor_->publish_state(this->acceleration_y_);
  }
  if (this->acceleration_z_sensor_ != nullptr) {
    this->acceleration_z_sensor_->publish_state(this->acceleration_z_);
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (this->c_button_binary_sensor_ != nullptr) {
    this->c_button_binary_sensor_->publish_state(this->button_c_);
  }
  if (this->z_button_binary_sensor_ != nullptr) {
    this->z_button_binary_sensor_->publish_state(this->button_z_);
  }
#endif
}

uint32_t WiiNunchukComponent::decode_ident_(const uint8_t *ident) const {
  return (static_cast<uint32_t>(ident[5]) << 0) | (static_cast<uint32_t>(ident[4]) << 8) |
         (static_cast<uint32_t>(ident[3]) << 16) | (static_cast<uint32_t>(ident[2]) << 24);
}

}  // namespace esphome::wii_nunchuk
