#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>
#include <vector>

#include "fake_canbus.h"

namespace esphome::canbus::testing {

// Frames stay as large as on dev, and keep their field order for brace initialization. clang-tidy builds with
// defines.h, which turns CAN FD on.
#ifndef USE_CANBUS_FD
static_assert(sizeof(CanFrame::data) == CAN_MAX_DATA_LENGTH);
static_assert(sizeof(CanFrame) == 24);
static_assert(can_dlc_to_len(9) == 8);
static_assert(can_dlc_to_len(15) == 8);
#endif
static_assert(can_dlc_to_len(0) == 0);
static_assert(can_dlc_to_len(8) == 8);

TEST(CanbusSend, ClassicFrameBraceInitialization) {
  const CanFrame frame{true, true, 0x1234567, 8, {0, 1, 2, 3, 4, 5, 6, 7}};
  EXPECT_TRUE(frame.use_extended_id);
  EXPECT_TRUE(frame.remote_transmission_request);
  EXPECT_EQ(frame.can_id, 0x1234567u);
  EXPECT_EQ(payload_bytes(frame), (std::vector<uint8_t>{0, 1, 2, 3, 4, 5, 6, 7}));
}

TEST(CanbusSend, LengthIsClampedToOneFrame) {
  const std::vector<uint8_t> payload{1, 2, 3, 4, 5, 6, 7, 8, 9};
  for (size_t len : {size_t{0}, size_t{8}, size_t{9}, size_t{256}}) {
    QueuedCanbus bus;
    const size_t sent_len = std::min<size_t>(len, CAN_MAX_DATA_LENGTH);
    EXPECT_EQ(bus.send_data(0x123, false, false, len == 0 ? nullptr : payload.data(), len), ERROR_OK);
    ASSERT_EQ(bus.sent.size(), 1u) << len;
    EXPECT_EQ(bus.sent[0].can_data_length_code, sent_len);
    EXPECT_EQ(payload_bytes(bus.sent[0]), std::vector<uint8_t>(payload.begin(), payload.begin() + sent_len));
  }
}

TEST(CanbusSend, RemoteRequestDoesNotReadData) {
  QueuedCanbus bus;
  EXPECT_EQ(bus.send_data(0x123, false, true, nullptr, 8), ERROR_OK);
  ASSERT_EQ(bus.sent.size(), 1u);
  EXPECT_TRUE(bus.sent[0].remote_transmission_request);
  EXPECT_EQ(bus.sent[0].can_data_length_code, 8);
  // Zeroed, not left from the stack, since drivers and the verbose log still read them
  EXPECT_EQ(payload_bytes(bus.sent[0]), std::vector<uint8_t>(8, 0));
}

TEST(CanbusSend, FrameIdentifierMustFitItsWidth) {
  struct Case {
    bool extended;
    uint32_t can_id;
    Error expected;
  };
  for (const auto &c : {Case{false, CAN_STANDARD_ID_MAX, ERROR_OK}, Case{false, CAN_STANDARD_ID_MAX + 1, ERROR_FAILTX},
                        Case{true, CAN_EXTENDED_ID_MAX, ERROR_OK}, Case{true, CAN_EXTENDED_ID_MAX + 1, ERROR_FAILTX}}) {
    QueuedCanbus bus;
    CanFrame frame{};
    frame.use_extended_id = c.extended;
    frame.can_id = c.can_id;
    EXPECT_EQ(bus.send_data(frame), c.expected) << c.can_id;
    EXPECT_EQ(bus.sent.size(), c.expected == ERROR_OK ? 1u : 0u) << c.can_id;
  }
}

TEST(CanbusSend, DriverErrorIsReturned) {
  QueuedCanbus bus;
  bus.send_result = ERROR_ALLTXBUSY;
  EXPECT_EQ(bus.send_data(0x123, false, std::vector<uint8_t>{1}), ERROR_ALLTXBUSY);
}

static CanPayload three_bytes() { return {1, 2, 3}; }
static CanPayload too_long() { return std::vector<uint8_t>(20, 0xAB); }

TEST(CanbusSend, EveryPayloadConstructorRecordsATruncation) {
  EXPECT_FALSE(CanPayload({1, 2, 3, 4, 5, 6, 7, 8}).truncated());
  const CanPayload braces{1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_TRUE(braces.truncated());
  EXPECT_EQ(braces.size(), 8u);
  static constexpr uint8_t DATA[9] = {};
  EXPECT_TRUE(CanPayload(std::begin(DATA), std::end(DATA)).truncated());
  EXPECT_TRUE(CanPayload(std::vector<uint8_t>(9, 0)).truncated());
  EXPECT_FALSE(CanPayload(std::vector<uint8_t>(8, 0)).truncated());
}

TEST(CanbusSendAction, UsesTheBusOrItsOwnIdentifier) {
  struct Case {
    const char *name;
    bool own_id;
    bool extended;
    bool rtr;
    CanPayload (*data)();
    uint32_t expected_id;
    uint8_t expected_dlc;
  };
  for (const auto &c : {
           Case{"bus id", false, false, false, three_bytes, 0x100, 3},
           Case{"own extended id", true, true, false, three_bytes, 0x1ABCDE, 3},
           Case{"remote request", true, false, true, three_bytes, 0x1ABCDE, 3},
           Case{"lambda over 8 bytes", false, false, false, too_long, 0x100, 8},
       }) {
    QueuedCanbus bus;
    bus.set_can_id(0x100);
    CanbusSendAction<> action;
    action.set_parent(&bus);
    if (c.own_id) {
      action.set_can_id(0x1ABCDE);
      action.set_use_extended_id(c.extended);
    }
    action.set_remote_transmission_request(c.rtr);
    action.set_data_template(c.data);
    action.play();
    ASSERT_EQ(bus.sent.size(), 1u) << c.name;
    EXPECT_EQ(bus.sent[0].can_id, c.expected_id) << c.name;
    EXPECT_EQ(bus.sent[0].use_extended_id, c.extended) << c.name;
    EXPECT_EQ(bus.sent[0].remote_transmission_request, c.rtr) << c.name;
    EXPECT_EQ(bus.sent[0].can_data_length_code, c.expected_dlc) << c.name;
  }
}

TEST(CanbusSendAction, StaticPayload) {
  static constexpr uint8_t DATA[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  QueuedCanbus bus;
  bus.set_can_id(0x100);
  CanbusSendAction<> action;
  action.set_parent(&bus);
  action.set_data_static(DATA, sizeof(DATA));
  action.play();
  ASSERT_EQ(bus.sent.size(), 1u);
  EXPECT_EQ(payload_bytes(bus.sent[0]), (std::vector<uint8_t>{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}));
}

}  // namespace esphome::canbus::testing
