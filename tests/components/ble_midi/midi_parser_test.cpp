#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "esphome/components/ble_midi/midi_parser.h"

namespace esphome::ble_midi::testing {

// A BLE-MIDI packet is a header byte, then timestamp/MIDI byte pairs. Both
// header and timestamp have bit 7 set; their remaining bits carry a timestamp
// that this component ignores.
static constexpr uint8_t HDR = 0x80;
static constexpr uint8_t TS = 0x80;

struct Decoded {
  MessageType type;
  std::vector<uint8_t> bytes;
};

class MidiParserTest : public ::testing::Test {
 protected:
  void SetUp() override { this->parser_.init_sysex_buffer(64); }

  void feed_(const std::vector<uint8_t> &packet) {
    this->parser_.feed(packet.data(), packet.size(), [this](const MidiMessage &message) {
      this->messages_.push_back({message.type, std::vector<uint8_t>(message.data, message.data + message.length)});
    });
  }

  MidiParser parser_;
  std::vector<Decoded> messages_;
};

TEST_F(MidiParserTest, NoteOn) {
  this->feed_({HDR, TS, 0x90, 0x3C, 0x40});
  ASSERT_EQ(this->messages_.size(), 1u);
  EXPECT_EQ(this->messages_[0].type, MessageType::NOTE_ON);
  EXPECT_EQ(this->messages_[0].bytes, std::vector<uint8_t>({0x90, 0x3C, 0x40}));
}

TEST_F(MidiParserTest, ChannelIsTakenFromTheStatusByte) {
  this->feed_({HDR, TS, 0x95, 0x3C, 0x40});
  ASSERT_EQ(this->messages_.size(), 1u);
  MidiMessage message{this->messages_[0].type, this->messages_[0].bytes.data(), this->messages_[0].bytes.size()};
  EXPECT_EQ(message.channel(), 5);
  EXPECT_EQ(message.note(), 60);
  EXPECT_EQ(message.velocity(), 64);
}

TEST_F(MidiParserTest, RunningStatusReusesThePreviousStatusByte) {
  this->feed_({HDR, TS, 0x90, 0x3C, 0x40, 0x3E, 0x41});
  ASSERT_EQ(this->messages_.size(), 2u);
  EXPECT_EQ(this->messages_[1].type, MessageType::NOTE_ON);
  EXPECT_EQ(this->messages_[1].bytes, std::vector<uint8_t>({0x90, 0x3E, 0x41}));
}

TEST_F(MidiParserTest, RunningStatusSurvivesAcrossPackets) {
  this->feed_({HDR, TS, 0xB0, 0x07, 0x10});
  this->feed_({HDR, 0x07, 0x20});
  ASSERT_EQ(this->messages_.size(), 2u);
  EXPECT_EQ(this->messages_[1].type, MessageType::CONTROL_CHANGE);
  EXPECT_EQ(this->messages_[1].bytes, std::vector<uint8_t>({0xB0, 0x07, 0x20}));
}

TEST_F(MidiParserTest, ResetClearsRunningStatus) {
  this->feed_({HDR, TS, 0x90, 0x3C, 0x40});
  this->parser_.reset();
  this->messages_.clear();
  this->feed_({HDR, TS, 0x3E, 0x41});
  ASSERT_EQ(this->messages_.size(), 2u);
  EXPECT_EQ(this->messages_[0].type, MessageType::UNKNOWN);
  EXPECT_EQ(this->messages_[1].type, MessageType::UNKNOWN);
}

TEST_F(MidiParserTest, NoteOnWithZeroVelocityIsANoteOff) {
  this->feed_({HDR, TS, 0x90, 0x3C, 0x00});
  ASSERT_EQ(this->messages_.size(), 1u);
  EXPECT_EQ(this->messages_[0].type, MessageType::NOTE_OFF);
}

TEST_F(MidiParserTest, ControlChange) {
  this->feed_({HDR, TS, 0xB2, 0x07, 0x64});
  ASSERT_EQ(this->messages_.size(), 1u);
  MidiMessage message{this->messages_[0].type, this->messages_[0].bytes.data(), this->messages_[0].bytes.size()};
  EXPECT_EQ(message.type, MessageType::CONTROL_CHANGE);
  EXPECT_EQ(message.channel(), 2);
  EXPECT_EQ(message.control_number(), 7);
  EXPECT_EQ(message.control_value(), 100);
}

TEST_F(MidiParserTest, ProgramChangeHasASingleDataByte) {
  this->feed_({HDR, TS, 0xC0, 0x05, TS, 0xC0, 0x06});
  ASSERT_EQ(this->messages_.size(), 2u);
  EXPECT_EQ(this->messages_[0].type, MessageType::PROGRAM_CHANGE);
  EXPECT_EQ(this->messages_[0].bytes, std::vector<uint8_t>({0xC0, 0x05}));
  EXPECT_EQ(this->messages_[1].bytes, std::vector<uint8_t>({0xC0, 0x06}));
}

TEST_F(MidiParserTest, PitchBendIsCenteredOnZero) {
  this->feed_({HDR, TS, 0xE0, 0x00, 0x40, TS, 0xE0, 0x00, 0x00, TS, 0xE0, 0x7F, 0x7F});
  ASSERT_EQ(this->messages_.size(), 3u);
  auto bend = [this](size_t index) {
    MidiMessage message{this->messages_[index].type, this->messages_[index].bytes.data(),
                        this->messages_[index].bytes.size()};
    return message.pitch_bend();
  };
  EXPECT_EQ(this->messages_[0].type, MessageType::PITCH_BEND);
  EXPECT_EQ(bend(0), 0);
  EXPECT_EQ(bend(1), -8192);
  EXPECT_EQ(bend(2), 8191);
}

TEST_F(MidiParserTest, RealTimeMessageIsASingleByte) {
  this->feed_({HDR, TS, 0xF8});
  ASSERT_EQ(this->messages_.size(), 1u);
  EXPECT_EQ(this->messages_[0].type, MessageType::SYSTEM_REAL_TIME);
  EXPECT_EQ(this->messages_[0].bytes, std::vector<uint8_t>({0xF8}));
}

TEST_F(MidiParserTest, SystemCommonMessageWithTwoDataBytes) {
  this->feed_({HDR, TS, 0xF2, 0x01, 0x02});
  ASSERT_EQ(this->messages_.size(), 1u);
  EXPECT_EQ(this->messages_[0].type, MessageType::SYSTEM_COMMON);
  EXPECT_EQ(this->messages_[0].bytes, std::vector<uint8_t>({0xF2, 0x01, 0x02}));
}

TEST_F(MidiParserTest, SystemCommonClearsRunningStatus) {
  this->feed_({HDR, TS, 0x90, 0x3C, 0x40, TS, 0xF6});
  this->messages_.clear();
  this->feed_({HDR, 0x3E, 0x41});
  ASSERT_EQ(this->messages_.size(), 2u);
  EXPECT_EQ(this->messages_[0].type, MessageType::UNKNOWN);
}

TEST_F(MidiParserTest, SysexInASinglePacket) {
  this->feed_({HDR, TS, 0xF0, 0x7E, 0x7F, 0x06, 0x01, TS, 0xF7});
  ASSERT_EQ(this->messages_.size(), 1u);
  EXPECT_EQ(this->messages_[0].type, MessageType::SYSEX);
  EXPECT_EQ(this->messages_[0].bytes, std::vector<uint8_t>({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7}));
}

TEST_F(MidiParserTest, SysexReassembledAcrossPackets) {
  this->feed_({HDR, TS, 0xF0, 0x7E, 0x7F});
  EXPECT_TRUE(this->messages_.empty());
  this->feed_({HDR, 0x06, 0x01, TS, 0xF7});
  ASSERT_EQ(this->messages_.size(), 1u);
  EXPECT_EQ(this->messages_[0].bytes, std::vector<uint8_t>({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7}));
}

TEST_F(MidiParserTest, RealTimeMessageInterruptsSysex) {
  this->feed_({HDR, TS, 0xF0, 0x7E, TS, 0xF8, 0x7F, TS, 0xF7});
  ASSERT_EQ(this->messages_.size(), 2u);
  EXPECT_EQ(this->messages_[0].type, MessageType::SYSTEM_REAL_TIME);
  EXPECT_EQ(this->messages_[1].type, MessageType::SYSEX);
  EXPECT_EQ(this->messages_[1].bytes, std::vector<uint8_t>({0xF0, 0x7E, 0x7F, 0xF7}));
}

TEST_F(MidiParserTest, SysexLongerThanTheBufferIsTruncatedAndDoesNotOverflow) {
  MidiParser small_parser;
  small_parser.init_sysex_buffer(8);
  std::vector<uint8_t> packet{HDR, TS, 0xF0};
  for (uint8_t i = 0; i < 64; i++)
    packet.push_back(i & 0x7F);
  packet.push_back(TS);
  packet.push_back(0xF7);

  std::vector<size_t> lengths;
  small_parser.feed(packet.data(), packet.size(),
                    [&lengths](const MidiMessage &message) { lengths.push_back(message.length); });
  ASSERT_EQ(lengths.size(), 1u);
  EXPECT_EQ(lengths[0], 8u);
}

TEST_F(MidiParserTest, DataByteWithoutStatusContextIsReportedAsUnknown) {
  this->feed_({HDR, TS, 0x3C});
  ASSERT_EQ(this->messages_.size(), 1u);
  EXPECT_EQ(this->messages_[0].type, MessageType::UNKNOWN);
  EXPECT_EQ(this->messages_[0].bytes, std::vector<uint8_t>({0x3C}));
}

TEST_F(MidiParserTest, PacketWithoutAHeaderByteIsRejected) {
  this->feed_({0x3C, 0x40});
  ASSERT_EQ(this->messages_.size(), 2u);
  EXPECT_EQ(this->messages_[0].type, MessageType::UNKNOWN);
  EXPECT_EQ(this->messages_[1].type, MessageType::UNKNOWN);
}

TEST_F(MidiParserTest, EmptyAndSingleByteInputAreHandled) {
  this->feed_({});
  this->feed_({HDR});
  EXPECT_EQ(this->messages_.size(), 1u);
}

TEST_F(MidiParserTest, IncompleteMessageIsDiscardedWhenANewStatusByteArrives) {
  this->feed_({HDR, TS, 0x90, 0x3C, TS, 0xB0, 0x07, 0x10});
  ASSERT_EQ(this->messages_.size(), 1u);
  EXPECT_EQ(this->messages_[0].type, MessageType::CONTROL_CHANGE);
}

TEST_F(MidiParserTest, MultipleMessagesInOnePacket) {
  this->feed_({HDR, TS, 0x90, 0x3C, 0x40, TS, 0x80, 0x3C, 0x00, TS, 0xB0, 0x07, 0x7F});
  ASSERT_EQ(this->messages_.size(), 3u);
  EXPECT_EQ(this->messages_[0].type, MessageType::NOTE_ON);
  EXPECT_EQ(this->messages_[1].type, MessageType::NOTE_OFF);
  EXPECT_EQ(this->messages_[2].type, MessageType::CONTROL_CHANGE);
}

}  // namespace esphome::ble_midi::testing
