#include <gtest/gtest.h>

#include "esphome/components/ms8607/ms8607.h"

namespace esphome::ms8607::testing {

class TestableMS8607Component : public MS8607Component {
 public:
  using MS8607Component::MS8607Component;

  using CalibrationValues = MS8607Component::CalibrationValues;
  using CompensatedTemperature = MS8607Component::CompensatedTemperature;

  using MS8607Component::compensated_temperature;
  using MS8607Component::compensated_pressure;
  using MS8607Component::compensated_humidity;
};

/// Create a CalibrationValues object with provided values, defaults to the
/// "Example / Typical" values from datasheet:
/// https://www.te.com/commerce/DocumentDelivery/DDEController?Action=showdoc&DocId=Data+Sheet%7FMS8607-02BA01%7FB3%7Fpdf%7FEnglish%7FENG_DS_MS8607-02BA01_B3.pdf%7FCAT-BLPS0018
/*
Note that calibration values read from my MS8607 are in the same range
Pressure Sensitivity: 0xA932 == 43314
Pressure Offset: 0xAC7F == 44159
Pressure Sensitivity Temperature Coefficient: 0x6773 == 26483
Pressure Offset Temperature Coefficient: 0x6DC3 == 28009
Reference Temperature: 0x7691 == 30353
Temperature Coefficient of Temperature: 0x6B04 == 27396
*/
static TestableMS8607Component::CalibrationValues create_calibration_values(uint16_t c1 = 46372, uint16_t c2 = 43981,
                                                                            uint16_t c3 = 29059, uint16_t c4 = 27842,
                                                                            uint16_t c5 = 31553, uint16_t c6 = 28165) {
  TestableMS8607Component::CalibrationValues values;
  values.pressure_sensitivity = c1;
  values.pressure_offset = c2;
  values.pressure_sensitivity_temperature_coefficient = c3;
  values.pressure_offset_temperature_coefficient = c4;
  values.reference_temperature = c5;
  values.temperature_coefficient_of_temperature = c6;
  return values;
}

TEST(MS8607Test, Category1_TemperatureCriticalBoundaries) {
  auto calibration_values = create_calibration_values();
  uint32_t const reference_temperature = calibration_values.reference_temperature << 8;

  // 1. Exact Reference Temperature (20°C Standard Baseline)
  {
    uint32_t const d2_raw_temperature = reference_temperature;
    auto res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, 0);
    EXPECT_EQ(res.first_order_temperature, 2000);
    EXPECT_NEAR(res.temperature_float, 20.0f, 1e-3f);
  }

  // 2. Just Above Crossover (20.01°C)
  {
    uint32_t const d2_raw_temperature = reference_temperature + 2000;
    auto res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, 2000);
    EXPECT_EQ(res.first_order_temperature, 2006);
    EXPECT_NEAR(res.temperature_float, 20.06f, 1e-3f);
  }

  // 3. Just Below Crossover (19.99°C)
  {
    uint32_t const d2_raw_temperature = reference_temperature - 2000;
    auto res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, -2000);
    EXPECT_EQ(res.first_order_temperature, 1993);
    EXPECT_NEAR(res.temperature_float, 19.93f, 1e-3f);
  }

  // 4. Maximum Specified Temperature Before 2nd Order (+84.32°C)
  {
    uint32_t const d2_raw_temperature = 10013516;
    auto res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, 1935948);
    EXPECT_EQ(res.first_order_temperature, 8500);
    EXPECT_NEAR(res.temperature_float, 84.32f, 1e-3f);
  }

  // 5. Maximum Specified Temperature (+85°C)
  {
    uint32_t const d2_raw_temperature = 10034215;
    auto res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, 1956647);
    EXPECT_EQ(res.first_order_temperature, 8569);
    EXPECT_NEAR(res.temperature_float, 85.00f, 1e-3f);
  }

  // 6. Minimum Specified Temperature (-40°C)
  {
    uint32_t const d2_raw_temperature = 6537387;
    auto res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, -1540181);
    EXPECT_EQ(res.first_order_temperature, -3172);
    EXPECT_NEAR(res.temperature_float, -40.00f, 1e-3f);
  }

  // 7. Minimum Specified Temperature Before 2nd Order (-51.15°C)
  // Note: I suspect this is out of range
  {
    uint32_t const d2_raw_temperature = 6290732;
    auto res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, -1786836);
    EXPECT_EQ(res.first_order_temperature, -4000);
    EXPECT_NEAR(res.temperature_float, -51.15f, 1e-3f);
  }
}

TEST(MS8607Test, Category2_PressureCompensationMath) {
  auto calibration_values = create_calibration_values();

  // 1. Standard Mid-Scale Barometric Pressure (~1013 mbar @ 20°C)
  {
    uint32_t const d1 = 6268671;
    uint32_t const d2_raw_temperature = calibration_values.reference_temperature << 8;
    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);
    EXPECT_NEAR(pressure, 1013.00f, 1e-2f);
  }

  // 2. Minimum Linear Pressure Limit (10 mbar)
  {
    uint32_t const d1 = 10000;
    uint32_t const d2_raw_temperature = calibration_values.reference_temperature << 8;
    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);
    EXPECT_NEAR(pressure, -1754.82f, 1e-2f);
  }

  // 3. Maximum Extended Pressure Limit (2000 mbar)
  {
    uint32_t const d1 = 16777215;
    uint32_t const d2_raw_temperature = calibration_values.reference_temperature << 8;
    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);
    EXPECT_NEAR(pressure, 5660.27f, 1e-2f);
  }

  // 4. Low Pressure at Extreme Low Temperature (10 mbar @ -40°C)
  {
    uint32_t const d1 = 10000;
    uint32_t const d2_raw_temperature = 2000000;
    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);
    EXPECT_NEAR(pressure, 1012.40f, 1e-2f);
  }

  // 5. High Pressure at Extreme Low Temperature (2000 mbar @ -40°C)
  {
    uint32_t const d1 = 16000000;
    uint32_t const d2_raw_temperature = 2000000;
    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);
    EXPECT_NEAR(pressure, -2868.00f, 1e-2f);
  }

  // 6. Low Pressure at Extreme High Temperature (10 mbar @ +85°C)
  {
    uint32_t const d1 = 10000;
    uint32_t const d2_raw_temperature = 12000000;
    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);
    EXPECT_NEAR(pressure, -2274.28f, 1e-2f);
  }
}

TEST(MS8607Test, Category3_HumidityTemperatureCompensation) {
  // Note: D_RH is a 12-bit ADC value, which corresponds to the upper bits of
  // the raw 16-bit humidity value. So D_RH << 4 is passed as the humidity_float.

  // 1. Perfect Dry Boundary (0% RH @ 20°C)
  {
    float const d_rh = 0.0f;
    float const raw_humidity = d_rh * 16.0f;
    float const temp = 20.0f;
    float const hum = TestableMS8607Component::compensated_humidity(raw_humidity, temp);
    EXPECT_NEAR(hum, -6.0f, 1e-3f);
  }

  // 2. Saturated Wet Boundary (100% RH @ 20°C)
  {
    float const d_rh = 4095.0f;
    float const raw_humidity = d_rh * 16.0f;
    float const temp = 20.0f;
    float const hum = TestableMS8607Component::compensated_humidity(raw_humidity, temp);
    EXPECT_NEAR(hum, 117.9697f, 1e-3f);
  }

  // 3. Mid-Range Humidity at Reference Temp (50% RH @ 20°C)
  {
    float const d_rh = 2048.0f;
    float const raw_humidity = d_rh * 16.0f;
    float const temp = 20.0f;
    float const hum = TestableMS8607Component::compensated_humidity(raw_humidity, temp);
    EXPECT_NEAR(hum, 56.0f, 1e-3f);
  }

  // 4. Humidity Compensation at High Temperature (100% RH @ +85°C)
  {
    float const d_rh = 4095.0f;
    float const raw_humidity = d_rh * 16.0f;
    float const temp = 148.90f;
    float const hum = TestableMS8607Component::compensated_humidity(raw_humidity, temp);
    EXPECT_NEAR(hum, 141.1717f, 1e-3f);
  }

  // 5. Humidity Compensation at Extreme Low Temp (100% RH @ -40°C)
  {
    float const d_rh = 4095.0f;
    float const raw_humidity = d_rh * 16.0f;
    float const temp = -313.06f;
    float const hum = TestableMS8607Component::compensated_humidity(raw_humidity, temp);
    EXPECT_NEAR(hum, 58.0189f, 1e-3f);
  }

  // 6. Low Humidity at Extreme Low Temp (0% RH @ -40°C)
  {
    float const d_rh = 0.0f;
    float const raw_humidity = d_rh * 16.0f;
    float const temp = -313.06f;
    float const hum = TestableMS8607Component::compensated_humidity(raw_humidity, temp);
    EXPECT_NEAR(hum, -65.9508f, 1e-3f);
  }
}

TEST(MS8607Test, Category4_MathematicalEdgeCases) {
  // 1. All Zero Raw Input State (Fault / Sensor Error simulation)
  {
    auto calibration_values = create_calibration_values();
    uint32_t const d1 = 0;
    uint32_t const d2_raw_temperature = 0;
    float const d_rh = 0.0f;

    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);
    float const hum = TestableMS8607Component::compensated_humidity(d_rh * 16.0f, temp_res.temperature_float);

    EXPECT_NEAR(temp_res.temperature_float, -479.08f, 1e-2f);
    EXPECT_NEAR(pressure, 3063.58f, 1e-2f);
    EXPECT_NEAR(hum, -95.8344f, 1e-2f);
  }

  // 2. All Max Raw Input State (Fault / Line Floating simulation)
  {
    auto calibration_values = create_calibration_values();
    uint32_t const d1 = 16777215;
    uint32_t const d2_raw_temperature = 16777215;
    float const d_rh = 4095.0f;

    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);
    float const hum = TestableMS8607Component::compensated_humidity(d_rh * 16.0f, temp_res.temperature_float);

    EXPECT_NEAR(temp_res.temperature_float, 298.33f, 1e-2f);
    EXPECT_NEAR(pressure, 9326.51f, 1e-2f);
    EXPECT_NEAR(hum, 168.0691f, 1e-2f);
  }

  // 3. Maximum Positive dT Matrix Deviation
  {
    auto calibration_values = create_calibration_values();
    uint32_t const d2_raw_temperature = 16777215;
    auto res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, 8699647);
    EXPECT_NEAR(res.temperature_float, 298.33f, 1e-2f);
  }

  // 4. Maximum Negative dT Matrix Deviation
  {
    auto calibration_values = create_calibration_values();
    uint32_t const d2_raw_temperature = 1;
    auto res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, -8077567);
    EXPECT_NEAR(res.temperature_float, -479.08f, 1e-2f);
  }

  // 5. Sensitivity Scaling Overflow Point (C coefficients manually adjusted to max 65535)
  {
    auto calibration_values = create_calibration_values(65535, 65535, 65535, 65535, 65535, 65535);
    uint32_t const d1 = 16777215;
    uint32_t const d2_raw_temperature = 16777215;

    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);

    EXPECT_NEAR(temp_res.temperature_float, 20.01f, 1e-2f);
    EXPECT_NEAR(pressure, 7864.35f, 1e-2f);
  }
}

TEST(MS8607Test, Category5_DynamicSwingsAndEnvironmentalCrossStress) {
  auto calibration_values = create_calibration_values();

  // 1. High Altitude / Severe Cold (e.g., Weather Balloon Profile)
  {
    uint32_t const d1 = 10000;
    uint32_t const d2_raw_temperature = 1500000;
    float const d_rh = 100.0f;

    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);
    float const hum = TestableMS8607Component::compensated_humidity(d_rh * 16.0f, temp_res.temperature_float);

    EXPECT_NEAR(temp_res.temperature_float, -351.94f, 1e-2f);
    EXPECT_NEAR(pressure, 1469.86f, 1e-2f);
    EXPECT_NEAR(hum, -69.9219f, 1e-2f);
  }

  // 2. Hyperbaric Chamber / Tropical Heat Wave
  {
    uint32_t const d1 = 15000000;
    uint32_t const d2_raw_temperature = 11500000;
    float const d_rh = 3900.0f;

    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);
    float const hum = TestableMS8607Component::compensated_humidity(d_rh * 16.0f, temp_res.temperature_float);

    EXPECT_NEAR(temp_res.temperature_float, 132.77f, 1e-2f);
    EXPECT_NEAR(pressure, 6115.92f, 1e-2f);
    EXPECT_NEAR(hum, 132.365f, 1e-2f);
  }

  // 3. Rapid Freeze-Drying State Transition
  {
    uint32_t const d1 = 50000;
    uint32_t const d2_raw_temperature = 2500000;
    float const d_rh = 3800.0f;

    auto temp_res = TestableMS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    float const pressure = TestableMS8607Component::compensated_pressure(d1, calibration_values, temp_res);
    float const hum = TestableMS8607Component::compensated_humidity(d_rh * 16.0f, temp_res.temperature_float);

    EXPECT_NEAR(temp_res.temperature_float, -275.91f, 1e-2f);
    EXPECT_NEAR(pressure, 585.25f, 1e-2f);
    EXPECT_NEAR(hum, 55.7752f, 1e-2f);
  }
}

}  // namespace esphome::ms8607::testing
