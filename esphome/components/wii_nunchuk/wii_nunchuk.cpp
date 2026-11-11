#include "wii_nunchuk.h"

#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::wii_nunchuk {

ESPHOME_LOG_TAG(TAG, "wii_nunchuk");

static constexpr uint8_t WII_NUNCHUK_REGISTER_INIT_1 = 0xF0;
static constexpr uint8_t WII_NUNCHUK_REGISTER_INIT_2 = 0xFB;
static constexpr uint8_t WII_NUNCHUK_REGISTER_IDENT = 0xFA;
static constexpr uint8_t WII_NUNCHUK_REGISTER_STATE = 0x00;

static constexpr uint8_t WII_NUNCHUK_INIT_1_VALUE = 0x55;
static constexpr uint8_t WII_NUNCHUK_INIT_2_VALUE = 0x00;

static constexpr uint32_t WII_NUNCHUK_IDENT_NONE = 0x00000000;
static constexpr uint32_t WII_NUNCHUK_IDENT_NUNCHUK = 0xA4200000;

// The controller needs this long between the state request and the read
static constexpr uint32_t READ_DELAY_MS = 3;

void WiiNunchukComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->state_requested_) {
    // More than READ_DELAY_MS ticks of a 1 ms clock is at least READ_DELAY_MS
    if (now - this->last_request_ms_ <= READ_DELAY_MS)
      return;
    this->state_requested_ = false;
    this->read_state_();
    return;
  }
  if (now - this->last_request_ms_ < this->update_interval_ms_)
    return;
  this->last_request_ms_ = now;

  // The controller can be plugged in at any time and needs the handshake again after each replug
  if (!this->initialized_ && !this->status_comm_check(this->initialized_ = this->init_()))
    return;
  this->state_requested_ = this->write(&WII_NUNCHUK_REGISTER_STATE, 1) == i2c::ERROR_OK;
  if (!this->status_comm_check(this->state_requested_))
    this->initialized_ = false;
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
  if (!this->write_bytes(WII_NUNCHUK_REGISTER_INIT_1, &WII_NUNCHUK_INIT_1_VALUE, 1) ||
      !this->write_bytes(WII_NUNCHUK_REGISTER_INIT_2, &WII_NUNCHUK_INIT_2_VALUE, 1))
    return false;

  uint8_t ident[6];
  if (this->write(&WII_NUNCHUK_REGISTER_IDENT, 1) == i2c::ERROR_OK &&
      this->read(ident, sizeof(ident)) == i2c::ERROR_OK) {
    const uint32_t decoded_ident = encode_uint32(ident[2], ident[3], ident[4], ident[5]);
    if (decoded_ident != WII_NUNCHUK_IDENT_NONE && decoded_ident != WII_NUNCHUK_IDENT_NUNCHUK) {
      ESP_LOGW(TAG, "Unsupported Wii extension, ident 0x%08" PRIX32, decoded_ident);
    }
  }
  return true;
}

void WiiNunchukComponent::read_state_() {
  uint8_t data[6];
  if (!this->status_comm_result(this->read(data, sizeof(data)) == i2c::ERROR_OK)) {
    this->initialized_ = false;
    return;
  }
#ifdef USE_SENSOR
  this->joystick_x_sensor_.publish_state_if_not_dup(static_cast<int16_t>(data[0]) - (1 << 7));
  this->joystick_y_sensor_.publish_state_if_not_dup(static_cast<int16_t>(data[1]) - (1 << 7));
  this->acceleration_x_sensor_.publish_state_if_not_dup(static_cast<int16_t>((data[2] << 2) | ((data[5] & 0x0C) >> 2)) -
                                                        (1 << 9));
  this->acceleration_y_sensor_.publish_state_if_not_dup(static_cast<int16_t>((data[3] << 2) | ((data[5] & 0x30) >> 4)) -
                                                        (1 << 9));
  this->acceleration_z_sensor_.publish_state_if_not_dup(static_cast<int16_t>((data[4] << 2) | ((data[5] & 0xC0) >> 6)) -
                                                        (1 << 9));
#endif
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->c_button_binary_sensor_, (data[5] & 0x02) == 0);
  PUBLISH_STATE(this->z_button_binary_sensor_, (data[5] & 0x01) == 0);
#endif
}

}  // namespace esphome::wii_nunchuk
