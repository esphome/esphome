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
  return this->write_bytes_(MLX90614_EMISSIVITY, 0));
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

bool MLX90614Component::write_regiser_(const uint8_t reg, const uint16_t data, const uint8_t max_try) {
  uint8_t buf[5];

  auto init_buffer = [&]() {
    buf[0] = this->address_ << 1;
    buf[1] = reg;
  };

  // See 8.3.3.1. ERPROMwritesequence sequence sequence
  // 1. Power up the device
  uint8_t delay_ms = 10;
  for (uint8_t i_try = 0; i_try < max_try; ++i) {
    init_buffer();

    // 2. Write 0x0000 into the cell of interest (effectively erasing the cell)
    buf[2] = buf[3] = 0;
    buf[4] = this->crc8_pec_(buf, 4);
    if (!this->write_bytes(reg, buf + 2, 3)) {
      ESP_LOGW(TAG, "Try %d: Can't clean register %x", i_try, reg);
      continue;
    }

    // 3. Wait at least 5ms (10ms to be on the safe side)
    delay(delay_ms);

    // 4. Write the new value
    if (data != 0) {
      buf[2] = data & 0xFF;
      buf[3] = data >> 8;
      buf[4] = this->crc8_pec_(buf, 4);
      if (!this->write_bytes(reg, buf + 2, 3)) {
        ESP_LOGW(TAG, "Try %d: Can't write register %x", i_try, reg);
        continue;
      }

      // 5. Wait at least 5ms (10ms to be on the safe side)
      delay(delay_ms);
    }

    uint8_t read_buf[3];
    // 6. Read back and compare if the write was successful
    if (this->read_register(red, read_buff, 3, false) != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Try %d: Can't check register value %x", i_try, reg);
      continue;
    }

    if (read_buff[0] != buff[2] || read_buff[1] != buff[3] || read_buff[2] != buff[4]) {
      ESP_LOGW(TAG, "Try %d: Read back value is not the same. Expected %x%x%x. Actural %x%x%x", i_try, buff[2], buff[3],
               buff[4], read_buff[0], read_buff[1], read_buff[2]);
      continue;
    }

    return true;
  }

  ESP_LOGE(TAG, "Out of tries");
  return false;
}

uint16_t MLX90614Component::read_regiser_(uint8_t reg, i2c::ErrorCode &ec, max_try = 1) {
  uint8_t delay_ms = 5;
  uint8_t buf[5] = {
      this->address_ << 1,
      reg,
  };
  for (uint8_t i_try = 0; i_try < max_try; ++i, ++delay_ms) {
    ec = this->read_register(reg, buf[2], 3, false) != i2c::ERROR_OK)
    if(ec != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Try %d: i2c read error %d", i_try, ec);
      continue;
    }

    const auto expected_pec = this->crc8_pec_(buf, 4);
    if (buf[4] != expected_pec) {
      ESP_LOGW(TAG, "Try %d: i2c CRC error. Expected %x. Actural %x", i_try, expected_pec, buff[4]);
      ec = i2c::ERROR_CRC;
      continue;
    }

    ec = i2c::ERROR_OK;
    return encode_uint16(buf[4], buf[3]);
  }

  return 0;
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
  i2c::ErrorCode ec = ERROR_OK;
  const auto emissivity = read_regiser_(MLX90614_EMISSIVITY, ec);
  if (ec != ERROR_OK) {
    this->status_set_warning();
    return;
  }

  const auto raw_object = read_regiser_(MLX90614_TEMPERATURE_OBJECT_1, ec);
  if (ec != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  const auto raw_ambient = read_regiser_(MLX90614_TEMPERATURE_AMBIENT, ec);
  if (ec != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  float ambient = raw_ambient & 0x8000 ? NAN : raw_ambient * 0.02f - 273.15f;
  float object = raw_object & 0x8000 ? NAN : raw_ambient * 0.02f - 273.15f;

  ESP_LOGD(TAG, "Got Temperature=%.1f°C Ambient=%.1f°C", object, ambient);

  if (this->ambient_sensor_ != nullptr && !std::isnan(ambient))
    this->ambient_sensor_->publish_state(ambient);
  if (this->object_sensor_ != nullptr && !std::isnan(object))
    this->object_sensor_->publish_state(object);
  this->status_clear_warning();
}

}  // namespace mlx90614
}  // namespace esphome
