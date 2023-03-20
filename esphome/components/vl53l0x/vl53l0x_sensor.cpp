#include "vl53l0x_sensor.h"
#include "esphome/core/log.h"

/*
 * Most of the code in this integration is based on the VL53L0x library
 * by Pololu (Pololu Corporation), which in turn is based on the VL53L0X
 * API from ST.
 *
 * For more information about licensing, please view the included LICENSE.txt file
 * in the vl53l0x integration directory.
 */

namespace esphome {
namespace vl53l0x {

static const char *const TAG = "vl53l0x";

std::list<VL53L0XSensor *> VL53L0XSensor::vl53_sensors;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
bool VL53L0XSensor::enable_pin_setup_complete = false;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

VL53L0XSensor::VL53L0XSensor() { VL53L0XSensor::vl53_sensors.push_back(this); }

void VL53L0XSensor::dump_config() {
  LOG_SENSOR("", "VL53L0X", this);
  LOG_UPDATE_INTERVAL(this);
  LOG_I2C_DEVICE(this);
  if (this->enable_pin_ != nullptr) {
    LOG_PIN("  Enable Pin: ", this->enable_pin_);
  }
  ESP_LOGCONFIG(TAG, "  Timeout: %u%s", this->timeout_us_, this->timeout_us_ > 0 ? "us" : " (no timeout)");
}

void VL53L0XSensor::setup() {
  ESP_LOGD(TAG, "'%s' - setup BEGIN", this->name_.c_str());

  if (!esphome::vl53l0x::VL53L0XSensor::enable_pin_setup_complete) {
    for (auto &vl53_sensor : vl53_sensors) {
      if (vl53_sensor->enable_pin_ != nullptr) {
        // Set enable pin as OUTPUT and disable the enable pin to force vl53 to HW Standby mode
        vl53_sensor->enable_pin_->setup();
        vl53_sensor->enable_pin_->digital_write(false);
      }
    }
    esphome::vl53l0x::VL53L0XSensor::enable_pin_setup_complete = true;
  }

  if (this->enable_pin_ != nullptr) {
    // Enable the enable pin to cause FW boot (to get back to 0x29 default address)
    this->enable_pin_->digital_write(true);
    delayMicroseconds(100);
  }

  // Save the i2c address we want and force it to use the default 0x29
  // until we finish setup, then re-address to final desired address.
  uint8_t final_address = address_;
  this->set_i2c_address(0x29);

  reg(0x89) |= 0x01;
  reg(0x88) = 0x00;

  reg(0x80) = 0x01;
  reg(0xFF) = 0x01;
  reg(0x00) = 0x00;
  this->stop_variable_ = reg(0x91).get();

  reg(0x00) = 0x01;
  reg(0xFF) = 0x00;
  reg(0x80) = 0x00;
  reg(0x60) |= 0x12;
  if (this->long_range_)
    this->signal_rate_limit_ = 0.1;
  auto rate_value = static_cast<uint16_t>(signal_rate_limit_ * 128);
  write_byte_16(0x44, rate_value);

  reg(0x01) = 0xFF;

  // getSpadInfo()
  reg(0x80) = 0x01;
  reg(0xFF) = 0x01;
  reg(0x00) = 0x00;
  reg(0xFF) = 0x06;
  reg(0x83) |= 0x04;
  reg(0xFF) = 0x07;
  reg(0x81) = 0x01;
  reg(0x80) = 0x01;
  reg(0x94) = 0x6B;
  reg(0x83) = 0x00;

  this->timeout_start_us_ = micros();
  while (reg(0x83).get() == 0x00) {
    if (this->timeout_us_ > 0 && ((uint16_t) (micros() - this->timeout_start_us_) > this->timeout_us_)) {
      ESP_LOGE(TAG, "'%s' - setup timeout", this->name_.c_str());
      this->mark_failed();
      return;
    }
    yield();
  }

  reg(0x83) = 0x01;
  uint8_t tmp = reg(0x92).get();
  uint8_t spad_count = tmp & 0x7F;
  bool spad_type_is_aperture = tmp & 0x80;

  reg(0x81) = 0x00;
  reg(0xFF) = 0x06;
  reg(0x83) &= ~0x04;
  reg(0xFF) = 0x01;
  reg(0x00) = 0x01;
  reg(0xFF) = 0x00;
  reg(0x80) = 0x00;

  uint8_t ref_spad_map[6] = {};
  this->read_bytes(0xB0, ref_spad_map, 6);

  reg(0xFF) = 0x01;
  reg(0x4F) = 0x00;
  reg(0x4E) = 0x2C;
  reg(0xFF) = 0x00;
  reg(0xB6) = 0xB4;

  uint8_t first_spad_to_enable = spad_type_is_aperture ? 12 : 0;
  uint8_t spads_enabled = 0;
  for (int i = 0; i < 48; i++) {
    uint8_t &val = ref_spad_map[i / 8];
    uint8_t mask = 1 << (i % 8);

    if (i < first_spad_to_enable || spads_enabled == spad_count) {
      val &= ~mask;
    } else if (val & mask) {
      spads_enabled += 1;
    }
  }

  this->write_bytes(0xB0, ref_spad_map, 6);

  reg(0xFF) = 0x01;
  reg(0x00) = 0x00;
  reg(0xFF) = 0x00;
  reg(0x09) = 0x00;
  reg(0x10) = 0x00;
  reg(0x11) = 0x00;
  reg(0x24) = 0x01;
  reg(0x25) = 0xFF;
  reg(0x75) = 0x00;
  reg(0xFF) = 0x01;
  reg(0x4E) = 0x2C;
  reg(0x48) = 0x00;
  reg(0x30) = 0x20;
  reg(0xFF) = 0x00;
  if (this->long_range_) {
    reg(0x30) = 0x07;  // WAS 0x09
  } else {
    reg(0x30) = 0x09;
  }
  reg(0x54) = 0x00;
  reg(0x31) = 0x04;
  reg(0x32) = 0x03;
  reg(0x40) = 0x83;
  reg(0x46) = 0x25;
  reg(0x60) = 0x00;
  reg(0x27) = 0x00;
  reg(0x50) = 0x06;
  reg(0x51) = 0x00;
  reg(0x52) = 0x96;
  reg(0x56) = 0x08;
  if (this->long_range_) {
    reg(0x57) = 0x50;  // was 0x30
  } else {
    reg(0x57) = 0x30;
  }
  reg(0x61) = 0x00;
  reg(0x62) = 0x00;
  reg(0x64) = 0x00;
  reg(0x65) = 0x00;
  reg(0x66) = 0xA0;
  reg(0xFF) = 0x01;
  reg(0x22) = 0x32;
  reg(0x47) = 0x14;
  reg(0x49) = 0xFF;
  reg(0x4A) = 0x00;
  reg(0xFF) = 0x00;
  reg(0x7A) = 0x0A;
  reg(0x7B) = 0x00;
  reg(0x78) = 0x21;
  reg(0xFF) = 0x01;
  reg(0x23) = 0x34;
  reg(0x42) = 0x00;
  reg(0x44) = 0xFF;
  reg(0x45) = 0x26;
  reg(0x46) = 0x05;
  reg(0x40) = 0x40;
  reg(0x0E) = 0x06;
  reg(0x20) = 0x1A;
  reg(0x43) = 0x40;
  reg(0xFF) = 0x00;
  reg(0x34) = 0x03;
  reg(0x35) = 0x44;
  reg(0xFF) = 0x01;
  reg(0x31) = 0x04;
  reg(0x4B) = 0x09;
  reg(0x4C) = 0x05;
  reg(0x4D) = 0x04;
  reg(0xFF) = 0x00;
  reg(0x44) = 0x00;
  reg(0x45) = 0x20;
  reg(0x47) = 0x08;
  if (this->long_range_) {
    reg(0x48) = 0x48;  // was 0x28
  } else {
    reg(0x48) = 0x28;
  }
  reg(0x67) = 0x00;
  reg(0x70) = 0x04;
  reg(0x71) = 0x01;
  reg(0x72) = 0xFE;
  reg(0x76) = 0x00;
  reg(0x77) = 0x00;
  reg(0xFF) = 0x01;
  reg(0x0D) = 0x01;
  reg(0xFF) = 0x00;
  reg(0x80) = 0x01;
  reg(0x01) = 0xF8;
  reg(0xFF) = 0x01;
  reg(0x8E) = 0x01;
  reg(0x00) = 0x01;
  reg(0xFF) = 0x00;
  reg(0x80) = 0x00;

  reg(0x0A) = 0x04;
  reg(0x84) &= ~0x10;
  reg(0x0B) = 0x01;

  measurement_timing_budget_us_ = get_measurement_timing_budget_();
  reg(0x01) = 0xE8;
  set_measurement_timing_budget_(measurement_timing_budget_us_);
  reg(0x01) = 0x01;

  if (!perform_single_ref_calibration_(0x40)) {
    ESP_LOGW(TAG, "1st reference calibration failed!");
    this->mark_failed();
    return;
  }
  reg(0x01) = 0x02;
  if (!perform_single_ref_calibration_(0x00)) {
    ESP_LOGW(TAG, "2nd reference calibration failed!");
    this->mark_failed();
    return;
  }
  reg(0x01) = 0xE8;

  // Set the sensor to the desired final address
  // The following is different for VL53L0X vs VL53L1X
  // I2C_SXXXX_DEVICE_ADDRESS = 0x8A for VL53L0X
  // I2C_SXXXX__DEVICE_ADDRESS = 0x0001 for VL53L1X
  reg(0x8A) = final_address & 0x7F;
  this->set_i2c_address(final_address);

  ESP_LOGD(TAG, "'%s' - setup END", this->name_.c_str());
}

void VL53L0XSensor::update() {
  if (this->initiated_read_ || this->waiting_for_interrupt_) {
    this->publish_state(NAN);
    this->status_momentary_warning("update", 5000);
    ESP_LOGW(TAG, "%s - update called before prior reading complete - initiated:%d waiting_for_interrupt:%d",
             this->name_.c_str(), this->initiated_read_, this->waiting_for_interrupt_);
  }

  // initiate single shot measurement
  reg(0x80) = 0x01;
  reg(0xFF) = 0x01;

  reg(0x00) = 0x00;
  reg(0x91) = this->stop_variable_;
  reg(0x00) = 0x01;
  reg(0xFF) = 0x00;
  reg(0x80) = 0x00;

  reg(0x00) = 0x01;
  this->waiting_for_interrupt_ = false;
  this->initiated_read_ = true;
  // wait for timeout
}

void VL53L0XSensor::loop() {
  if (this->initiated_read_) {
    if (reg(0x00).get() & 0x01) {
      // waiting
    } else {
      // done
      // wait until reg(0x13) & 0x07 is set
      this->initiated_read_ = false;
      this->waiting_for_interrupt_ = true;
    }
  }
  if (this->waiting_for_interrupt_) {
    if (reg(0x13).get() & 0x07) {
      uint16_t range_mm = 0;
      this->read_byte_16(0x14 + 10, &range_mm);
      reg(0x0B) = 0x01;
      this->waiting_for_interrupt_ = false;

      if (range_mm >= 8190) {
        ESP_LOGD(TAG, "'%s' - Distance is out of range, please move the target closer", this->name_.c_str());
        this->publish_state(NAN);
        return;
      }

      float range_m = range_mm / 1e3f;
      ESP_LOGD(TAG, "'%s' - Got distance %.3f m", this->name_.c_str(), range_m);
      this->publish_state(range_m);
    }
  }
}

uint32_t VL53L0XSensor::get_measurement_timing_budget_() {
  SequenceStepEnables enables{};
  SequenceStepTimeouts timeouts{};

  uint16_t start_overhead = 1910;
  uint16_t end_overhead = 960;
  uint16_t msrc_overhead = 660;
  uint16_t tcc_overhead = 590;
  uint16_t dss_overhead = 690;
  uint16_t pre_range_overhead = 660;
  uint16_t final_range_overhead = 550;

  // "Start and end overhead times always present"
  uint32_t budget_us = start_overhead + end_overhead;

  get_sequence_step_enables_(&enables);
  get_sequence_step_timeouts_(&enables, &timeouts);

  if (enables.tcc)
    budget_us += (timeouts.msrc_dss_tcc_us + tcc_overhead);

  if (enables.dss) {
    budget_us += 2 * (timeouts.msrc_dss_tcc_us + dss_overhead);
  } else if (enables.msrc) {
    budget_us += (timeouts.msrc_dss_tcc_us + msrc_overhead);
  }

  if (enables.pre_range)
    budget_us += (timeouts.pre_range_us + pre_range_overhead);

  if (enables.final_range)
    budget_us += (timeouts.final_range_us + final_range_overhead);

  measurement_timing_budget_us_ = budget_us;  // store for internal reuse
  return budget_us;
}

bool VL53L0XSensor::set_measurement_timing_budget_(uint32_t budget_us) {
  SequenceStepEnables enables{};
  SequenceStepTimeouts timeouts{};

  uint16_t start_overhead = 1320;  // note that this is different than the value in get_
  uint16_t end_overhead = 960;
  uint16_t msrc_overhead = 660;
  uint16_t tcc_overhead = 590;
  uint16_t dss_overhead = 690;
  uint16_t pre_range_overhead = 660;
  uint16_t final_range_overhead = 550;

  uint32_t min_timing_budget = 20000;

  if (budget_us < min_timing_budget) {
    return false;
  }

  uint32_t used_budget_us = start_overhead + end_overhead;

  get_sequence_step_enables_(&enables);
  get_sequence_step_timeouts_(&enables, &timeouts);

  if (enables.tcc) {
    used_budget_us += (timeouts.msrc_dss_tcc_us + tcc_overhead);
  }

  if (enables.dss) {
    used_budget_us += 2 * (timeouts.msrc_dss_tcc_us + dss_overhead);
  } else if (enables.msrc) {
    used_budget_us += (timeouts.msrc_dss_tcc_us + msrc_overhead);
  }

  if (enables.pre_range) {
    used_budget_us += (timeouts.pre_range_us + pre_range_overhead);
  }

  if (enables.final_range) {
    used_budget_us += final_range_overhead;

    // "Note that the final range timeout is determined by the timing
    // budget and the sum of all other timeouts within the sequence.
    // If there is no room for the final range timeout, then an error
    // will be set. Otherwise the remaining time will be applied to
    // the final range."

    if (used_budget_us > budget_us) {
      // "Requested timeout too big."
      return false;
    }

    uint32_t final_range_timeout_us = budget_us - used_budget_us;

    // set_sequence_step_timeout() begin
    // (SequenceStepId == VL53L0X_SEQUENCESTEP_FINAL_RANGE)

    // "For the final range timeout, the pre-range timeout
    //  must be added. To do this both final and pre-range
    //  timeouts must be expressed in macro periods MClks
    //  because they have different vcsel periods."

    uint16_t final_range_timeout_mclks =
        timeout_microseconds_to_mclks_(final_range_timeout_us, timeouts.final_range_vcsel_period_pclks);

    if (enables.pre_range) {
      final_range_timeout_mclks += timeouts.pre_range_mclks;
    }

    write_byte_16(0x71, encode_timeout_(final_range_timeout_mclks));

    // set_sequence_step_timeout() end

    measurement_timing_budget_us_ = budget_us;  // store for internal reuse
  }
  return true;
}

void VL53L0XSensor::get_sequence_step_enables_(SequenceStepEnables *enables) {
  uint8_t sequence_config = reg(0x01).get();
  enables->tcc = (sequence_config >> 4) & 0x1;
  enables->dss = (sequence_config >> 3) & 0x1;
  enables->msrc = (sequence_config >> 2) & 0x1;
  enables->pre_range = (sequence_config >> 6) & 0x1;
  enables->final_range = (sequence_config >> 7) & 0x1;
}

void VL53L0XSensor::get_sequence_step_timeouts_(SequenceStepEnables const *enables, SequenceStepTimeouts *timeouts) {
  timeouts->pre_range_vcsel_period_pclks = get_vcsel_pulse_period_(VCSEL_PERIOD_PRE_RANGE);

  timeouts->msrc_dss_tcc_mclks = reg(0x46).get() + 1;
  timeouts->msrc_dss_tcc_us =
      timeout_mclks_to_microseconds_(timeouts->msrc_dss_tcc_mclks, timeouts->pre_range_vcsel_period_pclks);

  uint16_t value;
  read_byte_16(0x51, &value);
  timeouts->pre_range_mclks = decode_timeout_(value);
  timeouts->pre_range_us =
      timeout_mclks_to_microseconds_(timeouts->pre_range_mclks, timeouts->pre_range_vcsel_period_pclks);

  timeouts->final_range_vcsel_period_pclks = get_vcsel_pulse_period_(VCSEL_PERIOD_FINAL_RANGE);

  read_byte_16(0x71, &value);
  timeouts->final_range_mclks = decode_timeout_(value);

  if (enables->pre_range) {
    timeouts->final_range_mclks -= timeouts->pre_range_mclks;
  }

  timeouts->final_range_us =
      timeout_mclks_to_microseconds_(timeouts->final_range_mclks, timeouts->final_range_vcsel_period_pclks);
}

uint8_t VL53L0XSensor::get_vcsel_pulse_period_(VcselPeriodType type) {
  uint8_t vcsel;
  if (type == VCSEL_PERIOD_PRE_RANGE) {
    vcsel = reg(0x50).get();
  } else if (type == VCSEL_PERIOD_FINAL_RANGE) {
    vcsel = reg(0x70).get();
  } else {
    return 255;
  }

  return (vcsel + 1) << 1;
}

uint32_t VL53L0XSensor::get_macro_period_(uint8_t vcsel_period_pclks) {
  return ((2304UL * vcsel_period_pclks * 1655UL) + 500UL) / 1000UL;
}

uint32_t VL53L0XSensor::timeout_mclks_to_microseconds_(uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks) {
  uint32_t macro_period_ns = get_macro_period_(vcsel_period_pclks);
  return ((timeout_period_mclks * macro_period_ns) + (macro_period_ns / 2)) / 1000;
}

uint32_t VL53L0XSensor::timeout_microseconds_to_mclks_(uint32_t timeout_period_us, uint8_t vcsel_period_pclks) {
  uint32_t macro_period_ns = get_macro_period_(vcsel_period_pclks);
  return (((timeout_period_us * 1000) + (macro_period_ns / 2)) / macro_period_ns);
}

uint16_t VL53L0XSensor::decode_timeout_(uint16_t reg_val) {
  // format: "(LSByte * 2^MSByte) + 1"
  uint8_t msb = (reg_val >> 8) & 0xFF;
  uint8_t lsb = (reg_val >> 0) & 0xFF;
  return (uint16_t(lsb) << msb) + 1;
}

uint16_t VL53L0XSensor::encode_timeout_(uint16_t timeout_mclks) {
  // format: "(LSByte * 2^MSByte) + 1"
  uint32_t ls_byte = 0;
  uint16_t ms_byte = 0;

  if (timeout_mclks <= 0)
    return 0;

  ls_byte = timeout_mclks - 1;

  while ((ls_byte & 0xFFFFFF00) > 0) {
    ls_byte >>= 1;
    ms_byte++;
  }

  return (ms_byte << 8) | (ls_byte & 0xFF);
}

bool VL53L0XSensor::perform_single_ref_calibration_(uint8_t vhv_init_byte) {
  reg(0x00) = 0x01 | vhv_init_byte;  // VL53L0X_REG_SYSRANGE_MODE_START_STOP

  uint32_t start = millis();
  while ((reg(0x13).get() & 0x07) == 0) {
    if (millis() - start > 1000)
      return false;
    yield();
  }

  reg(0x0B) = 0x01;
  reg(0x00) = 0x00;

  return true;
}

}  // namespace vl53l0x
}  // namespace esphome
