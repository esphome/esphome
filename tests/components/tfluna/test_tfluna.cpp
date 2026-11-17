#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include "esphome/components/tfluna/tfluna.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::tfluna::testing {

static constexpr uint8_t TIMESTAMP_LOW = 0x06;
static constexpr uint8_t VERSION = 0x0A;
static constexpr uint8_t SAVE = 0x20;
static constexpr uint8_t RESTART = 0x21;
static constexpr uint8_t MODE = 0x23;
static constexpr uint8_t MODE_TRIGGER = 0x01;
static constexpr uint8_t TRIGGER = 0x24;
static constexpr uint8_t RESET = 0x29;
static constexpr uint8_t NO_FAULT = 0xFF;  // outside the register file

struct Fault {
  bool read;
  uint8_t reg;
};

// Register file behind read_register (register byte, then read) and write_register
// (register byte + data, no read). A fault is one NACK, then the bus heals.
class RegisterBus final : public i2c::I2CBus {
 public:
  std::array<uint8_t, 0x30> regs{};
  uint8_t fail_read{NO_FAULT};
  uint8_t fail_write{NO_FAULT};
  unsigned mode_writes{0};
  unsigned save_writes{0};

  i2c::ErrorCode write_readv(uint8_t /*address*/, const uint8_t *out, size_t out_len, uint8_t *in,
                             size_t in_len) override {
    if (out_len == 0 || out[0] + (out_len - 1) + in_len > this->regs.size())
      return i2c::ERROR_INVALID_ARGUMENT;
    const uint8_t reg = out[0];
    if (out_len > 1) {
      if (reg == this->fail_write) {
        this->fail_write = NO_FAULT;
        return i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      std::copy(out + 1, out + out_len, this->regs.begin() + reg);
      this->mode_writes += reg == MODE;
      this->save_writes += reg == SAVE;
      if (reg == RESET)
        this->regs[MODE] = 0;  // fixture choice: defaults are not trigger mode
      if (reg == TRIGGER)
        this->regs[TIMESTAMP_LOW]++;  // fixture choice: every trigger yields a new frame
    }
    if (in_len != 0) {
      if (reg == this->fail_read) {
        this->fail_read = NO_FAULT;
        return i2c::ERROR_NOT_ACKNOWLEDGED;
      }
      std::copy_n(this->regs.begin() + reg, in_len, in);
    }
    return i2c::ERROR_OK;
  }
};

class TFLunaTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // App lives for the whole binary; a timer another test left would fire on a dead object in run_timers()
    ASSERT_TRUE(scheduler_idle());
    this->owns_scheduler_ = true;
    this->bus_.regs[VERSION] = 3;
    this->bus_.regs[VERSION + 1] = 2;
    this->bus_.regs[VERSION + 2] = 1;
    this->device_.set_i2c_bus(&this->bus_);
    this->device_.set_i2c_address(0x10);
  }

  // Timeouts capture &device_ and running one publishes it to App; drop both before the fixture dies
  void TearDown() override {
    if (!this->owns_scheduler_)
      return;  // SetUp already failed; leave the other test's timers unrun
    App.scheduler.cancel_timeout(&this->device_, uint32_t{0});
    App.scheduler.cancel_timeout(&this->device_, uint32_t{1});
    App.scheduler.call(millis());
    EXPECT_FALSE(App.scheduler.next_schedule_in(millis()).has_value());
    // Recycled items hold no callback; free them so a later App reset has none of ours to lose
    App.scheduler.trim_freelist();
    LoopBlockingGuard clear{nullptr, nullptr, millis()};
  }

  // Real clock past the existing 100 ms reset timeout; no new device timing
  // Timers are serviced like the main loop during the real wait instead of one long sleep
  static void run_timers() {
    const uint32_t start = millis();
    while (millis() - start < 110) {
      App.scheduler.call(millis());
      delay(1);
    }
    App.scheduler.call(millis());
  }

  // Threaded hosts' next_schedule_in() skips timers still staged for the heap; merging them first runs no callback
  static bool scheduler_idle() {
    App.scheduler.process_to_add();
    return !App.scheduler.next_schedule_in(millis()).has_value();
  }

  void fail_at_(Fault fault) { (fault.read ? this->bus_.fail_read : this->bus_.fail_write) = fault.reg; }

  // One NACK at the read-mode, write-mode or SAVE step; fatal at boot, retried by the next update at runtime
  void expect_boot_nack_marks_failed_(Fault fault) {
    this->fail_at_(fault);
    this->device_.setup();
    EXPECT_TRUE(this->device_.is_failed());
  }

  void expect_runtime_nack_recovers_(Fault fault) {
    this->device_.setup();
    this->device_.factory_reset();
    this->fail_at_(fault);
    run_timers();
    EXPECT_FALSE(this->device_.is_failed());
    EXPECT_TRUE(this->device_.status_has_warning());
    this->device_.update();
    EXPECT_FALSE(this->device_.is_failed());
    EXPECT_EQ(this->bus_.regs[MODE], MODE_TRIGGER);
    EXPECT_FALSE(this->device_.status_has_warning());
  }

  RegisterBus bus_;
  TFLuna device_;
  bool owns_scheduler_{false};
};

// A timeout another suite left staged must block ownership before any callback can run
TEST_F(TFLunaTest, StagedForeignTimeoutBlocksOwnership) {
  TFLuna foreign;
  bool fired = false;
  App.scheduler.set_timeout(&foreign, uint32_t{0}, 1000, [&fired]() { fired = true; });
  EXPECT_FALSE(scheduler_idle());
  EXPECT_FALSE(fired);
  // Drop the timer before foreign and fired leave scope; no ASSERT may return early above this
  EXPECT_TRUE(App.scheduler.cancel_timeout(&foreign, uint32_t{0}));
  App.scheduler.call(millis());
  EXPECT_FALSE(fired);
  EXPECT_TRUE(scheduler_idle());
}

TEST_F(TFLunaTest, BootSelectsTriggerModeAndSaves) {
  this->device_.setup();
  EXPECT_FALSE(this->device_.is_failed());
  EXPECT_EQ(this->bus_.regs[MODE], MODE_TRIGGER);
  EXPECT_EQ(this->bus_.mode_writes, 1u);
  EXPECT_EQ(this->bus_.save_writes, 1u);
}

TEST_F(TFLunaTest, NormalFactoryResetRestoresTriggerMode) {
  this->device_.setup();
  this->device_.factory_reset();
  EXPECT_TRUE(this->device_.status_has_warning());
  run_timers();
  EXPECT_FALSE(this->device_.is_failed());
  EXPECT_EQ(this->bus_.regs[MODE], MODE_TRIGGER);
  EXPECT_EQ(this->bus_.mode_writes, 2u);
  EXPECT_EQ(this->bus_.save_writes, 2u);
  this->device_.update();
  EXPECT_FALSE(this->device_.status_has_warning());
}

TEST_F(TFLunaTest, ResetWriteNackSchedulesNothing) {
  this->device_.setup();
  this->bus_.fail_write = RESET;
  this->device_.factory_reset();
  run_timers();
  EXPECT_FALSE(this->device_.is_failed());
  EXPECT_TRUE(this->device_.status_has_warning());
  EXPECT_EQ(this->bus_.mode_writes, 1u);
  this->device_.update();  // no reconfigure was left pending; the next good read clears the warning
  EXPECT_EQ(this->bus_.mode_writes, 1u);
  EXPECT_FALSE(this->device_.status_has_warning());
}

TEST_F(TFLunaTest, DoublePressInWindowReconfiguresOnce) {
  this->device_.setup();
  this->device_.factory_reset();
  this->device_.factory_reset();
  run_timers();
  EXPECT_EQ(this->bus_.mode_writes, 2u);
  EXPECT_EQ(this->bus_.regs[MODE], MODE_TRIGGER);
}

// Original: the first NACK marks the component failed, so Scheduler::call() drops the second reset's timeout
TEST_F(TFLunaTest, ResetWhileReconfigurePendingWaitsForWindowThenRecovers) {
  this->device_.setup();
  this->device_.factory_reset();
  this->bus_.fail_read = MODE;
  run_timers();
  this->device_.factory_reset();
  this->device_.update();  // inside the 100 ms window: no early reconfigure
  EXPECT_EQ(this->bus_.mode_writes, 1u);
  run_timers();
  EXPECT_FALSE(this->device_.is_failed());
  EXPECT_EQ(this->bus_.regs[MODE], MODE_TRIGGER);
  EXPECT_EQ(this->bus_.mode_writes, 2u);
  this->device_.update();
  EXPECT_EQ(this->bus_.mode_writes, 2u);
  EXPECT_FALSE(this->device_.status_has_warning());
}

TEST_F(TFLunaTest, ResetWriteNackKeepsPendingReconfigure) {
  this->device_.setup();
  this->device_.factory_reset();
  this->bus_.fail_read = MODE;
  run_timers();
  this->bus_.fail_write = RESET;
  this->device_.factory_reset();
  run_timers();
  this->device_.update();
  EXPECT_FALSE(this->device_.is_failed());
  EXPECT_EQ(this->bus_.regs[MODE], MODE_TRIGGER);
  EXPECT_FALSE(this->device_.status_has_warning());
}

TEST_F(TFLunaTest, UpdateSkipsPollingWhileReconfigureFails) {
  this->device_.setup();
  this->device_.factory_reset();
  this->bus_.fail_read = MODE;
  run_timers();
  this->bus_.fail_read = MODE;
  this->device_.update();
  EXPECT_EQ(this->bus_.regs[TIMESTAMP_LOW], 0);  // no trigger issued
  EXPECT_TRUE(this->device_.status_has_warning());
  this->device_.update();
  EXPECT_EQ(this->bus_.regs[MODE], MODE_TRIGGER);
  EXPECT_FALSE(this->device_.status_has_warning());
}

TEST_F(TFLunaTest, RestartWriteNackWarnsThenNextReadClears) {
  this->device_.setup();
  this->bus_.fail_write = RESTART;
  this->device_.restart();
  EXPECT_FALSE(this->device_.is_failed());
  EXPECT_TRUE(this->device_.status_has_warning());
  this->device_.update();
  EXPECT_FALSE(this->device_.status_has_warning());
}

// A boot failure is permanent; runtime entry points must not touch the bus
TEST_F(TFLunaTest, FailedComponentSkipsRuntimeWrites) {
  this->bus_.fail_read = MODE;
  this->device_.setup();
  ASSERT_TRUE(this->device_.is_failed());
  this->device_.factory_reset();
  this->device_.restart();
  this->device_.update();
  EXPECT_TRUE(this->device_.is_failed());
  EXPECT_FALSE(this->device_.status_has_warning());
  EXPECT_EQ(this->bus_.regs[RESET], 0);
  EXPECT_EQ(this->bus_.regs[RESTART], 0);
  EXPECT_EQ(this->bus_.regs[TIMESTAMP_LOW], 0);
  EXPECT_EQ(this->bus_.mode_writes, 0u);
}

TEST_F(TFLunaTest, BootReadModeNackStillMarksFailed) { this->expect_boot_nack_marks_failed_(Fault{true, MODE}); }

TEST_F(TFLunaTest, BootWriteModeNackStillMarksFailed) { this->expect_boot_nack_marks_failed_(Fault{false, MODE}); }

TEST_F(TFLunaTest, BootSaveNackStillMarksFailed) { this->expect_boot_nack_marks_failed_(Fault{false, SAVE}); }

TEST_F(TFLunaTest, RuntimeReadModeNackWarnsThenNextUpdateRecovers) {
  this->expect_runtime_nack_recovers_(Fault{true, MODE});
}

TEST_F(TFLunaTest, RuntimeWriteModeNackWarnsThenNextUpdateRecovers) {
  this->expect_runtime_nack_recovers_(Fault{false, MODE});
}

TEST_F(TFLunaTest, RuntimeSaveNackWarnsThenNextUpdateRecovers) {
  this->expect_runtime_nack_recovers_(Fault{false, SAVE});
}

}  // namespace esphome::tfluna::testing
