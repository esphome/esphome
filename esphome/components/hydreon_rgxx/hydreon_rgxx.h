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

template<size_t num_sensors_> class HydreonRGxxComponent: public PollingComponent, public uart::UARTDevice {
 public:
  void set_sensor(sensor::Sensor *sensor, const std::string &protocolname, int index) {this->sensors_[index] = sensor; this->sensors_names_[index] = protocolname;}
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
  void process_line();
  void schedule_fail();
  void schedule_reboot();
  bool buffer_starts_with(const std::string &prefix);
  bool buffer_starts_with(const char *prefix);
  bool sensor_missing();

  sensor::Sensor *sensors_[num_sensors_] = {nullptr};
  std::string sensors_names_[num_sensors_];

  int16_t boot_count_=0;
  int16_t no_response_count_ =0;
  std::string buffer_;
  RGModel model_ = RG9;
  int sw_version_ = 0;

  //bit field showing which sensors we have received data for
  int sensors_received_ = -1;

};

}  // namespace hydreon_rgxx
}  // namespace esphome
#include "hydreon_rgxx.tcc"
