#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

#include "esphome/components/bme680_bsec/bme680_bsec.h"

// The unit test build defines USE_BSEC and builds against the library double in __sdk; clang-tidy does not.
#ifdef USE_BSEC

namespace esphome::bme680_bsec::testing {
namespace {

bsec_output_t output(uint8_t sensor_id, float signal, uint8_t accuracy = 0) {
  bsec_output_t out{};
  out.sensor_id = sensor_id;
  out.signal = signal;
  out.accuracy = accuracy;
  return out;
}

using Passes = std::vector<std::vector<std::string>>;

class BSECPublishTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bsec_double::armed = false;
    bsec_double::get_state_calls = 0;
    this->add_(this->temperature, "temperature");
    this->add_(this->pressure, "pressure");
    this->add_(this->humidity, "humidity");
    this->add_(this->gas_resistance, "gas_resistance");
    this->add_(this->iaq, "iaq");
    this->add_(this->iaq_accuracy, "iaq_accuracy");
    this->add_(this->co2_equivalent, "co2_equivalent");
    this->add_(this->breath_voc_equivalent, "breath_voc_equivalent");
    this->iaq_accuracy_text.add_on_state_callback(
        [this](const std::string &state) { this->events.push_back("iaq_accuracy_text=" + state); });
  }

  void attach_all() {
    this->component.set_temperature_sensor(&this->temperature);
    this->component.set_pressure_sensor(&this->pressure);
    this->component.set_humidity_sensor(&this->humidity);
    this->component.set_gas_resistance_sensor(&this->gas_resistance);
    this->component.set_iaq_sensor(&this->iaq);
    this->component.set_iaq_accuracy_sensor(&this->iaq_accuracy);
    this->component.set_co2_equivalent_sensor(&this->co2_equivalent);
    this->component.set_breath_voc_equivalent_sensor(&this->breath_voc_equivalent);
    this->component.set_iaq_accuracy_text_sensor(&this->iaq_accuracy_text);
  }

  // The next loop() pass reads these outputs from BSEC.
  void arm(const std::vector<bsec_output_t> &outputs) {
    ASSERT_LE(outputs.size(), BSEC_NUMBER_OUTPUTS);
    bsec_double::arm(outputs.data(), outputs.size());
  }

  // Runs loop() once and returns what that pass published.
  std::vector<std::string> pass() {
    size_t before = this->events.size();
    this->component.loop();
    return {this->events.begin() + before, this->events.end()};
  }

  Passes passes(size_t count) {
    Passes result;
    for (size_t i = 0; i < count; i++)
      result.push_back(this->pass());
    return result;
  }

  BME680BSECComponent component;
  sensor::Sensor temperature, pressure, humidity, gas_resistance, iaq, iaq_accuracy, co2_equivalent,
      breath_voc_equivalent;
  text_sensor::TextSensor iaq_accuracy_text;
  std::vector<std::string> events;

 private:
  void add_(sensor::Sensor &sensor, const char *name) {
    sensor.add_on_state_callback([this, name](float value) {
      char buf[64];
      snprintf(buf, sizeof(buf), "%s=%g", name, value);
      this->events.push_back(buf);
    });
  }
};

TEST_F(BSECPublishTest, PublishesOneStatePerLoopPassInOutputOrder) {
  this->attach_all();
  this->arm({
      output(BSEC_OUTPUT_STATIC_IAQ, 51.5f, 2),
      output(BSEC_OUTPUT_CO2_EQUIVALENT, 612.5f),
      output(BSEC_OUTPUT_BREATH_VOC_EQUIVALENT, 0.75f),
      output(BSEC_OUTPUT_RAW_PRESSURE, 101325.0f),
      output(BSEC_OUTPUT_RAW_GAS, 125000.0f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 21.5f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY, 44.5f),
  });
  // The pass that reads the outputs also publishes the first state.
  EXPECT_EQ(this->passes(12), (Passes{
                                  {"iaq=51.5"},
                                  {"iaq_accuracy_text=Calibrating"},
                                  {"iaq_accuracy=2"},
                                  {},  // state save opportunity
                                  {"co2_equivalent=612.5"},
                                  {"breath_voc_equivalent=0.75"},
                                  {"pressure=1013.25"},
                                  {"gas_resistance=125000"},
                                  {"temperature=21.5"},
                                  {"humidity=44.5"},
                                  {},
                                  {},
                              }));
  EXPECT_EQ(bsec_double::get_state_calls, 0);
}

TEST_F(BSECPublishTest, KeepsTheOrderBsecReturnsOutputsIn) {
  this->attach_all();
  this->arm({
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY, 44.5f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 21.5f),
      output(BSEC_OUTPUT_IAQ, 30.0f, 1),
      output(BSEC_OUTPUT_RAW_GAS, 125000.0f),
  });
  EXPECT_EQ(this->passes(8), (Passes{
                                 {"humidity=44.5"},
                                 {"temperature=21.5"},
                                 {"iaq=30"},
                                 {"iaq_accuracy_text=Uncertain"},
                                 {"iaq_accuracy=1"},
                                 {},
                                 {"gas_resistance=125000"},
                                 {},
                             }));
}

TEST_F(BSECPublishTest, UnconfiguredSensorStillTakesAPass) {
  this->component.set_temperature_sensor(&this->temperature);
  this->arm({
      output(BSEC_OUTPUT_RAW_PRESSURE, 100000.0f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 21.5f),
  });
  EXPECT_EQ(this->passes(3), (Passes{{}, {"temperature=21.5"}, {}}));
}

TEST_F(BSECPublishTest, AccuracyIsClampedAndSavesStateOnceCalibrated) {
  this->attach_all();
  this->component.set_state_save_interval(0);
  this->arm({output(BSEC_OUTPUT_IAQ, 30.0f, 7)});
  EXPECT_EQ(this->pass(), (std::vector<std::string>{"iaq=30"}));
  EXPECT_EQ(this->pass(), (std::vector<std::string>{"iaq_accuracy_text=Calibrated"}));
  EXPECT_EQ(this->pass(), (std::vector<std::string>{"iaq_accuracy=3"}));
  EXPECT_EQ(bsec_double::get_state_calls, 0);
  EXPECT_TRUE(this->pass().empty());
  EXPECT_EQ(bsec_double::get_state_calls, 1);
  EXPECT_TRUE(this->pass().empty());
  EXPECT_EQ(bsec_double::get_state_calls, 1);
}

TEST_F(BSECPublishTest, UnchangedAccuracyIsNotRepublished) {
  this->attach_all();
  this->arm({output(BSEC_OUTPUT_IAQ, 30.0f, 1)});
  this->passes(5);
  this->events.clear();
  this->arm({output(BSEC_OUTPUT_IAQ, 31.0f, 1)});
  EXPECT_EQ(this->passes(5), (Passes{{"iaq=31"}, {}, {}, {}, {}}));
  this->arm({output(BSEC_OUTPUT_IAQ, 32.0f, 2)});
  EXPECT_EQ(this->passes(5), (Passes{{"iaq=32"}, {"iaq_accuracy_text=Calibrating"}, {"iaq_accuracy=2"}, {}, {}}));
}

TEST_F(BSECPublishTest, NewReadingBeforeTheLastWentOut) {
  this->attach_all();
  this->arm({
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 20.0f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY, 40.0f),
  });
  EXPECT_EQ(this->pass(), (std::vector<std::string>{"temperature=20"}));
  this->arm({
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 21.0f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY, 41.0f),
  });
  // A state still waiting keeps its place and takes the newer value; the others queue behind it.
  EXPECT_EQ(this->passes(3), (Passes{{"humidity=41"}, {"temperature=21"}, {}}));
}

TEST_F(BSECPublishTest, WaitingAccuracyTakesTheNewerReading) {
  this->attach_all();
  this->component.set_state_save_interval(0);
  this->arm({output(BSEC_OUTPUT_IAQ, 30.0f, 1)});
  EXPECT_EQ(this->pass(), (std::vector<std::string>{"iaq=30"}));
  this->arm({output(BSEC_OUTPUT_IAQ, 31.0f, 3)});
  // The accuracy and save slots still waiting go out once, with the newer accuracy; IAQ queues behind them.
  EXPECT_EQ(this->passes(3), (Passes{{"iaq_accuracy_text=Calibrated"}, {"iaq_accuracy=3"}, {}}));
  EXPECT_EQ(bsec_double::get_state_calls, 1);
  EXPECT_EQ(this->passes(2), (Passes{{"iaq=31"}, {}}));
  EXPECT_EQ(bsec_double::get_state_calls, 1);
}

}  // namespace
}  // namespace esphome::bme680_bsec::testing
#endif  // USE_BSEC
