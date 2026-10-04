#include <cstring>
#include <gtest/gtest.h>
#include "esphome/components/ld2450/polygon.h"

namespace esphome::ld2450::testing {

static bool parse(Polygon &polygon, const char *str) { return polygon.parse(str, strlen(str)); }

static std::string format(const Polygon &polygon) {
  char buf[POLYGON_STR_SIZE];
  polygon.format(buf);
  return buf;
}

// --- Parsing ---

TEST(LD2450PolygonTest, ParseValid) {
  Polygon polygon;
  ASSERT_TRUE(parse(polygon, "-1000,500;1000,500;0,3000"));
  ASSERT_EQ(polygon.count, 3);
  EXPECT_EQ(polygon.points[0].x, -1000);
  EXPECT_EQ(polygon.points[0].y, 500);
  EXPECT_EQ(polygon.points[2].x, 0);
  EXPECT_EQ(polygon.points[2].y, 3000);
  EXPECT_EQ(format(polygon), "-1000,500;1000,500;0,3000");
}

TEST(LD2450PolygonTest, ParseAllowsSpacesAndTrailingSeparator) {
  Polygon polygon;
  ASSERT_TRUE(parse(polygon, " -1000 , 500 ; 1000,500;\t0,3000; "));
  EXPECT_EQ(format(polygon), "-1000,500;1000,500;0,3000");
}

TEST(LD2450PolygonTest, ParseEmptyDisablesZone) {
  Polygon polygon;
  ASSERT_TRUE(parse(polygon, "-1000,500;1000,500;0,3000"));
  ASSERT_TRUE(parse(polygon, ""));
  EXPECT_TRUE(polygon.empty());
  ASSERT_TRUE(parse(polygon, "-1000,500;1000,500;0,3000"));
  ASSERT_TRUE(parse(polygon, "   "));
  EXPECT_TRUE(polygon.empty());
  EXPECT_EQ(format(polygon), "");
}

TEST(LD2450PolygonTest, ParseLimits) {
  Polygon polygon;
  EXPECT_TRUE(parse(polygon, "-4860,0;4860,0;4860,7560"));
  EXPECT_FALSE(parse(polygon, "-4861,0;4860,0;4860,7560"));
  EXPECT_FALSE(parse(polygon, "-4860,0;4861,0;4860,7560"));
  EXPECT_FALSE(parse(polygon, "-4860,-1;4860,0;4860,7560"));
  EXPECT_FALSE(parse(polygon, "-4860,0;4860,0;4860,7561"));
  EXPECT_FALSE(parse(polygon, "-4860,0;4860,0;4860,99999999999"));
}

TEST(LD2450PolygonTest, ParsePointCount) {
  Polygon polygon;
  EXPECT_FALSE(parse(polygon, "0,0"));
  EXPECT_FALSE(parse(polygon, "0,0;100,100"));

  std::string text;
  for (int i = 0; i < MAX_POLYGON_POINTS; i++) {
    text += "-4860,7560;";
  }
  ASSERT_TRUE(parse(polygon, text.c_str()));
  EXPECT_EQ(polygon.count, MAX_POLYGON_POINTS);
  // The longest possible polygon fits in the format buffer
  EXPECT_EQ(format(polygon) + ";", text);

  text += "0,0";
  EXPECT_FALSE(parse(polygon, text.c_str()));
}

TEST(LD2450PolygonTest, ParseInvalidKeepsPreviousPolygon) {
  Polygon polygon;
  ASSERT_TRUE(parse(polygon, "-1000,500;1000,500;0,3000"));
  const char *invalid[] = {
      "junk", "1,2;3", "1,2;3,4;5,", "1;2;3", "1,2 3,4 5,6", "1,2;;3,4;5,6", "--1,2;3,4;5,6", "1,2;3,4;5,6x",
  };
  for (const char *str : invalid) {
    EXPECT_FALSE(parse(polygon, str)) << str;
    EXPECT_EQ(format(polygon), "-1000,500;1000,500;0,3000") << str;
  }
}

TEST(LD2450PolygonTest, IsValid) {
  Polygon polygon;
  EXPECT_TRUE(polygon.is_valid());
  polygon.count = 2;
  EXPECT_FALSE(polygon.is_valid());
  polygon.count = MAX_POLYGON_POINTS + 1;
  EXPECT_FALSE(polygon.is_valid());
  polygon.count = 3;
  EXPECT_TRUE(polygon.is_valid());
  polygon.points[1].y = -5;
  EXPECT_FALSE(polygon.is_valid());
}

// --- Point in polygon ---

TEST(LD2450PolygonTest, ContainsEmpty) {
  Polygon polygon;
  EXPECT_FALSE(polygon.contains(0, 0));
  EXPECT_FALSE(polygon.contains(100, 1000));
}

TEST(LD2450PolygonTest, ContainsSquare) {
  Polygon polygon;
  ASSERT_TRUE(parse(polygon, "-1000,1000;1000,1000;1000,3000;-1000,3000"));
  EXPECT_TRUE(polygon.contains(0, 2000));
  EXPECT_TRUE(polygon.contains(-999, 1001));
  EXPECT_TRUE(polygon.contains(999, 2999));
  EXPECT_FALSE(polygon.contains(0, 999));
  EXPECT_FALSE(polygon.contains(0, 3001));
  EXPECT_FALSE(polygon.contains(-1001, 2000));
  EXPECT_FALSE(polygon.contains(1001, 2000));
}

TEST(LD2450PolygonTest, ContainsWithReversedWinding) {
  Polygon clockwise;
  Polygon counter_clockwise;
  ASSERT_TRUE(parse(clockwise, "0,1000;2000,3000;-2000,3000"));
  ASSERT_TRUE(parse(counter_clockwise, "-2000,3000;2000,3000;0,1000"));
  for (int16_t x = -2500; x <= 2500; x += 250) {
    for (int16_t y = 500; y <= 3500; y += 250) {
      EXPECT_EQ(clockwise.contains(x, y), counter_clockwise.contains(x, y)) << x << "," << y;
    }
  }
  EXPECT_TRUE(clockwise.contains(0, 2000));
  EXPECT_FALSE(clockwise.contains(1500, 2000));
}

TEST(LD2450PolygonTest, ContainsConcave) {
  // U shape: the notch between the two arms is outside
  Polygon polygon;
  ASSERT_TRUE(parse(polygon, "-3000,1000;3000,1000;3000,4000;1000,4000;1000,2000;-1000,2000;-1000,4000;-3000,4000"));
  EXPECT_TRUE(polygon.contains(-2000, 3000));
  EXPECT_TRUE(polygon.contains(2000, 3000));
  EXPECT_TRUE(polygon.contains(0, 1500));
  EXPECT_FALSE(polygon.contains(0, 3000));
  EXPECT_FALSE(polygon.contains(0, 4500));
}

TEST(LD2450PolygonTest, ContainsFarOutsideRadarArea) {
  // The radar reports 15-bit coordinates; the cross products must not overflow
  Polygon polygon;
  ASSERT_TRUE(parse(polygon, "-4860,0;4860,0;4860,7560;-4860,7560"));
  EXPECT_FALSE(polygon.contains(32767, 32767));
  EXPECT_FALSE(polygon.contains(-32767, -32767));
  EXPECT_FALSE(polygon.contains(-32767, 3000));
  EXPECT_TRUE(polygon.contains(0, 3000));
}

}  // namespace esphome::ld2450::testing
