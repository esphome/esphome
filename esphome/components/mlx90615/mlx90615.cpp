#include "mlx90615.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mlx90615 {

static const uint8_t MLX90615_EMISSIVITY = 0x13;
static const uint8_t MLX90615_TEMPERATURE_AMBIENT = 0x26;
static const uint8_t MLX90615_TEMPERATURE_OBJECT = 0x27;

static const char *const TAG = "mlx90615";

void MLX90615Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90615...");
  if (!this->write_emissivity_()) {
    ESP_LOGE(TAG, "Communication with MLX90615 failed!");
    this->mark_failed();
    return;
  }
}

bool MLX90615Component::write_emissivity_() {
  if (std::isnan(this->emissivity_))
    return true;
  uint16_t value = (uint16_t) (this->emissivity_ * 65535);
  if (!this->write_bytes_(MLX90615_EMISSIVITY, 0)) {
    return false;
  }
  delay(10);
  if (!this->write_bytes_(MLX90615_EMISSIVITY, value)) {
    return false;
  }
  delay(10);
  return true;
}

uint8_t MLX90615Component::crc8_pec_(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++) {
    uint8_t in = data[i];
    for (uint8_t j = 0; j < 8; j++) {
      bool carry = (crc ^ in) & 0x80;
      crc <<= 1;
      if (carry)
        crc ^= 0x07;
      in <<= 1;
    }
  }
  return crc;
}

bool MLX90615Component::write_bytes_(uint8_t reg, uint16_t data) {
  uint8_t buf[5];
  buf[0] = this->address_ << 1;
  buf[1] = reg;
  buf[2] = data & 0xFF;
  buf[3] = data >> 8;
  buf[4] = this->crc8_pec_(buf, 4);
  return this->write_bytes(reg, buf + 2, 3);
}

void MLX90615Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90615:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MLX90615 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Ambient", this->ambient_sensor_);
  LOG_SENSOR("  ", "Object", this->object_sensor_);
}

float MLX90615Component::get_setup_priority() const { return setup_priority::DATA; }

void MLX90615Component::update() {
  uint8_t emissivity[3];
  if (this->read_register(MLX90615_EMISSIVITY, emissivity, 3, false) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  uint8_t raw_object[3];
  if (this->read_register(MLX90615_TEMPERATURE_OBJECT, raw_object, 3, false) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  uint8_t raw_ambient[3];
  if (this->read_register(MLX90615_TEMPERATURE_AMBIENT, raw_ambient, 3, false) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  float ambient = raw_ambient[1] & 0x80 ? NAN : encode_uint16(raw_ambient[1], raw_ambient[0]) * 0.02f - 273.15f;
  float object = raw_object[1] & 0x80 ? NAN : encode_uint16(raw_object[1], raw_object[0]) * 0.02f - 273.15f;

  ESP_LOGD(TAG, "Got Temperature=%.1f°C Ambient=%.1f°C", object, ambient);

  if (this->ambient_sensor_ != nullptr && !std::isnan(ambient))
    this->ambient_sensor_->publish_state(ambient);
  if (this->object_sensor_ != nullptr && !std::isnan(object))
    this->object_sensor_->publish_state(object);
  this->status_clear_warning();
}

}  // namespace mlx90615
}  // namespace esphome
