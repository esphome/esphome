#include "ags10.h"

#include <cinttypes>

namespace esphome {
namespace ags10 {
static const char *const TAG = "ags10";

// Data acquisition.
static const uint8_t REG_TVOC = 0x00;
// Zero-point calibration.
static const uint8_t REG_CALIBRATION = 0x01;
// Read version.
static const uint8_t REG_VERSION = 0x11;
// Read current resistance.
static const uint8_t REG_RESISTANCE = 0x20;
// Modify target address.
static const uint8_t REG_ADDRESS = 0x21;

// Zero-point calibration with current resistance.
static const uint16_t ZP_CURRENT = 0x0000;
// Zero-point reset.
static const uint16_t ZP_DEFAULT = 0xFFFF;

void AGS10Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ags10...");

  auto version = this->read_version_();
  if (version) {
    ESP_LOGD(TAG, "AGS10 Sensor Version: 0x%02X", *version);
    if (this->version_ != nullptr) {
      this->version_->publish_state(*version);
    }
  } else {
    ESP_LOGE(TAG, "AGS10 Sensor Version: unknown");
  }

  auto resistance = this->read_resistance_();
  if (resistance) {
    ESP_LOGD(TAG, "AGS10 Sensor Resistance: 0x%08" PRIX32, *resistance);
    if (this->resistance_ != nullptr) {
      this->resistance_->publish_state(*resistance);
    }
  } else {
    ESP_LOGE(TAG, "AGS10 Sensor Resistance: unknown");
  }

  ESP_LOGD(TAG, "Sensor initialized");
}

void AGS10Component::update() {
  auto tvoc = this->read_tvoc_();
  if (tvoc) {
    this->tvoc_->publish_state(*tvoc);
    this->status_clear_warning();
  } else {
    this->status_set_warning();
  }
}

void AGS10Component::dump_config() {
  ESP_LOGCONFIG(TAG, "AGS10:");
  LOG_I2C_DEVICE(this);
  switch (this->error_code_) {
    case NONE:
      break;
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, "Communication with AGS10 failed!");
      break;
    case CRC_CHECK_FAILED:
      ESP_LOGE(TAG, "The crc check failed");
      break;
    case ILLEGAL_STATUS:
      ESP_LOGE(TAG, "AGS10 is not ready to return TVOC data or sensor in pre-heat stage.");
      break;
    case UNSUPPORTED_UNITS:
      ESP_LOGE(TAG, "AGS10 returns TVOC data in unsupported units.");
      break;
    default:
      ESP_LOGE(TAG, "Unknown error: %d", this->error_code_);
      break;
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "TVOC Sensor", this->tvoc_);
  LOG_SENSOR("  ", "Firmware Version Sensor", this->version_);
  LOG_SENSOR("  ", "Resistance Sensor", this->resistance_);
}

/**
 * Sets new I2C address of AGS10.
 */
bool AGS10Component::new_i2c_address(uint8_t newaddress) {
  uint8_t rev_newaddress = ~newaddress;
  std::array<uint8_t, 5> data{newaddress, rev_newaddress, newaddress, rev_newaddress, 0};
  data[4] = calc_crc8_(data, 4);
  if (!this->write_bytes(REG_ADDRESS, data)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->status_set_warning();
    ESP_LOGE(TAG, "couldn't write the new I2C address 0x%02X", newaddress);
    return false;
  }
  this->set_i2c_address(newaddress);
  ESP_LOGW(TAG, "changed I2C address to 0x%02X", newaddress);
  this->error_code_ = NONE;
  this->status_clear_warning();
  return true;
}

bool AGS10Component::set_zero_point_with_factory_defaults() { return this->set_zero_point_with(ZP_DEFAULT); }

bool AGS10Component::set_zero_point_with_current_resistance() { return this->set_zero_point_with(ZP_CURRENT); }

bool AGS10Component::set_zero_point_with(uint16_t value) {
  std::array<uint8_t, 5> data{0x00, 0x0C, (uint8_t) ((value >> 8) & 0xFF), (uint8_t) (value & 0xFF), 0};
  data[4] = calc_crc8_(data, 4);
  if (!this->write_bytes(REG_CALIBRATION, data)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->status_set_warning();
    ESP_LOGE(TAG, "unable to set zero-point calibration with 0x%02X", value);
    return false;
  }
  if (value == ZP_CURRENT) {
    ESP_LOGI(TAG, "zero-point calibration has been set with current resistance");
  } else if (value == ZP_DEFAULT) {
    ESP_LOGI(TAG, "zero-point calibration has been reset to the factory defaults");
  } else {
    ESP_LOGI(TAG, "zero-point calibration has been set with 0x%02X", value);
  }
  this->error_code_ = NONE;
  this->status_clear_warning();
  return true;
}

optional<uint32_t> AGS10Component::read_tvoc_() {
  auto data = this->read_and_check_<5>(REG_TVOC);
  if (!data) {
    return nullopt;
  }

  auto res = *data;
  auto status_byte = res[0];

  int units = status_byte & 0x0e;
  int status_bit = status_byte & 0x01;

  if (status_bit != 0) {
    this->error_code_ = ILLEGAL_STATUS;
    ESP_LOGW(TAG, "Reading AGS10 data failed: illegal status (not ready or sensor in pre-heat stage)!");
    return nullopt;
  }

  if (units != 0) {
    this->error_code_ = UNSUPPORTED_UNITS;
    ESP_LOGE(TAG, "Reading AGS10 data failed: unsupported units (%d)!", units);
    return nullopt;
  }

  return encode_uint24(res[1], res[2], res[3]);
}

optional<uint8_t> AGS10Component::read_version_() {
  auto data = this->read_and_check_<5>(REG_VERSION);
  if (data) {
    auto res = *data;
    return res[3];
  }
  return nullopt;
}

optional<uint32_t> AGS10Component::read_resistance_() {
  auto data = this->read_and_check_<5>(REG_RESISTANCE);
  if (data) {
    auto res = *data;
    return encode_uint32(res[0], res[1], res[2], res[3]);
  }
  return nullopt;
}

template<size_t N> optional<std::array<uint8_t, N>> AGS10Component::read_and_check_(uint8_t a_register) {
  auto data = this->read_bytes<N>(a_register);
  if (!data.has_value()) {
    this->error_code_ = COMMUNICATION_FAILED;
    ESP_LOGE(TAG, "Reading AGS10 version failed!");
    return optional<std::array<uint8_t, N>>();
  }
  auto len = N - 1;
  auto res = *data;
  auto crc_byte = res[len];

  if (crc_byte != calc_crc8_(res, len)) {
    this->error_code_ = CRC_CHECK_FAILED;
    ESP_LOGE(TAG, "Reading AGS10 version failed: crc error!");
    return optional<std::array<uint8_t, N>>();
  }

  return data;
}

template<size_t N> uint8_t AGS10Component::calc_crc8_(std::array<uint8_t, N> dat, uint8_t num) {
  uint8_t i, byte1, crc = 0xFF;
  for (byte1 = 0; byte1 < num; byte1++) {
    crc ^= (dat[byte1]);
    for (i = 0; i < 8; i++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc = (crc << 1);
      }
    }
  }
  return crc;
}
}  // namespace ags10
}  // namespace esphome
