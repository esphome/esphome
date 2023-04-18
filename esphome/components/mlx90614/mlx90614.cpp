#include "mlx90614.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mlx90614 {

static const uint8_t MLX90614_RAW_IR_1 = 0x04;
static const uint8_t MLX90614_RAW_IR_2 = 0x05;
static const uint8_t MLX90614_TEMPERATURE_AMBIENT = 0x06;
static const uint8_t MLX90614_TEMPERATURE_OBJECT_1 = 0x07;
static const uint8_t MLX90614_TEMPERATURE_OBJECT_2 = 0x08;

static const uint8_t MLX90614_TOMAX = 0x20;
static const uint8_t MLX90614_TOMIN = 0x21;
static const uint8_t MLX90614_PWMCTRL = 0x22;
static const uint8_t MLX90614_TARANGE = 0x23;
static const uint8_t MLX90614_EMISSIVITY = 0x24;
static const uint8_t MLX90614_CONFIG = 0x25;
static const uint8_t MLX90614_ADDR = 0x2E;
static const uint8_t MLX90614_ID1 = 0x3C;
static const uint8_t MLX90614_ID2 = 0x3D;
static const uint8_t MLX90614_ID3 = 0x3E;
static const uint8_t MLX90614_ID4 = 0x3F;

static const char *const TAG = "mlx90614";

void MLX90614Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90614...");
  if (!this->write_emissivity_()) {
    ESP_LOGE(TAG, "Communication with MLX90614 failed!");
    this->mark_failed();
    return;
  }
}

bool MLX90614Component::write_emissivity_() {
  if (std::isnan(this->emissivity_))
    return true;
  uint16_t value = (uint16_t) (this->emissivity_ * 65535);
  if (!this->write_bytes_(MLX90614_EMISSIVITY, 0)) {
    return false;
  }
  delay(10);
  if (!this->write_bytes_(MLX90614_EMISSIVITY, value)) {
    return false;
  }
  delay(10);
  return true;
}

uint8_t MLX90614Component::crc8_pec_(const uint8_t *data, uint8_t len) {
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

bool MLX90614Component::write_bytes_(uint8_t reg, uint16_t data) {
  uint8_t buf[5];
  buf[0] = this->address_ << 1;
  buf[1] = reg;
  buf[2] = data & 0xFF;
  buf[3] = data >> 8;
  buf[4] = this->crc8_pec_(buf, 4);
  return this->write_bytes(reg, buf + 2, 3);
}

void MLX90614Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90614:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MLX90614 failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Ambient", this->ambient_sensor_);
  LOG_SENSOR("  ", "Object", this->object_sensor_);
}

float MLX90614Component::get_setup_priority() const { return setup_priority::DATA; }

void MLX90614Component::update() {
  uint8_t emissivity[3];
  if (this->read_register(MLX90614_EMISSIVITY, emissivity, 3, false) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  uint8_t raw_object[3];
  if (this->read_register(MLX90614_TEMPERATURE_OBJECT_1, raw_object, 3, false) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  uint8_t raw_ambient[3];
  if (this->read_register(MLX90614_TEMPERATURE_AMBIENT, raw_ambient, 3, false) != i2c::ERROR_OK) {
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

}  // namespace mlx90614
}  // namespace esphome
