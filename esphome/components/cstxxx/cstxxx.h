#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::cstxxx {

static constexpr uint16_t REG_TOUCH_DATA = 0xD000;
static constexpr uint16_t REG_FINGER_COUNT = 0xD005;
static constexpr uint16_t REG_COMMAND_MODE = 0xD101;
static constexpr uint16_t REG_NORMAL_MODE = 0xD109;
static constexpr uint16_t REG_RESOLUTION = 0xD1F8;
static constexpr uint16_t REG_FW_CHECK_CODE = 0xD1FC;
static constexpr uint16_t REG_CHIP_INFO = 0xD204;
static constexpr uint16_t REG_FW_VERSION = 0xD208;

static constexpr uint8_t FINGER_COUNT_IDX = REG_FINGER_COUNT - REG_TOUCH_DATA;
static constexpr uint8_t SYNC_IDX = FINGER_COUNT_IDX + 1;
static constexpr uint8_t SYNC_BYTE = 0xAB;
static constexpr uint16_t FW_CHECK_CODE = 0xCACA;

/// Bytes of a touch report holding `points` records; two status bytes follow the first record.
constexpr size_t report_size(uint8_t points) { return points * 5 + 2; }

struct TouchRecord {
  uint8_t id;
  uint8_t event;
  uint16_t x;
  uint16_t y;
  uint8_t z;
};

/// Decodes record `index` of a touch report.
inline TouchRecord decode_touch(const uint8_t *report, uint8_t index) {
  const uint8_t *r = report + index * 5 + (index == 0 ? 0 : 2);
  return {static_cast<uint8_t>(r[0] >> 4), static_cast<uint8_t>(r[0] & 0x0F),
          static_cast<uint16_t>(r[1] << 4 | r[3] >> 4), static_cast<uint16_t>(r[2] << 4 | (r[3] & 0x0F)), r[4]};
}

class CSTTouchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  /// Sets up the interrupt pin, if configured, to fire on `type`.
  void setup_interrupt_pin_(gpio::InterruptType type) {
    if (this->interrupt_pin_ == nullptr)
      return;
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, type);
  }
  /// Reads two little-endian words starting at `reg`.
  bool ESPHOME_ALWAYS_INLINE read_words_(uint16_t reg, uint16_t &low, uint16_t &high) {
    uint16_t words[2];
    if (this->read_register16(reg, reinterpret_cast<uint8_t *>(words), sizeof(words)) != i2c::ERROR_OK)
      return false;
    low = convert_little_endian(words[0]);
    high = convert_little_endian(words[1]);
    return true;
  }
  /// Enters command mode, marking the component failed if the write fails.
  bool enter_command_mode_() {
    return this->setup_check(this->write_register16(REG_COMMAND_MODE, nullptr, 0) == i2c::ERROR_OK,
                             LOG_STR("Failed to enter command mode"));
  }
  /// Checks the firmware check code, marking the component failed if it is unreadable or wrong.
  bool check_firmware_() {
    uint16_t boot_time, check_code;
    if (!this->setup_check(this->read_words_(REG_FW_CHECK_CODE, boot_time, check_code)))
      return false;
    if (check_code == FW_CHECK_CODE)
      return true;
    this->fail_check_code_(check_code);
    return false;
  }
  /// Logs a wrong firmware check code and marks the component failed.
  void fail_check_code_(uint16_t check_code);

  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};
  bool setup_complete_{};
};

}  // namespace esphome::cstxxx
