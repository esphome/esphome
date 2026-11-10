#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "esphome/components/keeloq/keeloq.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"

namespace esphome::keeloq::testing {

namespace {

// Made-up serial and the device key that codegen derives from the made-up manufacturer key
// 0x1A2B3C4D5E6F7081 (checked in test_keeloq.py). Not a real key.
constexpr uint32_t SERIAL = 0x1234500;
constexpr uint32_t DEVICE_KEY_HIGH = 0x109E9228;
constexpr uint32_t DEVICE_KEY_LOW = 0x9CAB2950;

struct Remote {
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, SERIAL, DEVICE_KEY_HIGH, DEVICE_KEY_LOW};

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

void forget_stored_counters() {
  if (global_preferences != nullptr) {
    global_preferences->reset();
  }
}

// Test-only decryption, the inverse of encrypt(); production only encrypts.
uint32_t decrypt(uint32_t data, uint64_t key) {
  uint32_t x = data;
  for (uint16_t r = 0; r < 528; r++) {
    uint32_t idx = (x >> 0 & 1) | (x >> 8 & 1) << 1 | (x >> 19 & 1) << 2 | (x >> 25 & 1) << 3 | (x >> 30 & 1) << 4;
    uint32_t nlf = (0x3A5C742Eu >> idx) & 1;
    x = (x << 1) | ((x >> 31 & 1) ^ (x >> 15 & 1) ^ nlf ^ static_cast<uint32_t>(key >> ((15u - r) & 63u) & 1));
  }
  return x;
}

}  // namespace

TEST(KeeloqCipherTest, MatchesPublishedVectors) {
  // Published vectors of two KeeLoq implementations: plain, key, cipher.
  struct Vector {
    uint32_t plain;
    uint64_t key;
    uint32_t cipher;
  };
  static constexpr Vector VECTORS[] = {
      {0xF741E2DB, 0x5CEC6701B79FD949, 0xE44F4CDF},
      {0x32CCD896, 0xEA125C50361424B1, 0x69EBBA91},
      {0xBB4A4F8E, 0xEA125C50361424B1, 0x32CCD896},
  };
  for (const auto &v : VECTORS) {
    const uint32_t key[2] = {static_cast<uint32_t>(v.key), static_cast<uint32_t>(v.key >> 32)};
    EXPECT_EQ(encrypt(v.plain, key), v.cipher);
    EXPECT_EQ(decrypt(v.cipher, v.key), v.plain);
  }
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

TEST(KeeloqRemoteTest, ReservedBlockWrapsToo) {
  // Receivers compare counters modulo 2^16: 0xFFFC reserves 0xFFFC to 0x0003, so a reboot continues at 4.
  forget_stored_counters();
  {
    Remote before;
    before.remote.set_counter(0xFFFC);
    for (int i = 0; i < 4; i++) {
      before.remote.next_frame(0x8, 0x0001);
    }
    EXPECT_EQ(before.remote.get_counter(), 0);
  }
  Remote after;
  EXPECT_EQ(after.remote.get_counter(), 4);
}

TEST(KeeloqRemoteTest, FailedSaveStillSends) {
  // Without setup() the remote has no storage, so saving the counter fails.
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, SERIAL, DEVICE_KEY_HIGH, DEVICE_KEY_LOW};
  remote.next_frame(0x8, 0x0001);
  EXPECT_EQ(remote.get_counter(), 1);
}

TEST(KeeloqRemoteTest, JaroliftLearnSendsUpDownThenStop) {
  // As other Jarolift implementations pair: 0xA twice, a pause, then 0x4 twice, each with a new counter.
  forget_stored_counters();
  CapturingTransmitter transmitter;
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, SERIAL, DEVICE_KEY_HIGH, DEVICE_KEY_LOW};
  remote.set_counter(10);
  remote.learn(&transmitter, 0x0001);
  ASSERT_EQ(transmitter.frames.size(), 1u);
  delay(1010);  // NOLINT
  App.scheduler.call(millis());
  ASSERT_EQ(transmitter.frames.size(), 2u);
  EXPECT_EQ(transmitter.frames[0].first.command, 0xA);
  EXPECT_EQ(transmitter.frames[1].first.command, 0x4);
  for (const auto &[data, send_times] : transmitter.frames) {
    EXPECT_EQ(data.address, SERIAL);
    EXPECT_EQ(send_times, 2u);
  }
  EXPECT_EQ(remote.get_counter(), 12);
}

TEST(KeeloqRemoteTest, LearnWhileOneIsPendingIsSkipped) {
  forget_stored_counters();
  CapturingTransmitter transmitter;
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, SERIAL, DEVICE_KEY_HIGH, DEVICE_KEY_LOW};
  remote.learn(&transmitter, 0x0001);
  remote.learn(&transmitter, 0x0002);
  delay(1010);  // NOLINT
  App.scheduler.call(millis());
  ASSERT_EQ(transmitter.frames.size(), 2u);
  EXPECT_EQ(transmitter.frames[1].first.command, 0x4);
  // The first pairing finished, so the next learn runs.
  remote.learn(&transmitter, 0x0002);
  EXPECT_EQ(transmitter.frames.size(), 3u);
}

}  // namespace esphome::keeloq::testing
