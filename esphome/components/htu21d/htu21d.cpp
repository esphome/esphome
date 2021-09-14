#include "htu21d.h"
#include "esphome/core/log.h"

namespace esphome {
namespace htu21d {

static const char *const TAG = "htu21d";

static const uint8_t HTU21D_ADDRESS = 0x40;
static const uint8_t HTU21D_REGISTER_RESET = 0xFE;
static const uint8_t HTU21D_REGISTER_TEMPERATURE = 0xE3;
static const uint8_t HTU21D_REGISTER_HUMIDITY = 0xE5;
static const uint8_t HTU21D_REGISTER_STATUS = 0xE7;

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
  uint16_t raw_temperature;
  if (!this->read_byte_16(HTU21D_REGISTER_TEMPERATURE, &raw_temperature, 50)) {
    this->status_set_warning();
    return;
  }

  float temperature = (float(raw_temperature & 0xFFFC)) * 175.72f / 65536.0f - 46.85f;

  uint16_t raw_humidity;
  if (!this->read_byte_16(HTU21D_REGISTER_HUMIDITY, &raw_humidity, 50)) {
    this->status_set_warning();
    return;
  }

  float humidity = (float(raw_humidity & 0xFFFC)) * 125.0f / 65536.0f - 6.0f;
  ESP_LOGD(TAG, "Got Temperature=%.1f°C Humidity=%.1f%%", temperature, humidity);

  if (this->temperature_ != nullptr)
    this->temperature_->publish_state(temperature);
  if (this->humidity_ != nullptr)
    this->humidity_->publish_state(humidity);
  this->status_clear_warning();
}
float HTU21DComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace htu21d
}  // namespace esphome
