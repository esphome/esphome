#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include "esphome/core/line_reader.h"
#include "esphome/components/uart/uart.h"

namespace esphome::hydreon_rgxx {

enum RGModel {
  RG9 = 1,
  RG15 = 2,
};

enum RG15Resolution {
  FORCE_LOW = 1,
  FORCE_HIGH = 2,
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

class HydreonRGxxComponent final : public PollingComponent, public uart::UARTDevice {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(too_cold)
  SUB_BINARY_SENSOR(lens_bad)
  SUB_BINARY_SENSOR(em_sat)
#endif

 public:
  void set_sensor(sensor::Sensor *sensor, int index) { this->sensors_[index] = sensor; }
  void set_model(RGModel model) { model_ = model; }
  void set_resolution(RG15Resolution resolution) { resolution_ = resolution; }
  void set_request_temperature(bool b) { request_temperature_ = b; }

  /// Schedule data readings.
  void update() override;
  /// Read data once available
  void loop() override;
  /// Setup the sensor and test for a connection.
  void setup() override;
  void dump_config() override;

  void set_disable_led(bool disable_led) { this->disable_led_ = disable_led; }

 protected:
  /// line is NUL terminated
  void process_line_(StringRef line);
  void schedule_reboot_();
  int num_sensors_missing_();

  sensor::Sensor *sensors_[NUM_SENSORS] = {nullptr};

  int16_t boot_count_ = 0;
  int16_t no_response_count_ = 0;

  RGModel model_ = RG9;
  RG15Resolution resolution_ = FORCE_HIGH;

  int sw_version_ = 0;
  bool too_cold_ = false;
  bool lens_bad_ = false;
  bool em_sat_ = false;
  bool request_temperature_ = false;
  bool disable_led_ = false;
  LineReader<80> line_reader_;

  // bit field showing which sensors we have received data for
  int sensors_received_ = -1;
};

class HydreonRGxxBinaryComponent final {
 public:
  HydreonRGxxBinaryComponent(HydreonRGxxComponent *parent) {}
};

}  // namespace esphome::hydreon_rgxx
