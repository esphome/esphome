// The blocking default of OneWireBus::submit(), which a bit-bang bus uses in a build that also has an RMT bus.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/one_wire/one_wire_bus.h"

namespace esphome::one_wire::testing {

constexpr uint64_t ROM = 0x120000065C17EE28ULL;
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
    done->pending_during_call = done->transaction->pending_;
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
  EXPECT_EQ(this->done_.calls, std::vector<TransactionStatus>{TransactionStatus::OK});
}

TEST_F(SubmitDefault, AddressZeroIsSkipRom) {
  this->transaction_.address = 0;
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  const Ops expected{RESET, 0xCC, 0x4E, 0x01, 0x02, READ, READ, READ};
  EXPECT_EQ(this->bus_.ops, expected);
  EXPECT_EQ(this->done_.calls, std::vector<TransactionStatus>{TransactionStatus::OK});
}

TEST_F(SubmitDefault, BytesArriveInBusOrder) {
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  EXPECT_EQ(this->read_[0], 0xA0);
  EXPECT_EQ(this->read_[1], 0xA1);
  EXPECT_EQ(this->read_[2], 0xA2);
}

TEST_F(SubmitDefault, NoPresenceTouchesNothingAfterTheReset) {
  this->bus_.presence = 0;
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  EXPECT_EQ(this->bus_.ops, Ops{RESET});
  EXPECT_EQ(this->done_.calls, std::vector<TransactionStatus>{TransactionStatus::NO_PRESENCE});
  EXPECT_EQ(this->read_[0], 0);
}

TEST_F(SubmitDefault, HeldLowReadsAsNoPresence) {
  this->bus_.presence = -1;
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  EXPECT_EQ(this->bus_.ops, Ops{RESET});
  EXPECT_EQ(this->done_.calls, std::vector<TransactionStatus>{TransactionStatus::NO_PRESENCE});
}

TEST_F(SubmitDefault, WithoutCallback) {
  this->transaction_.on_done = nullptr;
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  EXPECT_FALSE(this->transaction_.pending_);
  EXPECT_EQ(this->bus_.ops.size(), 9u);
}

TEST_F(SubmitDefault, RejectsAPendingTransaction) {
  this->transaction_.pending_ = true;
  EXPECT_FALSE(this->bus_.submit(&this->transaction_));

  EXPECT_TRUE(this->bus_.ops.empty());
  EXPECT_TRUE(this->done_.calls.empty());
  EXPECT_TRUE(this->transaction_.pending_);
}

TEST_F(SubmitDefault, IsFreeAgainWhenTheCallbackRuns) {
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));

  EXPECT_FALSE(this->done_.pending_during_call);
  EXPECT_TRUE(this->bus_.submit(&this->transaction_));
  EXPECT_EQ(this->done_.calls.size(), 2u);
}

}  // namespace esphome::one_wire::testing
