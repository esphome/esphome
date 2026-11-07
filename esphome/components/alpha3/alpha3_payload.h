#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::alpha3 {

struct HydraulicTelemetry {
  float flow_m3_h;
  float head_m;
};

struct ElectricalTelemetry {
  float voltage;
  float current;
  float power;
  float speed_rpm;
};

struct OperationHistory {
  uint32_t starts;
  float operating_hours;
};

struct OperationStatus {
  uint8_t source;
  uint8_t operation;
  uint8_t control;
  float setpoint;
};

struct SetpointLimits {
  float default_value;
  float minimum;
  float maximum;
  float resulting_minimum;
};

bool decode_model_b_hydraulic(const uint8_t *payload, size_t size, HydraulicTelemetry &output);
bool decode_extended_hydraulic(const uint8_t *payload, size_t size, HydraulicTelemetry &output);
bool decode_electrical(const uint8_t *payload, size_t size, uint8_t version, ElectricalTelemetry &output);
bool decode_operation_history(const uint8_t *payload, size_t size, OperationHistory &output);
bool decode_energy_kwh(const uint8_t *payload, size_t size, double &output);
bool decode_operation_status(const uint8_t *payload, size_t size, OperationStatus &output);
bool decode_setpoint_limits(const uint8_t *payload, size_t size, SetpointLimits &output);
bool decode_setpoint(const uint8_t *payload, size_t size, float &output);
bool replace_operation_mode(const uint8_t *input, size_t size, uint8_t mode, std::array<uint8_t, 4> &output);
bool replace_control_mode(const uint8_t *input, size_t size, uint8_t mode, std::array<uint8_t, 7> &output);
bool replace_setpoint(const uint8_t *input, size_t size, float setpoint, std::array<uint8_t, 18> &output);
const char *operation_mode_to_string(uint8_t value);
const char *control_mode_to_string(uint8_t value);
const char *control_source_to_string(uint8_t value);

}  // namespace esphome::alpha3
