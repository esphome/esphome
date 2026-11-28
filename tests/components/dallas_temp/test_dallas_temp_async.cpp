// The trigger graph of dallas_temp on a bus that runs transactions in the background.

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <deque>

#include "esphome/components/dallas_temp/dallas_temp.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome::dallas_temp::testing {

using one_wire::Transaction;
using one_wire::TransactionStatus;

constexpr uint64_t ROM = 0x120000065C17EE28ULL;
constexpr uint8_t CONVERT = 0x44;
constexpr uint8_t READ_SCRATCH_PAD = 0xBE;
constexpr uint32_t WAIT_10_BIT_MS = 188;

/// Runs what was submitted when the test says so, like the FIFO of the real bus. Blocking access is an error,
/// except in setup(), which reads the scratch pad with the blocking calls.
class FakeBus final : public one_wire::OneWireBus {
 public:
  std::deque<Transaction *> queue;  ///< Mirror of pending_ that the tests can inspect
  uint8_t scratch_pad[9]{};
  unsigned submits{0};
  bool run_at_once{false};
  TransactionStatus run_at_once_status{TransactionStatus::TRANSACTION_STATUS_OK};
  bool blocking{false};
  int presence{1};  ///< What a blocking reset returns

  bool submit(Transaction *transaction) override {
    ++this->submits;
    if (!this->pending_.push(transaction))
      return false;
    this->queue.push_back(transaction);
    if (this->run_at_once)
      this->finish(this->run_at_once_status);
    return true;
  }

  void finish(TransactionStatus status) {
    ASSERT_FALSE(this->queue.empty());
    Transaction *transaction = this->pending_.pop();
    ASSERT_EQ(transaction, this->queue.front());
    this->queue.pop_front();
    if (status == TransactionStatus::TRANSACTION_STATUS_OK && transaction->read_len != 0)
      std::memcpy(transaction->read_data, this->scratch_pad, transaction->read_len);
    transaction->on_done(transaction->context, status);
  }

  void write8(uint8_t val) override { EXPECT_TRUE(this->blocking) << "blocking write8"; }
  void write64(uint64_t val) override { EXPECT_TRUE(this->blocking) << "blocking write64"; }
  uint8_t read8() override {
    EXPECT_TRUE(this->blocking) << "blocking read8";
    return this->scratch_pad[this->blocking_read_++ % sizeof(this->scratch_pad)];
  }
  uint64_t read64() override {
    ADD_FAILURE() << "blocking read64";
    return 0;
  }

 protected:
  int reset_int() override {
    EXPECT_TRUE(this->blocking) << "blocking reset";
    this->blocking_read_ = 0;
    return this->presence;
  }
  void reset_search() override {}
  uint64_t search_int() override { return 0; }

  one_wire::TransactionQueue pending_;
  size_t blocking_read_{0};
};

class DallasAsync : public ::testing::Test {
 protected:
  FakeBus bus_;
  DallasTemperatureSensor sensor_;
  unsigned publications_{0};

  void SetUp() override {
    this->sensor_.set_one_wire_bus(&this->bus_);
    this->sensor_.set_address(ROM);
    this->sensor_.set_resolution(10);
    this->sensor_.add_on_state_callback([this](float) { ++this->publications_; });
    this->set_scratch_pad_(0x0190, 0x10);  // 25 degrees
    // setup() reads the scratch pad with the blocking calls and sets the address of the transaction.
    // The resolution in the scratch pad already matches, so it writes nothing.
    this->bus_.blocking = true;
    this->sensor_.setup();
    this->bus_.blocking = false;
    ASSERT_FALSE(this->sensor_.status_has_warning());
  }

  // Drop a pending wait so it does not outlive the sensor into a later test
  void TearDown() override {
    App.scheduler.cancel_timeout(&this->sensor_, uint32_t{0});
    App.scheduler.call(millis());
  }

  void set_scratch_pad_(uint16_t raw, uint8_t count_per_c) {
    const uint8_t bytes[8] = {
        static_cast<uint8_t>(raw), static_cast<uint8_t>(raw >> 8), 0x4B, 0x46, 0x3F, 0xFF, 0x0C, count_per_c};
    std::memcpy(this->bus_.scratch_pad, bytes, 8);
    this->bus_.scratch_pad[8] = crc8(bytes, 8);
  }

  static void run_timers(uint32_t duration = 0) {
    App.scheduler.call(millis());
    delay(duration);
    App.scheduler.call(millis());
  }

  /// update(), the conversion ends, the wait ends: the read is now running.
  void run_until_reading_() {
    this->sensor_.update();
    this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
    run_timers(WAIT_10_BIT_MS + 30);
    ASSERT_EQ(this->bus_.queue.size(), 1u);
    ASSERT_EQ(*this->bus_.queue.front()->write_data, READ_SCRATCH_PAD);
  }
};

TEST_F(DallasAsync, UpdateWithoutAddressDoesNothing) {
  this->sensor_.set_address(0);
  this->sensor_.update();
  EXPECT_EQ(this->bus_.submits, 0u);
}

TEST_F(DallasAsync, UpdateClearsTheWarningAndStartsTheConversion) {
  this->sensor_.status_set_warning();
  this->sensor_.update();

  EXPECT_FALSE(this->sensor_.status_has_warning());
  ASSERT_EQ(this->bus_.queue.size(), 1u);
  const Transaction *convert = this->bus_.queue.front();
  EXPECT_EQ(convert->address, ROM);
  EXPECT_EQ(convert->write_len, 1);
  EXPECT_EQ(*convert->write_data, CONVERT);
  EXPECT_EQ(convert->read_len, 0);
  EXPECT_EQ(this->publications_, 0u);
}

TEST_F(DallasAsync, ReadComesAfterTheWaitOfAGoodConversion) {
  this->sensor_.update();
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
  EXPECT_TRUE(this->bus_.queue.empty());
  run_timers(WAIT_10_BIT_MS + 30);
  ASSERT_EQ(this->bus_.queue.size(), 1u);
  const Transaction *read = this->bus_.queue.front();
  EXPECT_EQ(read->address, ROM);
  EXPECT_EQ(read->write_len, 1);
  EXPECT_EQ(*read->write_data, READ_SCRATCH_PAD);
  EXPECT_EQ(read->read_len, 9);
}

TEST_F(DallasAsync, EveryFailedConversionPublishesNanWithAWarningAndSkipsTheRead) {
  for (const auto status :
       {TransactionStatus::TRANSACTION_STATUS_NO_PRESENCE, TransactionStatus::TRANSACTION_STATUS_BUS_LOW,
        TransactionStatus::TRANSACTION_STATUS_CAPTURE_ERROR, TransactionStatus::TRANSACTION_STATUS_TIMEOUT,
        TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}) {
    // A good reading first: the sensor still holds it with a valid checksum, and it must not be published again
    this->run_until_reading_();
    this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
    EXPECT_FLOAT_EQ(this->sensor_.state, 25.0f);
    const unsigned submits = this->bus_.submits;

    this->sensor_.update();
    this->bus_.finish(status);
    EXPECT_TRUE(std::isnan(this->sensor_.state)) << static_cast<int>(status);
    EXPECT_TRUE(this->sensor_.status_has_warning()) << static_cast<int>(status);
    run_timers(WAIT_10_BIT_MS + 30);
    EXPECT_TRUE(this->bus_.queue.empty()) << static_cast<int>(status);
    EXPECT_EQ(this->bus_.submits, submits + 1) << "only the conversion, no read";
  }
  EXPECT_EQ(this->publications_, 10u);
}

TEST_F(DallasAsync, TheWaitStartsWhenTheConversionIsDone) {
  this->sensor_.update();
  run_timers(WAIT_10_BIT_MS - 30);
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
  run_timers(60);  // Past the wait if it had started at update()
  EXPECT_TRUE(this->bus_.queue.empty());
  run_timers(WAIT_10_BIT_MS);
  EXPECT_EQ(this->bus_.queue.size(), 1u);
}

TEST_F(DallasAsync, ReadOkPublishesTheTemperature) {
  this->run_until_reading_();
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);

  EXPECT_EQ(this->publications_, 1u);
  EXPECT_FLOAT_EQ(this->sensor_.state, 25.0f);
  EXPECT_FALSE(this->sensor_.status_has_warning());
}

TEST_F(DallasAsync, ReadOkWithABadChecksumPublishesNan) {
  this->bus_.scratch_pad[8] ^= 0xFF;
  this->run_until_reading_();
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);

  EXPECT_EQ(this->publications_, 1u);
  EXPECT_TRUE(std::isnan(this->sensor_.state));
  EXPECT_TRUE(this->sensor_.status_has_warning());
}

TEST_F(DallasAsync, TheResetValueOf85IsDropped) {
  this->set_scratch_pad_(85 * 16, 0x10);
  this->run_until_reading_();
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);

  EXPECT_EQ(this->publications_, 1u);
  EXPECT_TRUE(std::isnan(this->sensor_.state));
}

TEST_F(DallasAsync, EveryFailedReadPublishesNanWithAWarning) {
  for (const auto status :
       {TransactionStatus::TRANSACTION_STATUS_NO_PRESENCE, TransactionStatus::TRANSACTION_STATUS_BUS_LOW,
        TransactionStatus::TRANSACTION_STATUS_CAPTURE_ERROR, TransactionStatus::TRANSACTION_STATUS_TIMEOUT,
        TransactionStatus::TRANSACTION_STATUS_HARDWARE_ERROR}) {
    // A good reading first: valid bytes are left in the buffer and must not be taken for this one
    this->run_until_reading_();
    this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
    EXPECT_FLOAT_EQ(this->sensor_.state, 25.0f);

    this->run_until_reading_();
    this->bus_.finish(status);
    EXPECT_TRUE(std::isnan(this->sensor_.state)) << static_cast<int>(status);
    EXPECT_TRUE(this->sensor_.status_has_warning()) << static_cast<int>(status);
  }
  EXPECT_EQ(this->publications_, 10u);
}

TEST_F(DallasAsync, UpdateDuringTheConversionSkipsTheCycle) {
  this->sensor_.update();
  const Transaction *convert = this->bus_.queue.front();
  this->sensor_.update();

  EXPECT_EQ(this->bus_.queue.size(), 1u);
  EXPECT_EQ(this->bus_.queue.front(), convert);
  EXPECT_TRUE(convert->is_pending());

  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
  run_timers(WAIT_10_BIT_MS + 30);
  ASSERT_EQ(this->bus_.queue.size(), 1u) << "exactly one read for the two updates";
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
  EXPECT_EQ(this->publications_, 1u);
}

TEST_F(DallasAsync, UpdateDuringTheWaitStartsTheWaitAgain) {
  this->sensor_.update();
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
  this->sensor_.update();  // The new conversion is held up on a busy bus
  ASSERT_EQ(this->bus_.queue.size(), 1u);

  run_timers(WAIT_10_BIT_MS + 30);  // Past the wait of the first conversion
  ASSERT_EQ(this->bus_.queue.size(), 1u) << "no read right behind the new conversion";
  EXPECT_EQ(*this->bus_.queue.front()->write_data, CONVERT);

  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
  run_timers(WAIT_10_BIT_MS - 30);
  EXPECT_TRUE(this->bus_.queue.empty()) << "the read waits for the new conversion";
  run_timers(60);
  ASSERT_EQ(this->bus_.queue.size(), 1u);
  EXPECT_EQ(*this->bus_.queue.front()->write_data, READ_SCRATCH_PAD);
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
  EXPECT_EQ(this->publications_, 1u);
  EXPECT_FLOAT_EQ(this->sensor_.state, 25.0f);
}

TEST_F(DallasAsync, AFailedConversionDuringTheWaitEndsTheCycle) {
  this->sensor_.update();
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
  this->sensor_.update();
  const unsigned submits = this->bus_.submits;
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_NO_PRESENCE);
  EXPECT_TRUE(std::isnan(this->sensor_.state));

  run_timers(WAIT_10_BIT_MS + 30);
  EXPECT_TRUE(this->bus_.queue.empty()) << "the wait of the first conversion must not read";
  EXPECT_EQ(this->bus_.submits, submits);
  EXPECT_EQ(this->publications_, 1u);
  EXPECT_TRUE(std::isnan(this->sensor_.state));
  EXPECT_TRUE(this->sensor_.status_has_warning());
}

TEST_F(DallasAsync, UpdateDuringTheReadSkipsTheCycleAndLeavesTheReadAlone) {
  this->run_until_reading_();
  const Transaction *read = this->bus_.queue.front();
  const Transaction before = *read;
  const unsigned submits = this->bus_.submits;

  this->sensor_.update();

  EXPECT_EQ(this->bus_.submits, submits);
  ASSERT_EQ(this->bus_.queue.size(), 1u);
  EXPECT_EQ(this->bus_.queue.front(), read);
  EXPECT_TRUE(read->is_pending());
  EXPECT_EQ(read->address, before.address);
  EXPECT_EQ(read->write_data, before.write_data);
  EXPECT_EQ(read->write_len, before.write_len);
  EXPECT_EQ(read->read_data, before.read_data);
  EXPECT_EQ(read->read_len, before.read_len);
  EXPECT_EQ(read->on_done, before.on_done);
  EXPECT_EQ(read->context, before.context);

  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);
  EXPECT_EQ(this->publications_, 1u);
  EXPECT_FLOAT_EQ(this->sensor_.state, 25.0f);
  run_timers(WAIT_10_BIT_MS + 30);
  EXPECT_TRUE(this->bus_.queue.empty());
}

TEST_F(DallasAsync, ASkippedCycleKeepsTheWarning) {
  this->sensor_.update();
  this->sensor_.status_set_warning();
  this->sensor_.update();  // The conversion is still pending

  EXPECT_TRUE(this->sensor_.status_has_warning());
  EXPECT_EQ(this->bus_.queue.size(), 1u);
}

TEST_F(DallasAsync, AGoodCycleClearsTheWarningOfTheCycleBefore) {
  this->run_until_reading_();
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
  EXPECT_TRUE(std::isnan(this->sensor_.state));
  EXPECT_TRUE(this->sensor_.status_has_warning());

  this->run_until_reading_();
  this->bus_.finish(TransactionStatus::TRANSACTION_STATUS_OK);

  EXPECT_EQ(this->publications_, 2u);
  EXPECT_FLOAT_EQ(this->sensor_.state, 25.0f);
  EXPECT_FALSE(this->sensor_.status_has_warning());
}

TEST_F(DallasAsync, ABusThatFinishesInsideSubmitWorksToo) {
  this->bus_.run_at_once = true;
  this->sensor_.update();
  EXPECT_EQ(this->publications_, 0u) << "the wait still applies";
  run_timers(WAIT_10_BIT_MS + 30);

  EXPECT_TRUE(this->bus_.queue.empty());
  EXPECT_EQ(this->publications_, 1u);
  EXPECT_FLOAT_EQ(this->sensor_.state, 25.0f);
}

TEST_F(DallasAsync, AFailureInsideSubmitKeepsItsWarning) {
  this->bus_.run_at_once = true;
  this->bus_.run_at_once_status = TransactionStatus::TRANSACTION_STATUS_NO_PRESENCE;
  this->sensor_.update();

  EXPECT_EQ(this->publications_, 1u);
  EXPECT_TRUE(std::isnan(this->sensor_.state));
  EXPECT_TRUE(this->sensor_.status_has_warning());
}

TEST_F(DallasAsync, AFailedReadInSetupSetsTheWarning) {
  DallasTemperatureSensor sensor;
  sensor.set_one_wire_bus(&this->bus_);
  sensor.set_address(ROM);
  sensor.set_resolution(10);
  this->bus_.blocking = true;
  this->bus_.presence = 0;
  sensor.setup();
  this->bus_.blocking = false;

  EXPECT_TRUE(sensor.status_has_warning());
}

}  // namespace esphome::dallas_temp::testing
