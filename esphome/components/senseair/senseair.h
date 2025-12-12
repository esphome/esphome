#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace senseair {

enum SenseAirStatus : uint8_t {
  FATAL_ERROR = 1 << 0,
  OFFSET_ERROR = 1 << 1,
  ALGORITHM_ERROR = 1 << 2,
  OUTPUT_ERROR = 1 << 3,
  SELF_DIAGNOSTIC_ERROR = 1 << 4,
  OUT_OF_RANGE_ERROR = 1 << 5,
  MEMORY_ERROR = 1 << 6,
  RESERVED = 1 << 7
};

class SenseAirComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }

  void update() override;
  void dump_config() override;

  void background_calibration();
  void background_calibration_result();
  void abc_get_period();
  void abc_enable();
  void abc_disable();

 protected:
  bool senseair_write_command_(const uint8_t *command, uint8_t *response, uint8_t response_length);

  sensor::Sensor *co2_sensor_{nullptr};
};

template<typename... Ts> class SenseAirBackgroundCalibrationAction : public Action<Ts...> {
 public:
  SenseAirBackgroundCalibrationAction(SenseAirComponent *senseair) : senseair_(senseair) {}

  void play(const Ts &...x) override { this->senseair_->background_calibration(); }

 protected:
  SenseAirComponent *senseair_;
};

template<typename... Ts> class SenseAirBackgroundCalibrationResultAction : public Action<Ts...> {
 public:
  SenseAirBackgroundCalibrationResultAction(SenseAirComponent *senseair) : senseair_(senseair) {}

  void play(const Ts &...x) override { this->senseair_->background_calibration_result(); }

 protected:
  SenseAirComponent *senseair_;
};

template<typename... Ts> class SenseAirABCEnableAction : public Action<Ts...> {
 public:
  SenseAirABCEnableAction(SenseAirComponent *senseair) : senseair_(senseair) {}

  void play(const Ts &...x) override { this->senseair_->abc_enable(); }

 protected:
  SenseAirComponent *senseair_;
};

template<typename... Ts> class SenseAirABCDisableAction : public Action<Ts...> {
 public:
  SenseAirABCDisableAction(SenseAirComponent *senseair) : senseair_(senseair) {}

  void play(const Ts &...x) override { this->senseair_->abc_disable(); }

 protected:
  SenseAirComponent *senseair_;
};

template<typename... Ts> class SenseAirABCGetPeriodAction : public Action<Ts...> {
 public:
  SenseAirABCGetPeriodAction(SenseAirComponent *senseair) : senseair_(senseair) {}

  void play(const Ts &...x) override { this->senseair_->abc_get_period(); }

 protected:
  SenseAirComponent *senseair_;
};

}  // namespace senseair
}  // namespace esphome
