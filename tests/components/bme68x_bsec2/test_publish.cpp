#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

#include "esphome/components/bme68x_bsec2/bme68x_bsec2.h"

// The unit test build defines USE_BSEC2 and builds against the library double in __sdk; clang-tidy does not.
#ifdef USE_BSEC2

namespace esphome::bme68x_bsec2::testing {
namespace {

class TestBSEC2 : public BME68xBSEC2Component {
 public:
  uint32_t get_hash() override { return 0; }
  using BME68xBSEC2Component::publish_;
  using BME68xBSEC2Component::PUBLISH_QUEUE_SIZE;
  // setup() leaves the loop disabled until the first reading.
  void start_idle() { this->disable_loop(); }
};

std::string format_state(const char *name, float value) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s=%g", name, value);
  return buf;
}

bsec_output_t output(uint8_t sensor_id, float signal, uint8_t accuracy = 0) {
  bsec_output_t out{};
  out.sensor_id = sensor_id;
  out.signal = signal;
  out.accuracy = accuracy;
  return out;
}

class BSEC2PublishTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bsec2_double::get_state_calls = 0;
    this->add_(this->temperature, "temperature");
    this->add_(this->pressure, "pressure");
    this->add_(this->humidity, "humidity");
    this->add_(this->gas_resistance, "gas_resistance");
    this->add_(this->iaq, "iaq");
    this->add_(this->iaq_static, "iaq_static");
    this->add_(this->iaq_accuracy, "iaq_accuracy");
    this->add_(this->co2_equivalent, "co2_equivalent");
    this->add_(this->breath_voc_equivalent, "breath_voc_equivalent");
    this->iaq_accuracy_text.add_on_state_callback(
        [this](const std::string &state) { this->events.push_back("iaq_accuracy_text=" + state); });
    this->component.start_idle();
  }

  void attach_all() {
    this->component.set_temperature_sensor(&this->temperature);
    this->component.set_pressure_sensor(&this->pressure);
    this->component.set_humidity_sensor(&this->humidity);
    this->component.set_gas_resistance_sensor(&this->gas_resistance);
    this->component.set_iaq_sensor(&this->iaq);
    this->component.set_iaq_static_sensor(&this->iaq_static);
    this->component.set_iaq_accuracy_sensor(&this->iaq_accuracy);
    this->component.set_co2_equivalent_sensor(&this->co2_equivalent);
    this->component.set_breath_voc_equivalent_sensor(&this->breath_voc_equivalent);
    this->component.set_iaq_accuracy_text_sensor(&this->iaq_accuracy_text);
  }

  void publish(const std::vector<bsec_output_t> &outputs) { this->component.publish_(outputs.data(), outputs.size()); }

  // Runs loop() once and returns what that pass published.
  std::vector<std::string> pass() {
    size_t before = this->events.size();
    this->component.loop();
    return {this->events.begin() + before, this->events.end()};
  }

  // Runs loop() until it disables itself; returns the publishes, one entry per pass.
  std::vector<std::vector<std::string>> drain() {
    std::vector<std::vector<std::string>> passes;
    while (this->component.is_in_loop_state()) {
      passes.push_back(this->pass());
      if (passes.size() > 100) {
        ADD_FAILURE() << "loop never disabled itself";
        break;
      }
    }
    return passes;
  }

  TestBSEC2 component;
  sensor::Sensor temperature, pressure, humidity, gas_resistance, iaq, iaq_static, iaq_accuracy, co2_equivalent,
      breath_voc_equivalent;
  text_sensor::TextSensor iaq_accuracy_text;
  std::vector<std::string> events;

 private:
  void add_(sensor::Sensor &sensor, const char *name) {
    sensor.add_on_state_callback([this, name](float value) { this->events.push_back(format_state(name, value)); });
  }
};

using Passes = std::vector<std::vector<std::string>>;

TEST_F(BSEC2PublishTest, PublishesOneStatePerLoopPassInOutputOrder) {
  this->attach_all();
  this->publish({
      output(BSEC_OUTPUT_IAQ, 51.5f, 1),
      output(BSEC_OUTPUT_STATIC_IAQ, 48.25f, 2),
      output(BSEC_OUTPUT_CO2_EQUIVALENT, 612.5f),
      output(BSEC_OUTPUT_BREATH_VOC_EQUIVALENT, 0.75f),
      output(BSEC_OUTPUT_RAW_PRESSURE, 101325.0f),
      output(BSEC_OUTPUT_RAW_GAS, 125000.0f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 21.5f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY, 44.5f),
  });
  // Nothing goes out until loop() runs.
  EXPECT_TRUE(this->events.empty());
  EXPECT_TRUE(this->component.is_in_loop_state());

  EXPECT_EQ(this->drain(), (Passes{
                               {"iaq=51.5"},
                               {"iaq_static=48.25"},
                               {"co2_equivalent=612.5"},
                               {"breath_voc_equivalent=0.75"},
                               {"pressure=1013.25"},
                               {"gas_resistance=125000"},
                               {"temperature=21.5"},
                               {"humidity=44.5"},
                               {"iaq_accuracy=2"},
                               {"iaq_accuracy_text=Calibrating"},
                               {},  // state save opportunity
                               {},  // finds nothing left and disables the loop
                           }));
  EXPECT_EQ(bsec2_double::get_state_calls, 0);
}

TEST_F(BSEC2PublishTest, KeepsTheOrderBsecReturnsOutputsIn) {
  this->attach_all();
  this->publish({
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY, 44.5f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 21.5f),
      output(BSEC_OUTPUT_IAQ, 51.5f, 3),
      output(BSEC_OUTPUT_RAW_GAS, 125000.0f),
  });
  EXPECT_EQ(this->drain(), (Passes{
                               {"humidity=44.5"},
                               {"temperature=21.5"},
                               {"iaq=51.5"},
                               {"gas_resistance=125000"},
                               {"iaq_accuracy=3"},
                               {"iaq_accuracy_text=Calibrated"},
                               {},
                               {},
                           }));
  // Calibrated, but inside the default 6 h state save interval.
  EXPECT_EQ(bsec2_double::get_state_calls, 0);
}

TEST_F(BSEC2PublishTest, NoAccuracyWithoutAnIaqOutput) {
  this->attach_all();
  this->publish({
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 21.5f),
      output(BSEC_OUTPUT_RAW_PRESSURE, 100000.0f),
  });
  EXPECT_EQ(this->drain(), (Passes{{"temperature=21.5"}, {"pressure=1000"}, {}}));
}

TEST_F(BSEC2PublishTest, UnconfiguredSensorStillTakesAPass) {
  this->component.set_temperature_sensor(&this->temperature);
  this->publish({
      output(BSEC_OUTPUT_RAW_PRESSURE, 100000.0f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 21.5f),
  });
  EXPECT_EQ(this->drain(), (Passes{{}, {"temperature=21.5"}, {}}));
}

TEST_F(BSEC2PublishTest, EmptyReadingEnablesLoopUntilTheNextPass) {
  this->attach_all();
  this->publish({});
  EXPECT_TRUE(this->component.is_in_loop_state());
  EXPECT_EQ(this->drain(), (Passes{{}}));
}

TEST_F(BSEC2PublishTest, AccuracyIsClampedAndSavesStateOnceCalibrated) {
  this->attach_all();
  this->component.set_state_save_interval(0);
  this->publish({output(BSEC_OUTPUT_IAQ, 30.0f, 7)});
  EXPECT_EQ(this->pass(), (std::vector<std::string>{"iaq=30"}));
  EXPECT_EQ(this->pass(), (std::vector<std::string>{"iaq_accuracy=3"}));
  EXPECT_EQ(this->pass(), (std::vector<std::string>{"iaq_accuracy_text=Calibrated"}));
  EXPECT_EQ(bsec2_double::get_state_calls, 0);
  EXPECT_TRUE(this->pass().empty());
  EXPECT_EQ(bsec2_double::get_state_calls, 1);
  EXPECT_TRUE(this->component.is_in_loop_state());
  EXPECT_TRUE(this->pass().empty());
  EXPECT_FALSE(this->component.is_in_loop_state());
}

TEST_F(BSEC2PublishTest, UnchangedAccuracyIsNotRepublished) {
  this->attach_all();
  this->publish({output(BSEC_OUTPUT_IAQ, 30.0f, 1)});
  this->drain();
  this->events.clear();
  this->publish({output(BSEC_OUTPUT_IAQ, 31.0f, 1)});
  EXPECT_EQ(this->drain(), (Passes{{"iaq=31"}, {}, {}, {}, {}}));
  this->publish({output(BSEC_OUTPUT_IAQ, 32.0f, 2)});
  EXPECT_EQ(this->drain(), (Passes{{"iaq=32"}, {"iaq_accuracy=2"}, {"iaq_accuracy_text=Calibrating"}, {}, {}}));
}

TEST_F(BSEC2PublishTest, NewReadingBeforeTheLastWentOut) {
  this->attach_all();
  this->publish({
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 20.0f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY, 40.0f),
  });
  EXPECT_EQ(this->pass(), (std::vector<std::string>{"temperature=20"}));
  this->publish({
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 21.0f),
      output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY, 41.0f),
  });
  // Both readings go out in full, in arrival order.
  EXPECT_EQ(this->drain(), (Passes{{"humidity=40"}, {"temperature=21"}, {"humidity=41"}, {}}));
}

TEST_F(BSEC2PublishTest, ParallelModeFieldsAllGoOutInOrder) {
  this->attach_all();
  static constexpr const char *STATES[] = {"Stabilizing", "Uncertain", "Calibrating"};
  // One read in parallel mode returns up to 3 fields, each published in full before loop() runs.
  Passes expected;
  for (int field = 0; field < 3; field++) {
    const float f = field;
    this->publish({
        output(BSEC_OUTPUT_IAQ, 50.0f + f, field),
        output(BSEC_OUTPUT_STATIC_IAQ, 40.0f + f),
        output(BSEC_OUTPUT_CO2_EQUIVALENT, 600.0f + f),
        output(BSEC_OUTPUT_BREATH_VOC_EQUIVALENT, 1.0f + f),
        output(BSEC_OUTPUT_RAW_PRESSURE, 100000.0f + 100.0f * f),
        output(BSEC_OUTPUT_RAW_GAS, 120000.0f + f),
        output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, 20.0f + f),
        output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY, 45.0f + f),
    });
    for (const auto &[name, value] : std::initializer_list<std::pair<const char *, float>>{
             {"iaq", 50 + f},
             {"iaq_static", 40 + f},
             {"co2_equivalent", 600 + f},
             {"breath_voc_equivalent", 1 + f},
             {"pressure", 1000 + f},
             {"gas_resistance", 120000 + f},
             {"temperature", 20 + f},
             {"humidity", 45 + f},
             {"iaq_accuracy", f},
         }) {
      expected.push_back({format_state(name, value)});
    }
    expected.push_back({std::string("iaq_accuracy_text=") + STATES[field]});
    expected.push_back({});  // state save opportunity
  }
  expected.push_back({});
  // 3 full fields fill the queue exactly; nothing is dropped.
  ASSERT_EQ(expected.size(), TestBSEC2::PUBLISH_QUEUE_SIZE + 1u);
  EXPECT_EQ(this->drain(), expected);
}

TEST_F(BSEC2PublishTest, FullQueueDropsTheOldestState) {
  this->component.set_temperature_sensor(&this->temperature);
  const int size = TestBSEC2::PUBLISH_QUEUE_SIZE;
  // One state more than fits pushes out the first.
  for (int i = 0; i <= size; i++)
    this->publish({output(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, static_cast<float>(i))});
  auto passes = this->drain();
  ASSERT_EQ(passes.size(), static_cast<size_t>(size) + 1);
  EXPECT_EQ(passes.front(), (std::vector<std::string>{"temperature=1"}));
  EXPECT_EQ(passes[size - 1], (std::vector<std::string>{format_state("temperature", size)}));
  EXPECT_TRUE(passes.back().empty());
}

}  // namespace
}  // namespace esphome::bme68x_bsec2::testing
#endif  // USE_BSEC2
