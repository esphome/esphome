// The blocking default of OneWireBus::submit(), which a bit-bang bus uses in a build that also has a background bus,
// and the TransactionQueue that a background bus keeps its pending transactions in.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>
#include <vector>

#include "esphome/components/one_wire/one_wire_bus.h"

namespace esphome::one_wire::testing {

constexpr uint64_t ROM = 0x120000065C17EE28ULL;

// Drivers keep their transactions as members built with designated initializers
static_assert(std::is_aggregate_v<Transaction>);
const std::vector<uint8_t> COMMAND{0x4E, 0x01, 0x02};

// What went over the bus. Below 256: a written byte.
enum Op : int { RESET = 256, WRITE_ROM, WRITE_OTHER_WORD, READ };
using Ops = std::vector<int>;

/// Records what goes over the bus, and answers reads with 0xA0, 0xA1, ...
class FakeBus final : public OneWireBus {
 public:
  int presence{1};
  Ops ops;

  void write8(uint8_t val) override { this->ops.push_back(val); }
  void write64(uint64_t val) override { this->ops.push_back(val == ROM ? WRITE_ROM : WRITE_OTHER_WORD); }
  uint8_t read8() override {
    this->ops.push_back(READ);
    return this->next_read_++;
  }
  uint64_t read64() override {
    ADD_FAILURE() << "submit() reads byte by byte";
    return 0;
  }

 protected:
  int reset_int() override {
    this->ops.push_back(RESET);
    return this->presence;
  }
  void reset_search() override {}
  uint64_t search_int() override { return 0; }

  uint8_t next_read_{0xA0};
};

struct Done {
  std::vector<TransactionStatus> calls;
  bool pending_during_call{false};
  Transaction *transaction{nullptr};

  static void record(void *context, TransactionStatus status) {
    auto *done = static_cast<Done *>(context);
    done->calls.push_back(status);
    done->pending_during_call = done->transaction->is_pending();
  }
};

class SubmitDefault : public ::testing::Test {
 protected:
  FakeBus bus_;
  Done done_;
  uint8_t read_[3]{0, 0, 0};
  Transaction transaction_;

  void SetUp() override {
    this->transaction_.address = ROM;
    this->transaction_.write_data = COMMAND.data();
    this->transaction_.write_len = COMMAND.size();
    this->transaction_.read_data = this->read_;
    this->transaction_.read_len = sizeof(this->read_);
    this->transaction_.on_done = Done::record;
    this->transaction_.context = &this->done_;
    this->done_.transaction = &this->transaction_;
  }
};

TEST_F(SubmitDefault, MatchRomThenWriteThenRead) {
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  const Ops expected{RESET, 0x55, WRITE_ROM, 0x4E, 0x01, 0x02, READ, READ, READ};
  EXPECT_EQ(this->bus_.ops, expected);
  EXPECT_EQ(this->done_.calls, std::vector<TransactionStatus>{TransactionStatus::TRANSACTION_STATUS_OK});
}

TEST_F(SubmitDefault, AddressZeroIsSkipRom) {
  this->transaction_.address = 0;
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  const Ops expected{RESET, 0xCC, 0x4E, 0x01, 0x02, READ, READ, READ};
  EXPECT_EQ(this->bus_.ops, expected);
  EXPECT_EQ(this->done_.calls, std::vector<TransactionStatus>{TransactionStatus::TRANSACTION_STATUS_OK});
}

TEST_F(SubmitDefault, BytesArriveInBusOrder) {
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  EXPECT_EQ(this->read_[0], 0xA0);
  EXPECT_EQ(this->read_[1], 0xA1);
  EXPECT_EQ(this->read_[2], 0xA2);
}

struct ResetCase {
  int presence;
  TransactionStatus status;
};

class SubmitDefaultReset : public SubmitDefault, public ::testing::WithParamInterface<ResetCase> {};

TEST_P(SubmitDefaultReset, FailedResetTouchesNothingAfterTheReset) {
  this->bus_.presence = GetParam().presence;
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  EXPECT_EQ(this->bus_.ops, Ops{RESET});
  EXPECT_EQ(this->done_.calls, std::vector<TransactionStatus>{GetParam().status});
  EXPECT_EQ(this->read_[0], 0);
}

INSTANTIATE_TEST_SUITE_P(Presence, SubmitDefaultReset,
                         ::testing::Values(ResetCase{0, TransactionStatus::TRANSACTION_STATUS_NO_PRESENCE},
                                           ResetCase{-1, TransactionStatus::TRANSACTION_STATUS_BUS_LOW}));

TEST_F(SubmitDefault, WithoutCallback) {
  this->transaction_.on_done = nullptr;
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  EXPECT_FALSE(this->transaction_.is_pending());
  EXPECT_EQ(this->bus_.ops.size(), 9u);
}

TEST_F(SubmitDefault, RejectsAPendingTransaction) {
  TransactionQueue queue;  // Still queued on another bus
  ASSERT_TRUE(queue.push(&this->transaction_));
  EXPECT_FALSE(this->bus_.submit(&this->transaction_));

  EXPECT_TRUE(this->bus_.ops.empty());
  EXPECT_TRUE(this->done_.calls.empty());
  EXPECT_TRUE(this->transaction_.is_pending());
}

TEST_F(SubmitDefault, IsFreeAgainWhenTheCallbackRuns) {
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  EXPECT_FALSE(this->done_.pending_during_call);
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));
  EXPECT_EQ(this->done_.calls.size(), 2u);
}

TEST(TransactionQueue, FirstInFirstOut) {
  Transaction a, b, c;
  TransactionQueue queue;
  EXPECT_TRUE(queue.empty());
  EXPECT_TRUE(queue.push(&a));
  EXPECT_TRUE(queue.push(&b));
  EXPECT_EQ(queue.pop(), &a);
  EXPECT_TRUE(queue.push(&c));
  EXPECT_EQ(queue.front(), &b);
  EXPECT_EQ(queue.pop(), &b);
  EXPECT_EQ(queue.pop(), &c);
  EXPECT_TRUE(queue.empty());
}

TEST(TransactionQueue, DesignatedInitializerLeavesItIdle) {
  uint8_t command = 0x44;
  Transaction convert{.write_data = &command, .write_len = 1};
  EXPECT_FALSE(convert.is_pending());
  TransactionQueue queue;
  EXPECT_TRUE(queue.push(&convert));
  EXPECT_EQ(queue.pop(), &convert);
}

TEST(TransactionQueue, PendingUntilPopped) {
  Transaction a;
  TransactionQueue queue;
  EXPECT_TRUE(queue.push(&a));
  EXPECT_TRUE(a.is_pending());
  EXPECT_FALSE(queue.push(&a));
  EXPECT_EQ(queue.pop(), &a);
  EXPECT_FALSE(a.is_pending());
  EXPECT_TRUE(queue.empty());
  EXPECT_TRUE(queue.push(&a));  // Can be queued again, e.g. from on_done
  EXPECT_EQ(queue.front(), &a);
}

}  // namespace esphome::one_wire::testing
