#include "bmi160.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::bmi160 {

ESPHOME_LOG_TAG(TAG, "bmi160");

static constexpr uint8_t BMI160_REGISTER_CHIPID = 0x00;
static constexpr uint8_t BMI160_CHIP_ID = 0xD1;
static constexpr uint8_t BMI160_REGISTER_CMD = 0x7E;
static constexpr uint8_t BMI160_REGISTER_DATA_GYRO_X_LSB = 0x0C;
static constexpr uint8_t BMI160_REGISTER_DATA_TEMP_LSB = 0x20;

// PMU mode commands: set PMU mode (0b000100xx accel, 0b000101xx gyro) with mode NORMAL (0b01)
static constexpr uint8_t CMD_ACCEL_PMU_NORMAL = 0x11;
static constexpr uint8_t CMD_GYRO_PMU_NORMAL = 0x15;

// Accel needs 5 to 10 ms to wake up and no command may be sent meanwhile
static constexpr uint32_t ACCEL_WAKEUP_TIMEOUT_MS = 10;
// Gyro needs 51 to 81 ms to wake up
static constexpr uint32_t GYRO_WAKEUP_TIMEOUT_MS = 100;

static constexpr uint8_t BMI160_REGISTER_ACCEL_CONFIG = 0x40;
static constexpr uint8_t BMI160_REGISTER_ACCEL_RANGE = 0x41;
static constexpr uint8_t BMI160_REGISTER_GYRO_CONFIG = 0x42;
static constexpr uint8_t BMI160_REGISTER_GYRO_RANGE = 0x43;

static constexpr uint8_t ACCEL_FILTER_PERF = 0b10000000;
static constexpr uint8_t ACCEL_BANDWIDTH_RES_AVG16 = 0b01000000;
static constexpr uint8_t ACCEL_ODR_25_HZ = 0b0110;
static constexpr uint8_t ACCEL_RANGE_16G = 0b1100;
static constexpr uint8_t GYRO_BANDWIDTH_OSR4 = 0x00;
static constexpr uint8_t GYRO_ODR_25_HZ = 0x06;
static constexpr uint8_t GYRO_RANGE_2000_DPS = 0x00;

static constexpr i2c::I2CRegValue BMI160_CONFIG[] PROGMEM = {
    {BMI160_REGISTER_GYRO_CONFIG, GYRO_BANDWIDTH_OSR4 | GYRO_ODR_25_HZ},
    {BMI160_REGISTER_GYRO_RANGE, GYRO_RANGE_2000_DPS},
    {BMI160_REGISTER_ACCEL_CONFIG, ACCEL_FILTER_PERF | ACCEL_BANDWIDTH_RES_AVG16 | ACCEL_ODR_25_HZ},
    {BMI160_REGISTER_ACCEL_RANGE, ACCEL_RANGE_16G},
};

// Full scale over INT16_MAX for the 2000 deg/s and 16 g ranges set above
static constexpr float GYRO_SCALE = 2000.f / INT16_MAX;
static constexpr float ACCEL_SCALE = 16 * 9.80665f / INT16_MAX;
static constexpr float TEMPERATURE_SCALE = 64.5f / INT16_MAX;
static constexpr float TEMPERATURE_OFFSET_C = 23.f;

void BMI160Component::setup() {
  if (!this->check_chip_id(this, BMI160_REGISTER_CHIPID, BMI160_CHIP_ID))
    return;
  if (!this->setup_check(this->write_byte(BMI160_REGISTER_CMD, CMD_ACCEL_PMU_NORMAL)))
    return;
  this->set_timeout(ACCEL_WAKEUP_TIMEOUT_MS, [this]() {
    if (!this->setup_check(this->write_byte(BMI160_REGISTER_CMD, CMD_GYRO_PMU_NORMAL)))
      return;
    this->set_timeout(GYRO_WAKEUP_TIMEOUT_MS, [this]() {
      if (!this->setup_check(this->write_registers(BMI160_CONFIG)))
        return;
      this->setup_complete_ = true;
    });
  });
}

void BMI160Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BMI160:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Acceleration X", this->accel_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->accel_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->accel_z_sensor_);
  LOG_SENSOR("  ", "Gyro X", this->gyro_x_sensor_);
  LOG_SENSOR("  ", "Gyro Y", this->gyro_y_sensor_);
  LOG_SENSOR("  ", "Gyro Z", this->gyro_z_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

void BMI160Component::update() {
  if (!this->setup_complete_) {
    return;
  }

  int16_t data[6];
  if (!this->status_comm_check(this->read_bytes_16_le(BMI160_REGISTER_DATA_GYRO_X_LSB, data, 6)))
    return;
  float temperature = NAN;
  if (this->temperature_sensor_ != nullptr) {
    int16_t raw_temperature;
    if (!this->status_comm_check(this->read_bytes_16_le(BMI160_REGISTER_DATA_TEMP_LSB, &raw_temperature, 1)))
      return;
    temperature = raw_temperature * TEMPERATURE_SCALE + TEMPERATURE_OFFSET_C;
  }
  this->status_clear_warning();

  const float gyro_x = data[0] * GYRO_SCALE;
  const float gyro_y = data[1] * GYRO_SCALE;
  const float gyro_z = data[2] * GYRO_SCALE;
  const float accel_x = data[3] * ACCEL_SCALE;
  const float accel_y = data[4] * ACCEL_SCALE;
  const float accel_z = data[5] * ACCEL_SCALE;

  ESP_LOGD(TAG,
           "Got accel={x=%.3f m/s², y=%.3f m/s², z=%.3f m/s²}, "
           "gyro={x=%.3f °/s, y=%.3f °/s, z=%.3f °/s}, temp=%.3f°C",
           accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z, temperature);

  PUBLISH_STATE(this->accel_x_sensor_, accel_x);
  PUBLISH_STATE(this->accel_y_sensor_, accel_y);
  PUBLISH_STATE(this->accel_z_sensor_, accel_z);

  PUBLISH_STATE(this->temperature_sensor_, temperature);

  PUBLISH_STATE(this->gyro_x_sensor_, gyro_x);
  PUBLISH_STATE(this->gyro_y_sensor_, gyro_y);
  PUBLISH_STATE(this->gyro_z_sensor_, gyro_z);
}

}  // namespace esphome::bmi160
