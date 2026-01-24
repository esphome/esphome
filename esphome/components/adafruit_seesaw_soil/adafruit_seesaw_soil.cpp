// Implements communication with an Adafruit Soil Moisture sensor based on the seesaw platform.
// - Soil Sensor Overview https://learn.adafruit.com/adafruit-stemma-soil-sensor-i2c-capacitive-moisture-sensor
// - Seesaw Overview https://learn.adafruit.com/adafruit-seesaw-atsamd09-breakout/overview
// Delays are based on https://github.com/adafruit/Adafruit_Seesaw/blob/master/Adafruit_seesaw.cpp
// for touchRead (soil moisture/humidity) and getTemp (ambient air temperature)

#include "adafruit_seesaw_soil.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace adafruit_seesaw_soil {

static constexpr auto TAG = "adafruit_seesaw_soil";

static constexpr uint8_t SEESAW_STATUS_BASE = 0x00;
static constexpr uint8_t SEESAW_STATUS_HW_ID = 0x01;
static constexpr uint8_t SEESAW_STATUS_VERSION = 0x02;
static constexpr uint8_t SEESAW_STATUS_OPTIONS = 0x03;
static constexpr uint8_t SEESAW_STATUS_TEMP = 0x04;
static constexpr uint8_t SEESAW_STATUS_SWRST = 0x7F;

static constexpr uint8_t SEESAW_TOUCH_BASE = 0x0F;
static constexpr uint8_t SEESAW_TOUCH_CHANNEL_OFFSET = 0x10;
static constexpr uint8_t SEESAW_TOUCH_PIN = 0;

static constexpr uint8_t SEESAW_RST_DELAY_MS = 10;
static constexpr uint8_t SEESAW_READ_DELAY_MS = 80;

static constexpr uint8_t SEESAW_RESET_CMD[] = {SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, 0xFF};

constexpr uint16_t make_reg(uint8_t base, uint8_t cmd) { return base | (cmd << 2); }

static constexpr uint16_t SEESAW_HW_ID_REG = make_reg(SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID);
static constexpr uint8_t SEESAW_HW_ID_CMD[] = {SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID};

static constexpr uint16_t SEESAW_VERSION_REG = make_reg(SEESAW_STATUS_BASE, SEESAW_STATUS_VERSION);
static constexpr uint8_t SEESAW_VERSION_CMD[] = {SEESAW_STATUS_BASE, SEESAW_STATUS_VERSION};

static constexpr uint16_t SEESAW_TEMP_REG = make_reg(SEESAW_STATUS_BASE, SEESAW_STATUS_TEMP);
static constexpr uint8_t SEESAW_TEMP_CMD[] = {SEESAW_STATUS_BASE, SEESAW_STATUS_TEMP};

static constexpr uint16_t SEESAW_MOIST_REG =
    make_reg(SEESAW_TOUCH_BASE, SEESAW_TOUCH_CHANNEL_OFFSET + SEESAW_TOUCH_PIN);
static constexpr uint8_t SEESAW_MOIST_CMD[] = {SEESAW_TOUCH_BASE, SEESAW_TOUCH_CHANNEL_OFFSET + SEESAW_TOUCH_PIN};

static constexpr uint8_t SEESAW_STARTUP_RETRIES = 10;
static constexpr uint8_t SEESAW_READ_RETRIES = 3;

enum class SeesawHwId : uint8_t {
  CODE_SAMD09 = 0x55,    ///< seesaw HW ID code for SAMD09
  CODE_TINY806 = 0x84,   ///< seesaw HW ID code for ATtiny806
  CODE_TINY807 = 0x85,   ///< seesaw HW ID code for ATtiny807
  CODE_TINY816 = 0x86,   ///< seesaw HW ID code for ATtiny816
  CODE_TINY817 = 0x87,   ///< seesaw HW ID code for ATtiny817
  CODE_TINY1616 = 0x88,  ///< seesaw HW ID code for ATtiny1616
  CODE_TINY1617 = 0x89   ///< seesaw HW ID code for ATtiny1617
};

void AdafruitSeesawSoil::setup() {
  this->hardware_type_ = 0;
  this->loop_state_ = LoopState::BOOT;
}

void AdafruitSeesawSoil::loop() {
  // State machine for handling long-running setup and sensor read commands

  ESP_LOGV(TAG, "Looping: setup state %d", loop_state_);

  // Setup State
  switch (loop_state_) {
    case LoopState::BOOT:
      if (this->setup_retry_count_ >= SEESAW_STARTUP_RETRIES) {
        // Maximum retries, setup failed
        ESP_LOGE(TAG, "Initialization failed to detect HW ID");
        this->mark_failed();
        this->loop_state_ = LoopState::SETUP_FAILED;
      } else if (this->write(SEESAW_RESET_CMD, sizeof(SEESAW_RESET_CMD)) != i2c::ERROR_OK) {
        ESP_LOGE(TAG, "Reset failed");
        ++this->setup_retry_count_;
        this->loop_state_ = LoopState::BOOT;
      } else {
        this->loop_state_ = LoopState::RESET_COMMAND_SENT;
      }
      break;
    case LoopState::RESET_COMMAND_SENT:
      if (this->write(SEESAW_HW_ID_CMD, sizeof(SEESAW_HW_ID_CMD)) != i2c::ERROR_OK) {
        ESP_LOGE(TAG, "Failed to send HW ID command");
        ++this->setup_retry_count_;
        this->loop_state_ = LoopState::BOOT;
      } else {
        this->loop_state_ = LoopState::HW_ID_COMMAND_SENT;
      }
      break;
    case LoopState::HW_ID_COMMAND_SENT:
      if (this->read_register16(SEESAW_HW_ID_REG, &this->hardware_type_, 1) != i2c::ERROR_OK) {
        ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
        ++this->setup_retry_count_;
        this->loop_state_ = LoopState::BOOT;
      } else {
        switch (static_cast<SeesawHwId>(this->hardware_type_)) {
          case SeesawHwId::CODE_SAMD09:
          case SeesawHwId::CODE_TINY806:
          case SeesawHwId::CODE_TINY807:
          case SeesawHwId::CODE_TINY816:
          case SeesawHwId::CODE_TINY817:
          case SeesawHwId::CODE_TINY1616:
          case SeesawHwId::CODE_TINY1617:
            this->version_ = this->get_version();
            if (this->version_.has_value()) {
              ESP_LOGD(TAG, "%04u.%02u.%02u-%u", this->version_->year, this->version_->month, this->version_->day,
                       this->version_->pid);
            } else {
              ESP_LOGE(TAG, "Failed to read version");
            }
            this->loop_state_ = LoopState::WAITING_TO_START_READING;
            break;
          default:
            ESP_LOGE(TAG, "Initialization detected invalid HW ID %#04x", this->hardware_type_);
            this->mark_failed();
            this->loop_state_ = LoopState::SETUP_FAILED;
        }
      }
      break;
    case LoopState::SETUP_FAILED:
    case LoopState::WAITING_TO_START_READING:
      // No-op;
      break;
    case LoopState::WAITING_TO_UPDATE_TEMP:
      if (this->write(SEESAW_TEMP_CMD, sizeof(SEESAW_TEMP_CMD)) != i2c::ERROR_OK) {
        this->temperature_sensor_->publish_state(NAN);
        this->status_set_error(LOG_STR("Temperature reading failed"));
      } else {
        this->loop_state_ = LoopState::READ_TEMP_COMMAND_SENT;
        this->last_temperature_read_op_ = millis();
      }
      break;
    case LoopState::READ_TEMP_COMMAND_SENT:
      // Check time, read the register after a brief delay
      if (millis() - last_temperature_read_op_ > 10) {
        const auto temperature = get_temperature_c();
        this->temperature_sensor_->publish_state(temperature.value_or(NAN));
        if (!temperature) {
          this->status_set_error(LOG_STR("Temperature reading failed"));
        }
        if (this->moisture_sensor_) {
          this->loop_state_ = LoopState::WAITING_TO_UPDATE_MOIST;
        } else {
          this->loop_state_ = LoopState::WAITING_TO_START_READING;
        }
      }
      break;
    case LoopState::WAITING_TO_UPDATE_MOIST:
      if (this->moisture_sensor_) {
        if (this->write(SEESAW_MOIST_CMD, sizeof(SEESAW_MOIST_CMD)) != i2c::ERROR_OK) {
          this->moisture_sensor_->publish_state(NAN);
          this->status_set_error(LOG_STR("Moisture reading failed"));
        } else {
          this->loop_state_ = LoopState::READ_MOIST_COMMAND_SENT;
          this->last_moisture_read_op_ = millis();
        }
      } else {
        this->loop_state_ = LoopState::WAITING_TO_START_READING;
      }
      break;
    case LoopState::READ_MOIST_COMMAND_SENT:
      if (millis() - this->last_moisture_read_op_ > 30) {
        const auto moisture = get_moisture();
        this->moisture_sensor_->publish_state(moisture.value_or(NAN));
        if (!moisture) {
          this->status_set_error(LOG_STR("Moisture reading failed"));
        }
        this->loop_state_ = LoopState::WAITING_TO_START_READING;
      }
      break;
  }
}

void AdafruitSeesawSoil::update() {
  // Start a reading for each enabled sensor
  if (this->temperature_sensor_) {
    this->loop_state_ = LoopState::WAITING_TO_UPDATE_TEMP;
  } else if (this->moisture_sensor_) {
    this->loop_state_ = LoopState::WAITING_TO_UPDATE_MOIST;
  }
}

float AdafruitSeesawSoil::get_setup_priority() const { return setup_priority::DATA; }

void AdafruitSeesawSoil::dump_config() {
  if (this->version_.has_value()) {
    ESP_LOGCONFIG(TAG, "Adafruit Seesaw Soil: version %02u.%02u.%02u-%u hardwareType %#04x", this->version_->year,
                  this->version_->month, this->version_->day, this->version_->pid, hardware_type_);
  } else {
    ESP_LOGCONFIG(TAG, "Adafruit Seesaw Soil:");
  }
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }

  LOG_SENSOR("  ", "Ambient Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Soil Moisture", this->moisture_sensor_);
}

std::optional<AdafruitSeesawSoil::Version> AdafruitSeesawSoil::get_version() {
  if (this->write(SEESAW_VERSION_CMD, sizeof(SEESAW_VERSION_CMD)) != i2c::ERROR_OK) {
    return std::nullopt;
  }
  delayMicroseconds(1000);
  std::array<uint8_t, 4> buf;
  if (this->read_register16(SEESAW_VERSION_REG, buf.data(), buf.size()) != i2c::ERROR_OK) {
    return std::nullopt;
  }
  const uint32_t raw = encode_uint32(buf[0], buf[1], buf[2], buf[3]);
  return Version{.pid = static_cast<uint16_t>(raw >> 16),
                 .year = static_cast<uint8_t>(raw & 0x3F),
                 .month = static_cast<uint8_t>((raw >> 7) & 0xF),
                 .day = static_cast<uint8_t>((raw >> 11) & 0x1F)};
}

std::optional<float> AdafruitSeesawSoil::get_temperature_c() {
  std::array<uint8_t, 4> buf{SEESAW_STATUS_BASE, SEESAW_STATUS_TEMP, 0xFF, 0xFF};
  if (this->read_register16(SEESAW_TEMP_REG, buf.data(), buf.size()) != i2c::ERROR_OK) {
    return std::nullopt;
  }
  const uint32_t raw = encode_uint32(buf[0], buf[1], buf[2], buf[3]);
  return static_cast<float>(raw) / static_cast<float>(1 << 16);
}

std::optional<uint16_t> AdafruitSeesawSoil::get_moisture() {
  std::array<uint8_t, 2> buf{0xFF, 0xFF};
  if (this->read_register16(SEESAW_MOIST_REG, buf.data(), buf.size()) != i2c::ERROR_OK) {
    return std::nullopt;
  }
  uint16_t raw = encode_uint16(buf[0], buf[1]);
  return raw;
}

}  // namespace adafruit_seesaw_soil
}  // namespace esphome
