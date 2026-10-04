#include "bh1730.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bh1730 {

static const char *const TAG = "bh1730.sensor";

constexpr static const uint8_t BH1730_CMD = 0x80;
constexpr static const uint8_t BH1730_REG_CONTROL = 0x00;
constexpr static const uint8_t BH1730_REG_CONTROL_POWER = 0x01;
constexpr static const uint8_t BH1730_REG_CONTROL_ADC_EN = 0x02;
constexpr static const uint8_t BH1730_REG_CONTROL_ONE_TIME = 0x08;
constexpr static const uint8_t BH1730_REG_CONTROL_ADC_VALID = 0x10;

constexpr static const uint8_t BH1730_ITIME = 0xDA;
constexpr static const float BH1730_T_INT = (2.8 / 1000.0);
constexpr static const float BH1730_ITIME_MS = BH1730_T_INT * 964.0 * (256 - BH1730_ITIME);

constexpr static const uint8_t BH1730_REG_GAIN = 0x07;
constexpr static const uint8_t BH1730_REG_GAIN_X1 = 0x00;
constexpr static const uint8_t BH1730_REG_GAIN_X2 = 0x01;
constexpr static const uint8_t BH1730_REG_GAIN_X64 = 0x02;
constexpr static const uint8_t BH1730_REG_GAIN_X128 = 0x03;

constexpr static const uint8_t BH1730_REG_DATA0_LOW = 0x14;
constexpr static const uint8_t BH1730_REG_DATA0_HIGH = 0x15;
constexpr static const uint8_t BH1730_REG_DATA1_LOW = 0x16;
constexpr static const uint8_t BH1730_REG_DATA1_HIGH = 0x17;

/*
bh1730 properties:

IntegrationTime:
- 0 is manual integration, 1-255, default 0xDA

Gain:
- x1=0 (default), x2=1, x64=2, x128=3
*/

void BH1730Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BH1730...");
  {
    constexpr const uint8_t value = BH1730_REG_CONTROL_POWER;
    if (this->write_register(BH1730_REG_CONTROL | BH1730_CMD, &value, 1) != i2c::ERROR_OK) {
      this->mark_failed();
      return;
    }
  }

  switch (gain) {
    case BH1730_GAIN_X2: {
      if (this->write_register(BH1730_REG_GAIN | BH1730_CMD, &BH1730_REG_GAIN_X2, 1) != i2c::ERROR_OK) {
        ESP_LOGW(TAG, "Failed to set gain x2.");
        this->mark_failed();
        return;
      }
    } break;
    case BH1730_GAIN_X64: {
      if (this->write_register(BH1730_REG_GAIN | BH1730_CMD, &BH1730_REG_GAIN_X64, 1) != i2c::ERROR_OK) {
        ESP_LOGW(TAG, "Failed to set gain x64.");
        this->mark_failed();
        return;
      }
    } break;
    case BH1730_GAIN_X128: {
      if (this->write_register(BH1730_REG_GAIN | BH1730_CMD, &BH1730_REG_GAIN_X128, 1) != i2c::ERROR_OK) {
        ESP_LOGW(TAG, "Failed to set gain x128.");
        this->mark_failed();
        return;
      }
    } break;
    case BH1730_GAIN_X1: {
      default:
        if (this->write_register(BH1730_REG_GAIN | BH1730_CMD, &BH1730_REG_GAIN_X1, 1) != i2c::ERROR_OK) {
          ESP_LOGW(TAG, "Failed to set gain x1.");
          this->mark_failed();
          return;
        }
    } break;
  }
}

void BH1730Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BH1730:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with BH1730 failed!");
  }

  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Visible", this->visible_light_sensor);
  LOG_SENSOR("  ", "Infrared", this->infrared_light_sensor);
  LOG_SENSOR("  ", "Illuminance", this->lux_light_sensor);
}

void BH1730Component::update() {
  constexpr const uint8_t value = BH1730_REG_CONTROL_POWER | BH1730_REG_CONTROL_ADC_EN | BH1730_REG_CONTROL_ONE_TIME;
  if (this->write_register(BH1730_REG_CONTROL | BH1730_CMD, &value, 1) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Turning on BH1730 failed");
    this->status_set_warning();
    return;
  }

  this->set_timeout("read", 150, [this]() {
    uint16_t raw_value[2];
    if (this->read_register(BH1730_REG_DATA0_LOW | BH1730_CMD, reinterpret_cast<uint8_t *>(&raw_value), 4) !=
        i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Reading BH1730 data failed");
      if (lux_light_sensor) {
        lux_light_sensor->publish_state(NAN);
      }
      if (visible_light_sensor) {
        visible_light_sensor->publish_state(NAN);
      }
      if (infrared_light_sensor) {
        infrared_light_sensor->publish_state(NAN);
      }
      return;
    }

    const float data0 = static_cast<float>(raw_value[0]);
    const float data1 = static_cast<float>(raw_value[1]);

    if (visible_light_sensor) {
      visible_light_sensor->publish_state(data0);
    }
    if (infrared_light_sensor) {
      infrared_light_sensor->publish_state(data1);
    }

    if (lux_light_sensor) {
      const float div = data1 / data0;
      float lx = 0.0f;
      if (div < 0.26f) {
        lx = ((1.29f * data0) - (2.733f * data1)) / static_cast<float>(gain) * (102.6f / BH1730_ITIME_MS);
      } else if (div < 0.55f) {
        lx = ((0.795f * data0) - (0.859f * data1)) / static_cast<float>(gain) * (102.6f / BH1730_ITIME_MS);
      } else if (div < 1.09f) {
        lx = ((0.51f * data0) - (0.345f * data1)) / static_cast<float>(gain) * (102.6f / BH1730_ITIME_MS);
      } else if (div < 2.13f) {
        lx = ((0.276f * data0) - (0.13f * data1)) / static_cast<float>(gain) * (102.6f / BH1730_ITIME_MS);
      }
      lux_light_sensor->publish_state(lx);
    }
  });
}

float BH1730Component::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace bh1730
}  // namespace esphome
