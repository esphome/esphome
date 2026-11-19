#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include "esphome/components/bme280_base/bme280_base.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::bme280_base::testing {

static constexpr uint8_t REGISTER_CONTROLHUMID = 0xF2;
static constexpr uint8_t REGISTER_MEASUREMENTS = 0xF7;

// The datasheet's worked example: adc_T=519888 gives 25.08 C, adc_P=415148 gives 1006.53 hPa.
static constexpr BME280CalibrationData DATASHEET_CALIBRATION{
    27504, 26435, -1000, 36477, -10685, 3024, 2855, 140, -7, 15500, -14600, 6000, 75, 355, 0, 333, 0, 30,
};

// A chip whose measurement registers hold fixed raw values; the bus never fails.
class FakeBME280 final : public BME280Component {
 public:
  FakeBME280() { this->calibration_ = DATASHEET_CALIBRATION; }

  void set_raw(uint32_t pressure, uint32_t temperature, uint16_t humidity) {
    this->registers[0] = pressure >> 12;
    this->registers[1] = pressure >> 4;
    this->registers[2] = pressure << 4;
    this->registers[3] = temperature >> 12;
    this->registers[4] = temperature >> 4;
    this->registers[5] = temperature << 4;
    this->registers[6] = humidity >> 8;
    this->registers[7] = humidity;
  }

  std::array<uint8_t, 8> registers{};
  int ctrl_hum{-1};

 protected:
  bool read_byte(uint8_t a_register, uint8_t *data) override { return false; }
  bool write_byte(uint8_t a_register, uint8_t data) override {
    if (a_register == REGISTER_CONTROLHUMID)
      this->ctrl_hum = data;
    return true;
  }
  bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) override {
    EXPECT_EQ(a_register, REGISTER_MEASUREMENTS);
    EXPECT_EQ(len, this->registers.size());
    for (size_t i = 0; i < len; i++)
      data[i] = this->registers[i];
    return true;
  }
  bool read_byte_16_le(uint8_t a_register, uint16_t *data) override { return false; }
};

// update() arms a conversion timeout of up to 114 ms at 16x; run the scheduler past it.
static void run_update(FakeBME280 &sensor) {
  sensor.update();
  delay(150);  // NOLINT
  App.scheduler.call(millis());
}

TEST(BME280Base, DatasheetExampleCompensates) {
  FakeBME280 sensor;
  sensor::Sensor temperature, pressure, humidity;
  sensor.set_temperature_sensor(&temperature);
  sensor.set_pressure_sensor(&pressure);
  sensor.set_humidity_sensor(&humidity);
  sensor.set_raw(415148, 519888, 0x8000);

  run_update(sensor);

  EXPECT_NEAR(temperature.state, 25.08f, 0.01f);
  EXPECT_NEAR(pressure.state, 1006.53f, 0.01f);
  EXPECT_TRUE(std::isfinite(humidity.state));
}

TEST(BME280Base, MidpointRawCodesPublishWhenSamplingIsEnabled) {
  FakeBME280 sensor;
  sensor::Sensor temperature, pressure, humidity;
  sensor.set_temperature_sensor(&temperature);
  sensor.set_pressure_sensor(&pressure);
  sensor.set_humidity_sensor(&humidity);
  sensor.set_temperature_oversampling(BME280_OVERSAMPLING_1X);
  sensor.set_pressure_oversampling(BME280_OVERSAMPLING_1X);
  sensor.set_humidity_oversampling(BME280_OVERSAMPLING_1X);
  // 0x80000 and 0x8000 are what a skipped channel reports, but an enabled one can read them too.
  sensor.set_raw(0x80000, 0x80000, 0x8000);

  run_update(sensor);

  ASSERT_TRUE(temperature.has_state());
  EXPECT_NEAR(temperature.state, 26.47f, 0.01f);
  ASSERT_TRUE(pressure.has_state());
  EXPECT_TRUE(std::isfinite(pressure.state));
  ASSERT_TRUE(humidity.has_state());
  EXPECT_TRUE(std::isfinite(humidity.state));
  EXPECT_FALSE(sensor.status_has_warning());
  EXPECT_EQ(sensor.ctrl_hum, BME280_OVERSAMPLING_1X);
}

TEST(BME280Base, DisabledChannelsPublishNan) {
  FakeBME280 sensor;
  sensor::Sensor temperature, pressure, humidity;
  sensor.set_temperature_sensor(&temperature);
  sensor.set_pressure_sensor(&pressure);
  sensor.set_humidity_sensor(&humidity);
  sensor.set_pressure_oversampling(BME280_OVERSAMPLING_NONE);
  sensor.set_humidity_oversampling(BME280_OVERSAMPLING_NONE);
  sensor.set_raw(0x80000, 519888, 0x8000);

  run_update(sensor);

  EXPECT_NEAR(temperature.state, 25.08f, 0.01f);
  ASSERT_TRUE(pressure.has_state());
  EXPECT_TRUE(std::isnan(pressure.state));
  ASSERT_TRUE(humidity.has_state());
  EXPECT_TRUE(std::isnan(humidity.state));
  // The skipped humidity channel is written to the chip each cycle, not only in setup().
  EXPECT_EQ(sensor.ctrl_hum, BME280_OVERSAMPLING_NONE);
}

TEST(BME280Base, DisabledTemperaturePublishesNothing) {
  FakeBME280 sensor;
  sensor::Sensor temperature, pressure;
  sensor.set_temperature_sensor(&temperature);
  sensor.set_pressure_sensor(&pressure);
  sensor.set_temperature_oversampling(BME280_OVERSAMPLING_NONE);
  sensor.set_raw(415148, 0x80000, 0x8000);

  run_update(sensor);

  EXPECT_FALSE(temperature.has_state());
  EXPECT_FALSE(pressure.has_state());
  EXPECT_TRUE(sensor.status_has_warning());
}

}  // namespace esphome::bme280_base::testing
