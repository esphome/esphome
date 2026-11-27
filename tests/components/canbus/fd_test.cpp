// The rest of the test binary builds canbus without CAN FD, as every in-tree platform does. This file builds a
// second copy with USE_CANBUS_FD in its own namespace, so both frame sizes are tested in one binary.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <initializer_list>
#include <vector>

#include "esphome/core/application.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/optional.h"

#define USE_CANBUS_FD
#define canbus canbus_fd                         // NOLINT(cppcoreguidelines-macro-usage)
#include "esphome/components/canbus/canbus.cpp"  // NOLINT(bugprone-suspicious-include)
#include "fake_canbus.h"

namespace esphome::canbus::testing {

static_assert(sizeof(CanFrame::data) == CAN_FD_MAX_DATA_LENGTH);
static_assert(can_dlc_to_len(8) == 8);
static_assert(can_dlc_to_len(9) == 12);
static_assert(can_dlc_to_len(15) == 64);
static_assert(can_dlc_to_len(16) == CAN_INVALID_DLC);
static_assert(can_len_to_dlc(8, false) == 8);
static_assert(can_len_to_dlc(9, false) == CAN_INVALID_DLC);
static_assert(can_len_to_dlc(9, true) == 9);
static_assert(can_len_to_dlc(12, false) == 9);
static_assert(can_len_to_dlc(49, true) == 15);
static_assert(can_len_to_dlc(64, false) == 15);
static_assert(can_len_to_dlc(65, true) == CAN_INVALID_DLC);

// A bus of a platform that supports CAN FD, as register_canbus() sets it up
class FdCanbus : public QueuedCanbus {
 public:
  FdCanbus() { this->set_supports_fd(true); }
};

TEST(CanbusFd, ClassicBraceInitializationStillWorks) {
  const CanFrame frame{true, false, 0x1234567, 8, {0, 1, 2, 3, 4, 5, 6, 7}};
  EXPECT_EQ(frame.can_id, 0x1234567u);
  EXPECT_FALSE(frame.fd);
  EXPECT_FALSE(frame.bit_rate_switch);
  EXPECT_EQ(payload_bytes(frame), (std::vector<uint8_t>{0, 1, 2, 3, 4, 5, 6, 7}));
}

TEST(CanbusFd, PayloadLengthFollowsTheFrameFormat) {
  CanFrame frame{};
  frame.can_data_length_code = 15;
  EXPECT_EQ(frame.payload_length(), 8);
  frame.fd = true;
  EXPECT_EQ(frame.payload_length(), 64);
  frame.can_data_length_code = 16;
  EXPECT_EQ(frame.payload_length(), 0);
}

TEST(CanbusFd, FrameValidation) {
  struct Case {
    const char *name;
    bool fd;
    bool rtr;
    bool brs;
    uint8_t dlc;
    Error expected;
  };
  for (const auto &c : {
           Case{"fd 64 bytes", true, false, true, 15, ERROR_OK},
           Case{"fd invalid dlc", true, false, false, 16, ERROR_FAILTX},
           Case{"fd remote request", true, true, false, 8, ERROR_FAILTX},
           Case{"brs without fd", false, false, true, 8, ERROR_FAILTX},
           Case{"classic", false, false, false, 8, ERROR_OK},
       }) {
    FdCanbus bus;
    CanFrame frame{};
    frame.can_id = 0x123;
    frame.fd = c.fd;
    frame.remote_transmission_request = c.rtr;
    frame.bit_rate_switch = c.brs;
    frame.can_data_length_code = c.dlc;
    EXPECT_EQ(bus.send_data(frame), c.expected) << c.name;
    EXPECT_EQ(bus.sent.size(), c.expected == ERROR_OK ? 1u : 0u) << c.name;
  }
}

TEST(CanbusFd, LengthBasedSend) {
  struct Case {
    bool fd;
    bool pad;
    size_t len;
    uint8_t expected_dlc;  // CAN_INVALID_DLC: not sent
  };
  std::vector<uint8_t> payload(80, 0xAB);
  for (const auto &c : {
           Case{true, false, 12, 9},
           Case{true, false, 13, CAN_INVALID_DLC},
           Case{true, true, 13, 10},
           Case{true, true, 64, 15},
           Case{true, true, 65, CAN_INVALID_DLC},
           Case{false, false, 13, 8},  // classic still cuts to 8 bytes
       }) {
    FdCanbus bus;
    const Error result = bus.send_data(0x123, false, false, payload.data(), c.len, c.fd, false, c.pad);
    if (c.expected_dlc == CAN_INVALID_DLC) {
      EXPECT_EQ(result, ERROR_FAILTX) << c.len;
      EXPECT_TRUE(bus.sent.empty()) << c.len;
      continue;
    }
    ASSERT_EQ(bus.sent.size(), 1u) << c.len;
    const CanFrame &sent = bus.sent[0];
    EXPECT_EQ(sent.fd, c.fd);
    EXPECT_EQ(sent.can_data_length_code, c.expected_dlc) << c.len;
    const size_t copied = std::min<size_t>(c.len, sent.payload_length());
    std::vector<uint8_t> expected(sent.payload_length(), 0);
    std::fill_n(expected.begin(), copied, 0xAB);
    EXPECT_EQ(payload_bytes(sent), expected) << c.len;
  }
}

TEST(CanbusFd, ClassicBusInAnFdBuildRefusesFdFrames) {
  FdCanbus fd_bus;
  QueuedCanbus classic_bus;
  EXPECT_TRUE(fd_bus.supports_fd());
  EXPECT_FALSE(classic_bus.supports_fd());
  static constexpr uint8_t DATA[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  for (QueuedCanbus *bus : {static_cast<QueuedCanbus *>(&fd_bus), &classic_bus}) {
    const Error expected = bus->supports_fd() ? ERROR_OK : ERROR_FAILTX;
    EXPECT_EQ(bus->send_data(0x123, false, false, DATA, sizeof(DATA), true), expected);
    EXPECT_EQ(bus->send_data(0x123, false, false, DATA, sizeof(DATA)), ERROR_OK);
    ASSERT_EQ(bus->sent.size(), bus->supports_fd() ? 2u : 1u);
    EXPECT_FALSE(bus->sent.back().fd);
  }
}

TEST(CanbusFd, ClassicDriverAfterFdFrameSeesClearedFlags) {
  // Writes only the classic fields, as esp32_can and mcp2515 do
  class MixedCanbus : public QueuedCanbus {
   protected:
    Error read_message(CanFrame *frame) override {
      if (this->reads_ == 2)
        return ERROR_NOMSG;
      frame->can_id = 0x123;
      frame->can_data_length_code = 15;
      std::fill_n(frame->data, CAN_FRAME_DATA_LENGTH, 0xAB);
      if (this->reads_++ == 0) {
        frame->fd = true;
        frame->bit_rate_switch = true;
        frame->error_state_indicator = true;
      }
      return ERROR_OK;
    }
    uint8_t reads_{0};
  };
  MixedCanbus bus;
  std::vector<std::pair<bool, size_t>> received;
  bus.add_frame_callback([&](const CanFrame &frame) {
    EXPECT_EQ(frame.bit_rate_switch, frame.fd);
    EXPECT_EQ(frame.error_state_indicator, frame.fd);
    received.emplace_back(frame.fd, frame.payload().size());
  });
  bus.loop();
  EXPECT_EQ(received, (std::vector<std::pair<bool, size_t>>{{true, 64}, {false, 8}}));
}

static bool yes() { return true; }
static bool no() { return false; }

static CanPayload sixty_four_bytes() { return std::vector<uint8_t>(64, 0xAB); }
static CanPayload sixty_five_bytes() { return std::vector<uint8_t>(65, 0xAB); }
// Built from a range rather than a vector, so it goes through another constructor
static CanPayload sixty_five_byte_range() {
  static constexpr uint8_t DATA[65] = {};
  return CanPayload(std::begin(DATA), std::end(DATA));
}

TEST(CanbusFd, ActionRefusesAnFdLambdaPayloadThatDoesNotFit) {
  struct Case {
    CanPayload (*data)();
    bool expect_sent;
  };
  for (const auto &c :
       {Case{sixty_four_bytes, true}, Case{sixty_five_bytes, false}, Case{sixty_five_byte_range, false}}) {
    FdCanbus bus;
    bus.set_can_id(0x100);
    bus.set_default_fd(true);
    CanbusSendAction<> action;
    action.set_parent(&bus);
    action.set_data_template(c.data);
    action.play();
    ASSERT_EQ(bus.sent.size(), c.expect_sent ? 1u : 0u);
    if (c.expect_sent)
      EXPECT_EQ(bus.sent[0].payload_length(), 64);
  }
}

TEST(CanbusFd, ActionDefaultsAndOverrides) {
  using Flag = bool (*)();
  struct Case {
    const char *name;
    bool default_fd;
    bool default_brs;
    bool default_pad;
    Flag fd;
    Flag brs;
    Flag pad;
    size_t len;
    bool expect_sent;
    bool expect_fd;
    bool expect_brs;
    uint8_t expect_dlc;
  };
  static constexpr uint8_t DATA[13] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
  for (const auto &c : {
           Case{"classic by default", false, true, false, nullptr, nullptr, nullptr, 8, true, false, false, 8},
           Case{"bus default fd and brs", true, true, false, nullptr, nullptr, nullptr, 12, true, true, true, 9},
           Case{"13 bytes need padding", true, false, false, nullptr, nullptr, nullptr, 13, false, false, false, 0},
           Case{"bus default padding", true, false, true, nullptr, nullptr, nullptr, 13, true, true, false, 10},
           Case{"action padding", true, false, false, nullptr, nullptr, yes, 13, true, true, false, 10},
           Case{"action turns fd off", true, true, false, no, nullptr, nullptr, 8, true, false, false, 8},
           Case{"brs without fd", false, false, false, nullptr, yes, nullptr, 8, false, false, false, 0},
           Case{"action fd without brs", false, true, false, yes, no, nullptr, 8, true, true, false, 8},
       }) {
    FdCanbus bus;
    bus.set_can_id(0x100);
    bus.set_default_fd(c.default_fd);
    bus.set_default_bit_rate_switch(c.default_brs);
    bus.set_default_pad_data(c.default_pad);
    CanbusSendAction<> action;
    action.set_parent(&bus);
    action.set_data_static(DATA, c.len);
    if (c.fd != nullptr)
      action.set_fd(c.fd);
    if (c.brs != nullptr)
      action.set_bit_rate_switch(c.brs);
    if (c.pad != nullptr)
      action.set_pad_data(c.pad);
    action.play();
    ASSERT_EQ(bus.sent.size(), c.expect_sent ? 1u : 0u) << c.name;
    if (!c.expect_sent)
      continue;
    EXPECT_EQ(bus.sent[0].fd, c.expect_fd) << c.name;
    EXPECT_EQ(bus.sent[0].bit_rate_switch, c.expect_brs) << c.name;
    EXPECT_EQ(bus.sent[0].can_data_length_code, c.expect_dlc) << c.name;
  }
}

}  // namespace esphome::canbus::testing

#undef canbus
