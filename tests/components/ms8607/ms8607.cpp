#include <gtest/gtest.h>

#include "esphome/components/ms8607/ms8607.h"

namespace esphome::ms8607::testing {

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
static MS8607Component::CalibrationValues create_calibration_values(uint16_t c1 = 46372, uint16_t c2 = 43981,
                                                                    uint16_t c3 = 29059, uint16_t c4 = 27842,
                                                                    uint16_t c5 = 31553, uint16_t c6 = 28165) {
  MS8607Component::CalibrationValues values;
  values.pressure_sensitivity = c1;
  values.pressure_offset = c2;
  values.pressure_sensitivity_temperature_coefficient = c3;
  values.pressure_offset_temperature_coefficient = c4;
  values.reference_temperature = c5;
  values.temperature_coefficient_of_temperature = c6;
  return values;
}

TEST(MS8607Test, Category1TemperatureCriticalBoundaries) {
  auto calibration_values = create_calibration_values();
  uint32_t const reference_temperature = calibration_values.reference_temperature << 8;

  // Exact Reference Temperature (20°C Standard Baseline)
  {
    uint32_t const d2_raw_temperature = reference_temperature;
    auto res = MS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, 0);
    EXPECT_EQ(res.first_order_temperature, 2000);
    EXPECT_NEAR(res.temperature_float, 20.0f, 1e-3f);
  }

  // Just Above Crossover (20.01°C)
  {
    uint32_t const d2_raw_temperature = reference_temperature + 2000;
    auto res = MS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, 2000);
    EXPECT_EQ(res.first_order_temperature, 2006);
    EXPECT_NEAR(res.temperature_float, 20.06f, 1e-3f);
  }

  // Just Below Crossover (19.99°C)
  {
    uint32_t const d2_raw_temperature = reference_temperature - 2000;
    auto res = MS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, -2000);
    EXPECT_EQ(res.first_order_temperature, 1993);
    EXPECT_NEAR(res.temperature_float, 19.93f, 1e-3f);
  }

  // Maximum Specified Temperature Before 2nd Order (+84.32°C)
  {
    uint32_t const d2_raw_temperature = 10013516;
    auto res = MS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, 1935948);
    EXPECT_EQ(res.first_order_temperature, 8500);
    EXPECT_NEAR(res.temperature_float, 84.32f, 1e-3f);
  }

  // Maximum Specified Temperature (+85°C)
  {
    uint32_t const d2_raw_temperature = 10034215;
    auto res = MS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, 1956647);
    EXPECT_EQ(res.first_order_temperature, 8569);
    EXPECT_NEAR(res.temperature_float, 85.00f, 1e-3f);
  }

  // Minimum Specified Temperature (-40°C)
  {
    uint32_t const d2_raw_temperature = 6537387;
    auto res = MS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, -1540181);
    EXPECT_EQ(res.first_order_temperature, -3172);
    EXPECT_NEAR(res.temperature_float, -40.00f, 1e-3f);
  }

  // Minimum Specified Temperature Before 2nd Order (-51.15°C)
  // Note: I suspect this is out of range
  {
    uint32_t const d2_raw_temperature = 6290732;
    auto res = MS8607Component::compensated_temperature(d2_raw_temperature, calibration_values);
    EXPECT_EQ(res.d_t, -1786836);
    EXPECT_EQ(res.first_order_temperature, -4000);
    EXPECT_NEAR(res.temperature_float, -51.15f, 1e-3f);
  }
}

float test_pressure_helper(const uint32_t d1, const uint32_t d2,
                           const MS8607Component::CalibrationValues &calibration_values) {
  auto temp_res = MS8607Component::compensated_temperature(d2, calibration_values);
  return MS8607Component::compensated_pressure(d1, calibration_values, temp_res);
}

TEST(MS8607Test, Category2PressureCompensationMath) {
  auto calibration_values = create_calibration_values();

  // Standard Mid-Scale Barometric Pressure (~1013 mbar @ 20°C)
  {
    float const pressure =
        test_pressure_helper(6268671, calibration_values.reference_temperature << 8, calibration_values);
    EXPECT_NEAR(pressure, 1013.00f, 1e-2f);
  }

  // Lowest Pressure at Reference Temp (10 mbar @ 20°C)
  {
    float const pressure =
        test_pressure_helper(4000671, calibration_values.reference_temperature << 8, calibration_values);
    EXPECT_NEAR(pressure, 10.00f, 1e-2f);
  }

  // Highest Pressure at Reference Temp (1200 mbar @ 20°C)
  {
    float const pressure =
        test_pressure_helper(6691519, calibration_values.reference_temperature << 8, calibration_values);
    EXPECT_NEAR(pressure, 1200.00f, 1e-2f);
  }

  // Lowest Pressure at Lowest Temperature (10 mbar @ -40°C)
  {
    float const pressure = test_pressure_helper(3991039, 6537387, calibration_values);
    EXPECT_NEAR(pressure, 10.00f, 1e-2f);
  }

  // Sea Level Pressure at Lowest Temperature (1013 mbar @ -40°C)
  {
    float const pressure = test_pressure_helper(6626079, 6537387, calibration_values);
    EXPECT_NEAR(pressure, 1013.00f, 1e-2f);
  }

  // Highest Pressure at Lowest Temperature (1200 mbar @ -40°C)
  {
    float const pressure = test_pressure_helper(7117343, 6537387, calibration_values);
    EXPECT_NEAR(pressure, 1200.00f, 1e-2f);
  }

  // Lowest Pressure at Highest Temperature (10 mbar @ +85°C)
  {
    float const pressure = test_pressure_helper(4002959, 10034215, calibration_values);
    EXPECT_NEAR(pressure, 10.00f, 1e-2f);
  }

  // Sea Level Pressure at Highest Temperature (1013 mbar @ 85°C)
  {
    float const pressure = test_pressure_helper(5981727, 10034215, calibration_values);
    EXPECT_NEAR(pressure, 1013.00f, 1e-2f);
  }

  // Highest Pressure at Highest Temperature (1200 mbar @ +85°C)
  {
    float const pressure = test_pressure_helper(6350655, 10034215, calibration_values);
    EXPECT_NEAR(pressure, 1200.00f, 1e-2f);
  }

  // Lowest Pressure at Below Reference Temperature (10 mbar @ +0°C)
  {
    float const pressure = test_pressure_helper(3998959, 7514695, calibration_values);
    EXPECT_NEAR(pressure, 10.00f, 1e-2f);
  }

  // Sea Level Pressure at Below Reference Temperature (1013 mbar @ +0°C)
  {
    float const pressure = test_pressure_helper(6371807, 7514695, calibration_values);
    EXPECT_NEAR(pressure, 1013.00f, 1e-2f);
  }

  // Highest Pressure at at Below Reference Temperature (1200 mbar @ +0°C)
  {
    float const pressure = test_pressure_helper(6814191, 7514695, calibration_values);
    EXPECT_NEAR(pressure, 1200.00f, 1e-2f);
  }
}

TEST(MS8607Test, Category3HumidityTemperatureCompensation) {
  // Datasheet example: 0x7C80 (i.e. 31872) @ 20°C is 54.80%
  {
    float const humidity = MS8607Component::compensated_humidity(0x7C80, 20.0f);
    EXPECT_NEAR(humidity, 54.80f, 1e-2f);
  }

  // Datasheet example at lower temperature (0x7C80 @ 0°C)
  {
    float const humidity = MS8607Component::compensated_humidity(0x7C80, 0.0f);
    EXPECT_NEAR(humidity, 54.80f - 3.6f, 1e-2f);
  }

  // Datasheet example at higher temperature (0x7C80 @ 40°C)
  {
    float const humidity = MS8607Component::compensated_humidity(0x7C80, 40.0f);
    EXPECT_NEAR(humidity, 54.80f + 3.6f, 1e-2f);
  }

  // Lowest humidity, without temperature compensation
  {
    float const humidity = MS8607Component::compensated_humidity(0, 20.0f);
    EXPECT_NEAR(humidity, -6.0f, 1e-2f);
  }

  // Highest humidity, without temperature compensation
  {
    // bottom two bits are status bits, and are stripped by the caller
    float const humidity = MS8607Component::compensated_humidity(0xFFFC, 20.0f);
    EXPECT_NEAR(humidity, 118.99f, 1e-2f);
  }

  // 100% humidity at 20°
  {
    float const humidity = MS8607Component::compensated_humidity(55575, 20.0f);
    EXPECT_NEAR(humidity, 100.00f, 1e-2f);
  }

  // Humidity Compensation at High Temperature (100% RH @ +85°C)
  {
    float const humidity = MS8607Component::compensated_humidity(55575, 85.0f);
    EXPECT_NEAR(humidity, 111.70f, 1e-2f);
  }

  // Humidity Compensation at Extreme Low Temp (100% RH @ -40°C)
  {
    float const humidity = MS8607Component::compensated_humidity(55575, -40.0f);
    EXPECT_NEAR(humidity, 89.20f, 1e-2f);
  }

  // Low Humidity at Extreme Low Temp (0% RH @ -40°C)
  {
    float const humidity = MS8607Component::compensated_humidity(0, -40.0f);
    EXPECT_NEAR(humidity, -16.80f, 1e-2f);
  }
}

TEST(MS8607Test, Category4MathematicalEdgeCases) {
  auto calibration_values = create_calibration_values();

  // Minimum value accepted by read_* methods
  {
    // these values are below what the data sheet accepts, but should avoid Undefined Behavior
    auto temp_res = MS8607Component::compensated_temperature(6315741, calibration_values);
    float const pressure = MS8607Component::compensated_pressure(3961087, calibration_values, temp_res);
    float const humidity = MS8607Component::compensated_humidity(0, temp_res.temperature_float);

    EXPECT_NEAR(temp_res.temperature_float, -50.0f, 1e-2f);
    EXPECT_NEAR(pressure, 0.0f, 1e-2f);
    EXPECT_NEAR(humidity, -18.6f, 1e-2f);

    // Check humidity CRC, if we got all zeros on that read. The Humidity read should never be all
    // zeros, but if it is, this shows that the CRC would pass and the component should bail
    uint8_t bytes[3];
    bytes[0] = bytes[1] = bytes[2] = 0;
    uint8_t const actual_crc = crc8(bytes, 2, 0, 0x31, true);
    EXPECT_EQ(actual_crc, bytes[2]);
  }

  // Maximum values accepted by read_* methods
  {
    // these values are above what the data sheet accepts, but should avoid Undefined Behavior
    auto temp_res = MS8607Component::compensated_temperature(10491904, calibration_values);
    float const pressure = MS8607Component::compensated_pressure(6857823, calibration_values, temp_res);
    float const hum = MS8607Component::compensated_humidity(0xFFFC, temp_res.temperature_float);

    EXPECT_NEAR(temp_res.temperature_float, 100.0f, 1e-2f);
    EXPECT_NEAR(pressure, 1500.0f, 1e-2f);
    EXPECT_NEAR(hum, 133.39f, 1e-2f);
  }
}

}  // namespace esphome::ms8607::testing
