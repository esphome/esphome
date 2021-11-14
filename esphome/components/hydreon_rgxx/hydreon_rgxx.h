#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace hydreon_rgxx {

enum RGModel {
  RG9 = 1,
  RG15 = 2,
};

class HydreonRGxxComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void set_sensor(sensor::Sensor *sensor, int index) { this->sensors_[index] = sensor; }
  void set_model(RGModel model) { model_ = model; }

  /// Schedule data readings.
  void update() override;
  /// Read data once available
  void loop() override;
  /// Setup the sensor and test for a connection.
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  void process_line_();
  void schedule_reboot_();
  bool buffer_starts_with_(const std::string &prefix);
  bool buffer_starts_with_(const char *prefix);
  bool sensor_missing_();

  sensor::Sensor *sensors_[HYDREON_RGXX_NUM_SENSORS] = {nullptr};

  int16_t boot_count_ = 0;
  int16_t no_response_count_ = 0;
  std::string buffer_;
  RGModel model_ = RG9;
  int sw_version_ = 0;

  // bit field showing which sensors we have received data for
  int sensors_received_ = -1;
};

}  // namespace hydreon_rgxx
}  // namespace esphome
