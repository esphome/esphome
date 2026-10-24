#include <gtest/gtest.h>

#include <vector>

#include "esphome/components/remote_base/remote_base.h"

namespace esphome::remote_base::testing {

class RecordingDumper : public RemoteReceiverDumperBase {
 public:
  RecordingDumper(std::vector<int> *calls, int id, bool matches, bool secondary = false)
      : calls_(calls), id_(id), matches_(matches), secondary_(secondary) {}
  bool dump(RemoteReceiveData /*src*/) override {
    this->calls_->push_back(this->id_);
    return this->matches_;
  }
  bool is_secondary() override { return this->secondary_; }

 protected:
  std::vector<int> *calls_;
  int id_;
  bool matches_;
  bool secondary_;
};

class TestReceiver : public RemoteReceiverBase {
 public:
  TestReceiver() : RemoteReceiverBase(nullptr) {}
  void receive() { this->call_listeners_dumpers_(); }
};

TEST(RemoteReceiverDumpers, PrimariesRunInOrderAndAMatchSkipsTheSecondary) {
  std::vector<int> calls;
  RecordingDumper raw(&calls, 0, true, true), first(&calls, 1, false), second(&calls, 2, true), third(&calls, 3, false);
  TestReceiver receiver;
  receiver.register_dumper(&raw);
  receiver.register_dumper(&first);
  receiver.register_dumper(&second);
  receiver.register_dumper(&third);
  receiver.receive();
  EXPECT_EQ(calls, (std::vector<int>{1, 2, 3}));
}

TEST(RemoteReceiverDumpers, SecondaryRunsWhenNoPrimaryMatched) {
  std::vector<int> calls;
  RecordingDumper raw(&calls, 0, true, true), miss(&calls, 1, false);
  TestReceiver receiver;
  receiver.register_dumper(&miss);
  receiver.register_dumper(&raw);
  receiver.receive();
  EXPECT_EQ(calls, (std::vector<int>{1, 0}));
}

TEST(RemoteReceiverDumpers, OnlyTheFirstSecondaryRuns) {
  std::vector<int> calls;
  RecordingDumper raw(&calls, 0, true, true), extra_raw(&calls, 9, true, true);
  TestReceiver receiver;
  receiver.register_dumper(&raw);
  receiver.register_dumper(&extra_raw);
  receiver.receive();
  EXPECT_EQ(calls, (std::vector<int>{0}));
}

// External components (vevor decoders) register themselves from their own setup()
TEST(RemoteReceiverDumpers, RegisteringAfterConstructionAddsTheDumper) {
  std::vector<int> calls;
  RecordingDumper early(&calls, 1, false), late(&calls, 2, false);
  TestReceiver receiver;
  receiver.register_dumper(&early);
  receiver.receive();
  receiver.register_dumper(&late);
  receiver.receive();
  EXPECT_EQ(calls, (std::vector<int>{1, 1, 2}));
}

TEST(RemoteReceiverDumpers, CodegenTableKeepsTheSameDispatch) {
  std::vector<int> calls;
  RecordingDumper raw(&calls, 0, true, true), miss(&calls, 1, false), extra_raw(&calls, 9, true, true);
  RemoteReceiverDumperBase *const table[] = {&raw, &miss, &extra_raw};
  TestReceiver receiver;
  receiver.set_dumpers(table, 3);
  receiver.receive();
  EXPECT_EQ(calls, (std::vector<int>{1, 0}));
}

TEST(RemoteReceiverDumpers, RegisterDumperKeepsTheCodegenTable) {
  std::vector<int> calls;
  RecordingDumper early(&calls, 1, false), from_table(&calls, 2, false), late(&calls, 3, false);
  TestReceiver receiver;
  receiver.register_dumper(&early);  // an external component registering from to_code()
  RemoteReceiverDumperBase *const table[] = {&from_table};
  receiver.set_dumpers(table, 1);
  receiver.register_dumper(&late);  // or from its own setup()
  receiver.receive();
  EXPECT_EQ(calls, (std::vector<int>{1, 2, 3}));
}

}  // namespace esphome::remote_base::testing
