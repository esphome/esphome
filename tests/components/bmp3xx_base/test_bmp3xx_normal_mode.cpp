#include <gtest/gtest.h>

#include <array>
#include <cmath>
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
    // Nonzero trim parameters, so the readings go through the compensation
    for (int i = 0; i < 21; i++)
      this->registers[BMP388_TRIM_PARAMS + i] = 0x11 * (i % 7 + 1);
    this->registers[BMP388_EVENT] = 0x01;  // por_detected
  }

  // The chip lost power: it is asleep with its power-on settings and empty data registers
  void power_on_reset() {
    this->registers[BMP388_PWR_CTRL] = 0x00;
    this->registers[BMP388_OSR] = 0x02;
    this->registers[BMP388_ODR] = 0x00;
    this->registers[BMP388_CONFIG] = 0x00;
    this->registers[BMP388_INT_STATUS] = 0x00;
    std::memset(&this->registers[BMP388_DATA_0], 0, 6);
  }

  // Raw 24-bit pressure and temperature readings, as the chip stores them
  void set_raw(uint32_t pressure, uint32_t temperature) {
    for (int i = 0; i < 3; i++) {
      this->registers[BMP388_DATA_0 + i] = pressure >> (8 * i);
      this->registers[BMP388_DATA_3 + i] = temperature >> (8 * i);
    }
  }

  // The pressure the component should publish for these raw readings
  float pressure_for(uint32_t pressure, uint32_t temperature) {
    return this->bmp388_compensate_pressure_(pressure, this->bmp388_compensate_temperature_(temperature)) / 100.0f;
  }

  uint8_t mode() const { return (this->registers[BMP388_PWR_CTRL] >> 4) & 0x03; }

  std::array<uint8_t, 256> registers{};
  bool fail_writes{false};

 protected:
  bool read_byte(uint8_t a_register, uint8_t *data) override {
    *data = this->registers[a_register];
    if (a_register == BMP388_INT_STATUS)
      this->registers[a_register] = 0;
    return true;
  }
  bool write_byte(uint8_t a_register, uint8_t data) override { return this->write_bytes(a_register, &data, 1); }
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
  sensor::Sensor pressure;
  int published{0};

  explicit NormalModeFixture(bool with_pressure = false) {
    static char name[] = "test_bmp3xx_normal_mode";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    this->temperature.add_on_state_callback([this](float) { this->published++; });
    this->bmp.set_temperature_sensor(&this->temperature);
    if (with_pressure)
      this->bmp.set_pressure_sensor(&this->pressure);
    this->bmp.set_output_data_rate_config(ODR_12_5_HZ);
    this->bmp.setup();
  }

  // A new result arrives and is polled
  void new_result() {
    this->bmp.registers[BMP388_INT_STATUS] = INT_STATUS_DRDY;
    this->bmp.update();
  }
};

TEST(BMP3XXNormalModeTest, SetupStartsNormalModeAtDataRate) {
  NormalModeFixture f;
  EXPECT_FALSE(f.bmp.is_failed());
  EXPECT_EQ(f.bmp.registers[BMP388_ODR], ODR_12_5_HZ);
  EXPECT_EQ(f.bmp.mode(), NORMAL_MODE);
}

TEST(BMP3XXNormalModeTest, KeepsReadingWhenNoNewResultSinceLastPoll) {
  NormalModeFixture f;
  f.new_result();
  ASSERT_EQ(f.published, 1);

  // drdy was cleared by that read and no new result has arrived yet
  f.bmp.update();
  EXPECT_EQ(f.published, 2);
  EXPECT_FALSE(f.bmp.status_has_warning());
}

TEST(BMP3XXNormalModeTest, WaitsForFirstResultAfterSetup) {
  NormalModeFixture f;
  f.bmp.update();
  EXPECT_EQ(f.published, 0);
  EXPECT_TRUE(f.bmp.status_has_warning());

  f.new_result();
  EXPECT_EQ(f.published, 1);
  EXPECT_FALSE(f.bmp.status_has_warning());
}

TEST(BMP3XXNormalModeTest, SensorResetIsDetectedAndSettingsRestored) {
  NormalModeFixture f;
  const uint8_t osr = f.bmp.registers[BMP388_OSR];
  f.new_result();
  ASSERT_EQ(f.published, 1);

  // Nothing is published from the reset chip's empty data registers
  f.bmp.power_on_reset();
  f.bmp.update();
  EXPECT_EQ(f.published, 1);
  EXPECT_TRUE(f.bmp.status_has_warning());
  EXPECT_EQ(f.bmp.registers[BMP388_OSR], osr);
  EXPECT_EQ(f.bmp.registers[BMP388_ODR], ODR_12_5_HZ);
  EXPECT_EQ(f.bmp.mode(), NORMAL_MODE);

  // Still nothing while the first measurement with the restored settings is pending
  f.bmp.update();
  EXPECT_EQ(f.published, 1);
  EXPECT_TRUE(f.bmp.status_has_warning());

  // The first result with the restored settings is published again
  f.new_result();
  EXPECT_EQ(f.published, 2);
  EXPECT_FALSE(f.bmp.status_has_warning());
}

TEST(BMP3XXNormalModeTest, FailedRestoreIsRetried) {
  NormalModeFixture f;
  f.new_result();
  ASSERT_EQ(f.published, 1);

  f.bmp.power_on_reset();
  f.bmp.fail_writes = true;
  f.bmp.update();
  EXPECT_EQ(f.bmp.mode(), SLEEP_MODE);

  f.bmp.fail_writes = false;
  f.bmp.update();
  EXPECT_EQ(f.bmp.mode(), NORMAL_MODE);
  EXPECT_EQ(f.published, 1);

  f.new_result();
  EXPECT_EQ(f.published, 2);
  EXPECT_FALSE(f.bmp.status_has_warning());
}

TEST(BMP3XXNormalModeTest, PublishesEachNewPressureReading) {
  NormalModeFixture f(true);
  f.bmp.set_raw(6500000, 8300000);
  f.new_result();
  const float first = f.bmp.pressure_for(6500000, 8300000);
  ASSERT_TRUE(std::isfinite(first));
  EXPECT_FLOAT_EQ(f.pressure.state, first);

  f.bmp.set_raw(6600000, 8350000);
  f.new_result();
  const float second = f.bmp.pressure_for(6600000, 8350000);
  ASSERT_NE(second, first);
  EXPECT_FLOAT_EQ(f.pressure.state, second);

  // The reset chip's empty registers are never published
  f.bmp.power_on_reset();
  f.bmp.update();
  f.bmp.update();
  EXPECT_FLOAT_EQ(f.pressure.state, second);

  f.bmp.set_raw(6700000, 8400000);
  f.new_result();
  EXPECT_FLOAT_EQ(f.pressure.state, f.bmp.pressure_for(6700000, 8400000));
}

}  // namespace
}  // namespace esphome::bmp3xx_base::testing
