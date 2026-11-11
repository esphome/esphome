#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/keeloq/keeloq.h"
#include "esphome/components/logger/logger.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::keeloq::testing {

namespace {

// Made-up serial and the device key that codegen derives from the made-up manufacturer key
// 0x1A2B3C4D5E6F7081 (checked in test_keeloq.py). Not a real key.
constexpr uint32_t SERIAL = 0x1234500;
constexpr uint32_t DEVICE_KEY_HIGH = 0x109E9228;
constexpr uint32_t DEVICE_KEY_LOW = 0x9CAB2950;
// A neighbour's serial and its device key from the same manufacturer key (checked in test_keeloq.py).
constexpr uint32_t NEIGHBOUR_SERIAL = 0x7654300;
constexpr uint32_t NEIGHBOUR_KEY_HIGH = 0xDDFB7151;
constexpr uint32_t NEIGHBOUR_KEY_LOW = 0xFFB17E07;
// Where a remote without serial stores what it took over (made from type and id in the generated code).
constexpr uint32_t TAKE_OVER_KEY = 0x12345678;

struct Remote {
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, SERIAL, DEVICE_KEY_HIGH, DEVICE_KEY_LOW};

  // Restores what an earlier Remote stored, like a reboot.
  Remote() { static_cast<Component &>(this->remote).setup(); }
};

// What would go on air: the frame, how often it is sent and the wait between the copies.
struct Sent {
  remote_base::KeeloqData frame;
  uint32_t send_times;
  uint32_t send_wait;
};

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  std::vector<Sent> frames;
  // When set, the next send starts a learn of channel 2 on this remote, as an on_transmit automation can.
  KeeloqRemote *learn_while_sending{nullptr};

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
    this->frames.push_back({*data, send_times, send_wait});
    if (this->learn_while_sending != nullptr) {
      KeeloqRemote *remote = this->learn_while_sending;
      this->learn_while_sending = nullptr;  // once
      remote->learn(this, 0x0002);
    }
  }
};

// A remote and its transmitter for the learn tests. The destructor drops a stop frame that is still pending when an
// assertion ends a test early, so the scheduler never runs it on a remote that is gone.
struct Pairing {
  CapturingTransmitter transmitter;
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, SERIAL, DEVICE_KEY_HIGH, DEVICE_KEY_LOW};

  ~Pairing() {
    App.scheduler.cancel_timeout(&this->remote, uint32_t{0});  // 0 is the learn timeout's id
    App.scheduler.call(millis());
  }
};

// As codegen sets up a remote with receiver_id: frames of every serial decrypt with the made-up manufacturer key.
void listen(KeeloqRemote &remote) { remote.set_manufacturer_key(0x1A2B3C4D, 0x5E6F7081); }

// As codegen sets up a remote without serial: it takes serial, counter and channels over from a hand remote.
void without_serial(KeeloqRemote &remote) {
  listen(remote);
  remote.set_take_over_key(TAKE_OVER_KEY);
}

// Sends a frame and returns it as a receiver captures it: from the first mark, the guard merged in.
remote_base::RawTimings on_air(const remote_base::KeeloqData &data) {
  remote_base::RemoteTransmitData tx;
  remote_base::KeeloqProtocol().encode(&tx, data);
  remote_base::RawTimings merged;
  for (int32_t item : tx.get_data()) {
    if (merged.empty() && item < 0) {
      continue;
    }
    if (!merged.empty() && (merged.back() < 0) == (item < 0)) {
      merged.back() += item;
    } else {
      merged.push_back(item);
    }
  }
  return merged;
}

bool receive(KeeloqRemote &remote, const remote_base::RawTimings &timings) {
  return remote.on_receive(remote_base::RemoteReceiveData(timings, 25, remote_base::TOLERANCE_MODE_PERCENTAGE));
}

// Returns what the step logged. The test logger only prints while it has a baud rate.
template<typename F> std::string log_of(F step) {
  const uint32_t baud_rate = logger::global_logger->get_baud_rate();
  logger::global_logger->set_baud_rate(115200);
  ::testing::internal::CaptureStdout();
  step();
  std::string log = ::testing::internal::GetCapturedStdout();
  logger::global_logger->set_baud_rate(baud_rate);
  return log;
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
    EXPECT_EQ(decrypt(v.cipher, key), v.plain);
  }
}

TEST(KeeloqRemoteTest, JaroliftFramesMatchTheReferenceFirmware) {
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
  {
    Remote before;
    before.remote.set_counter(100);
  }
  Remote after;
  EXPECT_EQ(after.remote.get_counter(), 100);
}

TEST(KeeloqRemoteTest, ReservedBlockWrapsToo) {
  // Receivers compare counters modulo 2^16: 0xFFFC reserves 0xFFFC to 0x0003, so a reboot continues at 4.
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

TEST(KeeloqRemoteTest, SendTransmitsTheNextFrame) {
  // Up for channel 1, sent 3 times with 10 ms between the copies, as repeat sets them.
  CapturingTransmitter transmitter;
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, SERIAL, DEVICE_KEY_HIGH, DEVICE_KEY_LOW};
  remote.set_counter(5);
  remote.send(&transmitter, 0x8, 0x0001, 3, 10000);
  ASSERT_EQ(transmitter.frames.size(), 1u);
  EXPECT_EQ(transmitter.frames[0].frame.command, 0x8);
  EXPECT_EQ(transmitter.frames[0].send_times, 3u);
  EXPECT_EQ(transmitter.frames[0].send_wait, 10000u);
  EXPECT_EQ(remote.get_counter(), 6);  // the frame took counter 5
}

TEST(KeeloqRemoteTest, ChannelMaskZeroSendsNothing) {
  // The schema rejects a mask of 0, but a channels lambda can return it.
  CapturingTransmitter transmitter;
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, SERIAL, DEVICE_KEY_HIGH, DEVICE_KEY_LOW};
  remote.send(&transmitter, 0x8, 0x0000, 2, 0);
  remote.learn(&transmitter, 0x0000);
  EXPECT_TRUE(transmitter.frames.empty());
  EXPECT_EQ(remote.get_counter(), 0);  // no counter used up
  // No stop frame is pending: learn(0) started nothing (the learn timeout's id is 0)
  EXPECT_FALSE(App.scheduler.cancel_timeout(&remote, uint32_t{0}));
}

TEST(KeeloqRemoteTest, JaroliftLearnSendsUpDownThenStop) {
  // As other Jarolift implementations pair: 0xA twice, a pause, then 0x4 twice, each with a new counter.
  Pairing p;
  p.remote.set_counter(10);
  p.remote.learn(&p.transmitter, 0x0001);
  ASSERT_EQ(p.transmitter.frames.size(), 1u);
  delay(1010);  // NOLINT
  App.scheduler.call(millis());
  ASSERT_EQ(p.transmitter.frames.size(), 2u);
  EXPECT_EQ(p.transmitter.frames[0].frame.command, 0xA);
  EXPECT_EQ(p.transmitter.frames[1].frame.command, 0x4);
  for (const Sent &sent : p.transmitter.frames) {
    EXPECT_EQ(sent.frame.address, SERIAL);
    EXPECT_EQ(sent.send_times, 2u);
  }
  EXPECT_EQ(p.remote.get_counter(), 12);
}

TEST(KeeloqRemoteTest, LearnWhileOneIsPendingIsSkipped) {
  Pairing p;
  p.remote.learn(&p.transmitter, 0x0001);
  p.remote.learn(&p.transmitter, 0x0002);
  delay(1010);  // NOLINT
  App.scheduler.call(millis());
  ASSERT_EQ(p.transmitter.frames.size(), 2u);
  EXPECT_EQ(p.transmitter.frames[1].frame.command, 0x4);
  // The first pairing finished, so the next learn runs.
  p.remote.learn(&p.transmitter, 0x0002);
  EXPECT_EQ(p.transmitter.frames.size(), 3u);
  // It finishes too.
  delay(1010);  // NOLINT
  App.scheduler.call(millis());
  ASSERT_EQ(p.transmitter.frames.size(), 4u);
  EXPECT_EQ(p.transmitter.frames[3].frame.command, 0x4);
}

TEST(KeeloqRemoteTest, LearnStartedWhileSendingIsSkipped) {
  // An on_transmit automation that starts another learn runs inside the first frame's send.
  Pairing p;
  p.transmitter.learn_while_sending = &p.remote;
  p.remote.learn(&p.transmitter, 0x0001);
  delay(1010);  // NOLINT
  App.scheduler.call(millis());
  // Only the first learn went out: up+down, then stop.
  ASSERT_EQ(p.transmitter.frames.size(), 2u);
  EXPECT_EQ(p.transmitter.frames[0].frame.command, 0xA);
  EXPECT_EQ(p.transmitter.frames[1].frame.command, 0x4);
}

TEST(KeeloqRemoteTest, ARemoteWithoutSerialDoesNotLearn) {
  CapturingTransmitter transmitter;
  KeeloqRemote clone{KEELOQ_TYPE_JAROLIFT, 0, 0, 0};
  without_serial(clone);
  clone.learn(&transmitter, 0x0001);
  EXPECT_TRUE(transmitter.frames.empty());
}

TEST(KeeloqRemoteTest, ARemoteWithoutSerialSendsNothingBeforeATakeOver) {
  CapturingTransmitter transmitter;
  KeeloqRemote clone{KEELOQ_TYPE_JAROLIFT, 0, 0, 0};
  without_serial(clone);
  static_cast<Component &>(clone).setup();
  clone.send(&transmitter, 0x8, 0x0001, 2, 0);
  EXPECT_TRUE(transmitter.frames.empty());
  EXPECT_EQ(clone.get_counter(), 0);  // no counter used up
}

TEST(KeeloqReceiveTest, ReportsSerialCounterAndChannelsOfAFrame) {
  Remote hand;
  hand.remote.set_counter(0x2048);
  remote_base::RawTimings timings = on_air(hand.remote.next_frame(0x8, 0x0F0F));

  // Any serial decrypts with the manufacturer key, so the hand remote's serial and counter can be read off.
  KeeloqRemote listener{KEELOQ_TYPE_JAROLIFT, NEIGHBOUR_SERIAL, NEIGHBOUR_KEY_HIGH, NEIGHBOUR_KEY_LOW};
  listen(listener);
  KeeloqFrame frame{};
  listener.add_on_frame_callback([&frame](KeeloqFrame x) { frame = x; });
  ASSERT_TRUE(receive(listener, timings));
  EXPECT_EQ(frame.serial, SERIAL);
  EXPECT_EQ(frame.counter, 0x2048);
  EXPECT_EQ(frame.channels, 0x0F0F);
  EXPECT_EQ(frame.command, 0x8);
}

TEST(KeeloqReceiveTest, IgnoresAFrameOfAnotherManufacturerKey) {
  // A device key that the manufacturer key does not give for this serial, as on a remote of another make.
  KeeloqRemote stranger{KEELOQ_TYPE_JAROLIFT, SERIAL, 0x01020304, 0x05060708};
  remote_base::RawTimings timings = on_air(stranger.next_frame(0x8, 0x0001));
  Remote r;
  EXPECT_FALSE(receive(r.remote, timings));
}

TEST(KeeloqReceiveTest, FollowsTheCounterOfItsSerialForwardOnly) {
  // The hand remote with the same serial sent 120, so ESPHome continues at 121. An older 90 changes nothing.
  Remote hand;
  Remote r;
  r.remote.set_counter(100);
  hand.remote.set_counter(120);
  receive(r.remote, on_air(hand.remote.next_frame(0x8, 0x0001)));
  EXPECT_EQ(r.remote.get_counter(), 121);
  hand.remote.set_counter(90);
  receive(r.remote, on_air(hand.remote.next_frame(0x8, 0x0001)));
  EXPECT_EQ(r.remote.get_counter(), 121);
}

TEST(KeeloqReceiveTest, TakeOverStoresSerialAndCounterAndSurvivesARestart) {
  Remote hand;
  hand.remote.set_counter(0x2048);
  remote_base::RawTimings timings = on_air(hand.remote.next_frame(0x8, 0x0001));

  // No serial in the configuration: nothing to send until it has taken one over.
  KeeloqRemote clone{KEELOQ_TYPE_JAROLIFT, 0, 0, 0};
  without_serial(clone);
  static_cast<Component &>(clone).setup();
  EXPECT_FALSE(clone.ready_to_send());
  clone.take_over_next_frame();
  ASSERT_TRUE(receive(clone, timings));
  EXPECT_TRUE(clone.ready_to_send());
  EXPECT_EQ(clone.get_serial(), SERIAL);
  EXPECT_EQ(clone.get_counter(), 0x2049);
  // It sends what the hand remote would send next.
  hand.remote.set_counter(0x2049);
  EXPECT_EQ(clone.next_frame(0x8, 0x0001).encrypted, hand.remote.next_frame(0x8, 0x0001).encrypted);

  // After a restart (or an update with the same id) both are back.
  KeeloqRemote restarted{KEELOQ_TYPE_JAROLIFT, 0, 0, 0};
  without_serial(restarted);
  static_cast<Component &>(restarted).setup();
  EXPECT_TRUE(restarted.ready_to_send());
  EXPECT_EQ(restarted.get_serial(), SERIAL);
  EXPECT_EQ(restarted.get_counter(), 0x2049 + 8);  // after the block reserved for the frame above
}

TEST(KeeloqReceiveTest, TakeOverKeepsTheHandRemotesChannelsUnlessConfigured) {
  // The hand remote sends for channels 1 and 9 (0x0101); that becomes the default for `command: up` alone.
  Remote hand;
  remote_base::RawTimings timings = on_air(hand.remote.next_frame(0x8, 0x0101));

  KeeloqRemote clone{KEELOQ_TYPE_JAROLIFT, 0, 0, 0};
  without_serial(clone);
  static_cast<Component &>(clone).setup();
  clone.take_over_next_frame();
  receive(clone, timings);
  EXPECT_EQ(clone.get_channels(), 0x0101);
  KeeloqRemote restarted{KEELOQ_TYPE_JAROLIFT, 0, 0, 0};
  without_serial(restarted);
  static_cast<Component &>(restarted).setup();
  EXPECT_EQ(restarted.get_channels(), 0x0101);

  // A channels in the configuration wins, like a configured serial.
  global_preferences->reset();  // another device
  KeeloqRemote configured{KEELOQ_TYPE_JAROLIFT, 0, 0, 0};
  without_serial(configured);
  configured.set_channels(0x00F0);
  static_cast<Component &>(configured).setup();
  configured.take_over_next_frame();
  receive(configured, timings);
  EXPECT_EQ(configured.get_channels(), 0x00F0);
  App.scheduler.cancel_timeout(&clone, uint32_t{1});
}

TEST(KeeloqReceiveTest, DoesNotTakeOverWithoutBeingAsked) {
  Remote hand;
  KeeloqRemote clone{KEELOQ_TYPE_JAROLIFT, 0, 0, 0};
  without_serial(clone);
  static_cast<Component &>(clone).setup();
  receive(clone, on_air(hand.remote.next_frame(0x8, 0x0001)));
  EXPECT_FALSE(clone.ready_to_send());
}

TEST(KeeloqReceiveTest, SetCounterBeforeATakeOverStoresNoSerial) {
  // Nothing taken over yet, so set_counter stores nothing: after a restart the remote still waits for a take-over
  // instead of sending with serial 0.
  KeeloqRemote clone{KEELOQ_TYPE_JAROLIFT, 0, 0, 0};
  without_serial(clone);
  static_cast<Component &>(clone).setup();
  clone.set_counter(5);

  KeeloqRemote restarted{KEELOQ_TYPE_JAROLIFT, 0, 0, 0};
  without_serial(restarted);
  static_cast<Component &>(restarted).setup();
  EXPECT_FALSE(restarted.ready_to_send());
}

TEST(KeeloqReceiveTest, SetCounterBeforeATakeOverWarns) {
  // The counter only lives in RAM until the next take-over replaces it, and the log says so.
  KeeloqRemote clone{KEELOQ_TYPE_JAROLIFT, 0, 0, 0};
  without_serial(clone);
  static_cast<Component &>(clone).setup();
  std::string log = log_of([&clone]() { clone.set_counter(5); });
  EXPECT_NE(log.find("No serial taken over yet"), std::string::npos) << log;
}

TEST(KeeloqRemoteTest, SetCounterWithASerialLogsNoWarning) {
  Remote r;
  std::string log = log_of([&r]() { r.remote.set_counter(5); });
  EXPECT_EQ(log.find("No serial taken over yet"), std::string::npos) << log;
}

TEST(KeeloqReceiveTest, WithASerialTakeOverTakesOnlyTheCounterOfThatSerial) {
  // ESPHome was set to 5000 by mistake. A neighbour's remote is ignored, then the own hand remote at 0x2048 counts.
  KeeloqRemote neighbour{KEELOQ_TYPE_JAROLIFT, NEIGHBOUR_SERIAL, NEIGHBOUR_KEY_HIGH, NEIGHBOUR_KEY_LOW};
  Remote hand;
  Remote r;
  listen(r.remote);
  r.remote.set_counter(5000);
  r.remote.take_over_next_frame();
  neighbour.set_counter(0x0100);
  ASSERT_TRUE(receive(r.remote, on_air(neighbour.next_frame(0x8, 0x0001))));  // it decrypts, but is not its serial
  EXPECT_EQ(r.remote.get_counter(), 5000);
  hand.remote.set_counter(0x2048);
  receive(r.remote, on_air(hand.remote.next_frame(0x8, 0x0001)));
  EXPECT_EQ(r.remote.get_serial(), SERIAL);
  EXPECT_EQ(r.remote.get_counter(), 0x2049);
  EXPECT_EQ(r.remote.get_channels(), 0);  // only the counter, no channels
  // Stored at once: a restart continues ahead of the hand remote's 0x2048, inside the 16-code window.
  Remote restarted;
  uint16_t ahead = restarted.remote.get_counter() - 0x2048;
  EXPECT_GE(ahead, 1);
  EXPECT_LE(ahead, 16);
}

// A motor-like listener for SERIAL; it counts the presses on_command would report.
struct Listener {
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, SERIAL, DEVICE_KEY_HIGH, DEVICE_KEY_LOW};
  std::vector<KeeloqFrame> commands;
  Listener() {
    listen(this->remote);
    static_cast<Component &>(this->remote).setup();
    this->remote.add_on_command_callback([this](KeeloqFrame x) { this->commands.push_back(x); });
  }
};

// The hand remote, as another device with the same serial and key.
struct HandRemote {
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, SERIAL, DEVICE_KEY_HIGH, DEVICE_KEY_LOW};
  explicit HandRemote(uint16_t counter) { this->remote.set_counter(counter); }
  remote_base::KeeloqData next_frame(uint8_t command, uint16_t channels) {
    return this->remote.next_frame(command, channels);
  }
  void set_counter(uint16_t counter) { this->remote.set_counter(counter); }
};

TEST(KeeloqReceiveTest, FollowedCounterSurvivesARestart) {
  // The hand remote with the same serial pressed 100 to 119; ESPHome only heard them. After a restart it
  // continues ahead of 119 and inside the 16-code window, not at its own reserved block.
  HandRemote hand(100);
  {
    Listener before;
    for (int i = 0; i < 20; i++) {
      receive(before.remote, on_air(hand.next_frame(0x8, 0x0001)));
    }
  }
  Listener after;
  uint16_t ahead = after.remote.get_counter() - 119;
  EXPECT_GE(ahead, 1);
  EXPECT_LE(ahead, 16);
}

TEST(KeeloqCommandTest, ANewPressFiresOnceEvenWhenSentTwice) {
  HandRemote hand(100);
  Listener motor;
  remote_base::RawTimings press = on_air(hand.next_frame(0x8, 0x0001));
  receive(motor.remote, press);
  receive(motor.remote, press);  // Jarolift sends each press twice with the same counter
  ASSERT_EQ(motor.commands.size(), 1u);
  EXPECT_EQ(motor.commands[0].counter, 100);
  EXPECT_EQ(motor.commands[0].command, 0x8);
}

TEST(KeeloqCommandTest, ARecordedOlderPressDoesNotFire) {
  HandRemote hand(100);
  Listener motor;
  remote_base::RawTimings recorded = on_air(hand.next_frame(0x8, 0x0001));  // counter 100
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));              // counter 101
  receive(motor.remote, recorded);
  ASSERT_EQ(motor.commands.size(), 1u);
  EXPECT_EQ(motor.commands[0].counter, 101);
}

TEST(KeeloqCommandTest, SixteenAheadFiresAtOnce) {
  // Counter 100 accepted; 116 is the edge of the 16-code window, so one press counts.
  Listener motor;
  HandRemote hand(100);
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  hand.set_counter(116);
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  EXPECT_EQ(motor.commands.size(), 2u);
}

TEST(KeeloqCommandTest, FarAheadNeedsTwoPressesInSequence) {
  // 1000 codes ahead is outside the 16-code window: the receiver waits for the next code in sequence.
  Listener motor;
  HandRemote hand(100);
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  hand.set_counter(1100);
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  EXPECT_EQ(motor.commands.size(), 1u);
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));  // 1101 follows 1100
  EXPECT_EQ(motor.commands.size(), 2u);
}

TEST(KeeloqCommandTest, BeyondTheResyncWindowNeverFires) {
  // 100 + 0x8000 is half the counter range away: as far behind as ahead, so it counts as old.
  Listener motor;
  HandRemote hand(100);
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  hand.set_counter(100 + 0x8000);
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  EXPECT_EQ(motor.commands.size(), 1u);
}

TEST(KeeloqCommandTest, ReplayProtectionSurvivesARestart) {
  HandRemote hand(100);
  remote_base::RawTimings recorded;
  {
    Listener before;
    for (int i = 0; i < 9; i++) {  // 100 to 108: the block of 8 forces a flash write
      recorded = on_air(hand.next_frame(0x8, 0x0001));
      receive(before.remote, recorded);
    }
  }
  Listener after;
  receive(after.remote, recorded);
  EXPECT_TRUE(after.commands.empty());
}

TEST(KeeloqCommandTest, FailedSaveOfAPressWarnsAndStillFires) {
  // Without setup() the motor has no storage, so saving the accepted press fails.
  HandRemote hand(100);
  KeeloqRemote motor{KEELOQ_TYPE_JAROLIFT, SERIAL, DEVICE_KEY_HIGH, DEVICE_KEY_LOW};
  int presses = 0;
  motor.add_on_command_callback([&presses](KeeloqFrame) { presses++; });
  remote_base::RawTimings press = on_air(hand.next_frame(0x8, 0x0001));
  std::string log = log_of([&]() { receive(motor, press); });
  EXPECT_NE(log.find("Could not save the last press"), std::string::npos) << log;
  EXPECT_EQ(presses, 1);  // on_command still fires
}

TEST(KeeloqCommandTest, SavedPressLogsNoWarning) {
  HandRemote hand(100);
  Listener motor;
  remote_base::RawTimings press = on_air(hand.next_frame(0x8, 0x0001));
  std::string log = log_of([&]() { receive(motor.remote, press); });
  EXPECT_EQ(log.find("Could not save the last press"), std::string::npos) << log;
}

TEST(KeeloqCommandTest, FiltersByChannelAndCommand) {
  // Up for channels 1 and 3 (channels 0x0005).
  KeeloqFrame up{.serial = SERIAL, .counter = 100, .channels = 0x0005, .command = 0x8};
  EXPECT_TRUE(command_matches(up, ANY_CHANNEL, ANY_COMMAND));
  EXPECT_TRUE(command_matches(up, 3, ANY_COMMAND));
  EXPECT_FALSE(command_matches(up, 2, ANY_COMMAND));
  EXPECT_TRUE(command_matches(up, 1, 0x8));
  EXPECT_FALSE(command_matches(up, 1, 0x2));
}

TEST(KeeloqCommandTest, OtherSerialsDoNotFire) {
  KeeloqRemote neighbour{KEELOQ_TYPE_JAROLIFT, NEIGHBOUR_SERIAL, NEIGHBOUR_KEY_HIGH, NEIGHBOUR_KEY_LOW};
  Listener motor;
  ASSERT_TRUE(receive(motor.remote, on_air(neighbour.next_frame(0x8, 0x0001))));  // it decrypts
  EXPECT_TRUE(motor.commands.empty());
}

}  // namespace esphome::keeloq::testing
