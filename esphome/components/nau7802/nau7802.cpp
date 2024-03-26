#include "nau7802.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace nau7802 {

static const char *const TAG = "nau7802";

// Only define what we need

static const uint8_t READ_BIT = 0x01;

static const uint8_t PU_CTRL_REG = 0x00;
static const uint8_t PU_CTRL_REGISTER_RESET = 0x01;
static const uint8_t PU_CTRL_POWERUP_DIGITAL = 0x02;
static const uint8_t PU_CTRL_POWERUP_ANALOG = 0x04;
static const uint8_t PU_CTRL_POWERUP_READY = 0x08;
static const uint8_t PU_CTRL_CYCLE_START = 0x10;
static const uint8_t PU_CTRL_CYCLE_READY = 0x20;
static const uint8_t PU_CTRL_AVDD_EXTERNAL = 0x80;

static const uint8_t CTRL1_REG = 0x01;
static const uint8_t CTRL1_LDO_SHIFT = 3;
static const uint8_t CTRL1_LDO_MASK = (0x7 << CTRL1_LDO_SHIFT);
static const uint8_t CTRL1_GAIN_MASK = 0x7;

static const uint8_t CTRL2_REG = 0x02;
static const uint8_t CTRL2_CRS_SHIFT = 4;
static const uint8_t CTRL2_CRS_MASK = (0x7 << CTRL2_CRS_SHIFT);
static const uint8_t CTRL2_CALS = 0x04;
static const uint8_t CTRL2_CAL_ERR = 0x08;

// only need the first data register for sequential read method
static const uint8_t ADCO_B2 = 0x12;

static const uint8_t ADC_REG = 0x15;
static const uint8_t ADC_CHPS_DISABLE = 0x30;

static const uint8_t PGA_REG = 0x1B;
static const uint8_t PGA_LDOMODE_ESR = 0x40;

static const uint8_t POWER_REG = 0x1C;
static const uint8_t POWER_PGA_CAP_EN = 0x80;

static const uint8_t DEVICE_REV = 0x1F;

void NAU7802Sensor::setup() {
  i2c::I2CRegister pu_ctrl = this->reg(PU_CTRL_REG);
  i2c::I2CRegister ctrl1 = this->reg(CTRL1_REG);
  i2c::I2CRegister ctrl2 = this->reg(CTRL2_REG);
  ESP_LOGCONFIG(TAG, "Setting up NAU7802 '%s'...", this->name_.c_str());
  uint8_t rev;

  if (this->read_register(DEVICE_REV | READ_BIT, &rev, 1)) {
    ESP_LOGE(TAG, "Failed I2C read during setup()");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Setting up NAU7802 Rev %d", rev);

  // reset
  pu_ctrl |= PU_CTRL_REGISTER_RESET;
  delay(10);
  pu_ctrl &= ~PU_CTRL_REGISTER_RESET;

  // power up digital hw
  pu_ctrl |= PU_CTRL_POWERUP_DIGITAL;

  delay(1);
  if (!(pu_ctrl.get() & PU_CTRL_POWERUP_READY)) {
    ESP_LOGE(TAG, "Failed to reset sensor during setup()");
    this->mark_failed();
    return;
  }

  // turn on AFE
  pu_ctrl |= PU_CTRL_POWERUP_ANALOG;
  delay(600);  // NOLINT
  pu_ctrl |= PU_CTRL_CYCLE_START;

  // set gain
  ctrl1 &= ~CTRL1_GAIN_MASK;
  ctrl1 |= this->gain_;

  // enable internal LDO
  if (this->ldo_ != NAU7802_LDO_EXTERNAL) {
    pu_ctrl |= PU_CTRL_AVDD_EXTERNAL;
    ctrl1 &= ~CTRL1_LDO_MASK;
    ctrl1 |= this->ldo_ << CTRL1_LDO_SHIFT;
  }

  // set sps
  ctrl2 &= ~CTRL2_CRS_MASK;
  ctrl2 |= this->sps_ << CTRL2_CRS_SHIFT;

  // disable ADC chopper clock
  i2c::I2CRegister adc_reg = this->reg(ADC_REG);
  adc_reg |= ADC_CHPS_DISABLE;

  // use low ESR caps
  i2c::I2CRegister pga_reg = this->reg(PGA_REG);
  pga_reg &= ~PGA_LDOMODE_ESR;

  // PGA stabilizer cap on output
  i2c::I2CRegister pwr_reg = this->reg(POWER_REG);
  pwr_reg |= POWER_PGA_CAP_EN;

  // Calibrate
  if (this->calibration_ != NAU7802_CALIBRATION_NONE) {
    ctrl2 |= this->calibration_;
    ctrl2 |= CTRL2_CALS;
    while (ctrl2.get() & CTRL2_CALS) {
      delay(10);
    }

    if (ctrl2.get() & CTRL2_CAL_ERR) {
      ESP_LOGE(TAG, "Failed to calibrate sensor during setup()");
      this->mark_failed();
    }
  }
}

void NAU7802Sensor::dump_config() {
  LOG_SENSOR("", "NAU7802", this);
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with NAU7802 failed earlier, during setup");
    return;
  }
  std::string calibration = "unknown";
  switch (this->calibration_) {
    case NAU7802_CALIBRATION_NONE:
      calibration = "None";
      break;
    case NAU7802_CALIBRATION_INTERNAL_OFFSET:
      calibration = "Internal Offset";
      break;
    case NAU7802_CALIBRATION_EXTERNAL_OFFSET:
      calibration = "External Offset";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Calibration: %s", calibration.c_str());
  std::string voltage = "unknown";
  switch (this->ldo_) {
    case NAU7802_LDO_2V4:
      voltage = "2.4V";
      break;
    case NAU7802_LDO_2V7:
      voltage = "2.7V";
      break;
    case NAU7802_LDO_3V0:
      voltage = "3.0V";
      break;
    case NAU7802_LDO_3V3:
      voltage = "3.3V";
      break;
    case NAU7802_LDO_3V6:
      voltage = "3.6V";
      break;
    case NAU7802_LDO_3V9:
      voltage = "3.9V";
      break;
    case NAU7802_LDO_4V2:
      voltage = "4.2V";
      break;
    case NAU7802_LDO_4V5:
      voltage = "4.5V";
      break;
    case NAU7802_LDO_EXTERNAL:
      voltage = "External";
      break;
  }
  ESP_LOGCONFIG(TAG, "  LDO Voltage: %s", voltage.c_str());
  int gain = 0;
  switch (this->gain_) {
    case NAU7802_GAIN_128:
      gain = 128;
      break;
    case NAU7802_GAIN_64:
      gain = 64;
      break;
    case NAU7802_GAIN_32:
      gain = 32;
      break;
    case NAU7802_GAIN_16:
      gain = 16;
      break;
    case NAU7802_GAIN_8:
      gain = 8;
      break;
    case NAU7802_GAIN_4:
      gain = 4;
      break;
    case NAU7802_GAIN_2:
      gain = 2;
      break;
    case NAU7802_GAIN_1:
      gain = 1;
      break;
  }
  ESP_LOGCONFIG(TAG, "  Gain: %dx", gain);
  int sps = 0;
  switch (this->sps_) {
    case NAU7802_SPS_320:
      sps = 320;
      break;
    case NAU7802_SPS_80:
      sps = 80;
      break;
    case NAU7802_SPS_40:
      sps = 40;
      break;
    case NAU7802_SPS_20:
      sps = 20;
      break;
    case NAU7802_SPS_10:
      sps = 10;
      break;
  }
  ESP_LOGCONFIG(TAG, "  Samples Per Second: %d", sps);
  LOG_UPDATE_INTERVAL(this);
}

float NAU7802Sensor::get_setup_priority() const { return setup_priority::DATA; }

void NAU7802Sensor::update() {
  if (this->is_failed()) {
    return;
  }

  if (!is_data_ready_()) {
    ESP_LOGW(TAG, "No measurements ready!");
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  // Get the most recent sample to publish
  uint8_t data[3];
  do {
    this->read_register(ADCO_B2, data, 3);
  } while (is_data_ready_());

  int32_t result = 0;
  result |= data[0] << 16;
  result |= data[1] << 8;
  result |= data[2];
  // extend sign bit
  if (result & 0x800000) {
    result |= 0xFF000000;
  }

  ESP_LOGD(TAG, "'%s': Got value %" PRId32, this->name_.c_str(), result);
  this->publish_state(result);
}

bool NAU7802Sensor::is_data_ready_() { return this->reg(PU_CTRL_REG).get() & PU_CTRL_CYCLE_READY; }

}  // namespace nau7802
}  // namespace esphome
