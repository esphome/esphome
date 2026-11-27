#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#ifdef USE_HOST

#include "esphome/components/host/preferences.h"
#include "esphome/components/mq_gas_sensors/mq_gas_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

namespace esphome::mq_gas_sensors::testing {
namespace fs = std::filesystem;

/// Voltage source whose reading the test sets directly.
class CalibrationSampler : public voltage_sampler::VoltageSampler {
 public:
  float sample() override { return this->value; }
  float value{1.0f};
};

/// Cancels the calibration timers a component owns when it leaves scope, also
/// when an assertion returns early, so no timer outlives the sensor it captures.
class CalibrationTimerGuard {
 public:
  explicit CalibrationTimerGuard(MQGasSensor &sensor) : sensor_(sensor) {}
  /// The scheduler is public, unlike Component::cancel_timeout(); the calibration
  /// start and its retry share id 0.
  ~CalibrationTimerGuard() { App.scheduler.cancel_timeout(&this->sensor_, uint32_t{0}); }

 private:
  MQGasSensor &sensor_;
};

/// Scratch host preferences, so a calibration never touches the real store.
class CalibrationPersistenceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->previous_preferences_ = global_preferences;
    this->previous_host_preferences_ = host::host_preferences;
    static uint32_t test_id = 0;
    char directory[48];
    std::snprintf(directory, sizeof(directory), "esphome_mq_gas_%u", ++test_id);
    this->prefdir_ = fs::temp_directory_path() / directory;
    fs::create_directories(this->prefdir_);
    setenv("ESPHOME_PREFDIR", this->prefdir_.string().c_str(), 1);
    global_preferences = &this->preferences_;
    host::host_preferences = &this->preferences_;
  }

  void TearDown() override {
    global_preferences = this->previous_preferences_;
    host::host_preferences = this->previous_host_preferences_;
    unsetenv("ESPHOME_PREFDIR");
    std::error_code ec;
    fs::remove_all(this->prefdir_, ec);
  }

  /// A 1.0 V sample is RS = 40 kOhm, so a clean-air ratio of 70 gives R0 = 4/7 kOhm.
  static void configure(MQGasSensor &sensor) {
    sensor.set_a(976.97f);
    sensor.set_b(-0.688f);
    sensor.set_regression_method(RegressionMethod::REGRESSION_METHOD_EXPONENTIAL);
    sensor.set_rl(10.0f);
    sensor.set_vcc(5.0f);
    sensor.set_samples(1);
    sensor.set_sample_interval(0);
    sensor.set_voltage_multiplier(1.0f);
    sensor.set_min_ppm(0.0f);
    sensor.set_max_ppm(10000.0f);
    sensor.set_preference_key(0x12345678);
    sensor.set_calibration(true, 70.0f, 0, 0, 2, true);
  }

  /// One loop pass per sample while the interval is zero. setup() creates the
  /// preference handle the calibration stores its result through.
  static void run_calibration(MQGasSensor &sensor) {
    sensor.setup();
    sensor.start_calibration();
    for (int i = 0; i < 8 && !sensor.has_r0(); i++)
      sensor.loop();
  }

  host::HostPreferences preferences_;
  host::HostPreferences *previous_host_preferences_{nullptr};
  ESPPreferences *previous_preferences_{nullptr};
  fs::path prefdir_;
};

TEST_F(CalibrationPersistenceTest, TwoValidSamplesBecomeTheReference) {
  CalibrationSampler sampler;
  sampler.value = 1.0f;
  MQGasSensor sensor{&sampler};
  configure(sensor);

  run_calibration(sensor);

  ASSERT_TRUE(sensor.has_r0());
  EXPECT_NEAR(40.0f / 70.0f, sensor.get_r0(), 1e-4f);
}

TEST_F(CalibrationPersistenceTest, OneSampleIsNotEnough) {
  CalibrationSampler sampler;
  sampler.value = 1.0f;
  MQGasSensor sensor{&sampler};
  CalibrationTimerGuard guard(sensor);
  configure(sensor);
  sensor.set_calibration(true, 70.0f, 0, 0, 1, false);

  run_calibration(sensor);

  EXPECT_FALSE(sensor.has_r0());
  EXPECT_TRUE(sensor.status_has_warning());
}

TEST_F(CalibrationPersistenceTest, StoredReferenceIsRestoredForTheSameKey) {
  CalibrationSampler sampler;
  sampler.value = 1.0f;
  {
    MQGasSensor sensor{&sampler};
    configure(sensor);
    run_calibration(sensor);
    ASSERT_TRUE(sensor.has_r0());
  }

  // A fresh backend reads the value from the scratch directory, so the reference
  // has to come from the saved file rather than from the in-memory cache of the
  // instance that stored it.
  host::HostPreferences reloaded;
  global_preferences = &reloaded;
  host::host_preferences = &reloaded;

  MQGasSensor restored{&sampler};
  configure(restored);
  restored.setup();

  ASSERT_TRUE(restored.has_r0());
  EXPECT_NEAR(40.0f / 70.0f, restored.get_r0(), 1e-4f);
}

TEST_F(CalibrationPersistenceTest, ADifferentKeyDoesNotRestoreTheReference) {
  CalibrationSampler sampler;
  sampler.value = 1.0f;
  {
    MQGasSensor sensor{&sampler};
    configure(sensor);
    run_calibration(sensor);
    ASSERT_TRUE(sensor.has_r0());
  }

  MQGasSensor rewired{&sampler};
  CalibrationTimerGuard guard(rewired);
  configure(rewired);
  rewired.set_preference_key(0x87654321);
  rewired.setup();

  EXPECT_FALSE(rewired.has_r0());
}

TEST_F(CalibrationPersistenceTest, AFailedManualRunKeepsTheReferenceUntilTheNextReading) {
  CalibrationSampler sampler;
  MQGasSensor sensor{&sampler};
  CalibrationTimerGuard guard(sensor);
  configure(sensor);
  sensor.set_r0(10.0f);
  sensor.setup();

  sampler.value = 0.0f;  // no usable sample, so the run fails
  sensor.start_calibration();
  sensor.loop();
  sensor.loop();
  EXPECT_FLOAT_EQ(10.0f, sensor.get_r0());
  EXPECT_TRUE(sensor.status_has_warning());

  sampler.value = 1.0f;
  sensor.update();
  EXPECT_TRUE(std::isfinite(sensor.state));
  EXPECT_FALSE(sensor.status_has_warning());
}

TEST_F(CalibrationPersistenceTest, AFailedSyncIsReportedAndStaysVisible) {
  CalibrationSampler sampler;
  MQGasSensor sensor{&sampler};
  configure(sensor);
  text_sensor::TextSensor log;
  std::string messages;
  log.add_on_state_callback([&messages](const std::string &message) { messages += message + "\n"; });
  sensor.set_log_sensor(&log);
  sensor.setup();
  // A directory where the preferences file belongs makes the write fail.
  fs::create_directory(this->prefdir_ / (std::string(App.get_name().c_str()) + ".prefs"));

  sensor.start_calibration();
  for (int i = 0; i < 8 && !sensor.has_r0(); i++)
    sensor.loop();

  ASSERT_TRUE(sensor.has_r0());
  EXPECT_NE(std::string::npos, messages.find("may not survive a reboot"));
  EXPECT_NE(std::string::npos, messages.find("not stored"));
  sensor.update();
  EXPECT_TRUE(sensor.status_has_warning());
}

}  // namespace esphome::mq_gas_sensors::testing

#endif  // USE_HOST
