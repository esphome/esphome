#include "mopeka_std_check.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace mopeka_std_check {

static const char *const TAG = "mopeka_std_check";
static const uint16_t SERVICE_UUID = 0xADA0;
static const uint8_t MANUFACTURER_DATA_LENGTH = 23;
static const uint16_t MANUFACTURER_ID = 0x000D;

void MopekaStdCheck::dump_config() {
  ESP_LOGCONFIG(TAG, "Mopeka Std Check");
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
bool MopekaStdCheck::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  {
    // Validate address.
    if (device.address_uint64() != this->address_) {
      return false;
    }

    ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());
  }

  {
    // Validate service uuid
    const auto &service_uuids = device.get_service_uuids();
    if (service_uuids.size() != 1) {
      return false;
    }
    const auto &service_uuid = service_uuids[0];
    if (service_uuid != esp32_ble_tracker::ESPBTUUID::from_uint16(SERVICE_UUID)) {
      return false;
    }
  }

  const auto &manu_datas = device.get_manufacturer_datas();

  if (manu_datas.size() != 1) {
    ESP_LOGE(TAG, "%s: Unexpected manu_datas size (%d)", device.address_str().c_str(), manu_datas.size());
    return false;
  }

  const auto &manu_data = manu_datas[0];

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGVV(TAG, "%s: Manufacturer data: %s", device.address_str().c_str(), format_hex_pretty(manu_data.data).c_str());
#endif

  if (manu_data.data.size() != MANUFACTURER_DATA_LENGTH) {
    ESP_LOGE(TAG, "%s: Unexpected manu_data size (%d)", device.address_str().c_str(), manu_data.data.size());
    return false;
  }

  // Now parse the data
  const u_int8_t hardware_id = manu_data.data[1] & 0xCF;
  if (static_cast<SensorType>(hardware_id) != STANDARD && static_cast<SensorType>(hardware_id) != XL) {
    ESP_LOGE(TAG, "%s: Unsupported Sensor Type (0x%X)", device.address_str().c_str(), hardware_id);
    return false;
  }

  // Get battery level first
  if (this->battery_level_ != nullptr) {
    uint8_t level = this->parse_battery_level_(manu_data.data);
    this->battery_level_->publish_state(level);
  }

  // Get temperature of sensor
  uint8_t temp_in_c = this->parse_temperature_(manu_data.data);
  if (this->temperature_ != nullptr) {
    this->temperature_->publish_state(temp_in_c);
  }

  // Get distance and level if either are sensors
  if ((this->distance_ != nullptr) || (this->level_ != nullptr)) {
    // Message contains 12 sensor dataset each 10 bytes long.
    // each sensor dataset contains 5 byte time and 5 byte value.
    // if value is 0 ignore and if combined time is too old ignore.

    // time in 10us ticks.
    // value is amplitude.
    const auto message = manu_data.data;

    std::vector<u_int16_t> measurements_time;
    std::vector<u_int16_t> measurements_value;

    u_int16_t time = 0;

    for (u_int8_t i = 0; i < 3; i++) {
      u_int8_t start = 4 + i * 5;
      u_int64_t d = message[start] | (message[start + 1] << 8) | (message[start + 2] << 16) |
                    (message[start + 3] << 24) | ((u_int64_t) message[start + 4] << 32);
      for (u_int8_t i = 0; i < 4; i++) {
        u_int8_t measurement_time = (d & 0x1F) + 1;
        d = d >> 5;
        u_int8_t measurement_value = (d & 0x1F);
        d = d >> 5;

        if (time > 255) {
          break;
        }
        time += measurement_time;

        if (measurement_value > 0 && measurement_value < 0b00011111 && time < 255) {
          u_int16_t value = ((u_int16_t) measurement_value - 1) * 4 + 6;

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
          ESP_LOGVV(TAG, "%s: Valid sensor data %u at time %u.", device.address_str().c_str(), value, time * 2);
#endif

          measurements_time.push_back(time * 2);
          measurements_value.push_back(value);
        }
      }
    }

    if (measurements_time.size() < 2) {
      // At least two measurement values must be present.
      ESP_LOGW(TAG, "%s: Poor read quality. Setting distance to 0.", device.address_str().c_str());
      if (this->distance_ != nullptr) {
        this->distance_->publish_state(0);
      }
      if (this->level_ != nullptr) {
        this->level_->publish_state(0);
      }
    } else {
      // Basic logic take first response and ignore all other echos.
      // Possible improvement is to add peak detection for echo values.
      // Expectation is first value at distance and second lower value at
      // 2*distance. Ignore low values as noise.

      float lpg_speed_of_sound = this->get_lpg_speed_of_sound(temp_in_c);

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
      ESP_LOGVV(TAG, "%s: Speed of sound in current fluid %f m/s", device.address_str().c_str(), lpg_speed_of_sound);
#endif

      uint32_t distance_value = measurements_time[0] * lpg_speed_of_sound / 100.0f / 2.0f;

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
  }

  return true;
}

float MopekaStdCheck::get_lpg_speed_of_sound(float temperature) {
  return 1040.71f - 4.87f * temperature - 137.5f * this->lpg_butane_ratio_ - 0.0107f * temperature * temperature -
         1.63f * temperature * this->lpg_butane_ratio_;
}

uint8_t MopekaStdCheck::parse_battery_level_(const std::vector<uint8_t> &message) {
  const float voltage = (float) ((message[2] / 256.0f) * 2.0f + 1.5f);
  ESP_LOGVV(TAG, "Sensor battery voltage: %f V", voltage);
  // convert voltage and scale for CR2032
  const float percent = (voltage - 2.2f) / 0.65f * 100.0f;
  if (percent < 0.0f) {
    return 0;
  }
  if (percent > 100.0f) {
    return 100;
  }
  return (uint8_t) percent;
}

uint8_t MopekaStdCheck::parse_temperature_(const std::vector<uint8_t> &message) {
  uint8_t tmp = message[3] & 0x3f;
  if (tmp == 0) {
    return -40;
  } else {
    return (uint8_t)((tmp - 25.0f) * 1.776964f);
  }
}

}  // namespace mopeka_std_check
}  // namespace esphome

#endif
