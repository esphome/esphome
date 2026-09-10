#include "alpha3_payload.h"

#include <cmath>
#include <cstring>

#include "alpha3_protocol.h"

namespace esphome::alpha3 {
namespace {

constexpr size_t MODEL_B_HYDRAULIC_SIZE = 24;
constexpr size_t EXTENDED_HYDRAULIC_SIZE = 36;
constexpr size_t ELECTRICAL_V1_SIZE = 37;
constexpr size_t ELECTRICAL_V2_SIZE = 41;
constexpr size_t OPERATION_HISTORY_SIZE = 28;
constexpr size_t OPERATION_STATUS_SIZE = 7;
constexpr size_t SETPOINT_LIMITS_SIZE = 28;
constexpr size_t SETPOINT_SIZE = 18;
constexpr double WATT_SECONDS_PER_KILOWATT_HOUR = 3600000.0;

uint32_t read_uint32_be(const uint8_t *input) {
  return (static_cast<uint32_t>(input[0]) << 24) | (static_cast<uint32_t>(input[1]) << 16) |
         (static_cast<uint32_t>(input[2]) << 8) | input[3];
}

float read_float_be(const uint8_t *input) {
  const uint32_t bits = read_uint32_be(input);
  float output;
  std::memcpy(&output, &bits, sizeof(output));
  return output;
}

double read_double_be(const uint8_t *input) {
  const uint64_t bits = (static_cast<uint64_t>(input[0]) << 56) | (static_cast<uint64_t>(input[1]) << 48) |
                        (static_cast<uint64_t>(input[2]) << 40) | (static_cast<uint64_t>(input[3]) << 32) |
                        (static_cast<uint64_t>(input[4]) << 24) | (static_cast<uint64_t>(input[5]) << 16) |
                        (static_cast<uint64_t>(input[6]) << 8) | input[7];
  double output;
  std::memcpy(&output, &bits, sizeof(output));
  return output;
}

void write_float_be(uint8_t *output, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  output[0] = static_cast<uint8_t>(bits >> 24);
  output[1] = static_cast<uint8_t>(bits >> 16);
  output[2] = static_cast<uint8_t>(bits >> 8);
  output[3] = static_cast<uint8_t>(bits);
}

}  // namespace

bool decode_model_b_hydraulic(const uint8_t *payload, size_t size, HydraulicTelemetry &output) {
  if (payload == nullptr || size < MODEL_B_HYDRAULIC_SIZE)
    return false;

  const HydraulicTelemetry decoded{read_float_be(payload) * 3600.0F, read_float_be(payload + 4) / PASCALS_PER_METER};
  output = decoded;
  return true;
}

bool decode_extended_hydraulic(const uint8_t *payload, size_t size, HydraulicTelemetry &output) {
  if (payload == nullptr || size < EXTENDED_HYDRAULIC_SIZE)
    return false;

  const HydraulicTelemetry decoded{read_float_be(payload) * 3600.0F, read_float_be(payload + 4) / PASCALS_PER_METER};
  output = decoded;
  return true;
}

bool decode_electrical(const uint8_t *payload, size_t size, uint8_t version, ElectricalTelemetry &output) {
  const size_t expected_size = version == 1 ? ELECTRICAL_V1_SIZE : version == 2 ? ELECTRICAL_V2_SIZE : 0;
  if (payload == nullptr || expected_size == 0 || size < expected_size)
    return false;

  const ElectricalTelemetry decoded{read_float_be(payload), read_float_be(payload + 8), read_float_be(payload + 12),
                                    read_float_be(payload + 20)};
  output = decoded;
  return true;
}

bool decode_operation_history(const uint8_t *payload, size_t size, OperationHistory &output) {
  if (payload == nullptr || size < OPERATION_HISTORY_SIZE)
    return false;

  const OperationHistory decoded{read_uint32_be(payload), static_cast<float>(read_uint32_be(payload + 8)) / 3600.0F};
  output = decoded;
  return true;
}

bool decode_energy_kwh(const uint8_t *payload, size_t size, double &output) {
  if (payload == nullptr || size < sizeof(double))
    return false;

  const double decoded = read_double_be(payload) / WATT_SECONDS_PER_KILOWATT_HOUR;
  output = decoded;
  return true;
}

bool decode_operation_status(const uint8_t *payload, size_t size, OperationStatus &output) {
  if (payload == nullptr || size < OPERATION_STATUS_SIZE)
    return false;

  const OperationStatus decoded{payload[0], payload[1], payload[2], read_float_be(payload + 3)};
  output = decoded;
  return true;
}

bool decode_setpoint_limits(const uint8_t *payload, size_t size, SetpointLimits &output) {
  if (payload == nullptr || size < SETPOINT_LIMITS_SIZE)
    return false;

  const SetpointLimits decoded{read_float_be(payload), read_float_be(payload + 4), read_float_be(payload + 8),
                               read_float_be(payload + 12)};
  output = decoded;
  return true;
}

bool decode_setpoint(const uint8_t *payload, size_t size, float &output) {
  if (payload == nullptr || size < SETPOINT_SIZE)
    return false;

  const float decoded = read_float_be(payload + 2);
  output = decoded;
  return true;
}

bool replace_operation_mode(const uint8_t *input, size_t size, uint8_t mode, std::array<uint8_t, 4> &output) {
  if (input == nullptr || size != output.size())
    return false;

  std::array<uint8_t, 4> replaced;
  std::memcpy(replaced.data(), input, replaced.size());
  replaced[1] = mode;
  output = replaced;
  return true;
}

bool replace_control_mode(const uint8_t *input, size_t size, uint8_t mode, std::array<uint8_t, 7> &output) {
  if (input == nullptr || size != output.size())
    return false;

  std::array<uint8_t, 7> replaced;
  std::memcpy(replaced.data(), input, replaced.size());
  replaced[2] = mode;
  output = replaced;
  return true;
}

bool replace_setpoint(const uint8_t *input, size_t size, float setpoint, std::array<uint8_t, 18> &output) {
  if (input == nullptr || size != output.size() || !std::isfinite(setpoint))
    return false;

  std::array<uint8_t, 18> replaced;
  std::memcpy(replaced.data(), input, replaced.size());
  write_float_be(replaced.data() + 2, setpoint);
  output = replaced;
  return true;
}

const char *operation_mode_to_string(uint8_t value) {
  switch (value) {
    case 0:
      return "Auto";
    case 1:
      return "Stop";
    case 2:
      return "Min";
    case 3:
      return "Max";
    case 4:
      return "UserDefined";
    case 5:
      return "ForcedStart";
    case 6:
      return "NoCmd";
    case 7:
      return "Hand";
    case 8:
      return "Standby";
    case 9:
      return "StopWithoutDelay";
    case 10:
      return "EmergencyRun";
    case 11:
      return "UserDefined2";
    default:
      return nullptr;
  }
}

const char *control_mode_to_string(uint8_t value) {
  switch (value) {
    case 0:
      return "ConstantPressure";
    case 1:
      return "ProportionalPressure";
    case 2:
      return "ConstantSpeed";
    case 13:
      return "AutoAdaptRadiator";
    case 14:
      return "AutoAdaptUnderfloor";
    case 15:
      return "AutoAdaptRadiatorAndUnderfloor";
    default:
      return nullptr;
  }
}

const char *control_source_to_string(uint8_t value) {
  switch (value) {
    case 0:
      return "Undefined";
    case 1:
      return "Panel";
    case 2:
      return "Bus";
    case 3:
      return "Link";
    case 4:
      return "ExternalIo";
    case 5:
      return "OffButton";
    case 8:
      return "EventAction";
    case 100:
      return "FirstOverruler";
    case 101:
      return "ClockScheduler";
    case 102:
      return "LevelControl";
    case 103:
      return "DryRun";
    case 104:
      return "AutoNight";
    case 105:
      return "OutdoorDependentOperation";
    case 106:
      return "AnalogueInfluence";
    case 107:
      return "TemperatureInfluence";
    case 108:
      return "FlowInfluence";
    case 109:
      return "LowFlowStop";
    case 110:
      return "Balancing";
    case 111:
      return "AirVenting";
    case 112:
      return "AutomaticTestRun";
    case 113:
      return "Standby";
    case 114:
      return "FlowPulseTrigger";
    case 115:
      return "OnOffTemperatureOperation";
    case 116:
      return "TestMode";
    case 117:
      return "LimitExceeded";
    case 118:
      return "HighWater";
    case 119:
      return "LevelSwitchInconsistency";
    case 120:
      return "MultiPump";
    case 121:
      return "LINBus";
    case 122:
      return "ModBus";
    case 123:
      return "PWM";
    case 124:
      return "SmartHydroBlockControl";
    case 125:
      return "PipeFilling";
    case 126:
      return "FallbackControlMode";
    default:
      return nullptr;
  }
}

}  // namespace esphome::alpha3
