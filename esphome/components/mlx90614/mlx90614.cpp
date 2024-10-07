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
  this->emissivity_write_ec_ = this->write_emissivity_();
  if (this->emissivity_write_ec_ != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Communication with MLX90614 failed!");
    this->mark_failed();
    return;
  }
}

i2c::ErrorCode MLX90614Component::write_emissivity_() {
  if (std::isnan(this->emissivity_)) {
    return i2c::ERROR_OK;
  }

  uint16_t read_emissivity;
  const auto ec = read_register_(MLX90614_EMISSIVITY, read_emissivity);
  if (i2c::ERROR_OK != ec) {
    return ec;
  }

  const auto desired_emissivity = uint16_t(this->emissivity_ * 0xFFFF);
  if (read_emissivity == desired_emissivity) {
    return ec;
  }

  return this->write_register_(MLX90614_EMISSIVITY, desired_emissivity);
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

i2c::ErrorCode MLX90614Component::write_register_(uint8_t reg, uint16_t data) {
  uint8_t buf[5];
  i2c::ErrorCode ec = i2c::ERROR_UNKNOWN;

  // See 8.3.3.1. ERPROM write sequence
  // 1. Power up the device
  const uint8_t delay_ms = 10;
  buf[0] = this->address_ << 1;
  buf[1] = reg;

  // 2. Write 0x0000 into the cell of interest (effectively erasing the cell)
  buf[2] = buf[3] = 0;
  buf[4] = this->crc8_pec_(buf, 4);
  ec = this->write_register(reg, buf + 2, 3);
  if (i2c::ERROR_OK != ec) {
    ESP_LOGW(TAG, "Can't clean register %x, error %d", reg, ec);
    return ec;
  }

  // 3. Wait at least 5ms (10ms to be on the safe side)
  delay(delay_ms);

  // 4. Write the new value
  if (data != 0) {
    buf[2] = data & 0xFF;
    buf[3] = data >> 8;
    buf[4] = this->crc8_pec_(buf, 4);
    ec = this->write_register(reg, buf + 2, 3);
    if (i2c::ERROR_OK != ec) {
      ESP_LOGW(TAG, "Can't write register %x, error %d", reg, ec);
      return ec;
    }
    // 5. Wait at least 5ms (10ms to be on the safe side)
    delay(delay_ms);
  }

  uint8_t read_buf[3];
  // 6. Read back and compare if the write was successful
  ec = this->read_register(reg, read_buf, 3, false);
  if (i2c::ERROR_OK != ec) {
    ESP_LOGW(TAG, "Can't check register %x value", reg);
    return ec;
  }

  if (read_buf[0] != buf[2] || read_buf[1] != buf[3] || read_buf[2] != buf[4]) {
    ESP_LOGW(TAG, "Read back value is not the same. Expected %x%x%x. Actual %x%x%x", buf[2], buf[3], buf[4],
             read_buf[0], read_buf[1], read_buf[2]);
    return i2c::ERROR_CRC;
  }

  return i2c::ERROR_OK;
}

i2c::ErrorCode MLX90614Component::read_register_(uint8_t reg, uint16_t &data) {
  uint8_t buf[6];
  // master write
  buf[0] = this->address_ << 1;
  buf[1] = reg;
  // master read
  buf[2] = (this->address_ << 1) | 0x01;

  const auto ec = this->read_register(reg, buf + 3, 3, false);

  if (i2c::ERROR_OK != ec) {
    ESP_LOGW(TAG, "i2c read error %d", ec);
    return ec;
  }

  const auto expected_pec = this->crc8_pec_(buf, 5);
  if (buf[5] != expected_pec) {
    ESP_LOGW(TAG, "i2c CRC error. Expected %x. Actual %x", expected_pec, buf[4]);
    return i2c::ERROR_CRC;
  }

  data = encode_uint16(buf[4], buf[3]);
  return i2c::ERROR_OK;
}

void MLX90614Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90614:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with MLX90614 failed!");
  }

  if (i2c::ERROR_OK != this->emissivity_write_ec_) {
    ESP_LOGE(TAG, "Emissivity update error %d", this->emissivity_write_ec_);
  }

  if (i2c::ERROR_OK != this->object_read_ec_) {
    ESP_LOGE(TAG, "Object temperature read error %d", this->object_read_ec_);
  }

  if (i2c::ERROR_OK != this->ambient_read_ec_) {
    ESP_LOGE(TAG, "Ambient temperature read error %d", this->ambient_read_ec_);
  }

  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Ambient", this->ambient_sensor_);
  LOG_SENSOR("  ", "Object", this->object_sensor_);
}

float MLX90614Component::get_setup_priority() const { return setup_priority::DATA; }

void MLX90614Component::update() {
  this->emissivity_write_ec_ = this->write_emissivity_();

  auto publish_sensor = [&](sensor::Sensor *sensor, uint8_t reg) {
    if (nullptr == sensor) {
      return i2c::ERROR_OK;
    }

    uint16_t raw;
    const auto ec = read_register_(reg, raw);
    sensor->publish_state(ec != i2c::ERROR_OK || (raw & 0x8000) ? NAN : float(raw) * 0.02f - 273.15f);
    return ec;
  };

  this->object_read_ec_ = publish_sensor(this->object_sensor_, MLX90614_TEMPERATURE_OBJECT_1);
  this->ambient_read_ec_ = publish_sensor(this->ambient_sensor_, MLX90614_TEMPERATURE_AMBIENT);

  if (this->ambient_read_ec_ == i2c::ERROR_OK && this->object_read_ec_ == i2c::ERROR_OK &&
      this->emissivity_write_ec_ == i2c::ERROR_OK) {
    this->status_clear_warning();
  } else {
    this->status_set_warning();
  }
}

}  // namespace mlx90614
}  // namespace esphome
