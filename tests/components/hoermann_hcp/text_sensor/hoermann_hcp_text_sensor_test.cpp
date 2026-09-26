#include <gtest/gtest.h>

#include <string>
#include <thread>

#include "esphome/components/text_sensor/text_sensor.h"

#include "../common.h"

namespace esphome::hoermann_hcp::testing {

namespace {

// Made up. 26 bytes on the wire: the first 14 arrive in one transfer, the other 12 in the next.
constexpr const char *SERIAL = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
// Made up as well, padded with a space to the 12 bytes the motor sends.
constexpr const char *FIRMWARE = "FW-TEST 1.0 ";

constexpr uint8_t SUB_SERIAL = 0x0C;
constexpr uint8_t SUB_FIRMWARE = 0x0D;
constexpr uint8_t FIRST_HALF = 0x80;

// A status poll as the bus controller writes it: its counter in the high byte, command 0x03 in the low.
RegisterValues status_poll(HoermannHcp &door, uint8_t counter = 0x05) {
  return status_answer(door, static_cast<uint16_t>((counter << 8) | 0x03));
}

// The write half of a payload transfer: the motor writes the bytes into the command block.
void write_transfer(HoermannHcp &door, uint8_t counter, uint8_t sub_code, const char *bytes, size_t len) {
  RegisterValues written;
  written.push_back(static_cast<uint16_t>((counter << 8) | 0x04));
  written.push_back(static_cast<uint16_t>(sub_code << 8));
  for (size_t i = 0; i < len; i += 2) {
    written.push_back(
        static_cast<uint16_t>((static_cast<uint8_t>(bytes[i]) << 8) | static_cast<uint8_t>(bytes[i + 1])));
  }
  door.on_write_registers(COMMAND_REG, written);
}

// A whole payload transfer, returning the answer the motor reads back.
RegisterValues transfer(HoermannHcp &door, uint8_t counter, uint8_t sub_code, const char *bytes, size_t len) {
  write_transfer(door, counter, sub_code, bytes, len);
  RegisterValues response;
  door.on_read_holding_registers(STATE_REG, 8, response);
  return response;
}

void send_serial(HoermannHcp &door) {
  transfer(door, FIRST_HALF | 0x05, SUB_SERIAL, SERIAL, 14);
  transfer(door, 0x06, SUB_SERIAL, SERIAL + 14, 12);
}

// A hub with both sensors configured, which is what makes it ask.
struct IdentityFixture {
  IdentityFixture() {
    this->door.set_serial_number_text_sensor(&this->serial);
    this->door.set_version_text_sensor(&this->version);
  }
  // What the sensors show once the loop has turned; empty while they have no state.
  std::string serial_shown() {
    this->door.update();
    return this->serial.has_state() ? this->serial.get_state() : "";
  }
  std::string version_shown() {
    this->door.update();
    return this->version.has_state() ? this->version.get_state() : "";
  }

  TestableHoermannHcp door;
  text_sensor::TextSensor serial;
  text_sensor::TextSensor version;
};

// The whole exchange as the motor runs it, with the loop turning in between as it would.
void run_identity_exchange(HoermannHcp &door) {
  status_poll(door, 0x03);
  status_poll(door, 0x04);
  send_serial(door);
  door.update();
  status_poll(door, 0x07);
  transfer(door, 0x08, SUB_FIRMWARE, FIRMWARE, 12);
  door.update();
}

}  // namespace

// With no sensor configured, polls and transfers are answered exactly as before.
TEST(HoermannHcpTextSensorTest, NothingChangesWithoutASensor) {
  HoermannHcp door;
  for (int poll = 0; poll < 2; poll++) {
    const RegisterValues response = status_poll(door);
    ASSERT_EQ(response.size(), 8u);
    EXPECT_EQ(response[1], 0x0301);
    EXPECT_EQ(response[2], 0x0000);
  }
  const RegisterValues answer = transfer(door, FIRST_HALF | 0x05, SUB_SERIAL, SERIAL, 14);
  EXPECT_EQ(answer[1] & 0x00FF, 0x0001);
}

// Like Hoermann's own bus accessory, the first status poll gets an ordinary answer and the next one carries the
// request, echoing the status command like any status answer. It is asked again only once 30 s have passed.
TEST(HoermannHcpTextSensorTest, SerialNumberIsAskedForAfterOneOrdinaryAnswer) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  EXPECT_EQ(status_poll(door)[1], 0x0301);

  const RegisterValues response = status_poll(door);
  ASSERT_EQ(response.size(), 8u);
  EXPECT_EQ(response[0], 0x0500);
  EXPECT_EQ(response[1], 0x0322);
  EXPECT_EQ(response[2], 0x0500);

  door.identity_asked_at_ -= 29000;
  EXPECT_EQ(status_poll(door)[1], 0x0301);
  door.identity_asked_at_ -= 2000;
  EXPECT_EQ(status_poll(door)[1], 0x0322);
}

// Three attempts at the serial number, then the firmware version is asked for anyway, three times as well.
TEST(HoermannHcpTextSensorTest, GivesUpAfterThreeAttemptsEach) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);
  for (int attempt = 0; attempt < 3; attempt++) {
    const RegisterValues response = status_poll(door);
    EXPECT_EQ(response[1], 0x0322);
    EXPECT_EQ(response[2], 0x0500);
    door.identity_asked_at_ -= 31000;
  }
  EXPECT_EQ(status_poll(door)[1], 0x0301);
  for (int attempt = 0; attempt < 3; attempt++) {
    const RegisterValues response = status_poll(door);
    EXPECT_EQ(response[1], 0x0322);
    EXPECT_EQ(response[2], 0x0600);
    door.identity_asked_at_ -= 31000;
  }
  EXPECT_EQ(status_poll(door)[1], 0x0301);
  EXPECT_EQ(door.identity_request_, 0);
}

// A first half left behind by a serial number that never completed is not shown.
TEST(HoermannHcpTextSensorTest, HalfASerialNumberIsNeverShown) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);
  transfer(door, FIRST_HALF | 0x05, SUB_SERIAL, SERIAL, 14);
  for (int attempt = 0; attempt < 4; attempt++) {
    door.identity_asked_at_ -= 31000;
    status_poll(door);
  }
  EXPECT_EQ(fixture.serial_shown(), "");
}

// Each half is acknowledged with the counter it came with, minus the half marker. The serial number is shown as
// soon as it is whole, and the firmware version is asked for right after.
TEST(HoermannHcpTextSensorTest, SerialNumberInTwoHalvesThenTheFirmwareVersion) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);

  RegisterValues answer = transfer(door, FIRST_HALF | 0x05, SUB_SERIAL, SERIAL, 14);
  ASSERT_EQ(answer.size(), 8u);
  EXPECT_EQ(answer[0], 0x0500);
  EXPECT_EQ(answer[1], 0x04FD);
  EXPECT_EQ(fixture.serial_shown(), "");

  answer = transfer(door, 0x06, SUB_SERIAL, SERIAL + 14, 12);
  EXPECT_EQ(answer[0], 0x0600);
  EXPECT_EQ(answer[1], 0x04FD);
  EXPECT_EQ(fixture.serial_shown(), SERIAL);
  EXPECT_EQ(fixture.version_shown(), "");

  const RegisterValues response = status_poll(door);
  EXPECT_EQ(response[1], 0x0322);
  EXPECT_EQ(response[2], 0x0600);

  answer = transfer(door, 0x07, SUB_FIRMWARE, FIRMWARE, 12);
  EXPECT_EQ(answer[1], 0x04FD);
  EXPECT_EQ(fixture.version_shown(), "FW-TEST 1.0");
}

// Once both values are in, nothing more is asked, however long the device keeps running.
TEST(HoermannHcpTextSensorTest, FinishedExchangeStaysFinished) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  run_identity_exchange(door);

  door.identity_asked_at_ -= 31000;
  EXPECT_EQ(status_poll(door)[1], 0x0301);
  EXPECT_EQ(door.identity_request_, 0);
}

// The text ends at the first byte that is not printable. 0xFF is below the printable range where char is signed,
// as on the host, and above it where char is unsigned, as on most targets. DEL is above it either way.
TEST(HoermannHcpTextSensorTest, PaddingEndsTheText) {
  for (const char pad : {'\xFF', '\x7F'}) {
    IdentityFixture fixture;
    auto &door = fixture.door;
    status_poll(door);
    const char first[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', pad};
    const char second[] = {pad, pad, pad, pad, pad, pad, pad, pad, pad, pad, pad, pad};
    transfer(door, FIRST_HALF | 0x05, SUB_SERIAL, first, sizeof(first));
    transfer(door, 0x06, SUB_SERIAL, second, sizeof(second));
    EXPECT_EQ(fixture.serial_shown(), "1234567890123");
  }
}

// A half that cannot be used is still acknowledged but not kept, so the request stays open for the retry: a
// first half too short, a second half too short, a second half without a first.
TEST(HoermannHcpTextSensorTest, UnusableSerialHalvesAreNotKept) {
  {
    IdentityFixture fixture;
    auto &door = fixture.door;
    status_poll(door);
    EXPECT_EQ(transfer(door, FIRST_HALF | 0x05, SUB_SERIAL, SERIAL, 12)[1], 0x04FD);
    transfer(door, 0x06, SUB_SERIAL, SERIAL + 14, 12);
    EXPECT_EQ(fixture.serial_shown(), "");
    EXPECT_EQ(door.identity_request_, 0x05);
  }
  {
    IdentityFixture fixture;
    auto &door = fixture.door;
    status_poll(door);
    transfer(door, FIRST_HALF | 0x05, SUB_SERIAL, SERIAL, 14);
    transfer(door, 0x06, SUB_SERIAL, SERIAL + 14, 10);
    EXPECT_EQ(fixture.serial_shown(), "");
    EXPECT_EQ(door.identity_request_, 0x05);
  }
}

// Older motors (index B1 seen) send the whole serial number in one frame, without the half marker. The bytes are
// the ones a B1 sent, with the serial number made up.
TEST(HoermannHcpTextSensorTest, SerialNumberInOneFrameThenTheFirmwareVersion) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);
  status_poll(door, 0x03);
  const char one_frame[12] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', 'B', '1', 0};
  EXPECT_EQ(transfer(door, 0x05, SUB_SERIAL, one_frame, 12)[1], 0x04FD);
  EXPECT_EQ(door.identity_request_, 0x06);
  EXPECT_EQ(fixture.serial_shown(), "123456789B1");
  auto answer = status_poll(door, 0x06);
  EXPECT_EQ(answer[1], 0x0322);
  EXPECT_EQ(answer[2], 0x0600);
}

// After a frame with the half marker, one without it can only be the second half, even if the first was unusable.
TEST(HoermannHcpTextSensorTest, ASecondHalfIsNeverTakenForTheWholeNumber) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);
  transfer(door, FIRST_HALF | 0x05, SUB_SERIAL, SERIAL, 12);
  transfer(door, 0x06, SUB_SERIAL, SERIAL + 14, 12);
  EXPECT_EQ(fixture.serial_shown(), "");
  EXPECT_EQ(door.identity_request_, 0x05);
}

// A serial number without any text at its start is not shown but logged, in one frame or in two halves. The
// firmware version is still asked for.
TEST(HoermannHcpTextSensorTest, SerialNumberThatIsNotTextIsLoggedNotShown) {
  const char zeros[14] = {};
  {
    IdentityFixture fixture;
    auto &door = fixture.door;
    status_poll(door);
    transfer(door, 0x05, SUB_SERIAL, zeros, 12);
    EXPECT_TRUE(door.serial_unreadable_);  // kept for the log
    EXPECT_EQ(door.identity_request_, 0x06);
    EXPECT_EQ(fixture.serial_shown(), "");
    EXPECT_FALSE(door.serial_unreadable_);  // logged once
  }
  {
    IdentityFixture fixture;
    auto &door = fixture.door;
    status_poll(door);
    transfer(door, FIRST_HALF | 0x05, SUB_SERIAL, zeros, 14);
    transfer(door, 0x06, SUB_SERIAL, zeros, 12);
    EXPECT_TRUE(door.serial_unreadable_);
    EXPECT_EQ(door.identity_request_, 0x06);
    EXPECT_EQ(fixture.serial_shown(), "");
    EXPECT_FALSE(door.serial_unreadable_);
  }
}

// Nor is a firmware version too short.
TEST(HoermannHcpTextSensorTest, ShortFirmwareVersionIsNotKept) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);
  send_serial(door);
  status_poll(door, 0x07);
  transfer(door, 0x08, SUB_FIRMWARE, FIRMWARE, 10);
  EXPECT_TRUE(door.firmware_unreadable_);  // kept for the log
  EXPECT_EQ(fixture.version_shown(), "");
  EXPECT_FALSE(door.firmware_unreadable_);  // logged once
  EXPECT_EQ(door.identity_request_, 0x06);
}

// Nor is one without any payload, which is still logged.
TEST(HoermannHcpTextSensorTest, EmptyFirmwareVersionIsLoggedNotShown) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);
  send_serial(door);
  status_poll(door, 0x07);
  transfer(door, 0x08, SUB_FIRMWARE, FIRMWARE, 0);
  EXPECT_TRUE(door.firmware_unreadable_);
  EXPECT_EQ(fixture.version_shown(), "");
  EXPECT_FALSE(door.firmware_unreadable_);
  EXPECT_EQ(door.identity_request_, 0x06);
}

// A readable firmware version right after a short one, before the loop has turned, is still shown.
TEST(HoermannHcpTextSensorTest, ReadableFirmwareVersionAfterAShortOneIsShown) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);
  send_serial(door);
  status_poll(door, 0x07);
  transfer(door, 0x08, SUB_FIRMWARE, FIRMWARE, 10);
  transfer(door, 0x09, SUB_FIRMWARE, FIRMWARE, 12);
  EXPECT_FALSE(door.firmware_unreadable_);
  EXPECT_EQ(fixture.version_shown(), "FW-TEST 1.0");
}

// All zeros is how a motor that does not report its version says so: nothing shown, not asked for again.
TEST(HoermannHcpTextSensorTest, AllZeroFirmwareVersionMeansNoneIsReported) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);
  send_serial(door);
  status_poll(door, 0x07);
  const char zeros[12] = {};
  transfer(door, 0x08, SUB_FIRMWARE, zeros, 12);
  EXPECT_EQ(fixture.version_shown(), "");
  EXPECT_FALSE(door.firmware_unreadable_);
  EXPECT_EQ(door.identity_request_, 0);
}

// A firmware version that is not text is not shown, is logged, and is not asked for again: it would come back
// the same.
TEST(HoermannHcpTextSensorTest, FirmwareVersionThatIsNotTextIsLoggedNotShown) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);
  send_serial(door);
  status_poll(door, 0x07);
  const char binary[12] = {0x01, 0x12, 0x34, 0x00, 0, 0, 0, 0, 0, 0, 0, 0};
  transfer(door, 0x08, SUB_FIRMWARE, binary, 12);
  EXPECT_TRUE(door.firmware_unreadable_);
  EXPECT_EQ(fixture.version_shown(), "");  // the raw bytes are not published
  EXPECT_FALSE(door.firmware_unreadable_);
  EXPECT_EQ(door.identity_request_, 0);
}

// A repeat of a transfer already taken, as after a lost acknowledgement, is acknowledged again. Answered as a
// status poll instead, it would carry the key press waiting in the slot.
TEST(HoermannHcpTextSensorTest, RepeatedTransferIsAcknowledgedNotAnsweredWithAKeyPress) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  run_identity_exchange(door);
  connect_controller(door);
  door.open_door();

  const RegisterValues answer = transfer(door, 0x08, SUB_FIRMWARE, FIRMWARE, 12);
  EXPECT_EQ(answer[1], 0x04FD);
  EXPECT_EQ(answer[2], 0x0000);
  EXPECT_EQ(status_poll(door)[2], 0x0210);
}

// An answer belongs to the frame whose write half took the transfer. A frame whose read went elsewhere leaves
// nothing behind for the next poll.
TEST(HoermannHcpTextSensorTest, AnAnswerBelongsToItsFrame) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);
  write_transfer(door, FIRST_HALF | 0x05, SUB_SERIAL, SERIAL, 14);
  RegisterValues ignored;
  door.on_read_holding_registers(COMMAND_REG, 8, ignored);

  EXPECT_EQ(status_poll(door)[1] & 0x00FF, 0x0022);
}

// Only the answer to a status poll carries a request, not the answer to another frame of the same length, and
// other transfers are not this exchange's to answer.
TEST(HoermannHcpTextSensorTest, RequestRidesOnlyOnAStatusPoll) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  status_poll(door);
  const RegisterValues other = transfer(door, 0x06, 0x19, "\x00\x0F", 2);
  EXPECT_EQ(other[1] & 0x00FF, 0x0001);
  EXPECT_EQ(status_poll(door, 0x07)[1], 0x0322);
}

// The request travels in the registers a key press would, so it waits for the press, the hold and the release.
TEST(HoermannHcpTextSensorTest, RequestWaitsForTheKeyPress) {
  IdentityFixture fixture;
  auto &door = fixture.door;
  door.key_press_delay_ms_ = 100;
  connect_controller(door);
  status_poll(door);
  door.open_door();

  EXPECT_EQ(status_poll(door)[2], 0x0210);
  const RegisterValues held = status_poll(door);
  EXPECT_EQ(held[1], 0x0301);
  EXPECT_EQ(held[2], 0x0000);
  door.key_press_delay_ms_ = 0;
  std::this_thread::sleep_for(KEY_PRESS_ELAPSED);
  const RegisterValues release = status_poll(door);
  EXPECT_EQ(release[1], 0x0301);
  EXPECT_EQ(release[2], 0x0110);
  EXPECT_EQ(status_poll(door)[1], 0x0322);
}

// Each value is published once it is in, and only once.
TEST(HoermannHcpTextSensorTest, EachValueIsPublishedOnce) {
  IdentityFixture fixture;
  int serial_publishes = 0;
  int version_publishes = 0;
  fixture.serial.add_on_state_callback([&serial_publishes](const std::string & /*state*/) { serial_publishes++; });
  fixture.version.add_on_state_callback([&version_publishes](const std::string & /*state*/) { version_publishes++; });

  fixture.door.update();
  EXPECT_EQ(serial_publishes, 0);
  EXPECT_EQ(version_publishes, 0);

  run_identity_exchange(fixture.door);
  EXPECT_EQ(fixture.serial.get_state(), SERIAL);
  EXPECT_EQ(fixture.version.get_state(), "FW-TEST 1.0");

  fixture.door.update();
  fixture.door.update();
  EXPECT_EQ(serial_publishes, 1);
  EXPECT_EQ(version_publishes, 1);
}

// Configuring only the firmware version is enough to ask.
TEST(HoermannHcpTextSensorTest, OneSensorIsEnough) {
  TestableHoermannHcp door;
  text_sensor::TextSensor version;
  door.set_version_text_sensor(&version);
  run_identity_exchange(door);
  EXPECT_EQ(version.get_state(), "FW-TEST 1.0");
}

}  // namespace esphome::hoermann_hcp::testing
