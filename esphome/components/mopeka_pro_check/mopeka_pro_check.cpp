#include "mopeka_pro_check.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace mopeka_pro_check {

static const char *const TAG = "mopeka_pro_check";
static const uint8_t MANUFACTURER_DATA_LENGTH = 10;
static const uint16_t MANUFACTURER_ID = 0x0059;
static const double MOPEKA_LPG_COEF[] = {0.573045, -0.002822, -0.00000535};  // Magic numbers provided by Mopeka

void MopekaProCheck::dump_config() {
  ESP_LOGCONFIG(TAG, "Mopeka Pro Check");
  LOG_SENSOR("  ", "Level", this->level_);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
  LOG_SENSOR("  ", "Reading Distance", this->distance_);
}

/**
 * Main parse function that gets called for all ble advertisements.
 * Check if advertisement is for our sensor and if so decode it and
 * update the sensor state data.
 */
bool MopekaProCheck::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    return false;
  }

  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  const auto &manu_datas = device.get_manufacturer_datas();

  if (manu_datas.size() != 1) {
    ESP_LOGE(TAG, "Unexpected manu_datas size (%d)", manu_datas.size());
    return false;
  }

  const auto &manu_data = manu_datas[0];

  ESP_LOGVV(TAG, "Manufacturer data:");
  for (const uint8_t byte : manu_data.data) {
    ESP_LOGVV(TAG, "0x%02x", byte);
  }

  if (manu_data.data.size() != MANUFACTURER_DATA_LENGTH) {
    ESP_LOGE(TAG, "Unexpected manu_data size (%d)", manu_data.data.size());
    return false;
  }

  // Now parse the data - See Datasheet for definition

  if (static_cast<SensorType>(manu_data.data[0]) != STANDARD_BOTTOM_UP &&
      static_cast<SensorType>(manu_data.data[0]) != LIPPERT_BOTTOM_UP &&
      static_cast<SensorType>(manu_data.data[0]) != PLUS_BOTTOM_UP) {
    ESP_LOGE(TAG, "Unsupported Sensor Type (0x%X)", manu_data.data[0]);
    return false;
  }

  // Get battery level first
  if (this->battery_level_ != nullptr) {
    uint8_t level = this->parse_battery_level_(manu_data.data);
    this->battery_level_->publish_state(level);
  }

  // Get distance and level if either are sensors
  if ((this->distance_ != nullptr) || (this->level_ != nullptr)) {
    uint32_t distance_value = this->parse_distance_(manu_data.data);
    SensorReadQuality quality_value = this->parse_read_quality_(manu_data.data);
    ESP_LOGD(TAG, "Distance Sensor: Quality (0x%X) Distance (%dmm)", quality_value, distance_value);
    if (quality_value < QUALITY_HIGH) {
      ESP_LOGW(TAG, "Poor read quality.");
    }
    if (quality_value < QUALITY_MED) {
      // if really bad reading set to 0
      ESP_LOGW(TAG, "Setting distance to 0");
      distance_value = 0;
    }

    // update distance sensor
    if (this->distance_ != nullptr) {
      this->distance_->publish_state(distance_value);
    }

    // update level sensor
    if (this->level_ != nullptr) {
      uint8_t tank_level = 0;
      if (distance_value >= this->full_mm_) {
        tank_level = 100;  // cap at 100%
      } else if (distance_value > this->empty_mm_) {
        tank_level = ((100.0f / (this->full_mm_ - this->empty_mm_)) * (distance_value - this->empty_mm_));
      }
      this->level_->publish_state(tank_level);
    }
  }

  // Get temperature of sensor
  if (this->temperature_ != nullptr) {
    uint8_t temp_in_c = this->parse_temperature_(manu_data.data);
    this->temperature_->publish_state(temp_in_c);
  }

  return true;
}

uint8_t MopekaProCheck::parse_battery_level_(const std::vector<uint8_t> &message) {
  float v = (float) ((message[1] & 0x7F) / 32.0f);
  // convert voltage and scale for CR2032
  float percent = (v - 2.2f) / 0.65f * 100.0f;
  if (percent < 0.0f) {
    return 0;
  }
  if (percent > 100.0f) {
    return 100;
  }
  return (uint8_t) percent;
}

uint32_t MopekaProCheck::parse_distance_(const std::vector<uint8_t> &message) {
  uint16_t raw = (message[4] * 256) + message[3];
  double raw_level = raw & 0x3FFF;
  double raw_t = (message[2] & 0x7F);

  return (uint32_t)(raw_level * (MOPEKA_LPG_COEF[0] + MOPEKA_LPG_COEF[1] * raw_t + MOPEKA_LPG_COEF[2] * raw_t * raw_t));
}

uint8_t MopekaProCheck::parse_temperature_(const std::vector<uint8_t> &message) { return (message[2] & 0x7F) - 40; }

SensorReadQuality MopekaProCheck::parse_read_quality_(const std::vector<uint8_t> &message) {
  return static_cast<SensorReadQuality>(message[4] >> 6);
}

}  // namespace mopeka_pro_check
}  // namespace esphome

#endif
