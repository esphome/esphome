#include "kuntze.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace kuntze {

static const char *const TAG = "kuntze";

static const uint8_t CMD_READ_REG = 0x03;
static const uint16_t REGISTER[] = {4136, 4160, 4680, 6000, 4688, 4728, 5832};

void Kuntze::on_modbus_data(const std::vector<uint8_t> &data) {
  auto get_16bit = [&](int i) -> uint16_t { return (uint16_t(data[i * 2]) << 8) | uint16_t(data[i * 2 + 1]); };

  this->waiting_ = false;
  ESP_LOGV(TAG, "Data: %s", hexencode(data).c_str());

  float value = (float) get_16bit(0);
  for (int i = 0; i < data[3]; i++)
    value /= 10.0;
  switch (this->state_) {
    case 1:
      ESP_LOGD(TAG, "pH=%.1f", value);
      if (this->ph_sensor_ != nullptr)
        this->ph_sensor_->publish_state(value);
      break;
    case 2:
      ESP_LOGD(TAG, "temperature=%.1f", value);
      if (this->temperature_sensor_ != nullptr)
        this->temperature_sensor_->publish_state(value);
      break;
    case 3:
      ESP_LOGD(TAG, "DIS1=%.1f", value);
      if (this->dis1_sensor_ != nullptr)
        this->dis1_sensor_->publish_state(value);
      break;
    case 4:
      ESP_LOGD(TAG, "DIS2=%.1f", value);
      if (this->dis2_sensor_ != nullptr)
        this->dis2_sensor_->publish_state(value);
      break;
    case 5:
      ESP_LOGD(TAG, "REDOX=%.1f", value);
      if (this->redox_sensor_ != nullptr)
        this->redox_sensor_->publish_state(value);
      break;
    case 6:
      ESP_LOGD(TAG, "EC=%.1f", value);
      if (this->ec_sensor_ != nullptr)
        this->ec_sensor_->publish_state(value);
      break;
    case 7:
      ESP_LOGD(TAG, "OCI=%.1f", value);
      if (this->oci_sensor_ != nullptr)
        this->oci_sensor_->publish_state(value);
      break;
  }
  if (++this->state_ > 7)
    this->state_ = 0;
}

void Kuntze::loop() {
  uint32_t now = App.get_loop_component_start_time();
  // timeout after 15 seconds
  if (this->waiting_ && (now - this->last_send_ > 15000)) {
    ESP_LOGW(TAG, "timed out waiting for response");
    this->waiting_ = false;
  }
  if (this->waiting_ || (this->state_ == 0))
    return;
  this->last_send_ = now;
  send(CMD_READ_REG, REGISTER[this->state_ - 1], 2);
  this->waiting_ = true;
}

void Kuntze::update() { this->state_ = 1; }

void Kuntze::dump_config() {
  ESP_LOGCONFIG(TAG, "Kuntze:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  LOG_SENSOR("", "pH", this->ph_sensor_);
  LOG_SENSOR("", "temperature", this->temperature_sensor_);
  LOG_SENSOR("", "DIS1", this->dis1_sensor_);
  LOG_SENSOR("", "DIS2", this->dis2_sensor_);
  LOG_SENSOR("", "REDOX", this->redox_sensor_);
  LOG_SENSOR("", "EC", this->ec_sensor_);
  LOG_SENSOR("", "OCI", this->oci_sensor_);
}

}  // namespace kuntze
}  // namespace esphome
