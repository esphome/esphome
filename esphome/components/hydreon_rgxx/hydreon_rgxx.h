#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace hydreon_rgxx {

enum RGModel {
  RG9 = 1,
  RG15 = 2,
};

#ifdef HYDREON_RGXX_NUM_SENSORS
static const uint8_t NUM_SENSORS = HYDREON_RGXX_NUM_SENSORS;
#else
static const uint8_t NUM_SENSORS = 1;
#endif

#ifndef HYDREON_RGXX_PROTOCOL_LIST
#define HYDREON_RGXX_PROTOCOL_LIST(F, SEP) F("")
#endif

#define HYDREON_RGXX_IGNORE_LIST(F, SEP) F("Emitters") SEP F("Event") SEP F("Reset")

class HydreonRGxxComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void set_sensor(sensor::Sensor *sensor, int index) { this->sensors_[index] = sensor; }
#ifdef USE_BINARY_SENSOR
  void set_too_cold_sensor(binary_sensor::BinarySensor *sensor) { this->too_cold_sensor_ = sensor; }
  void set_lens_bad_sensor(binary_sensor::BinarySensor *sensor) { this->lens_bad_sensor_ = sensor; }
  void set_em_sat_sensor(binary_sensor::BinarySensor *sensor) { this->em_sat_sensor_ = sensor; }
#endif
  void set_model(RGModel model) { model_ = model; }
  void set_request_temperature(bool b) { request_temperature_ = b; }

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
  int num_sensors_missing_();

  sensor::Sensor *sensors_[NUM_SENSORS] = {nullptr};
#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *too_cold_sensor_{nullptr};
  binary_sensor::BinarySensor *lens_bad_sensor_{nullptr};
  binary_sensor::BinarySensor *em_sat_sensor_{nullptr};
#endif

  int16_t boot_count_ = 0;
  int16_t no_response_count_ = 0;
  std::string buffer_;
  RGModel model_ = RG9;
  int sw_version_ = 0;
  bool too_cold_ = false;
  bool lens_bad_ = false;
  bool em_sat_ = false;
  bool request_temperature_ = false;

  // bit field showing which sensors we have received data for
  int sensors_received_ = -1;
};

class HydreonRGxxBinaryComponent : public Component {
 public:
  HydreonRGxxBinaryComponent(HydreonRGxxComponent *parent) {}
};

}  // namespace hydreon_rgxx
}  // namespace esphome
