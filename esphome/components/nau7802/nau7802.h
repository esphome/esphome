#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include <cinttypes>

namespace esphome {
namespace nau7802 {

enum NAU7802Gain {
  NAU7802_GAIN_128 = 0b111,
  NAU7802_GAIN_64 = 0b110,
  NAU7802_GAIN_32 = 0b101,
  NAU7802_GAIN_16 = 0b100,
  NAU7802_GAIN_8 = 0b011,
  NAU7802_GAIN_4 = 0b010,
  NAU7802_GAIN_2 = 0b001,
  NAU7802_GAIN_1 = 0b000,
};

enum NAU7802SPS {
  NAU7802_SPS_320 = 0b111,
  NAU7802_SPS_80 = 0b011,
  NAU7802_SPS_40 = 0b010,
  NAU7802_SPS_20 = 0b001,
  NAU7802_SPS_10 = 0b000,
};

enum NAU7802LDO {
  NAU7802_LDO_2V4 = 0b111,
  NAU7802_LDO_2V7 = 0b110,
  NAU7802_LDO_3V0 = 0b101,
  NAU7802_LDO_3V3 = 0b100,
  NAU7802_LDO_3V6 = 0b011,
  NAU7802_LDO_3V9 = 0b010,
  NAU7802_LDO_4V2 = 0b001,
  NAU7802_LDO_4V5 = 0b000,
  // Never write this to a register
  NAU7802_LDO_EXTERNAL = 0b1000,
};

enum NAU7802CalibrationModes {
  NAU7802_CALIBRATE_EXTERNAL_OFFSET = 0b10,
  NAU7802_CALIBRATE_INTERNAL_OFFSET = 0b00,
  NAU7802_CALIBRATE_GAIN = 0b11,
};

class NAU7802Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void set_samples_per_second(NAU7802SPS sps) { this->sps_ = sps; }
  void set_ldo_voltage(NAU7802LDO ldo) { this->ldo_ = ldo; }
  void set_gain(NAU7802Gain gain) { this->gain_ = gain; }
  void set_gain_calibration(float gain_calibration) { this->gain_calibration_ = gain_calibration; }
  void set_offset_calibration(int32_t offset_calibration) { this->offset_calibration_ = offset_calibration; }
  bool calibrate_external_offset() { return this->calibrate_(NAU7802_CALIBRATE_EXTERNAL_OFFSET); }
  bool calibrate_internal_offset() { return this->calibrate_(NAU7802_CALIBRATE_INTERNAL_OFFSET); }
  bool calibrate_gain() { return this->calibrate_(NAU7802_CALIBRATE_GAIN); }

  void setup() override;
  void loop() override;
  bool can_proceed() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  //
  // Internal state
  //
  enum class CalibrationState : uint8_t {
    INACTIVE,
    OFFSET,
    GAIN,
  } state_{CalibrationState::INACTIVE};

  float gain_calibration_;
  int32_t offset_calibration_;
  bool offset_calibration_failed_ = false;
  bool gain_calibration_failed_ = false;
  bool setup_complete_ = false;

  //
  // Config values
  //
  NAU7802LDO ldo_;
  NAU7802SPS sps_;
  NAU7802Gain gain_;

  //
  // Internal Methods
  //
  bool calibrate_(enum NAU7802CalibrationModes mode);
  void complete_setup_();
  void write_value_(uint8_t start_reg, size_t size, int32_t value);
  int32_t read_value_(uint8_t start_reg, size_t size);
  bool is_data_ready_();
  void set_calibration_failure_(bool failed);
};

template<typename... Ts>
class NAU7802CalbrateExternalOffsetAction : public Action<Ts...>, public Parented<NAU7802Sensor> {
 public:
  void play(Ts... x) override { this->parent_->calibrate_external_offset(); }
};

template<typename... Ts>
class NAU7802CalbrateInternalOffsetAction : public Action<Ts...>, public Parented<NAU7802Sensor> {
 public:
  void play(Ts... x) override { this->parent_->calibrate_internal_offset(); }
};

template<typename... Ts> class NAU7802CalbrateGainAction : public Action<Ts...>, public Parented<NAU7802Sensor> {
 public:
  void play(Ts... x) override { this->parent_->calibrate_gain(); }
};

}  // namespace nau7802
}  // namespace esphome
