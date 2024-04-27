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
  ESP_LOGCONFIG(TAG, "  Propane Butane mix: %.0f%%", this->propane_butane_mix_ * 100);
  ESP_LOGCONFIG(TAG, "  Tank distance empty: %" PRIi32 "mm", this->empty_mm_);
  ESP_LOGCONFIG(TAG, "  Tank distance full: %" PRIi32 "mm", this->full_mm_);
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
    ESP_LOGE(TAG, "[%s] Unexpected manu_datas size (%d)", device.address_str().c_str(), manu_datas.size());
    return false;
  }

  const auto &manu_data = manu_datas[0];

  ESP_LOGVV(TAG, "[%s] Manufacturer data: %s", device.address_str().c_str(), format_hex_pretty(manu_data.data).c_str());

  if (manu_data.data.size() != MANUFACTURER_DATA_LENGTH) {
    ESP_LOGE(TAG, "[%s] Unexpected manu_data size (%d)", device.address_str().c_str(), manu_data.data.size());
    return false;
  }

  // Now parse the data
  const auto *mopeka_data = (const mopeka_std_package *) manu_data.data.data();

  const u_int8_t hardware_id = mopeka_data->data_1 & 0xCF;
  if (static_cast<SensorType>(hardware_id) != STANDARD && static_cast<SensorType>(hardware_id) != XL &&
      static_cast<SensorType>(hardware_id) != ETRAILER) {
    ESP_LOGE(TAG, "[%s] Unsupported Sensor Type (0x%X)", device.address_str().c_str(), hardware_id);
    return false;
  }

  ESP_LOGVV(TAG, "[%s] Sensor slow update rate: %d", device.address_str().c_str(), mopeka_data->slow_update_rate);
  ESP_LOGVV(TAG, "[%s] Sensor sync pressed: %d", device.address_str().c_str(), mopeka_data->sync_pressed);
  for (u_int8_t i = 0; i < 3; i++) {
    ESP_LOGVV(TAG, "[%s] %u. Sensor data %u time %u.", device.address_str().c_str(), (i * 4) + 1,
              mopeka_data->val[i].value_0, mopeka_data->val[i].time_0);
    ESP_LOGVV(TAG, "[%s] %u. Sensor data %u time %u.", device.address_str().c_str(), (i * 4) + 2,
              mopeka_data->val[i].value_1, mopeka_data->val[i].time_1);
    ESP_LOGVV(TAG, "[%s] %u. Sensor data %u time %u.", device.address_str().c_str(), (i * 4) + 3,
              mopeka_data->val[i].value_2, mopeka_data->val[i].time_2);
    ESP_LOGVV(TAG, "[%s] %u. Sensor data %u time %u.", device.address_str().c_str(), (i * 4) + 4,
              mopeka_data->val[i].value_3, mopeka_data->val[i].time_3);
  }

  // Get battery level first
  if (this->battery_level_ != nullptr) {
    uint8_t level = this->parse_battery_level_(mopeka_data);
    this->battery_level_->publish_state(level);
  }

  // Get temperature of sensor
  uint8_t temp_in_c = this->parse_temperature_(mopeka_data);
  if (this->temperature_ != nullptr) {
    this->temperature_->publish_state(temp_in_c);
  }

  // Get distance and level if either are sensors
  if ((this->distance_ != nullptr) || (this->level_ != nullptr)) {
    // Message contains 12 sensor dataset each 10 bytes long.
    // each sensor dataset contains 5 byte time and 5 byte value.

    // time in 10us ticks.
    // value is amplitude.

    std::array<u_int8_t, 12> measurements_time = {};
    std::array<u_int8_t, 12> measurements_value = {};
    // Copy measurements over into my array.
    {
      u_int8_t measurements_index = 0;
      for (u_int8_t i = 0; i < 3; i++) {
        measurements_time[measurements_index] = mopeka_data->val[i].time_0 + 1;
        measurements_value[measurements_index] = mopeka_data->val[i].value_0;
        measurements_index++;
        measurements_time[measurements_index] = mopeka_data->val[i].time_1 + 1;
        measurements_value[measurements_index] = mopeka_data->val[i].value_1;
        measurements_index++;
        measurements_time[measurements_index] = mopeka_data->val[i].time_2 + 1;
        measurements_value[measurements_index] = mopeka_data->val[i].value_2;
        measurements_index++;
        measurements_time[measurements_index] = mopeka_data->val[i].time_3 + 1;
        measurements_value[measurements_index] = mopeka_data->val[i].value_3;
        measurements_index++;
      }
    }

    // Find best(strongest) value(amplitude) and it's belonging time in sensor dataset.
    u_int8_t number_of_usable_values = 0;
    u_int16_t best_value = 0;
    u_int16_t best_time = 0;
    {
      u_int16_t measurement_time = 0;
      for (u_int8_t i = 0; i < 12; i++) {
        // Time is summed up until a value is reported. This allows time values larger than the 5 bits in transport.
        measurement_time += measurements_time[i];
        if (measurements_value[i] != 0) {
          // I got a value
          number_of_usable_values++;
          if (measurements_value[i] > best_value) {
            // This value is better than a previous one.
            best_value = measurements_value[i];
            best_time = measurement_time;
          }
          // Reset measurement_time or next values.
          measurement_time = 0;
        }
      }
    }

    ESP_LOGV(TAG, "[%s] Found %u values with best data %u time %u.", device.address_str().c_str(),
             number_of_usable_values, best_value, best_time);

    if (number_of_usable_values < 1 || best_value < 2 || best_time < 2) {
      // At least two measurement values must be present.
      ESP_LOGW(TAG, "[%s] Poor read quality. Setting distance to 0.", device.address_str().c_str());
      if (this->distance_ != nullptr) {
        this->distance_->publish_state(0);
      }
      if (this->level_ != nullptr) {
        this->level_->publish_state(0);
      }
    } else {
      float lpg_speed_of_sound = this->get_lpg_speed_of_sound_(temp_in_c);
      ESP_LOGV(TAG, "[%s] Speed of sound in current fluid %f m/s", device.address_str().c_str(), lpg_speed_of_sound);

      uint32_t distance_value = lpg_speed_of_sound * best_time / 100.0f;

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

float MopekaStdCheck::get_lpg_speed_of_sound_(float temperature) {
  return 1040.71f - 4.87f * temperature - 137.5f * this->propane_butane_mix_ - 0.0107f * temperature * temperature -
         1.63f * temperature * this->propane_butane_mix_;
}

uint8_t MopekaStdCheck::parse_battery_level_(const mopeka_std_package *message) {
  const float voltage = (float) ((message->raw_voltage / 256.0f) * 2.0f + 1.5f);
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

uint8_t MopekaStdCheck::parse_temperature_(const mopeka_std_package *message) {
  uint8_t tmp = message->raw_temp;
  if (tmp == 0x0) {
    return -40;
  } else {
    return (uint8_t) ((tmp - 25.0f) * 1.776964f);
  }
}

}  // namespace mopeka_std_check
}  // namespace esphome

#endif
