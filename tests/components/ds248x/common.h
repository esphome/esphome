#pragma once

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "esphome/components/ds248x/ds248x.h"
#include "esphome/components/ds248x/ds248x_one_wire_bus.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::ds248x::testing {

static constexpr uint8_t ADDRESS = 0x18;
static constexpr uint8_t CHANNEL_COUNT = 8;
static constexpr uint32_t RECOVERY_RETRY_MS = 1000;  // Mirrors the driver's retry window
static constexpr int NO_COMMAND = -1;

static constexpr uint8_t CHANNEL_WRITE_CODES[8] = {0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87};
static constexpr uint8_t CHANNEL_READ_CODES[8] = {0xB8, 0xB1, 0xAA, 0xA3, 0x9C, 0x95, 0x8E, 0x87};
static constexpr uint8_t POINTER_CHANNEL = 0xD2;

// A DS2482-800 seen through its read pointer, with switchable NACKs and a 1-Wire command that never finishes.
class FakeDS2482 final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    EXPECT_EQ(address, ADDRESS);
    this->transfers++;
    if (this->nack_all || (size != 0 && data[0] == this->nack_command))
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size != 0 && data[0] == DS248X_COMMAND_READBYTE && this->byte_reads == this->nack_byte_read)
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    if (size != 0)
      this->command_(data, size);
    for (size_t i = 0; i < out_size; i++)
      out[i] = this->read_register_();
    return i2c::ERROR_OK;
  }

  int channel{0};
  unsigned transfers{0};
  unsigned channel_selects{0};
  bool nack_all{false};
  int nack_command{NO_COMMAND};
  // NACKs the Read Byte command once this many bytes have been read
  int nack_byte_read{NO_COMMAND};
  unsigned one_wire_commands{0};
  int byte_reads{0};
  // Bytes the wire returns to Read Byte commands, in order
  uint8_t wire_bytes[8]{};

  uint8_t config() const { return this->config_; }
  // The next 1-Wire command keeps 1WB set until a device reset; cleared once used
  bool stick_busy{false};

 protected:
  void command_(const uint8_t *data, size_t size) {
    const uint8_t arg = size > 1 ? data[1] : 0;
    switch (data[0]) {
      case DS248X_COMMAND_RESET:
        this->status_ = DS248X_STATUS_RST;
        this->busy_ = false;
        this->config_ = 0;
        this->channel = 0;
        this->pointer_ = DS248X_POINTER_STATUS;
        break;
      case DS248X_COMMAND_SETREADPTR:
        this->pointer_ = arg;
        break;
      case DS248X_COMMAND_WRITECONFIG:
        this->config_ = arg & 0x0F;
        this->status_ &= ~DS248X_STATUS_RST;
        this->pointer_ = DS248X_POINTER_CONFIG;
        break;
      case DS248X_COMMAND_CHANNELSELECT:
        this->channel_selects++;
        for (int i = 0; i < CHANNEL_COUNT; i++) {
          if (CHANNEL_WRITE_CODES[i] == arg)
            this->channel = i;
        }
        this->pointer_ = POINTER_CHANNEL;
        break;
      case DS248X_COMMAND_RESETWIRE:
      case DS248X_COMMAND_WRITEBYTE:
      case DS248X_COMMAND_READBYTE:
      case DS248X_COMMAND_TRIPLET:
        this->one_wire_commands++;
        if (data[0] == DS248X_COMMAND_READBYTE)
          this->data_ = this->wire_bytes[this->byte_reads++ % 8];
        // No devices on the wire: no presence pulse, and a triplet reads both bits as 1
        this->status_ = data[0] == DS248X_COMMAND_TRIPLET ? DS248X_STATUS_SBR | DS248X_STATUS_TSB : 0;
        if (this->stick_busy) {
          this->busy_ = true;
          this->stick_busy = false;
        }
        this->pointer_ = DS248X_POINTER_STATUS;
        break;
      default:
        ADD_FAILURE() << "unexpected command 0x" << std::hex << unsigned(data[0]);
    }
  }

  uint8_t read_register_() const {
    switch (this->pointer_) {
      case DS248X_POINTER_STATUS:
        return this->status_ | (this->busy_ ? DS248X_STATUS_BUSY : 0);
      case DS248X_POINTER_CONFIG:
        return this->config_;
      case POINTER_CHANNEL:
        return CHANNEL_READ_CODES[this->channel];
      case DS248X_POINTER_DATA:
        return this->data_;
      default:
        return 0xFF;
    }
  }

  uint8_t pointer_{DS248X_POINTER_STATUS};
  uint8_t status_{0};
  uint8_t config_{0};
  uint8_t data_{0};
  bool busy_{false};
};

class TestHub : public DS248xComponent {
 public:
  using DS248xComponent::last_recovery_ms_;

  // Moves the last recovery attempt back past the retry window
  void expire_backoff() { this->last_recovery_ms_ = millis() - RECOVERY_RETRY_MS - 1; }
};

// Application::setup() sets this bit once every component is set up; flip it to reach the post-setup paths.
class AppSetupState : public Application {
 public:
  static void set_complete(bool complete) {
    uint8_t Application::*state = &AppSetupState::app_state_;
    if (complete) {
      App.*state |= APP_STATE_SETUP_COMPLETE;
    } else {
      App.*state &= ~APP_STATE_SETUP_COMPLETE;
    }
  }
};

class DS248xTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->hub_.set_i2c_bus(&this->bus_);
    this->hub_.set_i2c_address(ADDRESS);
    this->hub_.set_channel_count(CHANNEL_COUNT);
  }

  void TearDown() override { AppSetupState::set_complete(false); }

  void setup_hub() {
    this->hub_.setup();
    ASSERT_FALSE(this->hub_.is_failed());
  }

  // A 1-Wire reset that hangs, followed by a device reset the chip does not acknowledge
  void fail_recovery() {
    this->bus_.stick_busy = true;
    this->bus_.nack_command = DS248X_COMMAND_RESET;
    bool presence = false;
    EXPECT_FALSE(this->hub_.ow_reset(presence));
    this->bus_.nack_command = NO_COMMAND;
  }

  FakeDS2482 bus_;
  TestHub hub_;
};

}  // namespace esphome::ds248x::testing
