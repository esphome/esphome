#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include "esphome/components/bmp280_base/bmp280_base.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::bmp280_base::testing {

static constexpr uint8_t REGISTER_PRESSUREDATA = 0xF7;
static constexpr uint8_t REGISTER_TEMPDATA = 0xFA;

// The datasheet's worked example: adc_T=519888 gives 25.08 C, adc_P=415148 gives 1006.53 hPa.
static constexpr BMP280CalibrationData DATASHEET_CALIBRATION{
    27504, 26435, -1000, 36477, -10685, 3024, 2855, 140, -7, 15500, -14600, 6000,
};

// A chip whose measurement registers hold fixed raw values; the bus never fails.
class FakeBMP280 final : public BMP280Component {
 public:
  FakeBMP280() { this->calibration_ = DATASHEET_CALIBRATION; }

  void set_raw(uint32_t pressure, uint32_t temperature) {
    this->pressure_ = {uint8_t(pressure >> 12), uint8_t(pressure >> 4), uint8_t(pressure << 4)};
    this->temperature_ = {uint8_t(temperature >> 12), uint8_t(temperature >> 4), uint8_t(temperature << 4)};
  }

 protected:
  bool bmp_read_byte(uint8_t a_register, uint8_t *data) override { return false; }
  bool bmp_write_byte(uint8_t a_register, uint8_t data) override { return true; }
  bool bmp_read_bytes(uint8_t a_register, uint8_t *data, size_t len) override {
    EXPECT_EQ(len, 3u);
    const auto &source = a_register == REGISTER_TEMPDATA ? this->temperature_ : this->pressure_;
    EXPECT_TRUE(a_register == REGISTER_TEMPDATA || a_register == REGISTER_PRESSUREDATA);
    for (size_t i = 0; i < len; i++)
      data[i] = source[i];
    return true;
  }
  bool bmp_read_byte_16_le(uint8_t a_register, uint16_t *data) override { return false; }

  std::array<uint8_t, 3> pressure_{};
  std::array<uint8_t, 3> temperature_{};
};

// update() arms a conversion timeout of up to 76 ms at 16x; run the scheduler past it.
static void run_update(FakeBMP280 &sensor) {
  sensor.update();
  delay(150);  // NOLINT
  App.scheduler.call(millis());
}

TEST(BMP280Base, DatasheetExampleCompensates) {
  FakeBMP280 sensor;
  sensor::Sensor temperature, pressure;
  sensor.set_temperature_sensor(&temperature);
  sensor.set_pressure_sensor(&pressure);
  sensor.set_raw(415148, 519888);

  run_update(sensor);

  EXPECT_NEAR(temperature.state, 25.08f, 0.01f);
  EXPECT_NEAR(pressure.state, 1006.53f, 0.01f);
}

TEST(BMP280Base, MidpointRawCodesPublishWhenSamplingIsEnabled) {
  FakeBMP280 sensor;
  sensor::Sensor temperature, pressure;
  sensor.set_temperature_sensor(&temperature);
  sensor.set_pressure_sensor(&pressure);
  sensor.set_temperature_oversampling(BMP280_OVERSAMPLING_1X);
  sensor.set_pressure_oversampling(BMP280_OVERSAMPLING_1X);
  // 0x80000 is what a skipped channel reports, but an enabled one can read it too.
  sensor.set_raw(0x80000, 0x80000);

  run_update(sensor);

  ASSERT_TRUE(temperature.has_state());
  EXPECT_NEAR(temperature.state, 26.47f, 0.01f);
  ASSERT_TRUE(pressure.has_state());
  EXPECT_TRUE(std::isfinite(pressure.state));
  EXPECT_FALSE(sensor.status_has_warning());
}

TEST(BMP280Base, DisabledPressurePublishesNan) {
  FakeBMP280 sensor;
  sensor::Sensor temperature, pressure;
  sensor.set_temperature_sensor(&temperature);
  sensor.set_pressure_sensor(&pressure);
  sensor.set_pressure_oversampling(BMP280_OVERSAMPLING_NONE);
  sensor.set_raw(0x80000, 519888);

  run_update(sensor);

  EXPECT_NEAR(temperature.state, 25.08f, 0.01f);
  ASSERT_TRUE(pressure.has_state());
  EXPECT_TRUE(std::isnan(pressure.state));
}

TEST(BMP280Base, DisabledTemperaturePublishesNothing) {
  FakeBMP280 sensor;
  sensor::Sensor temperature, pressure;
  sensor.set_temperature_sensor(&temperature);
  sensor.set_pressure_sensor(&pressure);
  sensor.set_temperature_oversampling(BMP280_OVERSAMPLING_NONE);
  sensor.set_raw(415148, 0x80000);

  run_update(sensor);

  EXPECT_FALSE(temperature.has_state());
  EXPECT_FALSE(pressure.has_state());
  EXPECT_TRUE(sensor.status_has_warning());
}

}  // namespace esphome::bmp280_base::testing
