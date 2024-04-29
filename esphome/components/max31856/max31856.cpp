#include "max31856.h"

#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace max31856 {

static const char *const TAG = "max31856";

enum MAX31856RegisterMasks : uint8_t { SPI_WRITE_M = 0x80 };

enum MAX31856Registers : uint8_t {
  MAX31856_MASK_REG = 0x02,    ///< Fault Mask register
  MAX31856_CJHF_REG = 0x03,    ///< Cold junction High temp fault register
  MAX31856_CJLF_REG = 0x04,    ///< Cold junction Low temp fault register
  MAX31856_LTHFTH_REG = 0x05,  ///< Linearized Temperature High Fault Threshold Register, MSB
  MAX31856_LTHFTL_REG = 0x06,  ///< Linearized Temperature High Fault Threshold Register, LSB
  MAX31856_LTLFTH_REG = 0x07,  ///< Linearized Temperature Low Fault Threshold Register, MSB
  MAX31856_LTLFTL_REG = 0x08,  ///< Linearized Temperature Low Fault Threshold Register, LSB
  MAX31856_CJTO_REG = 0x09,    ///< Cold-Junction Temperature Offset Register
  MAX31856_CJTH_REG = 0x0A,    ///< Cold-Junction Temperature Register, MSB
  MAX31856_CJTL_REG = 0x0B,    ///< Cold-Junction Temperature Register, LSB
  MAX31856_LTCBH_REG = 0x0C,   ///< Linearized TC Temperature, Byte 2
  MAX31856_LTCBM_REG = 0x0D,   ///< Linearized TC Temperature, Byte 1
  MAX31856_LTCBL_REG = 0x0E,   ///< Linearized TC Temperature, Byte 0
  MAX31856_SR_REG = 0x0F,      ///< Fault Status Register

  MAX31856_FAULT_CJRANGE = 0x80,  ///< Fault status Cold Junction Out-of-Range flag
  MAX31856_FAULT_TCRANGE = 0x40,  ///< Fault status Thermocouple Out-of-Range flag
  MAX31856_FAULT_CJHIGH = 0x20,   ///< Fault status Cold-Junction High Fault flag
  MAX31856_FAULT_CJLOW = 0x10,    ///< Fault status Cold-Junction Low Fault flag
  MAX31856_FAULT_TCHIGH = 0x08,   ///< Fault status Thermocouple Temperature High Fault flag
  MAX31856_FAULT_TCLOW = 0x04,    ///< Fault status Thermocouple Temperature Low Fault flag
  MAX31856_FAULT_OVUV = 0x02,     ///< Fault status Overvoltage or Undervoltage Input Fault flag
  MAX31856_FAULT_OPEN = 0x01,     ///< Fault status Thermocouple Open-Circuit Fault flag
};

constexpr static uint8_t MAX31856_CR0_REG = 0x00;
constexpr static uint8_t MAX31856_CR0_AUTOCONVERT = 1 << 7;
constexpr static uint8_t MAX31856_CR0_1SHOT = 1 << 6;
constexpr static uint8_t MAX31856_CR0_OCFAULT_ENABLED = 1 << 4;
constexpr static uint8_t MAX31856_CR0_CJ_SENSOR_DISABLE = 1 << 3;
constexpr static uint8_t MAX31856_CR0_FAULT_INT_MODE = 1 << 2;
constexpr static uint8_t MAX31856_CR0_FAULTCLR = 1 << 1;

constexpr static uint8_t MAX31856_CR1_REG = 0x01;

// Based on Adafruit's library: https://github.com/adafruit/Adafruit_MAX31856

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG

static const char *enum_type_to_str(MAX31856ThermocoupleType type) {
  switch (type) {
    case MAX31856_TCTYPE_B:
      return "B";
    case MAX31856_TCTYPE_E:
      return "E";
    case MAX31856_TCTYPE_J:
      return "J";
    case MAX31856_TCTYPE_K:
      return "K";
    case MAX31856_TCTYPE_N:
      return "N";
    case MAX31856_TCTYPE_R:
      return "R";
    case MAX31856_TCTYPE_S:
      return "S";
    case MAX31856_TCTYPE_T:
      return "T";
  }
  return "K";
}

#endif

static int enum_samples_per_value_to_int(MAX31856SamplesPerValue value) {
  switch (value) {
    case AVE_SAMPLES_1:
      return 1;
    case AVE_SAMPLES_2:
      return 2;
    case AVE_SAMPLES_4:
      return 4;
    case AVE_SAMPLES_8:
      return 8;
    case AVE_SAMPLES_16:
      return 16;
  }
  return 16;
}

void MAX31856Sensor::setup() {
  ESP_LOGVV(TAG, "Setting up MAX31856Sensor '%s'...", this->name_.c_str());

  this->spi_setup();

  // assert on any fault
  this->write_register_(MAX31856_MASK_REG, 0x0);

  // Set the thermocouple type and the number of samples to average
  this->write_register_(MAX31856_CR1_REG, static_cast<uint8_t>(samples_per_value_) | static_cast<uint8_t>(type_));

  // Turn on open circuit faults and set the filter
  cr0_ = MAX31856_CR0_OCFAULT_ENABLED | static_cast<uint8_t>(filter_);

  if (this->data_ready_ != nullptr) {
    // Enable autoconversion
    cr0_ |= MAX31856_CR0_AUTOCONVERT;

    // Data ready is active low
    this->data_ready_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

    ESP_LOGI(TAG, "Using autoconversion mode, update_interval will be ignored");
  }

  this->write_register_(MAX31856_CR0_REG, cr0_);

  ESP_LOGVV(TAG, "Completed setting up MAX31856Sensor '%s'...", this->name_.c_str());
}

void MAX31856Sensor::dump_config() {
  LOG_SENSOR("", "MAX31856", this);
  LOG_PIN("  CS Pin: ", this->cs_);
  if (this->data_ready_ != nullptr) {
    LOG_PIN("  Data Ready Pin: ", this->data_ready_);
  } else {
    LOG_UPDATE_INTERVAL(this);
  }
  ESP_LOGCONFIG(TAG, "  Mains Filter: %s",
                (filter_ == FILTER_60HZ ? "60 Hz" : (filter_ == FILTER_50HZ ? "50 Hz" : "Unknown!")));
  ESP_LOGCONFIG(TAG, "  Type: %s", enum_type_to_str(type_));
  ESP_LOGCONFIG(TAG, "  Samples per value: %d", enum_samples_per_value_to_int(samples_per_value_));
  ESP_LOGCONFIG(TAG, "  Mode: %s", (this->data_ready_ != nullptr) ? "autoconversion" : "polling");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "HasFaultBinarySensor", this->has_fault_binary_sensor_);
#endif
}

// This method is used for autoconversion mode
void MAX31856Sensor::loop() {
  ESP_LOGVV(TAG, "loop");

  if (this->data_ready_ == nullptr || this->have_faults_()) {
    return;
  }

  // Data ready is active low
  if (!this->data_ready_->digital_read()) {
    this->read_thermocouple_temperature_();
  }
}

// This method is used for polling mode
void MAX31856Sensor::update() {
  ESP_LOGVV(TAG, "update");

  if (this->data_ready_ != nullptr) {
    return;
  }

  // Start a oneshot reading
  this->write_register_(MAX31856_CR0_REG, cr0_ | MAX31856_CR0_1SHOT);

  // Datasheet max conversion time for 1 shot for 1 sample is 155ms for 60Hz / 185ms for 50Hz
  uint32_t timeout{};
  auto nsamples = enum_samples_per_value_to_int(samples_per_value_);
  if (filter_ == FILTER_60HZ) {
    timeout = 155 + static_cast<uint32_t>(std::ceil((nsamples - 1) * 33.33f));
  } else {
    timeout = 185 + (nsamples - 1) * 40;
  }
  this->set_timeout("MAX31856Sensor::read_oneshot_temperature_", timeout,
                    [this]() { this->read_oneshot_temperature_(); });
}

void MAX31856Sensor::call_setup() {
  this->setup();

  if (this->data_ready_ == nullptr) {
    this->start_poller();
  }
}

void MAX31856Sensor::read_oneshot_temperature_() {
  if (this->have_faults_()) {
    return;
  }

  read_thermocouple_temperature_();
}

void MAX31856Sensor::read_thermocouple_temperature_() {
  int32_t temp24 = this->read_register24_(MAX31856_LTCBH_REG);
  if (temp24 & 0x800000) {
    temp24 |= 0xFF000000;  // fix sign
  }

  temp24 >>= 5;  // bottom 5 bits are unused

  float temp_c = temp24;
  temp_c /= 128.0f;

  ESP_LOGD(TAG, "Got thermocouple temperature: %.3f°C", temp_c);
  this->publish_state(temp_c);
}

bool MAX31856Sensor::have_faults_() {
  uint8_t faults = this->read_register_(MAX31856_SR_REG);
  if (faults == 0) {
#ifdef USE_BINARY_SENSOR
    if (this->has_fault_binary_sensor_) {
      this->has_fault_binary_sensor_->publish_state(false);
    }
#endif
    this->status_clear_error();
    return false;
  }

  if (!this->status_has_error()) {
#ifdef USE_BINARY_SENSOR
    if (this->has_fault_binary_sensor_) {
      this->has_fault_binary_sensor_->publish_state(true);
    }
#endif
    this->status_set_error();

    const auto log_err = [&](uint8_t bit, const char *msg) {
      if ((faults & bit) != 0) {
        ESP_LOGE(TAG, "%s", msg);
      }
    };

    log_err(MAX31856_FAULT_CJRANGE, "Cold Junction Out-of-Range");
    log_err(MAX31856_FAULT_TCRANGE, "Thermocouple Out-of-Range");
    log_err(MAX31856_FAULT_CJHIGH, "Cold-Junction High Fault");
    log_err(MAX31856_FAULT_CJLOW, "Cold-Junction Low Fault");
    log_err(MAX31856_FAULT_TCHIGH, "Thermocouple Temperature High Fault");
    log_err(MAX31856_FAULT_TCLOW, "Thermocouple Temperature Low Fault");
    log_err(MAX31856_FAULT_OVUV, "Overvoltage or Undervoltage Input Fault");
    log_err(MAX31856_FAULT_OPEN, "Thermocouple Open-Circuit Fault");
  }

  return true;
}

void MAX31856Sensor::write_register_(uint8_t reg, uint8_t value) {
  ESP_LOGVV(TAG, "write_register_ raw reg=0x%02X value=0x%02X", reg, value);
  reg |= SPI_WRITE_M;
  ESP_LOGVV(TAG, "write_register_ masked reg=0x%02X value=0x%02X", reg, value);
  this->enable();
  ESP_LOGVV(TAG, "write_byte reg=0x%02X", reg);
  this->write_byte(reg);
  ESP_LOGVV(TAG, "write_byte value=0x%02X", value);
  this->write_byte(value);
  this->disable();
  ESP_LOGV(TAG, "write_register_ 0x%02X: 0x%02X", reg, value);
}

uint8_t MAX31856Sensor::read_register_(uint8_t reg) {
  ESP_LOGVV(TAG, "read_register_ 0x%02X", reg);
  this->enable();
  ESP_LOGVV(TAG, "write_byte reg=0x%02X", reg);
  this->write_byte(reg);
  const uint8_t value(this->read_byte());
  ESP_LOGVV(TAG, "read_byte value=0x%02X", value);
  this->disable();
  ESP_LOGV(TAG, "read_register_ reg=0x%02X: value=0x%02X", reg, value);
  return value;
}

uint32_t MAX31856Sensor::read_register24_(uint8_t reg) {
  ESP_LOGVV(TAG, "read_register_24_ 0x%02X", reg);
  this->enable();
  ESP_LOGVV(TAG, "write_byte reg=0x%02X", reg);
  this->write_byte(reg);
  const uint8_t msb(this->read_byte());
  ESP_LOGVV(TAG, "read_byte msb=0x%02X", msb);
  const uint8_t mid(this->read_byte());
  ESP_LOGVV(TAG, "read_byte mid=0x%02X", mid);
  const uint8_t lsb(this->read_byte());
  ESP_LOGVV(TAG, "read_byte lsb=0x%02X", lsb);
  this->disable();
  const uint32_t value((msb << 16) | (mid << 8) | lsb);
  ESP_LOGV(TAG, "read_register_24_ reg=0x%02X: value=0x%06" PRIX32, reg, value);
  return value;
}

float MAX31856Sensor::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace max31856
}  // namespace esphome
