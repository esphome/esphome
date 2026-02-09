#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "sen6x.h"

namespace esphome::sen6x {

template<typename... Ts> class StartFanAction : public Action<Ts...>, public Parented<SEN6XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->start_fan_cleaning(); }
};

template<typename... Ts>
class PerformForcedCO2RecalibrationAction : public Action<Ts...>, public Parented<SEN6XComponent> {
 public:
  TEMPLATABLE_VALUE(uint16_t, reference)

  void play(const Ts &...x) override { this->parent_->perform_forced_co2_recalibration(this->reference_.value(x...)); }
};

template<typename... Ts> class CO2SensorFactoryResetAction : public Action<Ts...>, public Parented<SEN6XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->co2_sensor_factory_reset(); }
};

template<typename... Ts> class ActivateSHTHeaterAction : public Action<Ts...>, public Parented<SEN6XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->activate_sht_heater(); }
};

template<typename... Ts> class GetSHTHeaterMeasurementsAction : public Action<Ts...>, public Parented<SEN6XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->get_sht_heater_measurements(); }
};

template<typename... Ts> class StartMeasurementAction : public Action<Ts...>, public Parented<SEN6XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->start_measurement(); }
};

template<typename... Ts> class StopMeasurementAction : public Action<Ts...>, public Parented<SEN6XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->stop_measurement(); }
};

template<typename... Ts>
class SetTemperatureCompensationAction : public Action<Ts...>, public Parented<SEN6XComponent> {
 public:
  TEMPLATABLE_VALUE(float, offset)
  TEMPLATABLE_VALUE(float, normalized_offset_slope)
  TEMPLATABLE_VALUE(uint16_t, time_constant)
  TEMPLATABLE_VALUE(uint16_t, slot)

  void play(const Ts &...x) override {
    this->parent_->apply_temperature_compensation(this->offset_.value(x...), this->normalized_offset_slope_.value(x...),
                                                  this->time_constant_.value(x...), this->slot_.value(x...));
  }
};

}  // namespace esphome::sen6x
