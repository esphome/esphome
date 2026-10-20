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
// Where a remote without serial stores what it took over (made from type and id in the generated code).
constexpr uint32_t TAKE_OVER_KEY = 0x12345678;

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

// take_over_next_frame() starts a timeout, which needs the application's scheduler.
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

TEST(KeeloqRemoteTest, ARemoteWithoutSerialDoesNotLearn) {
  forget_stored_counters();
  CapturingTransmitter transmitter;
  KeeloqRemote clone{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, 0};
  clone.set_take_over_key(TAKE_OVER_KEY);
  clone.learn(&transmitter, 0x0001, false);
  EXPECT_TRUE(transmitter.frames.empty());
}

TEST(KeeloqReceiveTest, ReportsSerialCounterAndChannelsOfAFrame) {
  forget_stored_counters();
  Remote hand;
  hand.remote.set_counter(0x2048);
  remote_base::RawTimings timings = on_air(hand.remote.next_frame(0x8, 0x0F0F));

  // Any serial decrypts with the manufacturer key, so the hand remote's serial and counter can be read off.
  KeeloqRemote listener{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, 0x7654300};
  KeeloqFrame frame{};
  listener.add_on_frame_callback([&frame](KeeloqFrame x) { frame = x; });
  ASSERT_TRUE(receive(listener, timings));
  EXPECT_EQ(frame.serial, SERIAL);
  EXPECT_EQ(frame.counter, 0x2048);
  EXPECT_EQ(frame.channels, 0x0F0F);
  EXPECT_EQ(frame.command, 0x8);
}

TEST(KeeloqReceiveTest, IgnoresAFrameOfAnotherManufacturerKey) {
  forget_stored_counters();
  KeeloqRemote stranger{KEELOQ_TYPE_JAROLIFT, 0x0102030405060708, SERIAL};
  remote_base::RawTimings timings = on_air(stranger.next_frame(0x8, 0x0001));
  Remote r;
  EXPECT_FALSE(receive(r.remote, timings));
}

TEST(KeeloqReceiveTest, FollowsTheCounterOfItsSerialForwardOnly) {
  // The hand remote with the same serial sent 120, so ESPHome continues at 121. An older 90 changes nothing.
  forget_stored_counters();
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
  construct_app();
  forget_stored_counters();
  Remote hand;
  hand.remote.set_counter(0x2048);
  remote_base::RawTimings timings = on_air(hand.remote.next_frame(0x8, 0x0001));

  // No serial in the configuration: nothing to send until it has taken one over.
  KeeloqRemote clone{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, 0};
  clone.set_take_over_key(TAKE_OVER_KEY);
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
  KeeloqRemote restarted{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, 0};
  restarted.set_take_over_key(TAKE_OVER_KEY);
  static_cast<Component &>(restarted).setup();
  EXPECT_TRUE(restarted.ready_to_send());
  EXPECT_EQ(restarted.get_serial(), SERIAL);
  EXPECT_EQ(restarted.get_counter(), 0x2049 + 8);  // after the block reserved for the frame above
}

TEST(KeeloqReceiveTest, TakeOverKeepsTheHandRemotesChannelsUnlessConfigured) {
  // The hand remote sends for channels 1 and 9 (0x0101); that becomes the default for `command: up` alone.
  construct_app();
  forget_stored_counters();
  Remote hand;
  remote_base::RawTimings timings = on_air(hand.remote.next_frame(0x8, 0x0101));

  KeeloqRemote clone{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, 0};
  clone.set_take_over_key(TAKE_OVER_KEY);
  static_cast<Component &>(clone).setup();
  clone.take_over_next_frame();
  receive(clone, timings);
  EXPECT_EQ(clone.get_channels(), 0x0101);
  uint16_t channels = 0;
  EXPECT_TRUE(clone.resolve_channels(false, 0, channels));  // 3. the taken-over channels
  EXPECT_EQ(channels, 0x0101);
  KeeloqRemote restarted{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, 0};
  restarted.set_take_over_key(TAKE_OVER_KEY);
  static_cast<Component &>(restarted).setup();
  EXPECT_EQ(restarted.get_channels(), 0x0101);

  // A channels in the configuration wins, like a configured serial.
  forget_stored_counters();
  KeeloqRemote configured{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, 0};
  configured.set_take_over_key(TAKE_OVER_KEY);
  configured.set_channels(0x00F0);
  static_cast<Component &>(configured).setup();
  configured.take_over_next_frame();
  receive(configured, timings);
  EXPECT_EQ(configured.get_channels(), 0x00F0);
  App.scheduler.cancel_timeout(&clone, uint32_t{1});
}

TEST(KeeloqRemoteTest, ChannelsPriority) {
  // 1. the action's channels, 2. the remote's configured channels, 3. the taken-over channels, 4. none: nothing is
  // sent.
  forget_stored_counters();
  uint16_t channels = 0;
  KeeloqRemote none{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, SERIAL};
  EXPECT_TRUE(none.resolve_channels(true, 0x0001, channels));
  EXPECT_EQ(channels, 0x0001);
  EXPECT_FALSE(none.resolve_channels(false, 0, channels));

  KeeloqRemote configured{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, SERIAL};
  configured.set_channels(0x00F0);
  EXPECT_TRUE(configured.resolve_channels(true, 0x0001, channels));
  EXPECT_EQ(channels, 0x0001);
  EXPECT_TRUE(configured.resolve_channels(false, 0, channels));
  EXPECT_EQ(channels, 0x00F0);
}

TEST(KeeloqReceiveTest, DoesNotTakeOverWithoutBeingAsked) {
  forget_stored_counters();
  Remote hand;
  KeeloqRemote clone{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, 0};
  clone.set_take_over_key(TAKE_OVER_KEY);
  static_cast<Component &>(clone).setup();
  receive(clone, on_air(hand.remote.next_frame(0x8, 0x0001)));
  EXPECT_FALSE(clone.ready_to_send());
}

TEST(KeeloqReceiveTest, WithASerialTakeOverTakesOnlyTheCounterOfThatSerial) {
  // ESPHome was set to 5000 by mistake. A neighbour's remote is ignored, then the own hand remote at 0x2048 counts.
  construct_app();
  forget_stored_counters();
  KeeloqRemote neighbour{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, 0x7654300};
  Remote hand;
  Remote r;
  r.remote.set_counter(5000);
  r.remote.take_over_next_frame();
  neighbour.set_counter(0x0100);
  receive(r.remote, on_air(neighbour.next_frame(0x8, 0x0001)));
  EXPECT_EQ(r.remote.get_counter(), 5000);
  hand.remote.set_counter(0x2048);
  receive(r.remote, on_air(hand.remote.next_frame(0x8, 0x0001)));
  EXPECT_EQ(r.remote.get_serial(), SERIAL);
  EXPECT_EQ(r.remote.get_counter(), 0x2049);
  // Stored at once: a restart continues ahead of the hand remote's 0x2048, inside the 16-code window.
  Remote restarted;
  uint16_t ahead = restarted.remote.get_counter() - 0x2048;
  EXPECT_GE(ahead, 1);
  EXPECT_LE(ahead, 16);
}

// A motor-like listener for SERIAL; it counts the presses on_command would report.
struct Listener {
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, SERIAL};
  std::vector<KeeloqFrame> commands;
  Listener() {
    static_cast<Component &>(this->remote).setup();
    this->remote.add_on_command_callback([this](KeeloqFrame x) { this->commands.push_back(x); });
  }
};

// The hand remote, as another device with the same serial and key.
struct HandRemote {
  KeeloqRemote remote{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, SERIAL};
  explicit HandRemote(uint16_t counter) { this->remote.set_counter(counter); }
  remote_base::KeeloqData next_frame(uint8_t command, uint16_t channels) {
    return this->remote.next_frame(command, channels);
  }
  void set_counter(uint16_t counter) { this->remote.set_counter(counter); }
};

TEST(KeeloqReceiveTest, FollowedCounterSurvivesARestart) {
  // The hand remote with the same serial pressed 100 to 119; ESPHome only heard them. After a restart it
  // continues ahead of 119 and inside the 16-code window, not at its own reserved block.
  forget_stored_counters();
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
  forget_stored_counters();
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
  forget_stored_counters();
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
  forget_stored_counters();
  Listener motor;
  HandRemote hand(100);
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  hand.set_counter(116);
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  EXPECT_EQ(motor.commands.size(), 2u);
}

TEST(KeeloqCommandTest, FarAheadNeedsTwoPressesInSequence) {
  // 1000 codes ahead is outside the 16-code window: the receiver waits for the next code in sequence.
  forget_stored_counters();
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
  forget_stored_counters();
  Listener motor;
  HandRemote hand(100);
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  hand.set_counter(100 + 0x8000);
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  receive(motor.remote, on_air(hand.next_frame(0x8, 0x0001)));
  EXPECT_EQ(motor.commands.size(), 1u);
}

TEST(KeeloqCommandTest, ReplayProtectionSurvivesARestart) {
  forget_stored_counters();
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
  forget_stored_counters();
  KeeloqRemote neighbour{KEELOQ_TYPE_JAROLIFT, MANUFACTURER_KEY, 0x7654300};
  Listener motor;
  receive(motor.remote, on_air(neighbour.next_frame(0x8, 0x0001)));
  EXPECT_TRUE(motor.commands.empty());
}

}  // namespace esphome::keeloq::testing
