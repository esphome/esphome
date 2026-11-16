#include <gtest/gtest.h>

#include <cmath>
#include "esphome/components/bmi270/bmm150_aux.h"

namespace esphome::bmi270::bmm150::testing {

// The axis decode before it used encode_value_le, kept as the reference
static BMM150Data reference_convert(const BMM150Trim &trim, const uint8_t *raw) {
  int16_t x = (int16_t) (((int16_t) (int8_t) raw[1] * 32) | (raw[0] >> 3));
  int16_t y = (int16_t) (((int16_t) (int8_t) raw[3] * 32) | (raw[2] >> 3));
  int16_t z = (int16_t) (((int16_t) (int8_t) raw[5] * 128) | (raw[4] >> 1));
  uint16_t rhall = (uint16_t) (((uint16_t) raw[7] << 6) | (raw[6] >> 2));
  return BMM150Data{
      .x = bmm150_compensate_xy(trim, trim.x1, trim.x2, x, rhall),
      .y = bmm150_compensate_xy(trim, trim.y1, trim.y2, y, rhall),
      .z = bmm150_compensate_z(trim, z, rhall),
  };
}

static bool same(float a, float b) { return (std::isnan(a) && std::isnan(b)) || a == b; }

TEST(BMM150Convert, MatchesTheReferenceForEveryWordOfEachField) {
  const BMM150Trim trim{.x1 = 0,
                        .y1 = 0,
                        .x2 = 26,
                        .y2 = 26,
                        .xy2 = -3,
                        .xy1 = 29,
                        .z2 = 760,
                        .z3 = 0,
                        .z4 = 0,
                        .z1 = 24786,
                        .xyz1 = 7101};
  // Sweep one field through every 16-bit value while the others hold mid-range readings
  for (int field = 0; field < 4; field++) {
    for (uint32_t word = 0; word <= 0xFFFF; word++) {
      uint8_t raw[8] = {0x10, 0x12, 0x20, 0xF0, 0x30, 0x08, 0x40, 0x1A};
      raw[2 * field] = word & 0xFF;
      raw[2 * field + 1] = word >> 8;
      const BMM150Data got = bmm150_convert(trim, raw);
      const BMM150Data want = reference_convert(trim, raw);
      ASSERT_TRUE(same(got.x, want.x) && same(got.y, want.y) && same(got.z, want.z))
          << "field " << field << " word 0x" << std::hex << word;
    }
  }
}

}  // namespace esphome::bmi270::bmm150::testing
