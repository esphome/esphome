#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "esphome/components/cst226/touchscreen/cst226_touchscreen.h"
#include "../common.h"

namespace esphome::cst226::testing {

TEST(CST226TouchscreenTest, SetupCompletes) {
  StatusBus bus;
  CST226Touchscreen hub;
  hub.set_i2c_bus(&bus);
  hub.setup();
  EXPECT_TRUE(hub.can_proceed());
  EXPECT_FALSE(hub.is_failed());
}

// Records the writes that carry no read, and answers reads through answer_setup_read().
class WriteLogBus final : public i2c::I2CBus {
 public:
  std::vector<std::vector<uint8_t>> writes;
  bool bad_check_code{false};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    if (out_size == 0) {
      this->writes.emplace_back(data, data + size);
    } else {
      answer_setup_read(data, size, out, out_size);
      if (this->bad_check_code)
        std::memset(out, 0, out_size);
    }
    return i2c::ERROR_OK;
  }
};

TEST(CST226TouchscreenTest, SetupEntersCommandModeAndLeavesIt) {
  WriteLogBus bus;
  CST226Touchscreen hub;
  hub.set_i2c_bus(&bus);
  hub.setup();
  ASSERT_GE(bus.writes.size(), 2u);
  EXPECT_EQ(bus.writes.front(), (std::vector<uint8_t>{0xD1, 0x01}));
  EXPECT_EQ(bus.writes.back(), (std::vector<uint8_t>{0xD1, 0x09}));
  EXPECT_FALSE(hub.is_failed());
}

TEST(CST226TouchscreenTest, SetupFailsOnAWrongCheckCode) {
  WriteLogBus bus;
  bus.bad_check_code = true;
  CST226Touchscreen hub;
  hub.set_i2c_bus(&bus);
  hub.setup();
  EXPECT_TRUE(hub.is_failed());
}

TEST(CST226TouchscreenTest, SetupFailsWithoutTheChip) {
  StatusBus bus;
  bus.fail = true;
  CST226Touchscreen hub;
  hub.set_i2c_bus(&bus);
  hub.setup();
  EXPECT_TRUE(hub.is_failed());
}

}  // namespace esphome::cst226::testing
