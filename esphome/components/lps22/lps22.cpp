#include "lps22.h"

namespace esphome {
namespace lps22 {

static constexpr const char *const TAG = "lps22";

static constexpr uint8_t WHO_AM_I = 0x0F;
static constexpr uint8_t LPS22HB_ID = 0xB1;
static constexpr uint8_t LPS22HH_ID = 0xB3;
static constexpr uint8_t CTRL_REG2 = 0x11;
static constexpr uint8_t CTRL_REG2_ONE_SHOT_MASK = 0b1;
static constexpr uint8_t STATUS = 0x27;
static constexpr uint8_t STATUS_T_DA_MASK = 0b10;
static constexpr uint8_t STATUS_P_DA_MASK = 0b01;
static constexpr uint8_t TEMP_L = 0x2b;
static constexpr uint8_t PRES_OUT_XL = 0x28;
static constexpr uint8_t REF_P_XL = 0x28;
static constexpr uint8_t READ_ATTEMPTS = 10;
static constexpr uint8_t READ_INTERVAL = 5;
static constexpr float PRESSURE_SCALE = 1. / 4096.;
static constexpr float TEMPERATURE_SCALE = 0.01;

void LPS22Component::setup() {}

void LPS22Component::dump_config() { LOG_I2C_DEVICE(this); }

void LPS22Component::update() {
  uint8_t value = 0x00;
  this->read_register(WHO_AM_I, &value, 1);
  if (value != LPS22HB_ID && value != LPS22HH_ID) {
    ESP_LOGW(TAG, "device IDs as %02x, which isn't a known LPS22HB or LPS22HH ID", value);
    return;
  }

  this->read_register(CTRL_REG2, &value, 1);
  value |= CTRL_REG2_ONE_SHOT_MASK;
  this->write_register(CTRL_REG2, &value, 1);

  this->set_retry(READ_INTERVAL, READ_ATTEMPTS, [this](uint8_t _) { return this->try_read_(); });
}

RetryResult LPS22Component::try_read_() {
  uint8_t value = 0x00;
  this->read_register(STATUS, &value, 1);
  const uint8_t expected_status_mask = STATUS_T_DA_MASK | STATUS_P_DA_MASK;
  if ((value & expected_status_mask) != expected_status_mask) {
    ESP_LOGD(TAG, "STATUS not ready: %x", value);
    return RetryResult::RETRY;
  }

  uint8_t t_buf[2]{0};
  this->read_register(TEMP_L, t_buf, 2);
  float temp =
      TEMPERATURE_SCALE * static_cast<float>(static_cast<int16_t>(t_buf[1]) << 8 | static_cast<int16_t>(t_buf[0]));
  this->temperature_sensor_->publish_state(temp);
  uint8_t p_buf[3]{0};
  this->read_register(PRES_OUT_XL, p_buf, 3);
  uint32_t p_lsb =
      static_cast<uint32_t>(p_buf[2]) << 16 | static_cast<uint32_t>(p_buf[1]) << 8 | static_cast<uint32_t>(p_buf[0]);
  this->pressure_sensor_->publish_state(PRESSURE_SCALE * static_cast<float>(p_lsb));
  return RetryResult::DONE;
}

}  // namespace lps22
}  // namespace esphome
