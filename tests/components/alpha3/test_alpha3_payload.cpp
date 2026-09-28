#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "esphome/components/alpha3/alpha3_payload.h"

namespace esphome::alpha3::testing {
namespace {

constexpr std::array<uint8_t, 24> MODEL_B_HYDRAULIC_PAYLOAD{{
    0x38, 0x92, 0xC4, 0x2D, 0x47, 0x16, 0x5F, 0xF5, 0x7F, 0xFF, 0xFF, 0xFF,
    0x7F, 0xFF, 0xFF, 0xFF, 0x41, 0x77, 0xFE, 0x0A, 0x7F, 0xFF, 0xFF, 0xFF,
}};

constexpr std::array<uint8_t, 37> ELECTRICAL_V1_PAYLOAD{{
    0x43, 0x14, 0x55, 0xC1, 0x43, 0x4C, 0x66, 0x00, 0x3D, 0x79, 0x46, 0x66, 0x41, 0x84, 0x47, 0x80, 0x41, 0x80, 0x5B,
    0xF0, 0x45, 0x85, 0xE7, 0x1A, 0xFF, 0xC0, 0x00, 0x00, 0x42, 0x1C, 0x00, 0x00, 0x42, 0x0C, 0x00, 0x00, 0x01,
}};

constexpr std::array<uint8_t, 28> HISTORY_PAYLOAD{{
    0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x22, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
}};

constexpr std::array<uint8_t, 26> ENERGY_PAYLOAD{{
    0x41, 0x50, 0x7A, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x41, 0x84, 0x47, 0x80, 0x00,
    0x01, 0x41, 0x84, 0x47, 0x80, 0x41, 0x07, 0xDF, 0x9E, 0xC8, 0x70, 0x00, 0x00,
}};

constexpr std::array<uint8_t, 7> TYPE303_STATUS_PAYLOAD{{0x00, 0x00, 0x02, 0x45, 0xA4, 0x60, 0x00}};
constexpr std::array<uint8_t, 18> TYPE302_USER_CONFIG_PAYLOAD{{
    0x00,
    0xFF,
    0x45,
    0xA4,
    0x60,
    0x00,
    0x3F,
    0x80,
    0x00,
    0x00,
    0x3F,
    0x80,
    0x00,
    0x00,
    0x3F,
    0x80,
    0x00,
    0x00,
}};
constexpr std::array<uint8_t, 28> SETPOINT_LIMITS_PAYLOAD{{
    0x3F, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x3F, 0x00,
    0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF,
}};
constexpr std::array<uint8_t, 36> EXTENDED_HYDRAULIC_PAYLOAD{{
    0x38, 0xE9, 0x04, 0x44, 0x46, 0x95, 0x6A, 0x43, 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 0x42, 0x0C,
    0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xBD, 0x05, 0x32, 0xA7, 0x40, 0xB8, 0xF5, 0x46, 0xBD, 0x30, 0xF1, 0x77,
}};

struct EnumString {
  uint8_t value;
  const char *name;
};

constexpr EnumString OPERATION_MODE_STRINGS[]{
    {0, "Auto"},          {1, "Stop"},         {2, "Min"},  {3, "Max"},     {4, "UserDefined"},
    {5, "ForcedStart"},   {6, "NoCmd"},        {7, "Hand"}, {8, "Standby"}, {9, "StopWithoutDelay"},
    {10, "EmergencyRun"}, {11, "UserDefined2"}};
constexpr EnumString CONTROL_SOURCE_STRINGS[]{{0, "Undefined"},
                                              {1, "Panel"},
                                              {2, "Bus"},
                                              {3, "Link"},
                                              {4, "ExternalIo"},
                                              {5, "OffButton"},
                                              {8, "EventAction"},
                                              {100, "FirstOverruler"},
                                              {101, "ClockScheduler"},
                                              {102, "LevelControl"},
                                              {103, "DryRun"},
                                              {104, "AutoNight"},
                                              {105, "OutdoorDependentOperation"},
                                              {106, "AnalogueInfluence"},
                                              {107, "TemperatureInfluence"},
                                              {108, "FlowInfluence"},
                                              {109, "LowFlowStop"},
                                              {110, "Balancing"},
                                              {111, "AirVenting"},
                                              {112, "AutomaticTestRun"},
                                              {113, "Standby"},
                                              {114, "FlowPulseTrigger"},
                                              {115, "OnOffTemperatureOperation"},
                                              {116, "TestMode"},
                                              {117, "LimitExceeded"},
                                              {118, "HighWater"},
                                              {119, "LevelSwitchInconsistency"},
                                              {120, "MultiPump"},
                                              {121, "LINBus"},
                                              {122, "ModBus"},
                                              {123, "PWM"},
                                              {124, "SmartHydroBlockControl"},
                                              {125, "PipeFilling"},
                                              {126, "FallbackControlMode"}};
constexpr EnumString CONTROL_MODE_STRINGS[]{{0, "ConstantPressure"},     {1, "ProportionalPressure"},
                                            {2, "ConstantSpeed"},        {13, "AutoAdaptRadiator"},
                                            {14, "AutoAdaptUnderfloor"}, {15, "AutoAdaptRadiatorAndUnderfloor"}};

}  // namespace

TEST(Alpha3PayloadDecode, DecodesCanonicalHydraulicElectricalHistoryAndEnergyPayloads) {
  HydraulicTelemetry hydraulic;
  ASSERT_TRUE(decode_model_b_hydraulic(MODEL_B_HYDRAULIC_PAYLOAD.data(), MODEL_B_HYDRAULIC_PAYLOAD.size(), hydraulic));
  EXPECT_NEAR(hydraulic.flow_m3_h, 0.251941F, 0.000001F);
  EXPECT_NEAR(hydraulic.head_m, 3.926556F, 0.000001F);

  ElectricalTelemetry electrical;
  ASSERT_TRUE(decode_electrical(ELECTRICAL_V1_PAYLOAD.data(), ELECTRICAL_V1_PAYLOAD.size(), 1, electrical));
  EXPECT_NEAR(electrical.voltage, 148.33498F, 0.0001F);
  EXPECT_NEAR(electrical.current, 0.060858F, 0.000001F);
  EXPECT_NEAR(electrical.power, 16.534912F, 0.0001F);
  EXPECT_NEAR(electrical.speed_rpm, 4284.8877F, 0.001F);

  OperationHistory history;
  ASSERT_TRUE(decode_operation_history(HISTORY_PAYLOAD.data(), HISTORY_PAYLOAD.size(), history));
  EXPECT_EQ(history.starts, 18U);
  EXPECT_NEAR(history.operating_hours, 18.2138889F, 0.00001F);

  double energy_kwh;
  ASSERT_TRUE(decode_energy_kwh(ENERGY_PAYLOAD.data(), ENERGY_PAYLOAD.size(), energy_kwh));
  EXPECT_NEAR(energy_kwh, 1.2, 0.0000001);
}

TEST(Alpha3PayloadDecode, DecodesVersionTwoElectricalPrefixAndExtendedHydraulicPayload) {
  std::array<uint8_t, 41> electrical_v2{};
  for (size_t i = 0; i < ELECTRICAL_V1_PAYLOAD.size(); i++)
    electrical_v2[i] = ELECTRICAL_V1_PAYLOAD[i];
  electrical_v2[37] = 0x42;
  electrical_v2[38] = 0x1C;
  electrical_v2[39] = 0x00;
  electrical_v2[40] = 0x00;

  ElectricalTelemetry electrical;
  ASSERT_TRUE(decode_electrical(electrical_v2.data(), electrical_v2.size(), 2, electrical));
  EXPECT_NEAR(electrical.voltage, 148.33498F, 0.0001F);
  EXPECT_NEAR(electrical.current, 0.060858F, 0.000001F);
  EXPECT_NEAR(electrical.power, 16.534912F, 0.0001F);
  EXPECT_NEAR(electrical.speed_rpm, 4284.8877F, 0.001F);

  HydraulicTelemetry extended;
  ASSERT_TRUE(
      decode_extended_hydraulic(EXTENDED_HYDRAULIC_PAYLOAD.data(), EXTENDED_HYDRAULIC_PAYLOAD.size(), extended));
  EXPECT_NEAR(extended.flow_m3_h, 0.3999996F, 0.000001F);
  EXPECT_NEAR(extended.head_m, 1.9507477F, 0.000001F);
}

TEST(Alpha3PayloadDecode, DecodesStatusAndSetpointConfigurationValues) {
  OperationStatus status;
  ASSERT_TRUE(decode_operation_status(TYPE303_STATUS_PAYLOAD.data(), TYPE303_STATUS_PAYLOAD.size(), status));
  EXPECT_EQ(status.source, 0);
  EXPECT_EQ(status.operation, 0);
  EXPECT_EQ(status.control, 2);
  EXPECT_FLOAT_EQ(status.setpoint, 5260.0F);

  SetpointLimits limits;
  ASSERT_TRUE(decode_setpoint_limits(SETPOINT_LIMITS_PAYLOAD.data(), SETPOINT_LIMITS_PAYLOAD.size(), limits));
  EXPECT_FLOAT_EQ(limits.default_value, 1.0F);
  EXPECT_FLOAT_EQ(limits.minimum, 2.0F);
  EXPECT_FLOAT_EQ(limits.maximum, 3.0F);
  EXPECT_FLOAT_EQ(limits.resulting_minimum, 0.5F);

  float setpoint;
  ASSERT_TRUE(decode_setpoint(TYPE302_USER_CONFIG_PAYLOAD.data(), TYPE302_USER_CONFIG_PAYLOAD.size(), setpoint));
  EXPECT_FLOAT_EQ(setpoint, 5260.0F);
}

TEST(Alpha3PayloadDecode, LeavesSentinelOutputsUntouchedForShortPayloads) {
  HydraulicTelemetry hydraulic{11.0F, 12.0F};
  EXPECT_FALSE(
      decode_model_b_hydraulic(MODEL_B_HYDRAULIC_PAYLOAD.data(), MODEL_B_HYDRAULIC_PAYLOAD.size() - 1, hydraulic));
  EXPECT_FLOAT_EQ(hydraulic.flow_m3_h, 11.0F);
  EXPECT_FLOAT_EQ(hydraulic.head_m, 12.0F);
  EXPECT_FALSE(
      decode_extended_hydraulic(EXTENDED_HYDRAULIC_PAYLOAD.data(), EXTENDED_HYDRAULIC_PAYLOAD.size() - 1, hydraulic));
  EXPECT_FLOAT_EQ(hydraulic.flow_m3_h, 11.0F);
  EXPECT_FLOAT_EQ(hydraulic.head_m, 12.0F);

  ElectricalTelemetry electrical{1.0F, 2.0F, 3.0F, 4.0F};
  EXPECT_FALSE(decode_electrical(ELECTRICAL_V1_PAYLOAD.data(), ELECTRICAL_V1_PAYLOAD.size() - 1, 1, electrical));
  EXPECT_FLOAT_EQ(electrical.voltage, 1.0F);
  EXPECT_FLOAT_EQ(electrical.current, 2.0F);
  EXPECT_FLOAT_EQ(electrical.power, 3.0F);
  EXPECT_FLOAT_EQ(electrical.speed_rpm, 4.0F);
  std::array<uint8_t, 41> electrical_v2{};
  EXPECT_FALSE(decode_electrical(electrical_v2.data(), electrical_v2.size() - 1, 2, electrical));
  EXPECT_FLOAT_EQ(electrical.voltage, 1.0F);
  EXPECT_FLOAT_EQ(electrical.current, 2.0F);
  EXPECT_FLOAT_EQ(electrical.power, 3.0F);
  EXPECT_FLOAT_EQ(electrical.speed_rpm, 4.0F);

  OperationHistory history{5, 6.0F};
  EXPECT_FALSE(decode_operation_history(HISTORY_PAYLOAD.data(), HISTORY_PAYLOAD.size() - 1, history));
  EXPECT_EQ(history.starts, 5U);
  EXPECT_FLOAT_EQ(history.operating_hours, 6.0F);

  double energy_kwh = 7.0;
  EXPECT_FALSE(decode_energy_kwh(ENERGY_PAYLOAD.data(), 7, energy_kwh));
  EXPECT_DOUBLE_EQ(energy_kwh, 7.0);

  OperationStatus status{8, 9, 10, 11.0F};
  EXPECT_FALSE(decode_operation_status(TYPE303_STATUS_PAYLOAD.data(), TYPE303_STATUS_PAYLOAD.size() - 1, status));
  EXPECT_EQ(status.source, 8);
  EXPECT_EQ(status.operation, 9);
  EXPECT_EQ(status.control, 10);
  EXPECT_FLOAT_EQ(status.setpoint, 11.0F);

  SetpointLimits limits{12.0F, 13.0F, 14.0F, 15.0F};
  EXPECT_FALSE(decode_setpoint_limits(SETPOINT_LIMITS_PAYLOAD.data(), SETPOINT_LIMITS_PAYLOAD.size() - 1, limits));
  EXPECT_FLOAT_EQ(limits.default_value, 12.0F);
  EXPECT_FLOAT_EQ(limits.minimum, 13.0F);
  EXPECT_FLOAT_EQ(limits.maximum, 14.0F);
  EXPECT_FLOAT_EQ(limits.resulting_minimum, 15.0F);

  float setpoint = 16.0F;
  EXPECT_FALSE(decode_setpoint(TYPE302_USER_CONFIG_PAYLOAD.data(), TYPE302_USER_CONFIG_PAYLOAD.size() - 1, setpoint));
  EXPECT_FLOAT_EQ(setpoint, 16.0F);
}

TEST(Alpha3PayloadMutate, PreservesEveryNonTargetByte) {
  constexpr std::array<uint8_t, 4> operation_input{{0xA5, 0x00, 0x0D, 0x7E}};
  std::array<uint8_t, 4> operation_output{};
  ASSERT_TRUE(replace_operation_mode(operation_input.data(), operation_input.size(), 3, operation_output));
  EXPECT_EQ(operation_output, (std::array<uint8_t, 4>{{0xA5, 0x03, 0x0D, 0x7E}}));

  constexpr std::array<uint8_t, 7> control_input{{0x44, 0x06, 0x02, 0x7F, 0xFF, 0xFF, 0xFF}};
  std::array<uint8_t, 7> control_output{};
  ASSERT_TRUE(replace_control_mode(control_input.data(), control_input.size(), 0, control_output));
  for (size_t i = 0; i < control_input.size(); i++)
    EXPECT_EQ(control_output[i], i == 2 ? 0 : control_input[i]);

  std::array<uint8_t, 18> setpoint_output{};
  ASSERT_TRUE(replace_setpoint(TYPE302_USER_CONFIG_PAYLOAD.data(), TYPE302_USER_CONFIG_PAYLOAD.size(), 1650.0F,
                               setpoint_output));
  for (size_t i = 0; i < TYPE302_USER_CONFIG_PAYLOAD.size(); i++) {
    if (i < 2 || i >= 6)
      EXPECT_EQ(setpoint_output[i], TYPE302_USER_CONFIG_PAYLOAD[i]) << "byte " << i;
  }
  EXPECT_EQ(setpoint_output[2], 0x44);
  EXPECT_EQ(setpoint_output[3], 0xCE);
  EXPECT_EQ(setpoint_output[4], 0x40);
  EXPECT_EQ(setpoint_output[5], 0x00);
}

TEST(Alpha3PayloadMutate, RejectsMalformedAndNonfiniteInputsWithoutChangingOutputs) {
  std::array<uint8_t, 4> operation_output{{1, 2, 3, 4}};
  EXPECT_FALSE(replace_operation_mode(nullptr, 4, 3, operation_output));
  EXPECT_EQ(operation_output, (std::array<uint8_t, 4>{{1, 2, 3, 4}}));

  std::array<uint8_t, 7> control_output{{1, 2, 3, 4, 5, 6, 7}};
  EXPECT_FALSE(
      replace_control_mode(TYPE303_STATUS_PAYLOAD.data(), TYPE303_STATUS_PAYLOAD.size() - 1, 0, control_output));
  EXPECT_EQ(control_output, (std::array<uint8_t, 7>{{1, 2, 3, 4, 5, 6, 7}}));

  std::array<uint8_t, 18> setpoint_output{};
  setpoint_output.fill(0xA5);
  EXPECT_FALSE(replace_setpoint(TYPE302_USER_CONFIG_PAYLOAD.data(), TYPE302_USER_CONFIG_PAYLOAD.size(),
                                std::numeric_limits<float>::quiet_NaN(), setpoint_output));
  EXPECT_FALSE(replace_setpoint(TYPE302_USER_CONFIG_PAYLOAD.data(), TYPE302_USER_CONFIG_PAYLOAD.size(),
                                std::numeric_limits<float>::infinity(), setpoint_output));
  for (uint8_t byte : setpoint_output)
    EXPECT_EQ(byte, 0xA5);
}

TEST(Alpha3PayloadStrings, MapsKnownEnumsAndRejectsUnknownValues) {
  for (const auto &entry : OPERATION_MODE_STRINGS)
    EXPECT_STREQ(operation_mode_to_string(entry.value), entry.name);
  for (const auto &entry : CONTROL_SOURCE_STRINGS)
    EXPECT_STREQ(control_source_to_string(entry.value), entry.name);
  for (const auto &entry : CONTROL_MODE_STRINGS)
    EXPECT_STREQ(control_mode_to_string(entry.value), entry.name);

  EXPECT_EQ(operation_mode_to_string(0xFF), nullptr);
  EXPECT_EQ(control_mode_to_string(0xFF), nullptr);
  EXPECT_EQ(control_source_to_string(0xFF), nullptr);
}

}  // namespace esphome::alpha3::testing
