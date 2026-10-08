#include "pmsa003i.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome::pmsa003i {

ESPHOME_LOG_TAG(TAG, "pmsa003i");

static const uint8_t COUNT_PAYLOAD_BYTES = 28;
static const uint8_t COUNT_PAYLOAD_LENGTH_BYTES = 2;
static const uint8_t COUNT_START_CHARACTER_BYTES = 2;
static const uint8_t COUNT_DATA_BYTES = COUNT_START_CHARACTER_BYTES + COUNT_PAYLOAD_LENGTH_BYTES + COUNT_PAYLOAD_BYTES;
static const uint8_t CHECKSUM_START_INDEX = COUNT_DATA_BYTES - 2;
static const uint8_t COUNT_16_BIT_VALUES = (COUNT_PAYLOAD_LENGTH_BYTES + COUNT_PAYLOAD_BYTES) / 2;
static const uint8_t START_CHARACTER_1 = 0x42;
static const uint8_t START_CHARACTER_2 = 0x4D;

// Timeout for determining when the device is ready for use, in milliseconds.
// The PMSA003I typically takes 2.3 seconds to perform its first measurement after a cold power up and
// I2C requests performed during that time will be NACKed. Use a slightly longer timeout to tolerate
// timing variation.  Note that although we consider the device ready as soon as it responds to I2C
// requests, it may take 30 seconds or more for the data to stabilize according to the datasheet.
static const uint32_t READY_TIMEOUT_MS = 3000;

// Poll interval for determining when the device is ready for use, in milliseconds.
static const uint32_t READY_POLL_INTERVAL_MS = 100;

void PMSA003IComponent::setup() {
  // Stop polling until the device is actually ready to prevent spurious I2C warnings during premature updates.
  this->stop_poller();
  this->poll_until_ready_or_timeout_(millis());
}

void PMSA003IComponent::poll_until_ready_or_timeout_(uint32_t start_time) {
  // Check whether the device is responding and identifies itself as expected.
  // We don't care about the actual sensor readings or the packet CRC in this case.
  uint8_t buffer[2];
  if (this->read(buffer, sizeof(buffer)) == i2c::ERROR_OK && buffer[0] == START_CHARACTER_1 &&
      buffer[1] == START_CHARACTER_2) {
    ESP_LOGD(TAG, "PMSA003I is ready");
    this->start_poller();
    return;
  }

  if (millis() - start_time < READY_TIMEOUT_MS) {
    this->set_timeout(READY_POLL_INTERVAL_MS, [this, start_time]() { this->poll_until_ready_or_timeout_(start_time); });
  } else {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  }
}

void PMSA003IComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "PMSA003I:");
  LOG_I2C_DEVICE(this);
}

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
  uint8_t buffer[COUNT_DATA_BYTES];

  const i2c::ErrorCode error = this->read(buffer, COUNT_DATA_BYTES);
  if (error != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C error %d", error);
    return false;
  }

  // https://github.com/adafruit/Adafruit_PM25AQI

  // Check that start byte is correct!
  if (buffer[0] != START_CHARACTER_1 || buffer[1] != START_CHARACTER_2) {
    ESP_LOGW(TAG, "Start character mismatch: %02X %02X != %02X %02X", buffer[0], buffer[1], START_CHARACTER_1,
             START_CHARACTER_2);
    return false;
  }

  const uint16_t payload_length = encode_uint16(buffer[2], buffer[3]);
  if (payload_length != COUNT_PAYLOAD_BYTES) {
    ESP_LOGW(TAG, "Payload length mismatch: %u != %u", payload_length, COUNT_PAYLOAD_BYTES);
    return false;
  }

  // Calculate checksum
  uint16_t checksum = 0;
  for (uint8_t i = 0; i < CHECKSUM_START_INDEX; i++) {
    checksum += buffer[i];
  }

  const uint16_t check = encode_uint16(buffer[CHECKSUM_START_INDEX], buffer[CHECKSUM_START_INDEX + 1]);
  if (checksum != check) {
    ESP_LOGW(TAG, "Checksum mismatch: %u != %u", checksum, check);
    return false;
  }

  // The data comes in endian'd, this solves it so it works on all platforms
  uint16_t buffer_u16[COUNT_16_BIT_VALUES];
  for (uint8_t i = 0; i < COUNT_16_BIT_VALUES; i++) {
    const uint8_t buffer_index = COUNT_START_CHARACTER_BYTES + i * 2;
    buffer_u16[i] = encode_uint16(buffer[buffer_index], buffer[buffer_index + 1]);
  }

  // put it into a nice struct :)
  memcpy((void *) data, (void *) buffer_u16, COUNT_16_BIT_VALUES * 2);

  return true;
}

}  // namespace esphome::pmsa003i
