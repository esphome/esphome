#pragma once

// Host double for the BSEC2 and BME68x libraries: just the types, constants and calls the component uses.
// Every call succeeds; the tests drive the publish path directly.
#include <cstdint>

// Shared with the BSEC 1.x double in bme680_bsec/__sdk; see that file.
#include <bsec_types.h>

static constexpr int8_t BME68X_OK = 0;
static constexpr int8_t BME68X_W_NO_NEW_DATA = 2;
static constexpr uint8_t BME68X_ENABLE = 1;
static constexpr uint8_t BME68X_SLEEP_MODE = 0;
static constexpr uint8_t BME68X_FORCED_MODE = 1;
static constexpr uint8_t BME68X_PARALLEL_MODE = 2;
static constexpr uint8_t BME68X_GASM_VALID_MSK = 0x20;

static constexpr uint32_t BSEC_INSTANCE_SIZE = 8;
static constexpr uint16_t BSEC_TOTAL_HEAT_DUR = 140;
#define BSEC_CHECK_INPUT(x, shift) ((x) & (1 << ((shift) - 1)))

struct bme68x_dev {
  int dummy;
};
struct bme68x_conf {
  uint8_t os_hum, os_temp, os_pres;
};
struct bme68x_heatr_conf {
  uint8_t enable;
  uint16_t heatr_temp;
  uint16_t heatr_dur;
  uint16_t *heatr_temp_prof;
  uint16_t *heatr_dur_prof;
  uint8_t profile_len;
  uint16_t shared_heatr_dur;
};
struct bme68x_data {
  uint8_t status;
  uint8_t gas_index;
  float temperature;
  float pressure;
  float humidity;
  float gas_resistance;
};

namespace bsec2_double {
// Counts bsec_get_state_m calls, which the component makes only when it saves its state.
inline int get_state_calls = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace bsec2_double

inline bsec_library_return_t bsec_init_m(void *) { return BSEC_OK; }
inline bsec_library_return_t bsec_get_version_m(void *, bsec_version_t *version) {
  *version = {};
  return BSEC_OK;
}
inline bsec_library_return_t bsec_set_configuration_m(void *, const uint8_t *, uint32_t, uint8_t *, uint32_t) {
  return BSEC_OK;
}
inline bsec_library_return_t bsec_update_subscription_m(void *, const bsec_sensor_configuration_t *, uint8_t,
                                                        bsec_sensor_configuration_t *, uint8_t *) {
  return BSEC_OK;
}
inline bsec_library_return_t bsec_sensor_control_m(void *, int64_t, bsec_bme_settings_t *settings) {
  *settings = {};
  return BSEC_OK;
}
inline bsec_library_return_t bsec_do_steps_m(void *, const bsec_input_t *, uint8_t, bsec_output_t *,
                                             uint8_t *num_outputs) {
  *num_outputs = 0;
  return BSEC_OK;
}
inline bsec_library_return_t bsec_set_state_m(void *, const uint8_t *, uint32_t, uint8_t *, uint32_t) {
  return BSEC_OK;
}
inline bsec_library_return_t bsec_get_state_m(void *, uint8_t, uint8_t *, uint32_t, uint8_t *, uint32_t,
                                              uint32_t *num_serialized_state) {
  bsec2_double::get_state_calls++;
  *num_serialized_state = 0;
  return BSEC_OK;
}

inline int8_t bme68x_init(bme68x_dev *) { return BME68X_OK; }
inline int8_t bme68x_get_conf(bme68x_conf *conf, bme68x_dev *) {
  *conf = {};
  return BME68X_OK;
}
inline int8_t bme68x_set_conf(bme68x_conf *, bme68x_dev *) { return BME68X_OK; }
inline int8_t bme68x_set_heatr_conf(uint8_t, const bme68x_heatr_conf *, bme68x_dev *) { return BME68X_OK; }
inline int8_t bme68x_set_op_mode(uint8_t, bme68x_dev *) { return BME68X_OK; }
inline uint32_t bme68x_get_meas_dur(uint8_t, bme68x_conf *, bme68x_dev *) { return 0; }
inline int8_t bme68x_get_data(uint8_t, bme68x_data *, uint8_t *n_data, bme68x_dev *) {
  *n_data = 0;
  return BME68X_W_NO_NEW_DATA;
}
