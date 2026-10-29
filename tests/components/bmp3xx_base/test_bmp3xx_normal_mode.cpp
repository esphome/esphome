#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>

#include "esphome/components/bmp3xx_base/bmp3xx_base.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/application.h"

namespace esphome::bmp3xx_base::testing {
namespace {

constexpr uint8_t INT_STATUS_DRDY = 0x08;

// A BMP390 register file in memory. Like the chip, reading INT_STATUS clears drdy.
class NormalModeBMP3XX final : public BMP3XXComponent {
 public:
  NormalModeBMP3XX() {
    this->registers[BMP388_CHIP_ID] = BMP390_ID;
    this->registers[BMP388_EVENT] = 0x01;  // por_detected
  }

  // The chip lost power: it is asleep with its power-on settings and empty data registers
  void power_on_reset() {
    this->registers[BMP388_PWR_CTRL] = 0x00;
    this->registers[BMP388_OSR] = 0x02;
    this->registers[BMP388_ODR] = 0x00;
    this->registers[BMP388_CONFIG] = 0x00;
    this->registers[BMP388_INT_STATUS] = 0x00;
    this->registers[BMP388_DATA_2] = 0x80;
    this->registers[BMP388_DATA_5] = 0x80;
  }

  std::array<uint8_t, 256> registers{};
  bool fail_writes{false};

 protected:
  bool read_byte(uint8_t a_register, uint8_t *data) override {
    *data = this->registers[a_register];
    if (a_register == BMP388_INT_STATUS)
      this->registers[a_register] = 0;
    return true;
  }
  bool write_byte(uint8_t a_register, uint8_t data) override {
    if (this->fail_writes)
      return false;
    this->registers[a_register] = data;
    return true;
  }
  bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) override {
    std::memcpy(data, &this->registers[a_register], len);
    return true;
  }
  bool write_bytes(uint8_t a_register, uint8_t *data, size_t len) override {
    if (this->fail_writes)
      return false;
    std::memcpy(&this->registers[a_register], data, len);
    return true;
  }
};

struct NormalModeFixture {
  NormalModeBMP3XX bmp;
  sensor::Sensor temperature;
  int published{0};

  NormalModeFixture() {
    static char name[] = "test_bmp3xx_normal_mode";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    this->temperature.add_on_state_callback([this](float) { this->published++; });
    this->bmp.set_temperature_sensor(&this->temperature);
    this->bmp.set_output_data_rate_config(ODR_12_5_HZ);
    this->bmp.setup();
  }

  // In normal mode update() reads and publishes the latest result itself, with nothing scheduled
  void update_and_read() { this->bmp.update(); }
};

TEST(BMP3XXNormalModeTest, SetupStartsNormalModeAtDataRate) {
  NormalModeFixture f;
  EXPECT_FALSE(f.bmp.is_failed());
  EXPECT_EQ(f.bmp.registers[BMP388_ODR], ODR_12_5_HZ);
  EXPECT_EQ((f.bmp.registers[BMP388_PWR_CTRL] >> 4) & 0x03, NORMAL_MODE);
}

TEST(BMP3XXNormalModeTest, KeepsReadingWhenNoNewResultSinceLastPoll) {
  NormalModeFixture f;
  f.bmp.registers[BMP388_INT_STATUS] = INT_STATUS_DRDY;
  f.update_and_read();
  ASSERT_EQ(f.published, 1);

  // drdy was cleared by that read and no new result has arrived yet
  f.update_and_read();
  EXPECT_EQ(f.published, 2);
  EXPECT_FALSE(f.bmp.status_has_warning());
}

TEST(BMP3XXNormalModeTest, WaitsForFirstResultAfterSetup) {
  NormalModeFixture f;
  f.update_and_read();
  EXPECT_EQ(f.published, 0);
  EXPECT_TRUE(f.bmp.status_has_warning());

  f.bmp.registers[BMP388_INT_STATUS] = INT_STATUS_DRDY;
  f.update_and_read();
  EXPECT_EQ(f.published, 1);
  EXPECT_FALSE(f.bmp.status_has_warning());
}

TEST(BMP3XXNormalModeTest, SensorResetIsDetectedAndSettingsRestored) {
  NormalModeFixture f;
  const uint8_t osr = f.bmp.registers[BMP388_OSR];
  f.bmp.registers[BMP388_INT_STATUS] = INT_STATUS_DRDY;
  f.update_and_read();
  ASSERT_EQ(f.published, 1);

  // Nothing is published from the reset chip's empty data registers
  f.bmp.power_on_reset();
  f.update_and_read();
  EXPECT_EQ(f.published, 1);
  EXPECT_TRUE(f.bmp.status_has_warning());
  EXPECT_EQ(f.bmp.registers[BMP388_OSR], osr);
  EXPECT_EQ(f.bmp.registers[BMP388_ODR], ODR_12_5_HZ);
  EXPECT_EQ((f.bmp.registers[BMP388_PWR_CTRL] >> 4) & 0x03, NORMAL_MODE);

  // The first result with the restored settings is published again
  f.bmp.registers[BMP388_INT_STATUS] = INT_STATUS_DRDY;
  f.update_and_read();
  EXPECT_EQ(f.published, 2);
  EXPECT_FALSE(f.bmp.status_has_warning());
}

TEST(BMP3XXNormalModeTest, FailedRestoreIsRetried) {
  NormalModeFixture f;
  f.bmp.registers[BMP388_INT_STATUS] = INT_STATUS_DRDY;
  f.update_and_read();
  ASSERT_EQ(f.published, 1);

  f.bmp.power_on_reset();
  f.bmp.fail_writes = true;
  f.update_and_read();
  EXPECT_EQ((f.bmp.registers[BMP388_PWR_CTRL] >> 4) & 0x03, SLEEP_MODE);

  f.bmp.fail_writes = false;
  f.update_and_read();
  EXPECT_EQ((f.bmp.registers[BMP388_PWR_CTRL] >> 4) & 0x03, NORMAL_MODE);
  EXPECT_EQ(f.published, 1);

  f.bmp.registers[BMP388_INT_STATUS] = INT_STATUS_DRDY;
  f.update_and_read();
  EXPECT_EQ(f.published, 2);
  EXPECT_FALSE(f.bmp.status_has_warning());
}

}  // namespace
}  // namespace esphome::bmp3xx_base::testing
