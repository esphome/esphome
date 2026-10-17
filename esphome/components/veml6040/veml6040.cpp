#include "veml6040.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>

namespace esphome::veml6040 {

ESPHOME_LOG_TAG(TAG, "veml6040");

static constexpr uint8_t CONFIG_REG = 0x00;
static constexpr uint8_t R_DATA_REG = 0x08;
static constexpr uint8_t G_DATA_REG = 0x09;
static constexpr uint8_t B_DATA_REG = 0x0A;
static constexpr uint8_t W_DATA_REG = 0x0B;

static constexpr uint8_t CONFIG_REG_IT_BIT = 4;

static constexpr uint16_t MAX_COUNTS = 0xFFFF;

// Lux per green count at 40 ms integration time; halves for each longer integration time step
static constexpr float LUX_PER_COUNT_40MS = 0.25168f;

// Correlated colour temperature fit from the Vishay application note "Designing the VEML6040 into an
// Application": CCT = A * ((R - B) / G + OFFSET) ^ EXPONENT
static constexpr float CCT_A = 4278.6f;
static constexpr float CCT_OFFSET = 0.5f;
static constexpr float CCT_EXPONENT = -1.2455f;

void VEML6040Component::setup() {
  // Auto mode (AF = 0, TRIG = 0) with shutdown bit cleared: the device measures continuously.
  // The high byte of the configuration register is reserved and must be written as zero.
  const uint8_t config[] = {static_cast<uint8_t>(this->integration_time_ << CONFIG_REG_IT_BIT), 0x00};
  if (this->write_register(CONFIG_REG, config, sizeof config) != i2c::ERROR_OK) {
    this->mark_failed();
    return;
  }
  // The data registers hold zeros until the first integration period has finished
  this->set_timeout(this->integration_time_ms_(), [this]() { this->data_ready_ = true; });
}

void VEML6040Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "VEML6040:\n"
                "  Integration time: %" PRIu32 " ms\n"
                "  Glass attenuation factor: %.2f",
                this->integration_time_ms_(), this->glass_attenuation_factor_);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Red channel", this->red_sensor_);
  LOG_SENSOR("  ", "Green channel", this->green_sensor_);
  LOG_SENSOR("  ", "Blue channel", this->blue_sensor_);
  LOG_SENSOR("  ", "White channel", this->white_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Color temperature", this->color_temperature_sensor_);
}

bool VEML6040Component::read_channel_(uint8_t reg, uint16_t &counts) {
  uint8_t data[2];
  if (this->read_register(reg, data, sizeof data) != i2c::ERROR_OK)
    return false;
  counts = encode_uint16(data[1], data[0]);
  return true;
}

void VEML6040Component::update() {
  if (!this->data_ready_)
    return;

  uint16_t red, green, blue, white;
  if (!this->read_channel_(R_DATA_REG, red) || !this->read_channel_(G_DATA_REG, green) ||
      !this->read_channel_(B_DATA_REG, blue) || !this->read_channel_(W_DATA_REG, white)) {
    this->status_set_warning();
    return;
  }
  this->status_clear_warning();

  ESP_LOGV(TAG, "Counts: red=%u green=%u blue=%u white=%u", red, green, blue, white);

  if (this->red_sensor_ != nullptr)
    this->red_sensor_->publish_state(red);
  if (this->green_sensor_ != nullptr)
    this->green_sensor_->publish_state(green);
  if (this->blue_sensor_ != nullptr)
    this->blue_sensor_->publish_state(blue);
  if (this->white_sensor_ != nullptr)
    this->white_sensor_->publish_state(white);

  const bool saturated = red == MAX_COUNTS || green == MAX_COUNTS || blue == MAX_COUNTS;
  if (saturated) {
    ESP_LOGW(TAG, "Sensor saturated; use a shorter integration time");
  }

  if (this->illuminance_sensor_ != nullptr) {
    float lux = NAN;
    if (green != MAX_COUNTS) {
      const float lux_per_count = LUX_PER_COUNT_40MS / static_cast<float>(1u << this->integration_time_);
      lux = green * lux_per_count * this->glass_attenuation_factor_;
    }
    this->illuminance_sensor_->publish_state(lux);
  }

  if (this->color_temperature_sensor_ != nullptr) {
    float cct = NAN;
    if (!saturated && green != 0) {
      const float index = (static_cast<float>(red) - static_cast<float>(blue)) / green + CCT_OFFSET;
      if (index > 0.0f)
        cct = CCT_A * powf(index, CCT_EXPONENT);
    }
    this->color_temperature_sensor_->publish_state(cct);
  }
}

}  // namespace esphome::veml6040
