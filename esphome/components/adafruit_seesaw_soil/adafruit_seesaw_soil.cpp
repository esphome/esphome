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
static constexpr uint16_t SEESAW_VERSION_REG = make_reg(SEESAW_STATUS_BASE, SEESAW_STATUS_VERSION);
static constexpr uint16_t SEESAW_TEMP_REG = make_reg(SEESAW_STATUS_BASE, SEESAW_STATUS_TEMP);
static constexpr uint16_t SEESAW_MOIST_REG =
    make_reg(SEESAW_TOUCH_BASE, SEESAW_TOUCH_CHANNEL_OFFSET + SEESAW_TOUCH_PIN);

static constexpr uint8_t SEESAW_STARTUP_RETRIES = 10;
static constexpr uint8_t SEESAW_READ_RETRIES = 10;

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
  if (this->write(SEESAW_RESET_CMD, sizeof(SEESAW_RESET_CMD)) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Reset failed");
  }
  delay(SEESAW_RST_DELAY_MS);
  // Get the HW_ID
  hardware_type_ = 0;
  for (uint8_t retries = 0; retries < SEESAW_STARTUP_RETRIES && hardware_type_ == 0; ++retries) {
    if (this->read_register16(SEESAW_HW_ID_REG, &hardware_type_, 1) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    }
    delay(SEESAW_RST_DELAY_MS);
  }
  if (hardware_type_ == 0) {
    ESP_LOGE(TAG, "Initialization failed to detect HW ID");
    this->mark_failed();
    return;
  } else {
    switch (static_cast<SeesawHwId>(hardware_type_)) {
      case SeesawHwId::CODE_SAMD09:
      case SeesawHwId::CODE_TINY806:
      case SeesawHwId::CODE_TINY807:
      case SeesawHwId::CODE_TINY816:
      case SeesawHwId::CODE_TINY817:
      case SeesawHwId::CODE_TINY1616:
      case SeesawHwId::CODE_TINY1617:
        break;  // no-op valid code
      default:
        ESP_LOGE(TAG, "Initialization detected invalid HW ID %u", hardware_type_);
        this->mark_failed();
        return;
    }
  }
  version_ = get_version();
  if (version_.has_value()) {
    ESP_LOGD(TAG, "%04u.%02u.%02u-%u", version_->year, version_->month, version_->day, version_->pid);
  } else {
    ESP_LOGE(TAG, "Failed to read version");
  }
}
void AdafruitSeesawSoil::update() {
  this->set_timeout(SEESAW_READ_DELAY_MS, [this] {
    if (temperature_sensor_) {
      std::optional<float> temp_reading;
      for (read_count_ = 0; temperature_sensor_ && !temp_reading && read_count_ < SEESAW_READ_RETRIES; ++read_count_) {
        temp_reading = get_temperature_c();
      }
      this->temperature_sensor_->publish_state(temp_reading.value_or(NAN));
      if (!temp_reading) {
        this->status_set_error(LOG_STR("Reading timed out"));
      }
    }
    if (humidity_sensor_) {
      std::optional<uint16_t> moist_reading;
      for (read_count_ = 0; humidity_sensor_ && !moist_reading && read_count_ < SEESAW_READ_RETRIES; ++read_count_) {
        moist_reading = get_moisture();
      }
      this->humidity_sensor_->publish_state(moist_reading.value_or(NAN));
      if (!moist_reading) {
        this->status_set_error(LOG_STR("Reading timed out"));
      }
    }
  });
}

float AdafruitSeesawSoil::get_setup_priority() const { return setup_priority::DATA; }

void AdafruitSeesawSoil::dump_config() {
  if (version_.has_value()) {
    ESP_LOGCONFIG(TAG, "Adafruit Seesaw Soil: version %02u.%02u.%02u-%u", version_->year, version_->month,
                  version_->day, version_->pid);
  } else {
    ESP_LOGCONFIG(TAG, "Adafruit Seesaw Soil:");
  }
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }

  LOG_SENSOR("  ", "Ambient Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Soil Moisture", this->humidity_sensor_);
}

std::optional<AdafruitSeesawSoil::Version> AdafruitSeesawSoil::get_version() {
  std::array<uint8_t, 4> buf;
  if (this->read_register16(SEESAW_VERSION_REG, buf.data(), buf.size()) != i2c::ERROR_OK) {
    return std::nullopt;
  }
  const uint32_t raw = (static_cast<uint32_t>(buf[0]) << 24) | (static_cast<uint32_t>(buf[1]) << 16) |
                       (static_cast<uint32_t>(buf[2]) << 8) | static_cast<uint32_t>(buf[3]);
  return Version{.pid = static_cast<uint16_t>(raw >> 16),
                 .year = static_cast<uint8_t>(raw & 0x3F),
                 .month = static_cast<uint8_t>((raw >> 7) & 0xF),
                 .day = static_cast<uint8_t>((raw >> 11) & 0x1F)};
}

std::optional<float> AdafruitSeesawSoil::get_temperature_c() {
  std::array<uint8_t, 4> buf{SEESAW_STATUS_BASE, SEESAW_STATUS_TEMP, 0xFF, 0xFF};
  if (this->write(buf.data(), 2) != i2c::ERROR_OK) {
    return std::nullopt;
  }
  delayMicroseconds(1000);
  if (this->read_register16(SEESAW_TEMP_REG, buf.data(), buf.size()) != i2c::ERROR_OK) {
    return std::nullopt;
  }
  const uint32_t raw = (static_cast<uint32_t>(buf[0]) << 24) | (static_cast<uint32_t>(buf[1]) << 16) |
                       (static_cast<uint32_t>(buf[2]) << 8) | static_cast<uint32_t>(buf[3]);
  return static_cast<float>(raw) / static_cast<float>(1 << 16);
}

std::optional<uint16_t> AdafruitSeesawSoil::get_moisture() {
  std::array<uint8_t, 2> buf{SEESAW_TOUCH_BASE, SEESAW_TOUCH_CHANNEL_OFFSET + SEESAW_TOUCH_PIN};
  if (this->write(buf.data(), 2) != i2c::ERROR_OK) {
    return std::nullopt;
  }
  delayMicroseconds(3000 + this->read_count_ * 1000);
  if (this->read_register16(SEESAW_MOIST_REG, buf.data(), buf.size()) != i2c::ERROR_OK) {
    return std::nullopt;
  }
  uint16_t raw = (static_cast<uint16_t>(buf[0]) << 8) | (static_cast<uint16_t>(buf[1]));
  return raw;
}

}  // namespace adafruit_seesaw_soil
}  // namespace esphome
