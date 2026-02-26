#include <gtest/gtest.h>
#include <ostream>
#include "esphome/components/bthome/bthome_decoder.h"
#include "esphome/core/helpers.h"

namespace esphome::bthome::testing {

static constexpr size_t MAX_PAYLOAD_SIZE = 5;

struct SensorTestCase {
  const char *name;
  const char *hex;  // uppercase hex payload: type byte + data bytes
  float expected;
  float tolerance;
  size_t len;
  uint8_t payload[MAX_PAYLOAD_SIZE];

  SensorTestCase(const char *name, const char *hex, float expected, float tolerance)
      : name(name), hex(hex), expected(expected), tolerance(tolerance), len(strlen(hex) / 2), payload{} {
    parse_hex(hex, payload, len);
  }
};

void PrintTo(const SensorTestCase &tc, std::ostream *os) {  // NOLINT(readability-identifier-naming)
  BTHomePayloadDecoder decoder(tc.payload, tc.len);
  auto it = decoder.begin();
  float actual = (it != decoder.end()) ? (*it).as_float() : 0.0f;
  *os << tc.name << " " << tc.hex << " (actual=" << actual << ", expected=" << tc.expected << " ±" << tc.tolerance
      << ")";
}

// clang-format off
// Test vectors from the BTHome specification table in bthome.py.
static const SensorTestCase TEST_CASES[] = {
    {"acceleration_u16",    "518756",     22.151f,        0.001f  },
    {"acceleration_i32_e6", "630057D0FF", -3.123456f,     0.0001f },
    {"battery",             "0161",       97.0f,          0.001f  },
    {"channel",             "6001",       1.0f,           0.001f  },
    {"co2",                 "12E204",     1250.0f,        0.1f    },
    {"conductivity",        "56E803",     1000.0f,        0.1f    },
    {"count_u8",            "0960",       96.0f,          0.001f  },
    {"count_u16",           "3D0960",     24585.0f,       1.0f    },
    {"count_u32",           "3E2A2C0960", 1611213866.0f,  128.0f  },
    {"count_i8",            "59EA",       -22.0f,         0.001f  },
    {"count_i16",           "5AEAEA",     -5398.0f,       1.0f    },
    {"count_i32",           "5BEA0234EA", -365690134.0f,  32.0f   },
    {"current_u16",         "434E34",     13.39f,         0.001f  },
    {"current_i16",         "5D02EA",     -5.63f,         0.001f  },
    {"dewpoint",            "08CA06",     17.38f,         0.001f  },
    {"direction",           "5E9F8C",     359.99f,        0.01f   },
    {"distance_mm",         "400C00",     12.0f,          0.001f  },
    {"distance_m",          "414E00",     7.8f,           0.01f   },
    {"duration",            "424E3400",   13.39f,         0.001f  },
    {"energy_kwh_u24",      "0A138A14",   1346.067f,      0.01f   },
    {"energy_kwh_u32",      "4D12138A14", 344593.17f,     0.5f    },
    {"gas_m3_u24",          "4B138A14",   1346.067f,      0.01f   },
    {"gas_m3_u32",          "4C41018A01", 25821.505f,     0.01f   },
    {"gyroscope",           "528756",     22.151f,        0.001f  },
    {"humidity_e2",         "03BF13",     50.55f,         0.01f   },
    {"humidity_u8",         "2E23",       35.0f,          0.001f  },
    {"illuminance",         "05138A14",   13460.67f,      0.1f    },
    {"mass_kg",             "065E1F",     80.3f,          0.01f   },
    {"mass_lb",             "073E1D",     74.86f,         0.01f   },
    {"moisture_e2",         "14020C",     30.74f,         0.01f   },
    {"moisture_u8",         "2F23",       35.0f,          0.001f  },
    {"pm25",                "0D120C",     3090.0f,        1.0f    },
    {"pm10",                "0E021C",     7170.0f,        1.0f    },
    {"power_u24",           "0B021B00",   69.14f,         0.01f   },
    {"power_i32",           "5C02FBFFFF", -12.78f,        0.01f   },
    {"precipitation",       "5FA00F",     400.0f,         0.1f    },
    {"pressure",            "04138A01",   1008.83f,       0.01f   },
    {"rotation",            "3F020C",     307.4f,         0.01f   },
    {"rotational_speed",    "61AC0D",     3500.0f,        1.0f    },
    {"speed_u16",           "444E34",     133.9f,         0.01f   },
    {"speed_i32_e6",        "624099DFFF", -2.123456f,     0.0001f },
    {"temperature_i8",      "57EA",       -22.0f,         0.001f  },
    {"temperature_i8_0_35", "58EA",       -7.7f,          0.01f   },
    {"temperature_e1",      "451101",     27.3f,          0.01f   },
    {"temperature_e2",      "02CA09",     25.06f,         0.001f  },
    {"timestamp",           "505D396164", 1684093277.0f,  128.0f  },
    {"tvoc",                "133301",     307.0f,         0.1f    },
    {"uv_index",            "4632",       5.0f,           0.01f   },
    {"voltage_e3",          "0C020C",     3.074f,         0.001f  },
    {"voltage_e1",          "4A020C",     307.4f,         0.1f    },
    {"volume_l_e1",         "478756",     2215.1f,        0.1f    },
    {"volume_ml",           "48DC87",     34780.0f,       1.0f    },
    {"volume_u32",          "4E87562A01", 19551.879f,     0.01f   },
    {"volume_storage",      "5587562A01", 19551.879f,     0.01f   },
    {"volume_flow_rate",    "49DC87",     34.78f,         0.001f  },
    {"water",               "4F87562A01", 19551.879f,     0.01f   },
};
// clang-format on

class BTHomeDecoderTest : public ::testing::TestWithParam<SensorTestCase> {};

TEST_P(BTHomeDecoderTest, AsFloat) {
  const SensorTestCase &tc = GetParam();
  BTHomePayloadDecoder decoder(tc.payload, tc.len);
  auto it = decoder.begin();
  ASSERT_NE(it, decoder.end());
  EXPECT_NEAR((*it).as_float(), tc.expected, tc.tolerance);
}

INSTANTIATE_TEST_SUITE_P(BTHomeSpec, BTHomeDecoderTest, ::testing::ValuesIn(TEST_CASES),
                         [](const ::testing::TestParamInfo<SensorTestCase> &info) {
                           return std::string(info.param.name);
                         });

struct BinarySensorTestCase {
  const char *name;
  const char *hex;  // uppercase hex payload: type byte + data byte
  bool expected;
  size_t len;
  uint8_t payload[MAX_PAYLOAD_SIZE];

  BinarySensorTestCase(const char *name, const char *hex, bool expected)
      : name(name), hex(hex), expected(expected), len(strlen(hex) / 2), payload{} {
    parse_hex(hex, payload, len);
  }
};

void PrintTo(const BinarySensorTestCase &tc, std::ostream *os) {  // NOLINT(readability-identifier-naming)
  BTHomePayloadDecoder decoder(tc.payload, tc.len);
  auto it = decoder.begin();
  bool actual = (it != decoder.end()) ? (*it).as_bool() : false;
  *os << tc.name << " " << tc.hex << " (actual=" << actual << ", expected=" << tc.expected << ")";
}

// clang-format off
static const BinarySensorTestCase BINARY_TEST_CASES[] = {
    {"battery_low",      "1501", true  },
    {"battery_charging", "1601", true  },
    {"carbon_monoxide",  "1700", false },
    {"cold",             "1801", true  },
    {"connectivity",     "1900", false },
    {"door",             "1A00", false },
    {"garage_door",      "1B01", true  },
    {"gas",              "1C01", true  },
    {"generic_boolean",  "0F01", true  },
    {"heat",             "1D00", false },
    {"light",            "1E01", true  },
    {"lock",             "1F01", true  },
    {"moisture",         "2001", true  },
    {"motion",           "2100", false },
    {"moving",           "2201", true  },
    {"occupancy",        "2301", true  },
    {"opening",          "1100", false },
    {"plug",             "2400", false },
    {"power",            "1001", true  },
    {"presence",         "2500", false },
    {"problem",          "2601", true  },
    {"running",          "2701", true  },
    {"safety",           "2800", false },
    {"smoke",            "2901", true  },
    {"sound",            "2A00", false },
    {"tamper",           "2B00", false },
    {"vibration",        "2C01", true  },
    {"window",           "2D01", true  },
};
// clang-format on

class BTHomeBinaryDecoderTest : public ::testing::TestWithParam<BinarySensorTestCase> {};

TEST_P(BTHomeBinaryDecoderTest, AsBool) {
  const BinarySensorTestCase &tc = GetParam();
  BTHomePayloadDecoder decoder(tc.payload, tc.len);
  auto it = decoder.begin();
  ASSERT_NE(it, decoder.end());
  EXPECT_EQ((*it).as_bool(), tc.expected);
}

INSTANTIATE_TEST_SUITE_P(BTHomeSpec, BTHomeBinaryDecoderTest, ::testing::ValuesIn(BINARY_TEST_CASES),
                         [](const ::testing::TestParamInfo<BinarySensorTestCase> &info) {
                           return std::string(info.param.name);
                         });

}  // namespace esphome::bthome::testing
