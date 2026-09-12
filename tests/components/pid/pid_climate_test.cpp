#include <gtest/gtest.h>
#include <cmath>

#ifdef USE_HOST
#include "esphome/components/host/preferences.h"
#endif
#include "esphome/components/pid/pid_climate.h"

namespace esphome::pid::testing {

class TestFloatOutput : public output::FloatOutput {
 public:
  float last_state{NAN};

 protected:
  void write_state(float state) override { this->last_state = state; }
};

struct PIDClimateTestState {
  sensor::Sensor temperature_sensor;
  PIDClimate climate;
  TestFloatOutput heat_output;
  TestFloatOutput cool_output;

  PIDClimateTestState() {
#ifdef USE_HOST
    esphome::host::setup_preferences();
    esphome::host::host_preferences->reset();
#endif

    this->climate.set_sensor(&this->temperature_sensor);
    this->climate.set_heat_output(&this->heat_output);
    this->climate.set_cool_output(&this->cool_output);
    this->climate.set_default_target_temperature(20.0f);
    this->climate.set_kp(1.0f);
    this->climate.setup();
  }

  void update(climate::ClimateMode mode, float target_temperature, float current_temperature) {
    this->climate.mode = mode;
    this->climate.target_temperature = target_temperature;
    this->temperature_sensor.publish_state(current_temperature);
  }

  void set_mode(climate::ClimateMode mode) {
    auto call = this->climate.make_call();
    call.set_mode(mode);
    call.perform();
  }
};

TEST(PIDClimateTest, HeatModeBlocksCoolingDemand) {
  PIDClimateTestState state;

  state.update(climate::CLIMATE_MODE_HEAT, 20.0f, 20.5f);

  EXPECT_FLOAT_EQ(state.climate.get_output_value(), -0.5f);
  EXPECT_FLOAT_EQ(state.climate.get_active_output_value(), 0.0f);
  EXPECT_FLOAT_EQ(state.heat_output.last_state, 0.0f);
  EXPECT_FLOAT_EQ(state.cool_output.last_state, 0.0f);
  EXPECT_EQ(state.climate.action, climate::CLIMATE_ACTION_IDLE);
}

TEST(PIDClimateTest, CoolModeBlocksHeatingDemand) {
  PIDClimateTestState state;

  state.update(climate::CLIMATE_MODE_COOL, 20.5f, 20.0f);

  EXPECT_FLOAT_EQ(state.climate.get_output_value(), 0.5f);
  EXPECT_FLOAT_EQ(state.climate.get_active_output_value(), 0.0f);
  EXPECT_FLOAT_EQ(state.heat_output.last_state, 0.0f);
  EXPECT_FLOAT_EQ(state.cool_output.last_state, 0.0f);
  EXPECT_EQ(state.climate.action, climate::CLIMATE_ACTION_IDLE);
}

TEST(PIDClimateTest, HeatModeAllowsHeatingDemand) {
  PIDClimateTestState state;

  state.update(climate::CLIMATE_MODE_HEAT, 20.5f, 20.0f);

  EXPECT_FLOAT_EQ(state.climate.get_active_output_value(), 0.5f);
  EXPECT_FLOAT_EQ(state.heat_output.last_state, 0.5f);
  EXPECT_FLOAT_EQ(state.cool_output.last_state, 0.0f);
  EXPECT_EQ(state.climate.action, climate::CLIMATE_ACTION_HEATING);
}

TEST(PIDClimateTest, CoolModeAllowsCoolingDemand) {
  PIDClimateTestState state;

  state.update(climate::CLIMATE_MODE_COOL, 20.0f, 20.5f);

  EXPECT_FLOAT_EQ(state.climate.get_active_output_value(), -0.5f);
  EXPECT_FLOAT_EQ(state.heat_output.last_state, 0.0f);
  EXPECT_FLOAT_EQ(state.cool_output.last_state, 0.5f);
  EXPECT_EQ(state.climate.action, climate::CLIMATE_ACTION_COOLING);
}

TEST(PIDClimateTest, ControlReappliesOutputImmediatelyOnModeChange) {
  PIDClimateTestState state;

  state.update(climate::CLIMATE_MODE_HEAT_COOL, 20.0f, 20.5f);
  EXPECT_FLOAT_EQ(state.climate.get_output_value(), -0.5f);
  EXPECT_FLOAT_EQ(state.climate.get_active_output_value(), -0.5f);
  EXPECT_FLOAT_EQ(state.cool_output.last_state, 0.5f);
  EXPECT_EQ(state.climate.action, climate::CLIMATE_ACTION_COOLING);

  state.set_mode(climate::CLIMATE_MODE_HEAT);
  EXPECT_FLOAT_EQ(state.climate.get_output_value(), -0.5f);
  EXPECT_FLOAT_EQ(state.climate.get_active_output_value(), 0.0f);
  EXPECT_FLOAT_EQ(state.heat_output.last_state, 0.0f);
  EXPECT_FLOAT_EQ(state.cool_output.last_state, 0.0f);
  EXPECT_EQ(state.climate.action, climate::CLIMATE_ACTION_IDLE);

  state.set_mode(climate::CLIMATE_MODE_OFF);
  EXPECT_FLOAT_EQ(state.climate.get_output_value(), 0.0f);
  EXPECT_FLOAT_EQ(state.climate.get_active_output_value(), 0.0f);
  EXPECT_FLOAT_EQ(state.heat_output.last_state, 0.0f);
  EXPECT_FLOAT_EQ(state.cool_output.last_state, 0.0f);
  EXPECT_EQ(state.climate.action, climate::CLIMATE_ACTION_OFF);
}

}  // namespace esphome::pid::testing
