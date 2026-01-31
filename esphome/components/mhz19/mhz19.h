#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace mhz19 {

enum MHZ19ABCLogic {
  MHZ19_ABC_NONE = 0,
  MHZ19_ABC_ENABLED,
  MHZ19_ABC_DISABLED,
};

enum MHZ19DetectionRange {
  MHZ19_DETECTION_RANGE_DEFAULT = 0,
  MHZ19_DETECTION_RANGE_0_2000PPM,
  MHZ19_DETECTION_RANGE_0_5000PPM,
  MHZ19_DETECTION_RANGE_0_10000PPM,
};

class MHZ19Component : public PollingComponent, public uart::UARTDevice {
 public:
  float get_setup_priority() const override;

  void setup() override;
  void update() override;
  void dump_config() override;

  void calibrate_zero();
  void abc_enable();
  void abc_disable();
  void range_set(MHZ19DetectionRange detection_ppm);

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_abc_enabled(bool abc_enabled) { abc_boot_logic_ = abc_enabled ? MHZ19_ABC_ENABLED : MHZ19_ABC_DISABLED; }
  void set_warmup_seconds(uint32_t seconds) { warmup_seconds_ = seconds; }
  void set_detection_range(MHZ19DetectionRange detection_range) { detection_range_ = detection_range; }

 protected:
  bool mhz19_write_command_(const uint8_t *command, uint8_t *response);

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};
  MHZ19ABCLogic abc_boot_logic_{MHZ19_ABC_NONE};

  uint32_t warmup_seconds_;

  MHZ19DetectionRange detection_range_{MHZ19_DETECTION_RANGE_DEFAULT};
};

template<typename... Ts> class MHZ19CalibrateZeroAction : public Action<Ts...>, public Parented<MHZ19Component> {
 public:
  void play(const Ts &...x) override { this->parent_->calibrate_zero(); }
};

template<typename... Ts> class MHZ19ABCEnableAction : public Action<Ts...>, public Parented<MHZ19Component> {
 public:
  void play(const Ts &...x) override { this->parent_->abc_enable(); }
};

template<typename... Ts> class MHZ19ABCDisableAction : public Action<Ts...>, public Parented<MHZ19Component> {
 public:
  void play(const Ts &...x) override { this->parent_->abc_disable(); }
};

template<typename... Ts> class MHZ19DetectionRangeSetAction : public Action<Ts...>, public Parented<MHZ19Component> {
 public:
  TEMPLATABLE_VALUE(MHZ19DetectionRange, detection_range)

  void play(const Ts &...x) override { this->parent_->range_set(this->detection_range_.value(x...)); }
};

}  // namespace mhz19
}  // namespace esphome
