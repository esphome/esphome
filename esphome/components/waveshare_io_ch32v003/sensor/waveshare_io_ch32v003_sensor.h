#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

#include "../waveshare_io_ch32v003.h"

namespace esphome::waveshare_io_ch32v003 {

class WaveshareIOCH32V003Sensor : public sensor::Sensor,
                                  public PollingComponent,
                                  public voltage_sampler::VoltageSampler,
                                  public Parented<WaveshareIOCH32V003Component> {
 public:
  void set_reference_voltage(float reference_voltage) { this->reference_voltage_ = reference_voltage; }

  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  float sample() override;

 protected:
  float reference_voltage_{9.9f};  // Default reference voltage for ADC calculations, can be overridden by user config
};

}  // namespace esphome::waveshare_io_ch32v003
