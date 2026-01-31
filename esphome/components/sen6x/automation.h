#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sen6x.h"

namespace esphome {
namespace sen6x {

template<typename... Ts> class StartFanAction : public Action<Ts...> {
 public:
  explicit StartFanAction(SEN6XComponent *sen6x) : sen6x_(sen6x) {}

  void play(Ts... x) override { this->sen6x_->start_fan_cleaning(); }

 protected:
  SEN6XComponent *sen6x_;
};

template<typename... Ts> class PerformForcedCO2RecalibrationAction : public Action<Ts...> {
 public:
  explicit PerformForcedCO2RecalibrationAction(SEN6XComponent *sen6x) : sen6x_(sen6x) {}

  void set_reference(TemplatableValue<uint16_t, Ts...> reference) { reference_ = reference; }

  void play(Ts... x) override { this->sen6x_->perform_forced_co2_recalibration(this->reference_.value(x...)); }

 protected:
  SEN6XComponent *sen6x_;
  TemplatableValue<uint16_t, Ts...> reference_;
};

template<typename... Ts> class CO2SensorFactoryResetAction : public Action<Ts...> {
 public:
  explicit CO2SensorFactoryResetAction(SEN6XComponent *sen6x) : sen6x_(sen6x) {}

  void play(Ts... x) override { this->sen6x_->co2_sensor_factory_reset(); }

 protected:
  SEN6XComponent *sen6x_;
};

template<typename... Ts> class ActivateSHTHeaterAction : public Action<Ts...> {
 public:
  explicit ActivateSHTHeaterAction(SEN6XComponent *sen6x) : sen6x_(sen6x) {}

  void play(Ts... x) override { this->sen6x_->activate_sht_heater(); }

 protected:
  SEN6XComponent *sen6x_;
};

template<typename... Ts> class GetSHTHeaterMeasurementsAction : public Action<Ts...> {
 public:
  explicit GetSHTHeaterMeasurementsAction(SEN6XComponent *sen6x) : sen6x_(sen6x) {}

  void play(Ts... x) override { this->sen6x_->get_sht_heater_measurements(); }

 protected:
  SEN6XComponent *sen6x_;
};

template<typename... Ts> class StartMeasurementAction : public Action<Ts...> {
 public:
  explicit StartMeasurementAction(SEN6XComponent *sen6x) : sen6x_(sen6x) {}

  void play(Ts... x) override { this->sen6x_->start_measurement(); }

 protected:
  SEN6XComponent *sen6x_;
};

template<typename... Ts> class StopMeasurementAction : public Action<Ts...> {
 public:
  explicit StopMeasurementAction(SEN6XComponent *sen6x) : sen6x_(sen6x) {}

  void play(Ts... x) override { this->sen6x_->stop_measurement(); }

 protected:
  SEN6XComponent *sen6x_;
};

}  // namespace sen6x
}  // namespace esphome
