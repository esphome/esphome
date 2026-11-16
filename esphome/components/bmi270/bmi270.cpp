#include "bmi270.h"
#include "bmi270_config.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/progmem.h"

namespace esphome::bmi270 {

#ifdef USE_BMI270_BMM150
using namespace bmm150;  // NOLINT - sole consumer of the BMM150 aux-device definitions
#endif

ESPHOME_LOG_TAG(TAG, "bmi270");

PROGMEM_STRING_TABLE(AccelRangeStrings, "±2g", "±4g", "±8g", "±16g");
PROGMEM_STRING_TABLE(GyroRangeStrings, "±2000°/s", "±1000°/s", "±500°/s", "±250°/s", "±125°/s");

#if defined(USE_ARDUINO) && !defined(USE_ESP32)
static constexpr size_t MAX_I2C_BUFFER_SIZE = 32;
#else
static constexpr size_t MAX_I2C_BUFFER_SIZE = 256;
#endif

// The BMI270 needs its config file written after every power-on before the sensors work
bool BMI270Component::load_config_file_() {
  // The config port is only reachable with advanced power save off
  if (!this->write_byte(BMI270_REG_PWR_CONF, BMI270_PWR_CONF_ADV_POWER_SAVE_OFF))
    return false;
  delay(1);
  if (!this->write_byte(BMI270_REG_INIT_CTRL, BMI270_INIT_CTRL_LOAD_START))
    return false;

  constexpr size_t cfg_len = sizeof(BMI270_CONFIG_FILE);
  // The register byte goes first, so each burst is copied once and sent with write()
  uint8_t buf[1 + MAX_I2C_BUFFER_SIZE];
  buf[0] = BMI270_REG_INIT_DATA;
  size_t index = 0;

  while (index != cfg_len) {
    // INIT_ADDR_0 takes the word address bits 0-3 and INIT_ADDR_1 bits 4-11
    const uint8_t addr[2] = {static_cast<uint8_t>((index / 2) & 0x0F), static_cast<uint8_t>((index / 2) >> 4)};
    if (this->write_register(BMI270_REG_INIT_ADDR_0, addr, sizeof(addr)) != i2c::ERROR_OK)
      return false;

    size_t burst = clamp_at_most(cfg_len - index, MAX_I2C_BUFFER_SIZE);
    progmem_memcpy(buf + 1, BMI270_CONFIG_FILE + index, burst);
    if (this->write(buf, burst + 1) != i2c::ERROR_OK)
      return false;

    index += burst;
  }

  if (!this->write_byte(BMI270_REG_INIT_CTRL, BMI270_INIT_CTRL_LOAD_DONE))
    return false;
  delay(20);  // The datasheet asks for at least 20 ms before INTERNAL_STATUS is valid

  uint8_t status = 0;
  if (!this->read_byte(BMI270_REG_INTERNAL_STATUS, &status))
    return false;
  if ((status & BMI270_INTERNAL_STATUS_MASK) != BMI270_INTERNAL_STATUS_INIT_OK) {
    ESP_LOGE(TAG, "Config load failed: INTERNAL_STATUS=0x%02X (expected 0x01)", status);
    return false;
  }
  return true;
}

void BMI270Component::setup() {
  MotionComponent::setup();
  if (!this->check_chip_id(this, BMI270_REG_CHIP_ID, BMI270_CHIP_ID_VALUE))
    return;

  if (!this->setup_check(this->write_byte(BMI270_REG_CMD, BMI270_CMD_SOFT_RESET)))
    return;
  delay(20);

  if (!this->setup_check(this->load_config_file_(), LOG_STR("Config file upload failed")))
    return;

  // Normal filter bandwidth; performance mode for the accelerometer, noise and filter performance for the gyroscope
  const uint8_t acc_conf =
      this->accel_odr_ | (BMI270_CONF_BWP_NORMAL << BMI270_CONF_BWP_SHIFT) | BMI270_ACC_CONF_PERF_MODE;
  const uint8_t gyr_conf = this->gyro_odr_ | (BMI270_CONF_BWP_NORMAL << BMI270_CONF_BWP_SHIFT) |
                           BMI270_GYR_CONF_NOISE_PERF | BMI270_GYR_CONF_FILTER_PERF;
  if (!this->setup_check(this->write_byte(BMI270_REG_ACC_CONF, acc_conf) &&
                         this->write_byte(BMI270_REG_ACC_RANGE, this->accel_range_) &&
                         this->write_byte(BMI270_REG_GYR_CONF, gyr_conf) &&
                         this->write_byte(BMI270_REG_GYR_RANGE, this->gyro_range_) &&
                         this->write_byte(BMI270_REG_PWR_CTRL, BMI270_PWR_CTRL_IMU)))
    return;
  delay(5);

  // Advanced power save stays off; FIFO self-wakeup on
  if (!this->setup_check(this->write_byte(BMI270_REG_PWR_CONF, BMI270_PWR_CONF_FIFO_SELF_WAKEUP)))
    return;

#ifdef USE_BMI270_BMM150
  // A missing magnetometer only disables magnetometer reporting
  if (this->aux_device_ == BMI270_AUX_DEVICE_BMM150) {
    this->magnetometer_ready_ = this->setup_magnetometer_();
    if (!this->magnetometer_ready_) {
      ESP_LOGW(TAG, "BMM150 magnetometer setup failed; continuing without it");
    }
  }
#endif
}

#ifdef USE_BMI270_BMM150
bool BMI270Component::wait_for_aux_idle_() {
  static constexpr uint32_t AUX_IDLE_TIMEOUT_MS = 20;
  uint32_t start = millis();
  while (true) {
    uint8_t status = 0;
    if (!this->read_byte(BMI270_REG_STATUS, &status))
      return false;
    if ((status & BMI270_STATUS_AUX_BUSY) == 0)
      return true;
    if (millis() - start >= AUX_IDLE_TIMEOUT_MS)
      return false;
    delay(1);
  }
}

bool BMI270Component::write_aux_register_(uint8_t reg, uint8_t value) {
  return this->write_byte(BMI270_REG_AUX_WR_DATA, value) && this->write_byte(BMI270_REG_AUX_WR_ADDR, reg) &&
         this->wait_for_aux_idle_();
}

bool BMI270Component::read_aux_block_(uint8_t reg, uint8_t *buf) {
  return this->write_byte(BMI270_REG_AUX_RD_ADDR, reg) && this->wait_for_aux_idle_() &&
         this->read_bytes(BMI270_REG_AUX_X_LSB, buf, BMI270_AUX_DATA_LEN);
}

// Follows the Bosch BMI270 aux-interface sequence; ends in auto-burst mode, where the BMI270 keeps
// AUX_X/Y/Z/R filled from the BMM150's data registers
bool BMI270Component::setup_magnetometer_() {
  if (!(this->write_byte(BMI270_REG_IF_CONF, BMI270_IF_CONF_AUX_IF_EN) &&
        this->write_byte(BMI270_REG_AUX_IF_CONF, BMI270_AUX_IF_CONF_MANUAL_EN | BMI270_AUX_IF_CONF_MAN_RD_BURST_8) &&
        this->write_byte(BMI270_REG_AUX_DEV_ID, this->aux_device_address_ << 1)))
    return false;

  // Suspend -> sleep, then a soft reset so the BMM150 starts from a known state even if it
  // stayed powered across a restart. The chip ID is only readable once the power bit is set.
  if (!this->write_aux_register_(BMM150_REG_POWER_CONTROL, BMM150_CMD_POWER_ON))
    return false;
  delay(BMM150_START_UP_TIME_MS);
  if (!this->write_aux_register_(BMM150_REG_POWER_CONTROL, BMM150_CMD_SOFT_RESET))
    return false;
  delay(BMM150_SOFT_RESET_TIME_MS);

  uint8_t chip_id[BMI270_AUX_DATA_LEN];
  if (!this->read_aux_block_(BMM150_REG_CHIP_ID, chip_id))
    return false;
  if (chip_id[0] != BMM150_CHIP_ID_VALUE) {
    ESP_LOGW(TAG, "BMM150 not detected (chip ID 0x%02X, expected 0x%02X)", chip_id[0], BMM150_CHIP_ID_VALUE);
    return false;
  }

  uint8_t trim[BMM150_TRIM_BLOCKS * BMM150_TRIM_BLOCK_LEN];
  for (uint8_t i = 0; i < BMM150_TRIM_BLOCKS; i++) {
    if (!this->read_aux_block_(BMM150_REG_TRIM_START + i * BMM150_TRIM_BLOCK_LEN, trim + i * BMM150_TRIM_BLOCK_LEN))
      return false;
  }
  this->mag_trim_ = bmm150_parse_trim(trim);

  if (!this->write_aux_register_(BMM150_REG_OP_MODE, BMM150_CMD_NORMAL_MODE_ODR_30HZ))
    return false;

  return this->write_byte(BMI270_REG_AUX_IF_CONF, BMI270_AUX_IF_CONF_FCU_WRITE_EN | BMI270_AUX_IF_CONF_MAN_RD_BURST_8 |
                                                      BMI270_AUX_IF_CONF_RD_BURST_8) &&
         this->write_byte(BMI270_REG_AUX_RD_ADDR, BMM150_REG_DATA_X_LSB) &&
         this->write_byte(BMI270_REG_PWR_CTRL, BMI270_PWR_CTRL_IMU | BMI270_PWR_CTRL_AUX_EN);
}
#endif  // USE_BMI270_BMM150

void BMI270Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BMI270 IMU:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed())
    return;

  ESP_LOGCONFIG(TAG,
                "  Accel range : %s\n"
                "  Gyro  range : %s",
                LOG_STR_ARG(AccelRangeStrings::get_log_str(this->accel_range_, AccelRangeStrings::LAST_INDEX)),
                LOG_STR_ARG(GyroRangeStrings::get_log_str(this->gyro_range_, GyroRangeStrings::LAST_INDEX)));
#ifdef USE_BMI270_BMM150
  if (this->aux_device_ == BMI270_AUX_DEVICE_BMM150) {
    ESP_LOGCONFIG(TAG, "  Aux device  : BMM150 magnetometer (%s)",
                  this->magnetometer_ready_ ? LOG_STR_LITERAL("ready") : LOG_STR_LITERAL("NOT DETECTED"));
  }
#endif
  MotionComponent::dump_config();
}

bool BMI270Component::update_data(motion::MotionData &data) {
  if (this->is_failed())
    return false;

  //  Accelerometer: registers 0x0C–0x11 (6 bytes: x_lsb, x_msb, y_lsb, y_msb, z_lsb, z_msb)
  // When the magnetometer is active, the 8 aux data bytes (0x04–0x0B) sit directly before
  // the accel data, so one burst read from AUX_X_LSB fetches both.
#ifdef USE_BMI270_BMM150
  const bool read_mag = this->magnetometer_ready_ && !this->magnetometer_callback_.empty();
  uint8_t buf[BMI270_AUX_DATA_LEN + REG_READ_LEN];
  const uint8_t *mag_raw = buf;
  const uint8_t *raw_data = read_mag ? buf + BMI270_AUX_DATA_LEN : buf;
  const bool read_ok = read_mag ? this->read_bytes(BMI270_REG_AUX_X_LSB, buf, sizeof(buf))
                                : this->read_bytes(BMI270_REG_DATA_8, buf, REG_READ_LEN);
#else
  uint8_t buf[REG_READ_LEN];
  const uint8_t *raw_data = buf;
  const bool read_ok = this->read_bytes(BMI270_REG_DATA_8, buf, REG_READ_LEN);
#endif
  if (!read_ok) {
    ESP_LOGW(TAG, "Failed to read IMU data");
    return false;
  }
  // Scale factor: LSB/g depends on range
  // raw is a signed 16-bit value; full-scale = range_g * 2^15 lsb
  // In PROGMEM; each read is a 32-bit load, which is safe from flash on ESP8266
  alignas(4) static constexpr float ACCEL_SCALE[] PROGMEM = {
      2.0f / 32768.0f,
      4.0f / 32768.0f,
      8.0f / 32768.0f,
      16.0f / 32768.0f,
  };
  float scale = ACCEL_SCALE[this->accel_range_];

  data.acceleration[motion::X_AXIS] = encode_value_le<int16_t>(raw_data) * scale;
  data.acceleration[motion::Y_AXIS] = encode_value_le<int16_t>(&raw_data[2]) * scale;
  data.acceleration[motion::Z_AXIS] = encode_value_le<int16_t>(&raw_data[4]) * scale;

  // Gyroscope: registers 0x12–0x17 (6 bytes)
  // Scale: full-scale range / 2^15
  alignas(4) static constexpr float GYRO_SCALE[] PROGMEM = {
      2000.0f / 32768.0f, 1000.0f / 32768.0f, 500.0f / 32768.0f, 250.0f / 32768.0f, 125.0f / 32768.0f,
  };
  static constexpr uint8_t GYR_OFFS = BMI270_REG_DATA_14 - BMI270_REG_DATA_8;
  scale = GYRO_SCALE[this->gyro_range_];

  data.angular_rate[motion::X_AXIS] = encode_value_le<int16_t>(&raw_data[GYR_OFFS]) * scale;
  data.angular_rate[motion::Y_AXIS] = encode_value_le<int16_t>(&raw_data[GYR_OFFS + 2]) * scale;
  data.angular_rate[motion::Z_AXIS] = encode_value_le<int16_t>(&raw_data[GYR_OFFS + 4]) * scale;

  if (!this->temperature_callback_.empty()) {
    //  Temperature: registers 0x22–0x23
    // Formula from datasheet: T[°C] = raw / 512 + 23
    static constexpr uint8_t TEMP_OFFS = BMI270_REG_TEMP_0 - BMI270_REG_DATA_8;
    int16_t raw_t = encode_value_le<int16_t>(&raw_data[TEMP_OFFS]);
    float temperature = (raw_t / 512.0f) + 23.0f;
    this->temperature_callback_.call(temperature);
  }

#ifdef USE_BMI270_BMM150
  if (read_mag) {
    const BMM150Data mag_data = bmm150_convert(this->mag_trim_, mag_raw);
    this->magnetometer_callback_.call(mag_data);
  }
#endif
  return true;
}

}  // namespace esphome::bmi270
