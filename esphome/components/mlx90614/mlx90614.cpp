#include "mlx90614.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::mlx90614 {

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

// SMBus packet error code: CRC-8 with polynomial 0x07, MSB first
static uint8_t crc8_pec(const uint8_t *data, uint8_t len) { return crc8(data, len, 0x00, 0x07, true); }

void MLX90614Component::setup() {
  this->emissivity_write_ec_ = this->write_emissivity_();
  if (this->emissivity_write_ec_ != i2c::ERROR_OK) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->status_set_warning("Setup failed to set emissivity. Will retry later");
  }
}

i2c::ErrorCode MLX90614Component::write_emissivity_() {
  if (std::isnan(this->emissivity_)) {
    return i2c::ERROR_OK;
  }

  // Skip the write when the EEPROM already holds the desired value to save write cycles
  uint16_t current_emissivity;
  const auto ec = this->read_register_(MLX90614_EMISSIVITY, current_emissivity);
  if (ec != i2c::ERROR_OK) {
    return ec;
  }

  const auto desired_emissivity = static_cast<uint16_t>(this->emissivity_ * 0xFFFF);
  if (current_emissivity == desired_emissivity) {
    return i2c::ERROR_OK;
  }

  return this->write_register_(MLX90614_EMISSIVITY, desired_emissivity);
}

i2c::ErrorCode MLX90614Component::write_register_(uint8_t reg, uint16_t data) {
  // The PEC covers the whole write transaction: SLA+W, command, data low, data high
  uint8_t buf[5];
  buf[0] = this->address_ << 1;
  buf[1] = reg;

  // See datasheet 8.3.3.1 EEPROM write sequence
  // 1. Write 0x0000 into the cell of interest (erases the cell)
  buf[2] = buf[3] = 0;
  buf[4] = crc8_pec(buf, 4);
  auto ec = this->write_register(reg, buf + 2, 3);
  if (ec != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Can't erase register 0x%02X, error %d", reg, ec);
    return ec;
  }

  // 2. Wait at least 5ms
  delay(10);

  // 3. Write the new value
  if (data != 0) {
    buf[2] = data & 0xFF;
    buf[3] = data >> 8;
    buf[4] = crc8_pec(buf, 4);
    ec = this->write_register(reg, buf + 2, 3);
    if (ec != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Can't write register 0x%02X, error %d", reg, ec);
      return ec;
    }
    // 4. Wait at least 5ms
    delay(10);
  }

  // 5. Read back to confirm the value was stored
  uint16_t read_back;
  ec = this->read_register_(reg, read_back);
  if (ec != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Can't check register 0x%02X value", reg);
    return ec;
  }

  if (read_back != data) {
    ESP_LOGW(TAG, "Read back mismatch on register 0x%02X. Expected 0x%04X, got 0x%04X", reg, data, read_back);
    return i2c::ERROR_CRC;
  }

  return i2c::ERROR_OK;
}

i2c::ErrorCode MLX90614Component::read_register_(uint8_t reg, uint16_t &data) {
  // The PEC covers the whole read transaction: SLA+W, command, SLA+R, data low, data high
  uint8_t buf[6];
  buf[0] = this->address_ << 1;
  buf[1] = reg;
  buf[2] = (this->address_ << 1) | 0x01;

  const auto ec = this->read_register(reg, buf + 3, 3);
  if (ec != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "i2c read error %d", ec);
    return ec;
  }

  const auto expected_pec = crc8_pec(buf, 5);
  if (buf[5] != expected_pec) {
    ESP_LOGW(TAG, "i2c CRC error. Expected 0x%02X, got 0x%02X", expected_pec, buf[5]);
    return i2c::ERROR_CRC;
  }

  data = encode_uint16(buf[4], buf[3]);
  return i2c::ERROR_OK;
}

void MLX90614Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90614:");
  LOG_I2C_DEVICE(this);
  if (this->emissivity_write_ec_ != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Emissivity update error %d", this->emissivity_write_ec_);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Ambient", this->ambient_sensor_);
  LOG_SENSOR("  ", "Object", this->object_sensor_);
}

void MLX90614Component::update() {
  if (this->emissivity_write_ec_ != i2c::ERROR_OK) {
    this->emissivity_write_ec_ = this->write_emissivity_();
    if (this->emissivity_write_ec_ != i2c::ERROR_OK) {
      this->status_set_warning("Failed to write emissivity");
      return;
    }
  }

  // Publishes NAN on a bus or CRC failure so a stuck reading is visible instead of silently stale
  auto publish_sensor = [this](sensor::Sensor *sensor, uint8_t reg) {
    if (sensor == nullptr) {
      return i2c::ERROR_OK;
    }

    uint16_t raw;
    const auto ec = this->read_register_(reg, raw);
    if (ec != i2c::ERROR_OK) {
      sensor->publish_state(NAN);
      return ec;
    }

    // Bit 15 set means the device flagged the reading as invalid
    const float temperature = (raw & 0x8000) ? NAN : raw * 0.02f - 273.15f;
    ESP_LOGD(TAG, "'%s': Got temperature=%.1f°C", sensor->get_name().c_str(), temperature);
    sensor->publish_state(temperature);
    return ec;
  };

  const auto object_ec = publish_sensor(this->object_sensor_, MLX90614_TEMPERATURE_OBJECT_1);
  const auto ambient_ec = publish_sensor(this->ambient_sensor_, MLX90614_TEMPERATURE_AMBIENT);

  if (object_ec == i2c::ERROR_OK && ambient_ec == i2c::ERROR_OK) {
    this->status_clear_warning();
  } else {
    this->status_set_warning("Failed to read some sensors");
  }
}

}  // namespace esphome::mlx90614
