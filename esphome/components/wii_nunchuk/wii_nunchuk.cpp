#include "wii_nunchuk.h"

#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::wii_nunchuk {

ESPHOME_LOG_TAG(TAG, "wii_nunchuk");

static constexpr uint8_t REGISTER_IDENT = 0xFA;
static constexpr uint8_t REGISTER_STATE = 0x00;

static constexpr uint32_t IDENT_NONE = 0x00000000;
static constexpr uint32_t IDENT_NUNCHUK = 0xA4200000;

void WiiNunchukComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_poll_ms_ < this->update_interval_ms_)
    return;
  this->last_poll_ms_ = now;

  // The controller can be plugged in at any time and needs the handshake again after each replug
  if (!this->initialized_) {
    this->status_comm_check(this->initialized_ = this->init_());
    return;
  }
  this->read_state_();
}

void WiiNunchukComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Wii Nunchuk:\n"
                "  Update Interval: %" PRIu32 " ms",
                this->update_interval_ms_);
  LOG_I2C_DEVICE(this);
#ifdef USE_SENSOR
  LOG_SENSOR_WITH_DEDUP("  ", "Joystick X", this->joystick_x_sensor_);
  LOG_SENSOR_WITH_DEDUP("  ", "Joystick Y", this->joystick_y_sensor_);
  LOG_SENSOR_WITH_DEDUP("  ", "Acceleration X", this->acceleration_x_sensor_);
  LOG_SENSOR_WITH_DEDUP("  ", "Acceleration Y", this->acceleration_y_sensor_);
  LOG_SENSOR_WITH_DEDUP("  ", "Acceleration Z", this->acceleration_z_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "C Button", this->c_button_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Z Button", this->z_button_binary_sensor_);
#endif
}

bool WiiNunchukComponent::init_() {
  // Turns off data encryption. Not write_registers(), which logs every failed write while unplugged
  if (!this->write_byte(0xF0, 0x55) || !this->write_byte(0xFB, 0x00))
    return false;

  uint8_t ident[6];
  if (this->write(&REGISTER_IDENT, 1) == i2c::ERROR_OK && this->read(ident, sizeof(ident)) == i2c::ERROR_OK) {
    const uint32_t decoded_ident = encode_uint32(ident[2], ident[3], ident[4], ident[5]);
    if (decoded_ident != IDENT_NONE && decoded_ident != IDENT_NUNCHUK) {
      ESP_LOGW(TAG, "Unsupported Wii extension, ident 0x%08" PRIX32, decoded_ident);
    }
  }
  return this->write(&REGISTER_STATE, 1) == i2c::ERROR_OK;
}

void WiiNunchukComponent::read_state_() {
  // The answer to the state request sent on the previous poll
  uint8_t data[6];
  if (!this->status_comm_check(this->read(data, sizeof(data)) == i2c::ERROR_OK)) {
    this->initialized_ = false;
    return;
  }
#ifdef USE_SENSOR
  auto accel = [&data](uint8_t i) -> int16_t { return ((data[i] << 2) | ((data[5] >> (2 * (i - 1))) & 0x03)) - 512; };
  this->joystick_x_sensor_.publish_state_if_not_dup(data[0] - 128);
  this->joystick_y_sensor_.publish_state_if_not_dup(data[1] - 128);
  this->acceleration_x_sensor_.publish_state_if_not_dup(accel(2));
  this->acceleration_y_sensor_.publish_state_if_not_dup(accel(3));
  this->acceleration_z_sensor_.publish_state_if_not_dup(accel(4));
#endif
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->c_button_binary_sensor_, (data[5] & 0x02) == 0);
  PUBLISH_STATE(this->z_button_binary_sensor_, (data[5] & 0x01) == 0);
#endif
  this->initialized_ = this->status_comm_result(this->write(&REGISTER_STATE, 1) == i2c::ERROR_OK);
}

}  // namespace esphome::wii_nunchuk
