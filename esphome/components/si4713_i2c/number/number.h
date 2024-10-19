#pragma once

#include "esphome/components/number/number.h"
#include "../si4713.h"

namespace esphome {
namespace si4713 {

class AcompGainNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_acomp_gain(value);
  }
};

class AcompThresholdNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_acomp_threshold(value);
  }
};

class AnalogLevelNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_analog_level(value);
  }
};

class AsqDurationHighNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_asq_duration_high(value);
  }
};

class AsqDurationLowNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_asq_duration_low(value);
  }
};

class AsqLevelHighNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_asq_level_high(value);
  }
};

class AsqLevelLowNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_asq_level_low(value);
  }
};

class DigitalSampleRateNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_digital_sample_rate(value);
  }
};

class LimiterReleaseTimeNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_limiter_release_time(value);
  }
};

class PilotDeviationNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_pilot_deviation(value);
  }
};

class PilotFrequencyNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_pilot_frequency(value);
  }
};

class RdsDeviationNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_rds_deviation(value);
  }
};

class RefClkFrequencyNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_refclk_frequency(value);
  }
};

class RefClkPrescalerNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_refclk_prescaler(value);
  }
};

class TunerAntcapNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_antcap(value);
  }
};

class TunerDeviationNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_deviation(value);
  }
};

class TunerFrequencyNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_frequency(value);
  }
};

class TunerPowerNumber : public number::Number, public Parented<Si4713Component> {
 protected:
  void control(float value) override {
    this->publish_state(value);
    this->parent_->set_tuner_power(value);
  }
};

}  // namespace si4713
}  // namespace esphome
