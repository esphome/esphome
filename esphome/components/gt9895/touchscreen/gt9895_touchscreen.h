#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::gt9895 {

// Goodix GT9895 capacitive touch controller. Registers have a 32-bit big-endian address; data is little-endian and
// each block ends with a 16-bit sum of its bytes.
static constexpr uint32_t GT9895_REG_FW_VERSION = 0x00010014;
static constexpr uint32_t GT9895_REG_RUNTIME_INFO = 0x00010070;
static constexpr size_t GT9895_FW_VERSION_SIZE = 28;
static constexpr size_t GT9895_RUNTIME_INFO_MAX_SIZE = 1024;

// A touch report: an 8-byte header, then 8 bytes for each contact and a checksum
static constexpr size_t GT9895_HEADER_SIZE = 8;
static constexpr size_t GT9895_CONTACT_SIZE = 8;
static constexpr size_t GT9895_CHECKSUM_SIZE = 2;
static constexpr uint8_t GT9895_MAX_TOUCHES = 10;
// Contacts read along with the header; more need a second read
static constexpr uint8_t GT9895_FIRST_READ_CONTACTS = 2;
static constexpr uint8_t GT9895_EVENT_TOUCH = 0x80;

class GT9895Touchscreen : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void update_touches() override;
  void read_info_();
  bool read_(uint32_t address, uint8_t *data, size_t len);
  void comm_failed_();
  static bool checksum_ok(const uint8_t *data, size_t len);

  InternalGPIOPin *interrupt_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  uint32_t touch_data_address_{0};
  uint32_t firmware_version_{0};
  char product_id_[9]{};
};

}  // namespace esphome::gt9895
