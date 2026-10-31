#include "sdp3x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::sdp3x {

ESPHOME_LOG_TAG(TAG, "sdp3x.sensor");
static constexpr uint16_t SDP3X_SOFT_RESET = 0x0006;
static constexpr uint16_t SDP3X_READ_ID1 = 0x367C;
static constexpr uint16_t SDP3X_READ_ID2 = 0xE102;
static constexpr uint16_t SDP3X_STOP_MEAS = 0x3FF9;
// The first reading is ready about 8 ms after the start command
static constexpr uint32_t FIRST_READING_DELAY_MS = 10;

// Product numbers from the SDP3x and SDP8xx datasheets
static const LogString *model_name(uint16_t product_id) {
  switch (product_id) {
    case 0x0101:
      return LOG_STR("SDP31-500Pa");
    case 0x0102:
      return LOG_STR("SDP32-125Pa");
    case 0x0201:
      return LOG_STR("SDP800-500Pa");
    case 0x0202:
      return LOG_STR("SDP800-125Pa");
    case 0x0204:
      return LOG_STR("SDP801-500Pa");
    case 0x020A:
      return LOG_STR("SDP810-500Pa");
    case 0x020B:
      return LOG_STR("SDP810-125Pa");
    case 0x020D:
      return LOG_STR("SDP811-500Pa");
    default:
      return LOG_STR("unknown");
  }
}

void SDP3XComponent::setup() {
  // Either may be refused when the sensor is idle, so the results are ignored
  this->write_command(SDP3X_STOP_MEAS);
  this->write_command(SDP3X_SOFT_RESET);

  // Identify before setup returns, so the boot config dump can print the model
  delay(20);
  uint16_t data[6];
  if (!this->write_command(SDP3X_READ_ID1) || !this->get_register(SDP3X_READ_ID2, data, 6) ||
      !this->write_command(this->start_command_)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
  this->product_id_ = data[1];
  this->set_timeout(FIRST_READING_DELAY_MS, [this] { this->update(); });
}

void SDP3XComponent::dump_config() {
  LOG_SENSOR("  ", "SDP3X", this);
  if (this->product_id_ != 0) {
    ESP_LOGCONFIG(TAG, "  Model: %s (0x%04X)", LOG_STR_ARG(model_name(this->product_id_)), this->product_id_);
  }
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

void SDP3XComponent::update() {
  uint16_t data[3];
  if (!this->status_comm_result(this->read_data(data, 3)))
    return;
  const auto pressure_raw = static_cast<int16_t>(data[0]);
  const auto scale_factor = static_cast<int16_t>(data[2]);
  // The scale factor is per Pa; publish hPa
  const float pressure = pressure_raw / (scale_factor * 100.0f);
  ESP_LOGV(TAG, "Pressure %.3f hPa (raw %d, scale %d)", pressure, pressure_raw, scale_factor);
  this->publish_state(pressure);
}

}  // namespace esphome::sdp3x
