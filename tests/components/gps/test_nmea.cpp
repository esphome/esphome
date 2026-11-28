#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "esphome/components/gps/nmea.h"
#include "esphome/core/helpers.h"

namespace esphome::gps::testing {

// Adds the checksum to a sentence written without it.
static std::string with_checksum(const std::string &body) {
  const uint8_t sum = xor_bytes(reinterpret_cast<const uint8_t *>(body.data()) + 1, body.size() - 1);
  char suffix[4];
  snprintf(suffix, sizeof(suffix), "*%02X", sum);
  return body + suffix;
}

static bool parse(std::string line, GPSData &data) { return parse_nmea_sentence(line.data(), line.size(), data); }

TEST(GPSNmea, Gga) {
  GPSData data;
  ASSERT_TRUE(parse("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47", data));
  EXPECT_EQ(data.latitude_e7, 481173000);
  EXPECT_EQ(data.longitude_e7, 115166667);
  EXPECT_EQ(data.altitude_e3, 545400);
  EXPECT_EQ(data.geoid_separation_e3, 46900);
  EXPECT_EQ(data.satellites, 8);
  EXPECT_EQ(data.hdop_e2, 90);
  EXPECT_EQ(data.hour, 12);
  EXPECT_EQ(data.minute, 35);
  EXPECT_EQ(data.second, 19);
  EXPECT_EQ(data.updated, GPS_FIELD_TIME | GPS_FIELD_LOCATION | GPS_FIELD_SATELLITES | GPS_FIELD_HDOP |
                              GPS_FIELD_ALTITUDE | GPS_FIELD_GEOID_SEPARATION);
  EXPECT_EQ(data.valid, data.updated);
}

TEST(GPSNmea, Rmc) {
  GPSData data;
  ASSERT_TRUE(parse("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A", data));
  EXPECT_EQ(data.latitude_e7, 481173000);
  EXPECT_EQ(data.longitude_e7, 115166667);
  EXPECT_EQ(data.speed_e3, 22400u);
  EXPECT_FLOAT_EQ(data.speed_kmh(), 41.4848f);
  EXPECT_EQ(data.course_e2, 8440);
  EXPECT_EQ(data.day, 23);
  EXPECT_EQ(data.month, 3);
  EXPECT_EQ(data.year, 2094);
  EXPECT_EQ(data.updated, GPS_FIELD_TIME | GPS_FIELD_LOCATION | GPS_FIELD_SPEED | GPS_FIELD_COURSE | GPS_FIELD_DATE);
}

TEST(GPSNmea, SouthWestHighPrecision) {
  GPSData data;
  ASSERT_TRUE(parse(with_checksum("$GNGGA,000000.00,3351.1234567,S,15112.7654321,W,2,12,0.55,-12.345,M,,M,,"), data));
  EXPECT_EQ(data.latitude_e7, -338520576);
  EXPECT_EQ(data.longitude_e7, -1512127572);
  EXPECT_EQ(data.altitude_e3, -12345);
  EXPECT_EQ(data.satellites, 12);
  EXPECT_EQ(data.hdop_e2, 55);
}

TEST(GPSNmea, LeadingNoise) {
  static const std::string GGA = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
  for (const std::string &noise : {std::string("\x01\xff"), std::string("\x7f\0\x80", 3), std::string("$GPGGA,1235")}) {
    GPSData data;
    EXPECT_TRUE(parse(noise + GGA, data));
    EXPECT_EQ(data.satellites, 8);
  }
}

TEST(GPSNmea, NulInsideSentence) {
  GPSData data;
  EXPECT_FALSE(
      parse(std::string("$GPGGA,123519,4807.038,N,") + '\0' + "01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47", data));
}

// Each float is the one nearest the decimal value, as strtof() gives it
TEST(GPSNmea, FloatsRoundToNearest) {
  GPSData data;
  char text[24];
  for (int32_t e7 = -1800000000; e7 <= 1800000000; e7 += 3999991) {
    data.latitude_e7 = data.longitude_e7 = e7;
    snprintf(text, sizeof(text), "%s%d.%07d", e7 < 0 ? "-" : "", std::abs(e7 / 10000000), std::abs(e7 % 10000000));
    ASSERT_EQ(data.longitude(), strtof(text, nullptr)) << text;
    ASSERT_EQ(data.latitude(), strtof(text, nullptr)) << text;
  }
  for (uint32_t e3 = 0; e3 <= 200000; e3 += 7) {
    data.speed_e3 = e3;
    data.altitude_e3 = static_cast<int32_t>(e3);
    snprintf(text, sizeof(text), "%u.%03u", e3 / 1000, e3 % 1000);
    ASSERT_EQ(data.speed_knots(), strtof(text, nullptr)) << text;
    ASSERT_EQ(data.altitude(), strtof(text, nullptr)) << text;
    const uint32_t kmh_e6 = e3 * 1852;
    snprintf(text, sizeof(text), "%u.%06u", kmh_e6 / 1000000, kmh_e6 % 1000000);
    ASSERT_EQ(data.speed_kmh(), strtof(text, nullptr)) << text;
  }
  for (uint16_t e2 = 0; e2 <= 36000; e2++) {
    data.course_e2 = data.hdop_e2 = e2;
    snprintf(text, sizeof(text), "%u.%02u", e2 / 100, e2 % 100);
    ASSERT_EQ(data.course(), strtof(text, nullptr)) << text;
    ASSERT_EQ(data.hdop(), strtof(text, nullptr)) << text;
  }
}

TEST(GPSNmea, GgaWithoutFixKeepsLocationAndTime) {
  GPSData data;
  ASSERT_TRUE(parse("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47", data));
  ASSERT_TRUE(parse(with_checksum("$GPGGA,123520.00,,,,,0,00,99.99,,,,,,"), data));
  EXPECT_EQ(data.updated, GPS_FIELD_SATELLITES | GPS_FIELD_HDOP);
  EXPECT_TRUE(data.has(GPS_FIELD_LOCATION));
  EXPECT_EQ(data.latitude_e7, 481173000);
  EXPECT_EQ(data.altitude_e3, 545400);
  EXPECT_EQ(data.satellites, 0);
  EXPECT_EQ(data.hdop_e2, 9999);
  EXPECT_EQ(data.second, 19);
}

TEST(GPSNmea, RmcWithoutFixSetsNothing) {
  GPSData data;
  ASSERT_TRUE(parse(with_checksum("$GNRMC,090552.42,V,,,,,,,021225,,,N,V"), data));
  EXPECT_EQ(data.updated, 0);
  EXPECT_EQ(data.valid, 0);
}

TEST(GPSNmea, TimeFraction) {
  GPSData data;
  ASSERT_TRUE(parse(with_checksum("$GNGGA,090552.42,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"), data));
  EXPECT_EQ(data.second, 52);
  EXPECT_EQ(data.millisecond, 420);
  ASSERT_TRUE(parse(with_checksum("$GNGGA,090553.1239,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,"), data));
  EXPECT_EQ(data.millisecond, 123);
  ASSERT_TRUE(parse("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47", data));
  EXPECT_EQ(data.millisecond, 0);
}

TEST(GPSNmea, GeoidSeparation) {
  GPSData data;
  ASSERT_TRUE(parse(with_checksum("$GNGGA,090552.42,3351.123,S,15112.765,E,1,08,0.9,45.4,M,-12.25,M,,"), data));
  EXPECT_EQ(data.geoid_separation_e3, -12250);
  EXPECT_FLOAT_EQ(data.geoid_separation(), -12.25f);
  EXPECT_TRUE(data.is_updated(GPS_FIELD_GEOID_SEPARATION));
  ASSERT_TRUE(parse(with_checksum("$GNGGA,090553.42,3351.123,S,15112.765,E,1,08,0.9,45.4,M,,M,,"), data));
  EXPECT_FALSE(data.is_updated(GPS_FIELD_GEOID_SEPARATION));
  EXPECT_TRUE(data.has(GPS_FIELD_GEOID_SEPARATION));
  EXPECT_EQ(data.geoid_separation_e3, -12250);
}

TEST(GPSNmea, LeapDayAndLeapSecond) {
  GPSData data;
  ASSERT_TRUE(parse(with_checksum("$GNRMC,235960.00,A,4807.038,N,01131.000,E,0.0,,290228,,,A,V"), data));
  EXPECT_EQ(data.day, 29);
  EXPECT_EQ(data.month, 2);
  EXPECT_EQ(data.year, 2028);
  EXPECT_EQ(data.second, 60);
}

TEST(GPSNmea, RmcFixWithoutCourse) {
  GPSData data;
  ASSERT_TRUE(parse(with_checksum("$GNRMC,235959.00,A,4807.038,N,01131.000,E,0.010,,311226,,,A,V"), data));
  EXPECT_EQ(data.updated, GPS_FIELD_TIME | GPS_FIELD_LOCATION | GPS_FIELD_SPEED | GPS_FIELD_DATE);
  EXPECT_EQ(data.year, 2026);
  EXPECT_EQ(data.month, 12);
  EXPECT_EQ(data.day, 31);
  EXPECT_EQ(data.hour, 23);
  EXPECT_EQ(data.speed_e3, 10u);
}

TEST(GPSNmea, RejectedLines) {
  static constexpr const char *LINES[] = {
      "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*48",   // wrong checksum
      "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",      // no checksum
      "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4",    // short checksum
      "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47",    // no start
      "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*470",  // long checksum
      "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47x",  // junk after checksum
      "",
  };
  for (const char *line : LINES) {
    GPSData data;
    EXPECT_FALSE(parse(line, data)) << line;
    EXPECT_EQ(data.valid, 0) << line;
  }
}

TEST(GPSNmea, RejectedSentences) {
  static constexpr const char *BODIES[] = {
      "$GPGGA,123519,48A7.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",         // letter in latitude
      "$GPGGA,123519,4860.000,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",         // 60 minutes
      "$GPGGA,123519,9100.000,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",         // latitude over 90
      "$GPGGA,123519,4807.038,X,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",         // bad hemisphere
      "$GPGGA,123519,4807.038,N,,E,1,08,0.9,545.4,M,46.9,M,,",                  // latitude without longitude
      "$GPGGA,126019,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,",         // 60 minutes past the hour
      "$GPGGA,123519,4807.038,N,01131.000,E,1,300,0.9,545.4,M,46.9,M,,",        // satellites over 255
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,360.1,230394,003.1,W",      // course over 360
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,231394,003.1,W",      // month 13
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,2303945,003.1,W",     // long date
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,310226,003.1,W",      // 31 February
      "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,290225,003.1,W",      // 29 February 2025
      "$GPRMC,123519,AX,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W",     // bad status
      "$GPRMC,123519,A,4807.038",                                               // missing hemisphere
      "$GPRMC,123519,A,,,01131.000,E,022.4,084.4,230394,003.1,W",               // longitude without latitude
      "$GPRMC,123519.bad,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W",  // bad time fraction
      "$GPRMC,123519.,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W",     // empty time fraction
      "$GPGGA,123519,4807.038,N,01131.000,E,1.5,08,0.9,545.4,M,46.9,M,,",       // fractional quality
      "$GPGGA,123519,4807.038,N,01131.000,E,1,8.9,0.9,545.4,M,46.9,M,,",        // fractional satellites
      "$GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00",      // not handled
      "$PUBX,00,123519,4807.038,N,01131.000,E,545.4,G3",                        // proprietary
  };
  for (const char *body : BODIES) {
    GPSData data;
    EXPECT_FALSE(parse(with_checksum(body), data)) << body;
    EXPECT_EQ(data.valid, 0) << body;
  }
}

}  // namespace esphome::gps::testing
