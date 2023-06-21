#include "gcja5.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace gcja5 {

static const char *const TAG = "gcja5";

void GCJA5Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up gcja5...");

  PM25AQIData data;
  bool successful_read = this->read_data_(&data);

  if (!successful_read) {
    this->mark_failed();
    return;
  }
}

void GCJA5Component::dump_config() { LOG_I2C_DEVICE(this); }

void GCJA5Component::update() {
  uint8_t statusByte = 55;
  
  bool successful_read = this->read_register(SNGCJA5_STATE, &statusByte, 1);
  
  if (successful_read != i2c::ERROR_OK)
    ESP_LOGE(TAG, "Could not read I2C");

  
  ESP_LOGV(TAG, "Full YYY status byte is %i", statusByte);
  ESP_LOGV(TAG, "Sensor Status is %i", (statusByte >> 6) & 0b11);
  
  uint8_t pmdata[2];
  this->read_register(SNGCJA5_PM2_5, pmdata, 2);


  // Update sensors
  /*if (successful_read) {
    this->status_clear_warning();
    ESP_LOGV(TAG, "Read success. Updating sensors.");

    if (this->pm_1_0_sensor_ != nullptr)
      this->pm_1_0_sensor_->publish_state(data.pm10_standard);
    if (this->pm_2_5_sensor_ != nullptr)
      this->pm_2_5_sensor_->publish_state(data.pm25_standard);
    if (this->pm_10_0_sensor_ != nullptr)
      this->pm_10_0_sensor_->publish_state(data.pm100_standard);

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
  }*/
}

bool GCJA5Component::read_data_(PM25AQIData *data) {
  // gcja5
  // First 3 registers are 32-bit uint      (12 bytes)
  // Following 6 registers are 16-bit uint  (12 bytes)
  // Last register is a single byte         (1 byte) = 25
  const uint8_t num_bytes = 25;
  uint8_t buffer[num_bytes];

  //this->read_bytes_raw(buffer, num_bytes);

  //uint16_t pm03;
  //this->read_bytes(0x0c, &pm03,2);

  uint8_t status;
  this->read_register(0x26, &status, 1);
  
  ESP_LOGV(TAG, "Full XXX status byte is %i", status);
  ESP_LOGV(TAG, "Sensor Status is %i", (status >> 6) & 0x03);
  
  
  
  return true;





  // https://github.com/adafruit/Adafruit_PM25AQI

  // The data coming in is in the proper bit order, but little-endian, so low byte first, then high byte
  // The data comes in endian'd, this solves it so it works on all platforms
  // First we convert the three registers
  uint32_t buffer_u32[3];
  /*for (uint8_t i = 0; i < 3; i++) {
    buffer_u32[i] = buffer[4 + i * 4 + 3];
    buffer_u32[i] += (buffer[4 + i * 4 + 2] << 8);
    buffer_u32[i] += (buffer[4 + i * 4 + 1] << 16);
    buffer_u32[i] += (buffer[4 + i * 4] << 24);
  }*/

  buffer_u32[0] = ((uint32_t)buffer[0]) | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
  buffer_u32[1] = ((uint32_t)buffer[4]) | ((uint32_t)buffer[5] << 8) | ((uint32_t)buffer[6] << 16) | ((uint32_t)buffer[7] << 24);
  buffer_u32[2] = ((uint32_t)buffer[8]) | ((uint32_t)buffer[9] << 8) | ((uint32_t)buffer[10] << 16) | ((uint32_t)buffer[11] << 24);
  

  uint16_t buffer_u16[6];
  /*for (uint8_t i = 0; i < 6; i++) {
    buffer_u16[i] = buffer[2 + i * 2 + 1];
    buffer_u16[i] += (buffer[2 + i * 2] << 8);
  }*/
  buffer_u16[0] = ((uint32_t)buffer[12]) | ((uint32_t)buffer[13] << 8);
  buffer_u16[1] = ((uint32_t)buffer[14]) | ((uint32_t)buffer[15] << 8);
  buffer_u16[2] = ((uint32_t)buffer[16]) | ((uint32_t)buffer[17] << 8);
  buffer_u16[3] = ((uint32_t)buffer[18]) | ((uint32_t)buffer[19] << 8);
  buffer_u16[4] = ((uint32_t)buffer[20]) | ((uint32_t)buffer[21] << 8);
  buffer_u16[5] = ((uint32_t)buffer[22]) | ((uint32_t)buffer[23] << 8);

  // put it into a nice struct :)
  memcpy((void *) data, (void *) buffer_u32, 12);
  memcpy((void *) data+12, (void *) buffer_u16, 12);
  //memcpy((void *) data+24, (void *) buffer[24], 1);
  //ESP_LOGV(TAG, "Full status byte is %i", buffer[24]);
  //ESP_LOGV(TAG, "Sensor Status is %i", (buffer[24] >> 6) & 0x03);

  ESP_LOGV(TAG, "PM 0.3-0.5 is %u", buffer_u16[0]);
  ESP_LOGV(TAG, "PM 0.5-1.0 is %u", buffer_u16[1]);
  ESP_LOGV(TAG, "PM 1.0-2.5 is %u", buffer_u16[2]);
  
  ESP_LOGV(TAG, "PM 2.5-5.0 is %u", buffer_u16[3]);
  ESP_LOGV(TAG, "PM 5.0-7.5 is %u", buffer_u16[4]);
  ESP_LOGV(TAG, "PM 7.5-10.0 is %u", buffer_u16[5]);
  
  
  
  return true;
}

  uint8_t get_status(PM25AQIData *data)
  {
    return (data->status >> 6) & 0x03; // Extract bits 7-6
  }

  uint8_t get_status_pd(PM25AQIData *data)
  {
    return (data->status >> 4) & 0x03; // Extract bits 4-5
  }
  uint8_t get_status_ld(PM25AQIData *data)
  {
    return (data->status >> 2) & 0x03; // Extract bits 2-3
  }
  uint8_t get_status_fan(PM25AQIData *data)
  {
    return data->status & 0x03; // Extract bits 0-1
  }

}  // namespace gcja5
}  // namespace esphome
