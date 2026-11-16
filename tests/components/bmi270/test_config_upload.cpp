#ifdef USE_HOST

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <vector>
#include "esphome/components/bmi270/bmi270.h"
#include "esphome/components/bmi270/bmi270_config.h"

namespace esphome::bmi270::testing {

// Host builds upload in 256-byte bursts
static constexpr size_t MAX_BURST = 256;

// A BMI270 register file: writes go to consecutive registers, except INIT_DATA, which streams into
// the config memory at the word address set in INIT_ADDR_0/1
class Bmi270Bus final : public i2c::I2CBus {
 public:
  std::array<uint8_t, 128> regs{};
  std::vector<uint8_t> config_memory = std::vector<uint8_t>(sizeof(BMI270_CONFIG_FILE));
  unsigned upload_writes{0};

  Bmi270Bus() {
    this->regs[BMI270_REG_CHIP_ID] = BMI270_CHIP_ID_VALUE;
    this->regs[BMI270_REG_INTERNAL_STATUS] = BMI270_INTERNAL_STATUS_INIT_OK;
  }

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    if (size == 0)
      return i2c::ERROR_INVALID_ARGUMENT;
    uint8_t reg = data[0];
    if (size > 1) {
      if (reg == BMI270_REG_INIT_ADDR_0 || reg == BMI270_REG_INIT_ADDR_0 + 1 || reg == BMI270_REG_INIT_DATA)
        ++this->upload_writes;
      if (reg == BMI270_REG_INIT_DATA) {
        size_t offset =
            2 * ((this->regs[BMI270_REG_INIT_ADDR_0] & 0x0F) | (this->regs[BMI270_REG_INIT_ADDR_0 + 1] << 4));
        if (offset + size - 1 > this->config_memory.size())
          return i2c::ERROR_INVALID_ARGUMENT;
        std::memcpy(this->config_memory.data() + offset, data + 1, size - 1);
      } else {
        for (size_t i = 1; i < size; i++)
          this->regs[(reg + i - 1) & 0x7F] = data[i];
      }
    }
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->regs[(reg + i) & 0x7F];
    return i2c::ERROR_OK;
  }
};

TEST(BMI270ConfigUpload, UploadsTheWholeConfigFileInOrder) {
  Bmi270Bus bus;
  BMI270Component imu;
  imu.set_i2c_bus(&bus);
  imu.set_i2c_address(0x68);
  imu.setup();

  EXPECT_FALSE(imu.is_failed());
  EXPECT_EQ(0, std::memcmp(bus.config_memory.data(), BMI270_CONFIG_FILE, sizeof(BMI270_CONFIG_FILE)));
  EXPECT_EQ(bus.regs[BMI270_REG_INIT_CTRL], BMI270_INIT_CTRL_LOAD_DONE);
  // One INIT_ADDR write and one INIT_DATA write per burst
  EXPECT_EQ(bus.upload_writes, 2 * ((sizeof(BMI270_CONFIG_FILE) + MAX_BURST - 1) / MAX_BURST));
}

}  // namespace esphome::bmi270::testing

#endif
