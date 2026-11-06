#pragma once

// Host double for the BSEC 1.x and BME680 libraries: just the types, constants and calls the component uses.
// A test arms one reading with bsec_double::arm(); the next sensor run hands those outputs to the component.
#include <cstdint>
#include <cstring>

#include "bsec_types.h"

static constexpr int8_t BME680_OK = 0;
static constexpr uint8_t BME680_I2C_INTF = 1;
static constexpr uint8_t BME680_SLEEP_MODE = 0;
static constexpr uint8_t BME680_FORCED_MODE = 1;
static constexpr uint16_t BME680_OST_SEL = 1;
static constexpr uint16_t BME680_OSP_SEL = 2;
static constexpr uint16_t BME680_OSH_SEL = 4;
static constexpr uint16_t BME680_GAS_SENSOR_SEL = 8 | 64 | 128;
static constexpr uint8_t BME680_NEW_DATA_MSK = 0x80;
static constexpr uint8_t BME680_GASM_VALID_MSK = 0x20;

static constexpr uint32_t BSEC_PROCESS_PRESSURE = 1 << (BSEC_INPUT_PRESSURE - 1);
static constexpr uint32_t BSEC_PROCESS_TEMPERATURE = 1 << (BSEC_INPUT_TEMPERATURE - 1);
static constexpr uint32_t BSEC_PROCESS_HUMIDITY = 1 << (BSEC_INPUT_HUMIDITY - 1);
static constexpr uint32_t BSEC_PROCESS_GAS = 1 << (BSEC_INPUT_GASRESISTOR - 1);

struct bme680_tph_sett {
  uint8_t os_hum, os_temp, os_pres;
};
struct bme680_gas_sett {
  uint8_t run_gas;
  uint16_t heatr_temp;
  uint16_t heatr_dur;
};
struct bme680_dev {
  uint8_t dev_id;
  uint8_t intf;
  int8_t amb_temp;
  bme680_tph_sett tph_sett;
  bme680_gas_sett gas_sett;
  uint8_t power_mode;
  int8_t (*read)(uint8_t, uint8_t, uint8_t *, uint16_t);
  int8_t (*write)(uint8_t, uint8_t, uint8_t *, uint16_t);
  void (*delay_ms)(uint32_t);
};
struct bme680_field_data {
  uint8_t status;
  int16_t temperature;
  uint32_t pressure;
  uint32_t humidity;
  uint32_t gas_resistance;
};

namespace bsec_double {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
inline bsec_output_t outputs[BSEC_NUMBER_OUTPUTS];
inline uint8_t num_outputs = 0;
inline bool armed = false;
// Counts bsec_get_state calls, which the component makes only when it saves its state.
inline int get_state_calls = 0;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

inline void arm(const bsec_output_t *reading, uint8_t count) {
  std::memcpy(outputs, reading, count * sizeof(bsec_output_t));
  num_outputs = count;
  armed = true;
}
}  // namespace bsec_double

inline bsec_library_return_t bsec_init() { return BSEC_OK; }
inline bsec_library_return_t bsec_get_version(bsec_version_t *version) {
  *version = {};
  return BSEC_OK;
}
inline bsec_library_return_t bsec_set_configuration(const uint8_t *, uint32_t, uint8_t *, uint32_t) { return BSEC_OK; }
inline bsec_library_return_t bsec_update_subscription(const bsec_sensor_configuration_t *, uint8_t,
                                                      bsec_sensor_configuration_t *, uint8_t *) {
  return BSEC_OK;
}
// Asks for data processing, without a measurement, only while a reading is armed. trigger_measurement stays 0, so
// read_() runs in the same loop() pass; on hardware it runs later from the measurement timeout.
inline bsec_library_return_t bsec_sensor_control(int64_t, bsec_bme_settings_t *settings) {
  *settings = {};
  if (bsec_double::armed)
    settings->process_data = BSEC_PROCESS_TEMPERATURE;
  return BSEC_OK;
}
inline bsec_library_return_t bsec_do_steps(const bsec_input_t *, uint8_t, bsec_output_t *outputs,
                                           uint8_t *num_outputs) {
  std::memcpy(outputs, bsec_double::outputs, bsec_double::num_outputs * sizeof(bsec_output_t));
  *num_outputs = bsec_double::num_outputs;
  bsec_double::armed = false;
  return BSEC_OK;
}
inline bsec_library_return_t bsec_set_state(const uint8_t *, uint32_t, uint8_t *, uint32_t) { return BSEC_OK; }
inline bsec_library_return_t bsec_get_state(uint8_t, uint8_t *, uint32_t, uint8_t *, uint32_t,
                                            uint32_t *num_serialized_state) {
  bsec_double::get_state_calls++;
  *num_serialized_state = 0;
  return BSEC_OK;
}

inline int8_t bme680_init(bme680_dev *) { return BME680_OK; }
inline int8_t bme680_set_sensor_settings(uint16_t, bme680_dev *) { return BME680_OK; }
inline int8_t bme680_set_sensor_mode(bme680_dev *) { return BME680_OK; }
inline int8_t bme680_get_sensor_mode(bme680_dev *) { return BME680_OK; }
inline void bme680_get_profile_dur(uint16_t *duration, const bme680_dev *) { *duration = 0; }
inline int8_t bme680_get_sensor_data(bme680_field_data *data, bme680_dev *) {
  *data = {};
  data->status = BME680_NEW_DATA_MSK;
  return BME680_OK;
}
