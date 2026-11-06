#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "esphome/components/keeloq/keeloq.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

#include <new>

namespace esphome::keeloq::testing {

namespace {

// Made-up key and serial from published Jarolift test vectors. Not a real key.
constexpr uint64_t MANUFACTURER_KEY = 0x1A2B3C4D5E6F7081;
constexpr uint32_t SERIAL = 0x1234500;

struct Remote {
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, SERIAL};

  // Restores what an earlier Remote stored, like a reboot.
  Remote() { static_cast<Component &>(this->remote).setup(); }
};

// Keeps what would go on air: the frame and how often it is sent.
class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  std::vector<std::pair<remote_base::KeeloqData, uint32_t>> frames;

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    remote_base::RawTimings timings = this->temp_.get_data();
    timings.erase(timings.begin());  // a receiver starts at the first mark
    auto data = remote_base::KeeloqProtocol().decode(
        remote_base::RemoteReceiveData(timings, 25, remote_base::TOLERANCE_MODE_PERCENTAGE));
    if (!data.has_value()) {
      ADD_FAILURE() << "not a KeeLoq frame";
      return;
    }
    this->frames.emplace_back(*data, send_times);
  }
};

// Runs the second half of the pairing sequence at once instead of after the pause.
class LearningRemote : public KeeloqRemote {
 public:
  LearningRemote() : KeeloqRemote(KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, SERIAL) {}
  using KeeloqRemote::finish_learn_;
};

void forget_stored_counters() {
  if (global_preferences != nullptr) {
    global_preferences->reset();
  }
}

// learn() starts a timeout, which needs the application's scheduler.
void construct_app() {
  static const bool CONSTRUCTED = (new (&App) Application(), true);
  (void) CONSTRUCTED;
}

}  // namespace

TEST(KeeloqCipherTest, MatchesPublishedVector) {
  // A published KeeLoq test vector.
  EXPECT_EQ(encrypt(0xF741E2DB, 0x5CEC6701B79FD949), 0xE44F4CDFu);
  EXPECT_EQ(decrypt(0xE44F4CDF, 0x5CEC6701B79FD949), 0xF741E2DBu);
}

TEST(KeeloqCipherTest, MatchesASecondImplementation) {
  // A published vector of another KeeLoq implementation.
  EXPECT_EQ(encrypt(0x32CCD896, 0xEA125C50361424B1), 0x69EBBA91u);
  EXPECT_EQ(decrypt(0x32CCD896, 0xEA125C50361424B1), 0xBB4A4F8Eu);
}

TEST(KeeloqRemoteTest, JaroliftFramesMatchTheReferenceFirmware) {
  forget_stored_counters();
  Remote r;
  // A group UP (0x8) for channels 1-4 and 9-12 (mask 0x0F0F) at counter 0x2048.
  r.remote.set_counter(0x2048);
  remote_base::KeeloqData frame = r.remote.next_frame(0x8, 0x0F0F);
  EXPECT_EQ(frame.encrypted, 0xF94A09F2u);
  EXPECT_EQ(frame.address, SERIAL);
  EXPECT_EQ(frame.command, 0x8);
  EXPECT_EQ(frame.group(), 0x0F);
  // Case c0: channel 1 UP (mask 0x0001) at counter 0x1000.
  r.remote.set_counter(0x1000);
  remote_base::KeeloqData channel = r.remote.next_frame(0x8, 0x0001);
  EXPECT_EQ(channel.encrypted, 0x04E3B802u);
  EXPECT_EQ(channel.group(), 0x00);
}

TEST(KeeloqRemoteTest, EachFrameTakesTheNextCounter) {
  forget_stored_counters();
  Remote r;
  r.remote.set_counter(5);
  remote_base::KeeloqData first = r.remote.next_frame(0x8, 0x0001);
  remote_base::KeeloqData second = r.remote.next_frame(0x8, 0x0001);
  EXPECT_EQ(r.remote.get_counter(), 7);
  EXPECT_NE(first.encrypted, second.encrypted);
}

TEST(KeeloqRemoteTest, CounterWrapsFrom0xFFFFTo0) {
  // Receivers compare counters modulo 2^16, so 0 follows 0xFFFF.
  forget_stored_counters();
  Remote r;
  r.remote.set_counter(0xFFFF);
  r.remote.next_frame(0x8, 0x0001);
  EXPECT_EQ(r.remote.get_counter(), 0);
}

TEST(KeeloqRemoteTest, RebootContinuesAfterTheReservedBlock) {
  // Three frames send 100, 101 and 102; 100 to 107 were reserved in flash before the first one.
  // After a reboot the next frame is 108: no code repeats and the jump stays below 16.
  forget_stored_counters();
  {
    Remote before;
    before.remote.set_counter(100);
    for (int i = 0; i < 3; i++) {
      before.remote.next_frame(0x8, 0x0001);
    }
  }
  Remote after;
  EXPECT_EQ(after.remote.get_counter(), 108);
}

TEST(KeeloqRemoteTest, RebootWithoutFramesKeepsTheCounter) {
  forget_stored_counters();
  {
    Remote before;
    before.remote.set_counter(100);
  }
  Remote after;
  EXPECT_EQ(after.remote.get_counter(), 100);
}

TEST(KeeloqRemoteTest, StoredCounterBelongsToTypeAndSerialOnly) {
  // Not to the id or the build: a new firmware for the same serial finds the counter again.
  EXPECT_EQ(KeeloqRemote::preference_key(KEELOQ_TYPE_JAROLIFT, SERIAL),
            KeeloqRemote::preference_key(KEELOQ_TYPE_JAROLIFT, SERIAL));
  EXPECT_NE(KeeloqRemote::preference_key(KEELOQ_TYPE_JAROLIFT, SERIAL),
            KeeloqRemote::preference_key(KEELOQ_TYPE_JAROLIFT, 0x7654300));
}

TEST(KeeloqRemoteTest, ReservedBlockWrapsToo) {
  // 0xFFFC reserves 0xFFFC to 0x0003, so a reboot continues at 4.
  forget_stored_counters();
  {
    Remote before;
    before.remote.set_counter(0xFFFC);
    before.remote.next_frame(0x8, 0x0001);
  }
  Remote after;
  EXPECT_EQ(after.remote.get_counter(), 4);
}

TEST(KeeloqRemoteTest, JaroliftLearnSendsUpDownThenStop) {
  // As other Jarolift implementations pair: 0xA twice, a pause, then 0x4 twice, each with a new counter.
  construct_app();
  forget_stored_counters();
  CapturingTransmitter transmitter;
  LearningRemote remote;
  remote.set_counter(10);
  remote.learn(&transmitter, 0x0001, false);
  ASSERT_EQ(transmitter.frames.size(), 1u);
  remote.finish_learn_(&transmitter, 0x0001);
  ASSERT_EQ(transmitter.frames.size(), 2u);
  EXPECT_EQ(transmitter.frames[0].first.command, 0xA);
  EXPECT_EQ(transmitter.frames[1].first.command, 0x4);
  for (const auto &[data, send_times] : transmitter.frames) {
    EXPECT_EQ(data.address, SERIAL);
    EXPECT_EQ(send_times, 2u);
  }
  EXPECT_EQ(remote.get_counter(), 12);
  App.scheduler.cancel_timeout(&remote, uint32_t{0});  // the remote ends with the test
}

TEST(KeeloqRemoteTest, JaroliftLegacyLearnSendsOneCommand) {
  // Motors made before 2010 learn from command 0x1, sent twice.
  forget_stored_counters();
  CapturingTransmitter transmitter;
  LearningRemote remote;
  remote.learn(&transmitter, 0x0001, true);
  ASSERT_EQ(transmitter.frames.size(), 1u);
  EXPECT_EQ(transmitter.frames[0].first.command, 0x1);
  EXPECT_EQ(transmitter.frames[0].second, 2u);
}

TEST(KeeloqRemoteTest, JaroliftSendsEachPressTwice) {
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, SERIAL};
  EXPECT_EQ(remote.default_send_times(), 2u);
}

}  // namespace esphome::keeloq::testing
