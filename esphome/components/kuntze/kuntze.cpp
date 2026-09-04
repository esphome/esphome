#include "kuntze.h"
#include "esphome/core/log.h"

namespace esphome::kuntze {

static const char *const TAG = "kuntze";

static constexpr uint16_t REGISTER_PH = 4136;
static constexpr uint16_t REGISTER_TEMPERATURE = 4160;
static constexpr uint16_t REGISTER_DIS1 = 4680;
static constexpr uint16_t REGISTER_DIS2 = 6000;
static constexpr uint16_t REGISTER_REDOX = 4688;
static constexpr uint16_t REGISTER_EC = 4728;
static constexpr uint16_t REGISTER_OCI = 5832;
static constexpr uint16_t REGISTER[] = {REGISTER_PH,    REGISTER_TEMPERATURE, REGISTER_DIS1, REGISTER_DIS2,
                                        REGISTER_REDOX, REGISTER_EC,          REGISTER_OCI};

void Kuntze::on_read_holding_registers(uint16_t start_address, std::span<const uint16_t> registers,
                                       modbus::ResponseStatus status) {
  if (!modbus::succeeded(status) || registers.size() < 2)
    return;

  // Each value is a register pair: the reading, then the number of decimal places in its low byte.
  float value = registers[0];
  for (uint16_t i = 0; i < (registers[1] & 0xFF); i++)
    value /= 10.0f;

  switch (start_address) {
    case REGISTER_PH:
      ESP_LOGD(TAG, "pH=%.1f", value);
      if (this->ph_sensor_ != nullptr)
        this->ph_sensor_->publish_state(value);
      break;
    case REGISTER_TEMPERATURE:
      ESP_LOGD(TAG, "temperature=%.1f", value);
      if (this->temperature_sensor_ != nullptr)
        this->temperature_sensor_->publish_state(value);
      break;
    case REGISTER_DIS1:
      ESP_LOGD(TAG, "DIS1=%.1f", value);
      if (this->dis1_sensor_ != nullptr)
        this->dis1_sensor_->publish_state(value);
      break;
    case REGISTER_DIS2:
      ESP_LOGD(TAG, "DIS2=%.1f", value);
      if (this->dis2_sensor_ != nullptr)
        this->dis2_sensor_->publish_state(value);
      break;
    case REGISTER_REDOX:
      ESP_LOGD(TAG, "REDOX=%.1f", value);
      if (this->redox_sensor_ != nullptr)
        this->redox_sensor_->publish_state(value);
      break;
    case REGISTER_EC:
      ESP_LOGD(TAG, "EC=%.1f", value);
      if (this->ec_sensor_ != nullptr)
        this->ec_sensor_->publish_state(value);
      break;
    case REGISTER_OCI:
      ESP_LOGD(TAG, "OCI=%.1f", value);
      if (this->oci_sensor_ != nullptr)
        this->oci_sensor_->publish_state(value);
      break;
  }
}

void Kuntze::update() {
  for (uint16_t reg : REGISTER)
    this->read_holding_registers(reg, 2);
}

void Kuntze::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Kuntze:\n"
                "  Address: 0x%02X",
                this->address_);
  LOG_SENSOR("", "pH", this->ph_sensor_);
  LOG_SENSOR("", "temperature", this->temperature_sensor_);
  LOG_SENSOR("", "DIS1", this->dis1_sensor_);
  LOG_SENSOR("", "DIS2", this->dis2_sensor_);
  LOG_SENSOR("", "REDOX", this->redox_sensor_);
  LOG_SENSOR("", "EC", this->ec_sensor_);
  LOG_SENSOR("", "OCI", this->oci_sensor_);
}

}  // namespace esphome::kuntze
