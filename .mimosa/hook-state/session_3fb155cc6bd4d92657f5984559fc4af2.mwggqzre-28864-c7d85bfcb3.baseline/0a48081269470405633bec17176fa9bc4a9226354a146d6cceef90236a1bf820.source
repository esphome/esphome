#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>

#include "esphome/components/bmp3xx_base/bmp3xx_base.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::bmp3xx_base::testing {

// A BMP390 register file in memory, with a switch to make writes fail
class FakeBMP3XX final : public BMP3XXComponent {
 public:
  FakeBMP3XX() {
    this->registers[BMP388_CHIP_ID] = BMP390_ID;
    this->registers[BMP388_EVENT] = 0x01;       // por_detected
    this->registers[BMP388_INT_STATUS] = 0x08;  // drdy
  }

  std::array<uint8_t, 256> registers{};
  bool fail_writes{false};

 protected:
  bool read_byte(uint8_t a_register, uint8_t *data) override {
    *data = this->registers[a_register];
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

struct Fixture {
  FakeBMP3XX sensor;

  Fixture() {
    static char name[] = "test_bmp3xx_base";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    this->sensor.setup();
  }

  ~Fixture() {
    // Run a pending read while the sensor is alive; App outlives each test
    this->run_scheduled_read();
  }

  // The read is scheduled after the measurement time, about 6 ms here; the host scheduler uses the real clock
  void run_scheduled_read() {
    delay(20);
    App.scheduler.call(millis());
  }
};

TEST(BMP3XXBaseTest, FailedWriteInUpdateSetsWarningNotFailed) {
  Fixture f;
  ASSERT_FALSE(f.sensor.is_failed());

  f.sensor.fail_writes = true;
  f.sensor.update();
  EXPECT_FALSE(f.sensor.is_failed());
  EXPECT_TRUE(f.sensor.status_has_warning());
}

TEST(BMP3XXBaseTest, NextGoodReadingClearsWarning) {
  Fixture f;
  f.sensor.fail_writes = true;
  f.sensor.update();
  ASSERT_TRUE(f.sensor.status_has_warning());

  f.sensor.fail_writes = false;
  f.sensor.update();
  EXPECT_EQ((f.sensor.registers[BMP388_PWR_CTRL] >> 4) & 0x03, FORCED_MODE);
  f.run_scheduled_read();
  EXPECT_FALSE(f.sensor.is_failed());
  EXPECT_FALSE(f.sensor.status_has_warning());
}

}  // namespace esphome::bmp3xx_base::testing
