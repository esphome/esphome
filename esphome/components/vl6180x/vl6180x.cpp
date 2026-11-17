#include "vl6180x.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::vl6180x {

ESPHOME_LOG_TAG(TAG, "vl6180x");

static constexpr uint32_t MEASUREMENT_ID = 0;

static constexpr uint8_t DEFAULT_ADDRESS = 0x29;
static constexpr uint8_t MODEL_ID = 0xB4;

static constexpr uint16_t REG_IDENTIFICATION_MODEL_ID = 0x000;
static constexpr uint16_t REG_SYSTEM_INTERRUPT_CLEAR = 0x015;
static constexpr uint16_t REG_SYSTEM_FRESH_OUT_OF_RESET = 0x016;
static constexpr uint16_t REG_SYSRANGE_START = 0x018;
static constexpr uint16_t REG_SYSRANGE_CROSSTALK_VALID_HEIGHT = 0x021;
static constexpr uint16_t REG_SYSRANGE_PART_TO_PART_OFFSET = 0x024;
static constexpr uint16_t REG_SYSRANGE_RANGE_CHECK_ENABLES = 0x02D;
static constexpr uint16_t REG_SYSALS_START = 0x038;
static constexpr uint16_t REG_SYSALS_ANALOGUE_GAIN = 0x03F;
static constexpr uint16_t REG_SYSALS_INTEGRATION_PERIOD = 0x040;
static constexpr uint16_t REG_RESULT_RANGE_STATUS = 0x04D;
static constexpr uint16_t REG_RESULT_ALS_STATUS = 0x04E;
static constexpr uint16_t REG_RESULT_RANGE_VAL = 0x062;
static constexpr uint16_t REG_RANGE_SCALING = 0x096;
static constexpr uint16_t REG_I2C_DEVICE_ADDRESS = 0x212;

// RESULT_INTERRUPT_STATUS_GPIO: a new range sample in bits 2:0, a new ALS sample in bits 5:3
static constexpr uint8_t RANGE_SAMPLE_READY = 0x04;
static constexpr uint8_t ALS_SAMPLE_READY = 0x04 << 3;

// Range conversion is at most the 50 ms convergence time set below plus readout
static constexpr uint32_t RANGE_TIME_MS = 70;
static constexpr uint32_t ALS_READOUT_MARGIN_MS = 400;

// Indexed by the range scaler, 1x to 3x (AN4545)
static constexpr uint16_t RANGE_SCALING_VALUES[] PROGMEM = {0, 253, 127, 84};

struct RegValue {
  uint16_t reg;
  uint8_t value;
};

// Private settings written once after a power-on reset (AN4545 table 7)
static constexpr RegValue MANDATORY_SETTINGS[] PROGMEM = {
    {0x0207, 0x01},
    {0x0208, 0x01},
    {0x0096, 0x00},
    {0x0097, 0xFD},
    {0x00E3, 0x00},
    {0x00E4, 0x04},
    {0x00E5, 0x02},
    {0x00E6, 0x01},
    {0x00E7, 0x03},
    {0x00F5, 0x02},
    {0x00D9, 0x05},
    {0x00DB, 0xCE},
    {0x00DC, 0x03},
    {0x00DD, 0xF8},
    {0x009F, 0x00},
    {0x00A3, 0x3C},
    {0x00B7, 0x00},
    {0x00BB, 0x3C},
    {0x00B2, 0x09},
    {0x00CA, 0x09},
    {0x0198, 0x01},
    {0x01B0, 0x17},
    {0x01AD, 0x00},
    {0x00FF, 0x05},
    {0x0100, 0x05},
    {0x0199, 0x05},
    {0x01A6, 0x1B},
    {0x01AC, 0x3E},
    {0x01A7, 0x1F},
    {0x0030, 0x00},
    {REG_SYSTEM_FRESH_OUT_OF_RESET, 0x00},
};

// Recommended settings (AN4545 section 9), with GPIO1 left floating and new-sample interrupts flagged
static constexpr RegValue DEFAULT_SETTINGS[] PROGMEM = {
    {0x010A, 0x30},  // averaging sample period
    {0x0031, 0xFF},  // repeat temperature calibration every 255 range measurements
    {0x002E, 0x01},  // run the first temperature calibration now
    {0x001C, 0x32},  // 50 ms maximum convergence time
    {0x0011, 0x20},  // GPIO1 off
    {0x0014, RANGE_SAMPLE_READY | ALS_SAMPLE_READY},
    {REG_SYSTEM_INTERRUPT_CLEAR, 0x07},
};

template<size_t N> static bool write_settings(i2c::I2CDevice *device, const RegValue (&table)[N]) {
  for (const auto &entry : table) {
    RegValue setting;
    progmem_memcpy(&setting, &entry, sizeof(setting));
    if (!device->write_byte_reg16(setting.reg, setting.value))
      return false;
  }
  return true;
}

#ifdef VL6180X_ENABLE_PIN_COUNT
SlotList<GPIOPin *, VL6180X_ENABLE_PIN_COUNT> VL6180XComponent::enable_pins;  // NOLINT
#endif

void VL6180XComponent::setup() {
#ifdef VL6180X_ENABLE_PIN_COUNT
  // Every sensor boots at 0x29, so all are held in reset and then released one at a time
  for (auto *pin : enable_pins) {
    pin->setup();
    pin->digital_write(false);
  }
  enable_pins.clear();
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->digital_write(true);
    delay(10);
  }
#endif
  const uint8_t address = this->address_;
  this->set_i2c_address(DEFAULT_ADDRESS);
  uint8_t model_id = 0;
  const bool read_ok = this->read_byte_reg16(REG_IDENTIFICATION_MODEL_ID, &model_id);
  const bool ok = read_ok && model_id == MODEL_ID && this->init_() &&
                  (address == DEFAULT_ADDRESS || this->write_byte_reg16(REG_I2C_DEVICE_ADDRESS, address));
  this->set_i2c_address(address);
  if (ok)
    return;
#ifdef VL6180X_ENABLE_PIN_COUNT
  // A sensor left at 0x29 would answer for the next one released
  if (this->enable_pin_ != nullptr)
    this->enable_pin_->digital_write(false);
#endif
  if (read_ok && model_id != MODEL_ID) {
    ESP_LOGE(TAG, "Wrong model ID 0x%02X", model_id);
    this->mark_failed(LOG_STR("Wrong model ID"));
    return;
  }
  this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
}

bool VL6180XComponent::init_() {
  uint8_t fresh;
  uint8_t offset;
  if (!this->read_byte_reg16(REG_SYSTEM_FRESH_OUT_OF_RESET, &fresh) ||
      !this->read_byte_reg16(REG_SYSRANGE_PART_TO_PART_OFFSET, &offset))
    return false;
  // The factory offset is for 1x; a sensor that kept power through an MCU reboot already holds it divided by the
  // scaler from the last setup
  int16_t part_offset = static_cast<int8_t>(offset);
  if (fresh & 0x01) {
    if (!write_settings(this, MANDATORY_SETTINGS))
      return false;
  } else {
    uint8_t scaling[2];
    if (this->read_register16(REG_RANGE_SCALING, scaling, 2) != i2c::ERROR_OK)
      return false;
    const uint16_t value = encode_uint16(scaling[0], scaling[1]);
    for (uint8_t scaler = 2; scaler <= 3; scaler++) {
      if (value == progmem_read_uint16(&RANGE_SCALING_VALUES[scaler]))
        part_offset *= scaler;
    }
  }

  uint8_t period[2];
  put_be16(period, this->als_integration_time_ms_ - 1);
  uint8_t scaling[2];
  put_be16(scaling, progmem_read_uint16(&RANGE_SCALING_VALUES[this->scaler_]));
  return write_settings(this, DEFAULT_SETTINGS) &&
         this->write_byte_reg16(REG_SYSALS_ANALOGUE_GAIN, 0x40 | this->als_gain_code_) &&
         this->write_register16(REG_SYSALS_INTEGRATION_PERIOD, period, 2) == i2c::ERROR_OK &&
         this->write_register16(REG_RANGE_SCALING, scaling, 2) == i2c::ERROR_OK &&
         this->write_byte_reg16(REG_SYSRANGE_PART_TO_PART_OFFSET, static_cast<uint8_t>(part_offset / this->scaler_)) &&
         this->write_byte_reg16(REG_SYSRANGE_CROSSTALK_VALID_HEIGHT, 20 / this->scaler_) &&
         // SNR check, plus range ignore only at 1x where its threshold is calibrated
         this->write_byte_reg16(REG_SYSRANGE_RANGE_CHECK_ENABLES, 0x10 | (this->scaler_ == 1 ? 0x01 : 0x00));
}

void VL6180XComponent::update() {
  // A measurement still running means the update interval is shorter than one cycle
  if (this->measuring_)
    return;
  this->measuring_ = true;
  if (this->range_sensor_ != nullptr) {
    this->start_range_();
  } else {
    this->start_als_();
  }
}

void VL6180XComponent::start_range_() {
  if (!this->status_comm_check(this->write_byte_reg16(REG_SYSRANGE_START, 0x01))) {
    this->measuring_ = false;
    return;
  }
  this->set_timeout(MEASUREMENT_ID, RANGE_TIME_MS, [this]() { this->read_range_(); });
}

void VL6180XComponent::read_range_() {
  uint8_t status[3];  // range status, ALS status, interrupt status
  uint8_t raw;
  if (!this->status_comm_check(this->read_register16(REG_RESULT_RANGE_STATUS, status, 3) == i2c::ERROR_OK) ||
      !this->status_comm_check((status[2] & 0x07) == RANGE_SAMPLE_READY) ||
      !this->status_comm_result(this->read_byte_reg16(REG_RESULT_RANGE_VAL, &raw) &&
                                this->write_byte_reg16(REG_SYSTEM_INTERRUPT_CLEAR, 0x07))) {
    this->measuring_ = false;
    return;
  }
  // No target, out of range or a self-test fault
  const uint8_t error = status[0] >> 4;
  if (error != 0) {
    ESP_LOGV(TAG, "Range error code %u", error);
    PUBLISH_STATE(this->range_sensor_, NAN);
  } else {
    PUBLISH_STATE(this->range_sensor_, static_cast<float>(raw * this->scaler_));
  }
  if (this->lux_sensor_ != nullptr) {
    this->start_als_();
  } else {
    this->measuring_ = false;
  }
}

void VL6180XComponent::start_als_() {
  if (!this->status_comm_check(this->write_byte_reg16(REG_SYSALS_START, 0x01))) {
    this->measuring_ = false;
    return;
  }
  this->set_timeout(MEASUREMENT_ID, this->als_integration_time_ms_ + ALS_READOUT_MARGIN_MS,
                    [this]() { this->read_als_(); });
}

void VL6180XComponent::read_als_() {
  this->measuring_ = false;
  uint8_t result[4];  // ALS status, interrupt status, ALS value
  if (!this->status_comm_check(this->read_register16(REG_RESULT_ALS_STATUS, result, 4) == i2c::ERROR_OK) ||
      !this->status_comm_check((result[1] & 0x38) == ALS_SAMPLE_READY) ||
      !this->status_comm_result(this->write_byte_reg16(REG_SYSTEM_INTERRUPT_CLEAR, 0x07)))
    return;
  // An overflow in bright light is a reading, not a fault
  if (result[0] >> 4) {
    ESP_LOGV(TAG, "ALS error code %u", result[0] >> 4);
    PUBLISH_STATE(this->lux_sensor_, NAN);
    return;
  }
  PUBLISH_STATE(this->lux_sensor_, encode_uint16(result[2], result[3]) * this->lux_per_count_);
}

void VL6180XComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "VL6180X:");
  LOG_I2C_DEVICE(this);
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Range", this->range_sensor_);
  LOG_SENSOR("  ", "Lux", this->lux_sensor_);
}

}  // namespace esphome::vl6180x
