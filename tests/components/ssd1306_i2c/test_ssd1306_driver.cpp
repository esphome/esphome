#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "esphome/components/ssd1306_i2c/ssd1306_i2c.h"
#include "esphome/core/application.h"

namespace esphome::ssd1306_i2c::testing {

// Only the bus and reset pin are simulated; the driver, allocator and component lifecycle are real.
class DisplayBus final : public i2c::I2CBus {
 public:
  struct Transfer {
    std::vector<uint8_t> bytes;
    bool failed;
  };

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *out, size_t out_size, uint8_t *in,
                             size_t in_size) override {
    EXPECT_EQ(address, 0x3C);
    EXPECT_EQ(in_size, 0U);
    const std::vector<uint8_t> bytes =
        out_size == 0 ? std::vector<uint8_t>{} : std::vector<uint8_t>{out, out + out_size};
    const bool failed = !this->online_ || (this->fail_ && this->fail_(bytes));
    this->transfers_.push_back({bytes, failed});
    if (failed) {
      return i2c::ERROR_NOT_ACKNOWLEDGED;
    }
    if (bytes.empty()) {
      return i2c::ERROR_OK;
    }
    if (bytes[0] == 0x00) {
      for (size_t i = 1; i < bytes.size(); i++) {
        this->command_(bytes[i]);
      }
    } else if (bytes[0] == 0x40) {
      for (size_t i = 1; i < bytes.size(); i++) {
        if (this->x_ >= 128 || this->page_ >= 8) {
          ADD_FAILURE() << "Write outside the simulated SSD1306 RAM";
          return i2c::ERROR_INVALID_ARGUMENT;
        }
        this->ram_[this->page_ * 128 + this->x_] = bytes[i];
        if (++this->x_ > this->max_x_) {
          this->x_ = this->min_x_;
          if (++this->page_ > this->max_page_) {
            this->page_ = this->min_page_;
          }
        }
      }
    }
    return i2c::ERROR_OK;
  }

  void reset_controller() {
    this->on_ = false;
    this->inverted_ = false;
    this->needed_ = 0;
    this->arguments_.clear();
    this->parameters_.fill(0);
    this->ram_.fill(0);
    this->min_x_ = this->x_ = this->min_page_ = this->page_ = 0;
    this->max_x_ = 127;
    this->max_page_ = 7;
  }

  size_t probes() const {
    return std::count_if(this->transfers_.begin(), this->transfers_.end(),
                         [](const Transfer &t) { return t.bytes.empty(); });
  }

  size_t data_writes() const {
    return std::count_if(this->transfers_.begin(), this->transfers_.end(),
                         [](const Transfer &t) { return !t.bytes.empty() && t.bytes[0] == 0x40; });
  }

  size_t off_commands() const {
    return std::count_if(this->transfers_.begin(), this->transfers_.end(), [](const Transfer &t) {
      return !t.failed && t.bytes == std::vector<uint8_t>{0x00, 0xAE};
    });
  }

  bool online_{true};
  bool on_{false};
  bool inverted_{false};
  std::array<uint8_t, 1024> ram_{};
  std::array<uint8_t, 256> parameters_{};
  std::vector<Transfer> transfers_;
  std::function<bool(const std::vector<uint8_t> &)> fail_;

 protected:
  void command_(uint8_t value) {
    if (this->needed_ != 0) {
      this->arguments_.push_back(value);
      if (--this->needed_ == 0) {
        this->parameters_[this->pending_] = this->arguments_[0];
        if (this->pending_ == 0x21) {
          this->min_x_ = this->x_ = this->arguments_[0];
          this->max_x_ = this->arguments_[1];
        } else if (this->pending_ == 0x22) {
          this->min_page_ = this->page_ = this->arguments_[0];
          this->max_page_ = this->arguments_[1];
        }
      }
      return;
    }
    if (value == 0xAE || value == 0xAF) {
      this->on_ = value == 0xAF;
    } else if (value == 0xA6 || value == 0xA7) {
      this->inverted_ = value == 0xA7;
    } else if (value == 0x21 || value == 0x22) {
      this->needed_ = 2;
    } else if (value == 0xD5 || value == 0xA8 || value == 0xD3 || value == 0xAD || value == 0x8D || value == 0x20 ||
               value == 0xDA || value == 0xD9 || value == 0xDB || value == 0x81) {
      this->needed_ = 1;
    }
    this->pending_ = value;
    this->arguments_.clear();
  }

  uint8_t pending_{0};
  unsigned needed_{0};
  unsigned min_x_{0}, max_x_{127}, min_page_{0}, max_page_{7}, x_{0}, page_{0};
  std::vector<uint8_t> arguments_;
};

class ResetPin final : public GPIOPin {
 public:
  explicit ResetPin(DisplayBus &bus) : bus_(bus) {}
  void setup() override {}
  void pin_mode(gpio::Flags flags) override {}
  gpio::Flags get_flags() const override { return gpio::FLAG_OUTPUT; }
  bool digital_read() override { return this->level_; }
  void digital_write(bool value) override {
    this->level_ = value;
    this->writes_.emplace_back(App.get_loop_component_start_time(), value);
    if (!value) {
      this->bus_.reset_controller();
    }
  }

  std::vector<std::pair<uint32_t, bool>> writes_;

 protected:
  DisplayBus &bus_;
  bool level_{true};
};

class SSD1306DriverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->display_.set_i2c_bus(&this->bus_);
    this->display_.set_i2c_address(0x3C);
    this->display_.set_auto_clear(false);
    this->display_.set_update_interval(std::numeric_limits<uint32_t>::max());
    this->display_.set_partial_updates();
    this->set_time_(0);
  }

  void set_time_(uint32_t now) {
    this->now_ = now;
    LoopBlockingGuard guard{&this->display_, nullptr, now};
  }

  void tick_(uint32_t elapsed) {
    this->set_time_(this->now_ + elapsed);
    this->display_.call();
  }

  void healthy_(bool reset_pin = false) {
    if (reset_pin) {
      this->display_.set_reset_pin(&this->pin_);
    }
    this->display_.call();
    ASSERT_FALSE(this->display_.is_failed());
    this->display_.update();
    ASSERT_FALSE(this->display_.status_has_warning());
    ASSERT_TRUE(this->bus_.on_);
    this->clear_trace_();
  }

  void clear_trace_() {
    this->bus_.transfers_.clear();
    this->pin_.writes_.clear();
  }

  void fail_data_once_() {
    this->bus_.fail_ = [failed = false](const std::vector<uint8_t> &bytes) mutable {
      if (!failed && !bytes.empty() && bytes[0] == 0x40) {
        failed = true;
        return true;
      }
      return false;
    };
  }

  void fail_command_once_(uint8_t command) {
    this->bus_.fail_ = [command, failed = false](const std::vector<uint8_t> &bytes) mutable {
      if (!failed && bytes == std::vector<uint8_t>{0x00, command}) {
        failed = true;
        return true;
      }
      return false;
    };
  }

  void expect_frame_(uint8_t value) {
    const int width = this->display_.get_width();
    const int pages = this->display_.get_height() / 8;
    const int column = width == 64 ? 32 : (width == 72 ? 28 : 0);
    for (int page = 0; page < pages; page++) {
      for (int x = 0; x < width; x++) {
        ASSERT_EQ(this->bus_.ram_[page * 128 + column + x], value) << "page=" << page << ", x=" << x;
      }
    }
  }

  DisplayBus bus_;
  ResetPin pin_{this->bus_};
  I2CSSD1306 display_;
  uint32_t now_{0};
};

TEST_F(SSD1306DriverTest, DirectOnlyDataFailureRecoversFromLoopWithoutBlankingOrReset) {
  this->healthy_(true);
  this->fail_data_once_();
  this->display_.fill(Color::WHITE);
  this->display_.display();
  ASSERT_TRUE(this->display_.status_has_warning());
  EXPECT_TRUE(this->display_.is_in_loop_state());
  this->tick_(1);
  EXPECT_FALSE(this->display_.status_has_warning());
  EXPECT_TRUE(this->display_.is_idle());
  EXPECT_EQ(this->bus_.off_commands(), 0U);
  EXPECT_TRUE(this->pin_.writes_.empty());
  this->expect_frame_(0xFF);
}

TEST_F(SSD1306DriverTest, RecoveryContinuesWithoutAnyFurtherDisplayOrUpdateCalls) {
  this->healthy_();
  this->bus_.online_ = false;
  this->display_.fill(Color::WHITE);
  this->display_.display();
  this->tick_(1);
  ASSERT_TRUE(this->display_.status_has_warning());
  this->bus_.online_ = true;
  this->tick_(999);
  EXPECT_TRUE(this->display_.status_has_warning());
  this->tick_(1);
  EXPECT_FALSE(this->display_.status_has_warning());
  this->expect_frame_(0xFF);
  EXPECT_TRUE(this->display_.is_idle());
}

TEST_F(SSD1306DriverTest, DirectCallsAndSettersCannotBypassTheOutageRetryLimit) {
  this->healthy_();
  this->bus_.online_ = false;
  this->display_.display();
  this->tick_(0);
  this->clear_trace_();
  for (unsigned i = 0; i < 999; i++) {
    this->tick_(1);
    this->display_.fill(Color::WHITE);
    this->display_.display();
    this->display_.set_invert(true);
    this->display_.set_contrast(0.25f);
    this->display_.turn_off();
    this->display_.update();
  }
  EXPECT_TRUE(this->bus_.transfers_.empty());
  this->tick_(1);
  ASSERT_EQ(this->bus_.transfers_.size(), 1U);
  EXPECT_EQ(this->bus_.probes(), 1U);
  EXPECT_TRUE(this->display_.status_has_warning());
}

TEST_F(SSD1306DriverTest, StaticOutageOnlyProbesAtTheRetryInterval) {
  this->healthy_(true);
  this->bus_.online_ = false;
  this->display_.update();
  this->tick_(0);
  this->clear_trace_();
  for (unsigned i = 0; i < 5000; i++) {
    this->tick_(1);
    this->display_.update();
  }
  EXPECT_EQ(this->bus_.probes(), 5U);
  EXPECT_EQ(this->bus_.transfers_.size(), 5U);
  EXPECT_TRUE(this->pin_.writes_.empty());
}

TEST_F(SSD1306DriverTest, InitialCommandFailureRecoversWithoutScheduledUpdates) {
  this->fail_command_once_(0xD5);
  this->display_.call();
  ASSERT_TRUE(this->display_.status_has_warning());
  ASSERT_FALSE(this->display_.is_failed());
  this->display_.fill(Color::WHITE);
  this->tick_(1);
  EXPECT_FALSE(this->display_.status_has_warning());
  EXPECT_TRUE(this->display_.is_idle());
  EXPECT_TRUE(this->bus_.on_);
  EXPECT_EQ(this->bus_.parameters_[0xD5], 0x80);
  this->expect_frame_(0xFF);
}

TEST_F(SSD1306DriverTest, InitialDisplayOnFailureCannotBeHiddenByDirectWrites) {
  this->fail_command_once_(0xAF);
  this->display_.call();
  ASSERT_TRUE(this->display_.status_has_warning());
  EXPECT_FALSE(this->bus_.on_);
  this->clear_trace_();
  this->display_.fill(Color::WHITE);
  this->display_.display();
  EXPECT_TRUE(this->bus_.transfers_.empty());
  EXPECT_TRUE(this->display_.status_has_warning());
  this->tick_(1);
  EXPECT_FALSE(this->display_.status_has_warning());
  EXPECT_TRUE(this->bus_.on_);
  this->expect_frame_(0xFF);
}

TEST_F(SSD1306DriverTest, AsyncResetPreservesDeferredSettingsAndTheLatestFrame) {
  this->healthy_(true);
  this->bus_.online_ = false;
  this->display_.display();
  this->tick_(0);
  this->bus_.online_ = true;
  this->clear_trace_();
  this->tick_(1000);
  ASSERT_EQ(this->pin_.writes_.size(), 1U);
  EXPECT_TRUE(this->pin_.writes_[0].second);
  this->clear_trace_();
  this->display_.set_invert(true);
  this->display_.set_contrast(0.25f);
  this->display_.turn_off();
  this->display_.fill(Color::WHITE);
  this->display_.display();
  EXPECT_TRUE(this->bus_.transfers_.empty());
  this->tick_(0);
  EXPECT_TRUE(this->pin_.writes_.empty());
  this->tick_(1);
  ASSERT_EQ(this->pin_.writes_.size(), 1U);
  EXPECT_FALSE(this->pin_.writes_[0].second);
  this->display_.fill(Color::BLACK);
  this->display_.draw_pixel_at(19, 16, Color::WHITE);
  this->display_.display();
  this->display_.update();
  EXPECT_TRUE(this->bus_.transfers_.empty());
  this->tick_(9);
  EXPECT_TRUE(this->bus_.transfers_.empty());
  this->tick_(1);
  ASSERT_EQ(this->pin_.writes_.size(), 2U);
  EXPECT_EQ(this->pin_.writes_[1].first - this->pin_.writes_[0].first, 10U);
  EXPECT_TRUE(this->pin_.writes_[1].second);
  EXPECT_FALSE(this->display_.status_has_warning());
  EXPECT_TRUE(this->display_.is_idle());
  EXPECT_FALSE(this->bus_.on_);
  EXPECT_FALSE(this->display_.is_on());
  EXPECT_TRUE(this->bus_.inverted_);
  EXPECT_EQ(this->bus_.parameters_[0x81], 63);
  EXPECT_EQ(this->bus_.ram_[2 * 128 + 19], 1);
}

TEST_F(SSD1306DriverTest, SoftRecoveryAlsoRestoresSettingsAfterUnobservedPowerLoss) {
  this->healthy_(true);
  this->display_.set_invert(true);
  this->display_.set_contrast(0.25f);
  this->display_.turn_off();
  this->clear_trace_();
  this->fail_data_once_();
  this->display_.fill(Color::WHITE);
  this->display_.display();
  ASSERT_TRUE(this->display_.status_has_warning());
  this->bus_.reset_controller();
  this->tick_(1);
  EXPECT_FALSE(this->display_.status_has_warning());
  EXPECT_TRUE(this->bus_.inverted_);
  EXPECT_EQ(this->bus_.parameters_[0x81], 63);
  EXPECT_FALSE(this->bus_.on_);
  EXPECT_TRUE(this->pin_.writes_.empty());
  this->expect_frame_(0xFF);
}

TEST_F(SSD1306DriverTest, FailedSoftRecoveryEscalatesAndRemainsRateLimited) {
  this->healthy_(true);
  this->bus_.fail_ = [](const std::vector<uint8_t> &bytes) { return !bytes.empty() && bytes[0] == 0x40; };
  this->display_.fill(Color::WHITE);
  this->display_.display();
  this->tick_(0);
  ASSERT_TRUE(this->display_.status_has_warning());
  EXPECT_TRUE(this->display_.is_in_loop_state());
  EXPECT_TRUE(this->pin_.writes_.empty());
  this->bus_.fail_ = {};
  this->clear_trace_();
  this->tick_(999);
  EXPECT_TRUE(this->bus_.transfers_.empty());
  this->tick_(1);
  ASSERT_EQ(this->pin_.writes_.size(), 1U);
  this->tick_(1);
  this->tick_(10);
  EXPECT_FALSE(this->display_.status_has_warning());
  this->expect_frame_(0xFF);
}

TEST_F(SSD1306DriverTest, FailedRecoveryPowerCommandKeepsTheLoopAndWarningActive) {
  this->healthy_();
  this->bus_.online_ = false;
  this->display_.display();
  this->bus_.online_ = true;
  this->fail_command_once_(0xAF);
  this->tick_(0);
  ASSERT_TRUE(this->display_.status_has_warning());
  EXPECT_TRUE(this->display_.is_in_loop_state());
  EXPECT_FALSE(this->bus_.on_);
  this->clear_trace_();
  this->display_.fill(Color::WHITE);
  this->display_.display();
  this->tick_(999);
  EXPECT_TRUE(this->bus_.transfers_.empty());
  this->tick_(1);
  EXPECT_FALSE(this->display_.status_has_warning());
  EXPECT_TRUE(this->display_.is_idle());
  EXPECT_TRUE(this->bus_.on_);
  this->expect_frame_(0xFF);
}

TEST_F(SSD1306DriverTest, FailureAfterAsyncResetSchedulesAnotherCompleteAttempt) {
  this->healthy_(true);
  this->bus_.online_ = false;
  this->display_.display();
  this->bus_.online_ = true;
  this->tick_(0);
  this->tick_(1);
  this->fail_command_once_(0xD5);
  this->tick_(10);
  ASSERT_TRUE(this->display_.status_has_warning());
  EXPECT_TRUE(this->display_.is_in_loop_state());
  this->clear_trace_();
  this->tick_(988);
  EXPECT_TRUE(this->bus_.transfers_.empty());
  EXPECT_TRUE(this->pin_.writes_.empty());
  this->tick_(1);
  ASSERT_EQ(this->pin_.writes_.size(), 1U);
  this->tick_(1);
  this->tick_(10);
  EXPECT_FALSE(this->display_.status_has_warning());
  EXPECT_TRUE(this->display_.is_idle());
  EXPECT_TRUE(this->bus_.on_);
}

TEST_F(SSD1306DriverTest, RetryIntervalHandlesUint32Wrap) {
  this->healthy_();
  this->set_time_(std::numeric_limits<uint32_t>::max() - 499);
  this->bus_.online_ = false;
  this->display_.display();
  this->tick_(0);
  this->bus_.online_ = true;
  this->clear_trace_();
  this->tick_(999);
  EXPECT_TRUE(this->bus_.transfers_.empty());
  this->tick_(1);
  EXPECT_FALSE(this->display_.status_has_warning());
  EXPECT_EQ(this->bus_.probes(), 1U);
}

TEST_F(SSD1306DriverTest, InvalidPlannerInputIsAnInternalErrorNotARecoveryLoop) {
  this->healthy_();
  this->display_.set_model(ssd1306_base::SSD1306_MODEL_COUNT);
  this->display_.update();
  EXPECT_TRUE(this->display_.is_failed());
  EXPECT_TRUE(this->display_.status_has_error());
  EXPECT_FALSE(this->display_.status_has_warning());
  EXPECT_TRUE(this->bus_.transfers_.empty());
  this->display_.loop();
  this->display_.display();
  this->display_.update();
  EXPECT_TRUE(this->display_.is_failed());
  EXPECT_TRUE(this->bus_.transfers_.empty());
}

TEST_F(SSD1306DriverTest, FailedPresenceCheckStillAllowsSafeDirectDrawingCalls) {
  this->bus_.online_ = false;
  this->display_.call();
  ASSERT_TRUE(this->display_.is_failed());
  this->clear_trace_();
  // No framebuffer was allocated, but mark_failed() cannot prevent these public calls.
  this->display_.fill(Color::WHITE);
  this->display_.draw_pixel_at(0, 0, Color::WHITE);
  this->display_.rectangle(0, 0, 8, 8, Color::WHITE);
  this->display_.display();
  this->display_.update();
  this->display_.loop();
  EXPECT_TRUE(this->display_.is_failed());
  EXPECT_TRUE(this->bus_.transfers_.empty());
}

TEST_F(SSD1306DriverTest, DrawingBeforeSetupDoesNotDereferenceAMissingFramebuffer) {
  this->display_.fill(Color::WHITE);
  this->display_.draw_pixel_at(127, 63, Color::WHITE);
  this->display_.display();
  this->display_.update();
  EXPECT_TRUE(this->bus_.transfers_.empty());
}

TEST_F(SSD1306DriverTest, PartialUpdatesUseRealI2CTransfersAndAnUnchangedFrameProbe) {
  this->healthy_();
  this->display_.draw_pixel_at(3, 8, Color::WHITE);
  this->display_.update();
  ASSERT_EQ(this->bus_.transfers_.size(), 2U);
  EXPECT_EQ(this->bus_.transfers_[0].bytes, (std::vector<uint8_t>{0x00, 0x21, 3, 3, 0x22, 1, 1}));
  EXPECT_EQ(this->bus_.transfers_[1].bytes, (std::vector<uint8_t>{0x40, 1}));
  EXPECT_EQ(this->bus_.ram_[128 + 3], 1);
  this->clear_trace_();
  this->display_.update();
  EXPECT_EQ(this->bus_.transfers_.size(), 1U);
  EXPECT_EQ(this->bus_.probes(), 1U);
}

class SSD1306GeometryTest : public SSD1306DriverTest,
                            public ::testing::WithParamInterface<ssd1306_base::SSD1306Model> {};

TEST_P(SSD1306GeometryTest, DirectDisplayAndUpdateKeepShadowCoherentInBothPowerStates) {
  this->display_.set_model(GetParam());
  this->healthy_();
  for (bool on : {true, false}) {
    if (!on) {
      this->display_.turn_off();
    }
    this->display_.fill(Color::WHITE);
    this->display_.display();
    this->expect_frame_(0xFF);
    this->display_.fill(Color::BLACK);
    this->display_.update();
    this->expect_frame_(0x00);
    this->display_.fill(Color::WHITE);
    this->display_.update();
    this->display_.fill(Color::BLACK);
    this->display_.display();
    this->display_.fill(Color::WHITE);
    this->display_.update();
    this->expect_frame_(0xFF);
    EXPECT_EQ(this->bus_.on_, on);
    EXPECT_FALSE(this->display_.status_has_warning());
    this->clear_trace_();
    this->display_.update();
    EXPECT_EQ(this->bus_.probes(), 1U);
    EXPECT_EQ(this->bus_.transfers_.size(), 1U);
  }
}

INSTANTIATE_TEST_SUITE_P(AllSupportedModels, SSD1306GeometryTest,
                         ::testing::Values(ssd1306_base::SSD1306_MODEL_128_32, ssd1306_base::SSD1306_MODEL_128_64,
                                           ssd1306_base::SSD1306_MODEL_96_16, ssd1306_base::SSD1306_MODEL_64_48,
                                           ssd1306_base::SSD1306_MODEL_64_32, ssd1306_base::SSD1306_MODEL_72_40));

class SSD1306DataFailureTest : public SSD1306DriverTest, public ::testing::WithParamInterface<unsigned> {};

TEST_P(SSD1306DataFailureTest, EveryDirectFrameChunkFailureInvalidatesTheShadow) {
  this->healthy_();
  this->bus_.fail_ = [index = GetParam(), chunk = 0U](const std::vector<uint8_t> &bytes) mutable {
    return !bytes.empty() && bytes[0] == 0x40 && chunk++ == index;
  };
  this->display_.fill(Color::WHITE);
  this->display_.display();
  ASSERT_TRUE(this->display_.status_has_warning());
  EXPECT_EQ(this->bus_.data_writes(), GetParam() + 1);
  this->bus_.fail_ = {};
  this->display_.fill(Color::BLACK);
  this->tick_(1);
  EXPECT_FALSE(this->display_.status_has_warning());
  this->expect_frame_(0x00);
  this->clear_trace_();
  this->display_.update();
  EXPECT_EQ(this->bus_.transfers_.size(), 1U);
  EXPECT_EQ(this->bus_.probes(), 1U);
}

INSTANTIATE_TEST_SUITE_P(AllFrameChunks, SSD1306DataFailureTest, ::testing::Range(0U, 64U));

class SSD1306WindowFailureTest : public SSD1306DriverTest, public ::testing::WithParamInterface<unsigned> {};

TEST_P(SSD1306WindowFailureTest, DirectAddressFailuresCannotCommitTheShadow) {
  this->healthy_();
  unsigned calls = 0;
  this->bus_.fail_ = [index = GetParam(), &calls](const std::vector<uint8_t> &bytes) {
    return !bytes.empty() && bytes[0] == 0 && calls++ == index;
  };
  this->display_.fill(Color::WHITE);
  this->display_.display();
  ASSERT_TRUE(this->display_.status_has_warning());
  EXPECT_EQ(calls, GetParam() + 1);
  EXPECT_EQ(this->bus_.data_writes(), 0U);
  this->bus_.fail_ = {};
  this->tick_(1);
  EXPECT_FALSE(this->display_.status_has_warning());
  this->expect_frame_(0xFF);
}

INSTANTIATE_TEST_SUITE_P(AllAddressWrites, SSD1306WindowFailureTest, ::testing::Range(0U, 7U));

TEST(SSD1306Legacy, PartialDisabledStillWritesEveryFrame) {
  DisplayBus bus;
  I2CSSD1306 display;
  display.set_i2c_bus(&bus);
  display.set_i2c_address(0x3C);
  display.set_auto_clear(false);
  display.setup();
  ASSERT_FALSE(display.is_failed());
  bus.transfers_.clear();
  display.fill(Color::WHITE);
  display.update();
  EXPECT_EQ(bus.data_writes(), 64U);
  EXPECT_TRUE(std::all_of(bus.ram_.begin(), bus.ram_.end(), [](uint8_t value) { return value == 0xFF; }));
  bus.transfers_.clear();
  display.update();
  EXPECT_EQ(bus.data_writes(), 64U);
  EXPECT_EQ(bus.probes(), 0U);
}

}  // namespace esphome::ssd1306_i2c::testing
