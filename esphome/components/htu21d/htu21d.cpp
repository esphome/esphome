#include "htu21d.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace htu21d {

static const char *const TAG = "htu21d";

static const uint8_t HTU21D_ADDRESS = 0x40;
static const uint8_t HTU21D_REGISTER_RESET = 0xFE;
static const uint8_t HTU21D_REGISTER_TEMPERATURE = 0xF3;
static const uint8_t HTU21D_REGISTER_HUMIDITY = 0xF5;
static const uint8_t HTU21D_WRITERHT_REG_CMD = 0xE6; /**< Write RH/T User Register 1 */
static const uint8_t HTU21D_REGISTER_STATUS = 0xE7;
static const uint8_t HTU21D_WRITEHEATER_REG_CMD = 0x51; /**< Write Heater Control Register */
static const uint8_t HTU21D_READHEATER_REG_CMD = 0x11;  /**< Read Heater Control Register */
static const uint8_t HTU21D_REG_HTRE_BIT = 0x02;        /**< Control Register Heater Bit */

void HTU21DComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HTU21D...");

  if (!this->write_bytes(HTU21D_REGISTER_RESET, nullptr, 0)) {
    this->mark_failed();
    return;
  }

  // Wait for software reset to complete
  delay(15);
}
void HTU21DComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HTU21D:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with HTU21D failed!");
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
}
void HTU21DComponent::update() {
  if (this->write(&HTU21D_REGISTER_TEMPERATURE, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }

  // According to the datasheet sht21 temperature readings can take up to 85ms
  this->set_timeout(85, [this]() {
    uint16_t raw_temperature;
    if (this->read(reinterpret_cast<uint8_t *>(&raw_temperature), 2) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }
    raw_temperature = i2c::i2ctohs(raw_temperature);

    float temperature = (float(raw_temperature & 0xFFFC)) * 175.72f / 65536.0f - 46.85f;

    ESP_LOGD(TAG, "Got Temperature=%.1f°C", temperature);

    if (this->temperature_ != nullptr)
      this->temperature_->publish_state(temperature);
    this->status_clear_warning();

    if (this->write(&HTU21D_REGISTER_HUMIDITY, 1) != i2c::ERROR_OK) {
      this->status_set_warning();
      return;
    }

    this->set_timeout(50, [this]() {
      uint16_t raw_humidity;
      if (this->read(reinterpret_cast<uint8_t *>(&raw_humidity), 2) != i2c::ERROR_OK) {
        this->status_set_warning();
        return;
      }
      raw_humidity = i2c::i2ctohs(raw_humidity);

      float humidity = (float(raw_humidity & 0xFFFC)) * 125.0f / 65536.0f - 6.0f;

      ESP_LOGD(TAG, "Got Humidity=%.1f%%", humidity);

      if (this->humidity_ != nullptr)
        this->humidity_->publish_state(humidity);

      int8_t heater_level;

      // HTU21D does have a heater module but does not have heater level
      // Setting heater level to 1 in case the heater is ON
      if (this->sensor_model_ == HTU21D_SENSOR_MODEL_HTU21D) {
        if (this->is_heater_enabled()) {
          heater_level = 1;
        } else {
          heater_level = 0;
        }
      } else {
        heater_level = this->get_heater_level();
      }

      ESP_LOGD(TAG, "Heater Level=%d", heater_level);

      if (this->heater_ != nullptr)
        this->heater_->publish_state(heater_level);
      this->status_clear_warning();
    });
  });
}

bool HTU21DComponent::is_heater_enabled() {
  uint8_t raw_heater;
  if (this->read_register(HTU21D_REGISTER_STATUS, reinterpret_cast<uint8_t *>(&raw_heater), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return false;
  }
  raw_heater = i2c::i2ctohs(raw_heater);
  return (bool) (((raw_heater) >> (HTU21D_REG_HTRE_BIT)) & 0x01);
}

void HTU21DComponent::set_heater(bool status) {
  uint8_t raw_heater;
  if (this->read_register(HTU21D_REGISTER_STATUS, reinterpret_cast<uint8_t *>(&raw_heater), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
  raw_heater = i2c::i2ctohs(raw_heater);
  if (status) {
    raw_heater |= (1 << (HTU21D_REG_HTRE_BIT));
  } else {
    raw_heater &= ~(1 << (HTU21D_REG_HTRE_BIT));
  }

  if (this->write_register(HTU21D_WRITERHT_REG_CMD, &raw_heater, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
}

void HTU21DComponent::set_heater_level(uint8_t level) {
  if (this->write_register(HTU21D_WRITEHEATER_REG_CMD, &level, 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return;
  }
}

int8_t HTU21DComponent::get_heater_level() {
  int8_t raw_heater;
  if (this->read_register(HTU21D_READHEATER_REG_CMD, reinterpret_cast<uint8_t *>(&raw_heater), 2) != i2c::ERROR_OK) {
    this->status_set_warning();
    return 0;
  }
  raw_heater = i2c::i2ctohs(raw_heater);
  return raw_heater;
}

float HTU21DComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace htu21d
}  // namespace esphome
