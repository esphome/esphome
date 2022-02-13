#include "pmsa003i.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace pmsa003i {

static const char *const TAG = "pmsa003i";

void PMSA003IComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up pmsa003i...");

  PM25AQIData data;
  bool successful_read = this->read_data_(&data);

  if (!successful_read) {
    this->mark_failed();
    return;
  }
}

void PMSA003IComponent::dump_config() { LOG_I2C_DEVICE(this); }

void PMSA003IComponent::update() {
  PM25AQIData data;

  bool successful_read = this->read_data_(&data);

  // Update sensors
  if (successful_read) {
    this->status_clear_warning();
    ESP_LOGV(TAG, "Read success. Updating sensors.");

    if (this->standard_units_) {
      if (this->pm_1_0_sensor_ != nullptr)
        this->pm_1_0_sensor_->publish_state(data.pm10_standard);
      if (this->pm_2_5_sensor_ != nullptr)
        this->pm_2_5_sensor_->publish_state(data.pm25_standard);
      if (this->pm_10_0_sensor_ != nullptr)
        this->pm_10_0_sensor_->publish_state(data.pm100_standard);
    } else {
      if (this->pm_1_0_sensor_ != nullptr)
        this->pm_1_0_sensor_->publish_state(data.pm10_env);
      if (this->pm_2_5_sensor_ != nullptr)
        this->pm_2_5_sensor_->publish_state(data.pm25_env);
      if (this->pm_10_0_sensor_ != nullptr)
        this->pm_10_0_sensor_->publish_state(data.pm100_env);
    }

    if (this->pmc_0_3_sensor_ != nullptr)
      this->pmc_0_3_sensor_->publish_state(data.particles_03um);
    if (this->pmc_0_5_sensor_ != nullptr)
      this->pmc_0_5_sensor_->publish_state(data.particles_05um);
    if (this->pmc_1_0_sensor_ != nullptr)
      this->pmc_1_0_sensor_->publish_state(data.particles_10um);
    if (this->pmc_2_5_sensor_ != nullptr)
      this->pmc_2_5_sensor_->publish_state(data.particles_25um);
    if (this->pmc_5_0_sensor_ != nullptr)
      this->pmc_5_0_sensor_->publish_state(data.particles_50um);
    if (this->pmc_10_0_sensor_ != nullptr)
      this->pmc_10_0_sensor_->publish_state(data.particles_100um);
  } else {
    this->status_set_warning();
    ESP_LOGV(TAG, "Read failure. Skipping update.");
  }
}

bool PMSA003IComponent::read_data_(PM25AQIData *data) {
  const uint8_t num_bytes = 32;
  uint8_t buffer[num_bytes];

  this->read_bytes_raw(buffer, num_bytes);

  // https://github.com/adafruit/Adafruit_PM25AQI

  // Check that start byte is correct!
  if (buffer[0] != 0x42) {
    return false;
  }

  // get checksum ready
  int16_t sum = 0;
  for (uint8_t i = 0; i < 30; i++) {
    sum += buffer[i];
  }

  // The data comes in endian'd, this solves it so it works on all platforms
  uint16_t buffer_u16[15];
  for (uint8_t i = 0; i < 15; i++) {
    buffer_u16[i] = buffer[2 + i * 2 + 1];
    buffer_u16[i] += (buffer[2 + i * 2] << 8);
  }

  // put it into a nice struct :)
  memcpy((void *) data, (void *) buffer_u16, 30);

  return (sum == data->checksum);
}

}  // namespace pmsa003i
}  // namespace esphome
