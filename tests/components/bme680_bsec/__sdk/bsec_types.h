// The BSEC library types and constants that both the BSEC 1.x double (bsec.h) and the BSEC2 double
// (bme68x_bsec2/__sdk/bsec2.h) use. The real libraries never share a build, but the all-components C++ unit
// test binary includes both doubles, so they share one definition. Sizes and fields are the union of both.
// An include guard, not #pragma once: the bme68x_bsec2 tests reach this file through another path.
#ifndef ESPHOME_TEST_BSEC_TYPES_H
#define ESPHOME_TEST_BSEC_TYPES_H

#include <cstdint>

static constexpr uint32_t BSEC_MAX_PROPERTY_BLOB_SIZE = 16;
static constexpr uint32_t BSEC_MAX_STATE_BLOB_SIZE = 16;
static constexpr uint32_t BSEC_MAX_WORKBUFFER_SIZE = 16;
static constexpr uint8_t BSEC_NUMBER_OUTPUTS = 20;
static constexpr uint8_t BSEC_MAX_PHYSICAL_SENSOR = 8;
#define BSEC_SAMPLE_RATE_LP 0.33333f
#define BSEC_SAMPLE_RATE_ULP 0.0033333f

enum {
  BSEC_INPUT_PRESSURE = 1,
  BSEC_INPUT_HUMIDITY = 2,
  BSEC_INPUT_TEMPERATURE = 3,
  BSEC_INPUT_GASRESISTOR = 4,
  BSEC_INPUT_HEATSOURCE = 14,
  BSEC_INPUT_PROFILE_PART = 24,
};

enum {
  BSEC_OUTPUT_IAQ = 1,
  BSEC_OUTPUT_STATIC_IAQ = 2,
  BSEC_OUTPUT_CO2_EQUIVALENT = 3,
  BSEC_OUTPUT_BREATH_VOC_EQUIVALENT = 4,
  BSEC_OUTPUT_RAW_PRESSURE = 7,
  BSEC_OUTPUT_RAW_GAS = 9,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE = 14,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY = 15,
};

enum bsec_library_return_t {
  BSEC_OK = 0,
  BSEC_I_SU_SUBSCRIBEDOUTPUTGATES = 12,
};

struct bsec_version_t {
  uint8_t major, minor, major_bugfix, minor_bugfix;
};

struct bsec_input_t {
  int64_t time_stamp;
  float signal;
  uint8_t signal_dimensions;
  uint8_t sensor_id;
};

struct bsec_output_t {
  int64_t time_stamp;
  float signal;
  uint8_t signal_dimensions;
  uint8_t sensor_id;
  uint8_t accuracy;
};

struct bsec_sensor_configuration_t {
  float sample_rate;
  uint8_t sensor_id;
};

// BSEC 1.x names the heater time heating_duration; BSEC2 names it heater_duration and adds the profile fields.
struct bsec_bme_settings_t {
  int64_t next_call;
  uint32_t process_data;
  uint16_t heater_temperature;
  uint16_t heating_duration;
  uint16_t heater_duration;
  uint16_t heater_temperature_profile[10];
  uint16_t heater_duration_profile[10];
  uint8_t heater_profile_len;
  uint8_t run_gas;
  uint8_t pressure_oversampling;
  uint8_t temperature_oversampling;
  uint8_t humidity_oversampling;
  uint8_t trigger_measurement;
  uint8_t op_mode;
};

#endif  // ESPHOME_TEST_BSEC_TYPES_H
