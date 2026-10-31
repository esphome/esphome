#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <type_traits>

#include "esphome/components/canbus/canbus.h"

namespace esphome::canbus::testing {

TEST(CanFrameTest, ClassicDefaults) {
  CanFrame frame;
  EXPECT_FALSE(frame.use_extended_id);
  EXPECT_FALSE(frame.remote_transmission_request);
  EXPECT_FALSE(frame.fd);
  EXPECT_FALSE(frame.bit_rate_switch);
  EXPECT_FALSE(frame.error_state_indicator);
  EXPECT_EQ(CAN_MAX_DATA_LENGTH, 8);
  EXPECT_EQ(CAN_FD_MAX_DATA_LENGTH, 64);
  EXPECT_EQ(sizeof(frame.data), 64);
  EXPECT_EQ(offsetof(CanFrame, data) % 8, 0);
  EXPECT_TRUE(std::is_aggregate_v<CanFrame>);
}

TEST(CanFrameTest, ClassicAggregateInitialization) {
  CanFrame frame{0x1234567, 8, true, true, false, false, false, {0, 1, 2, 3, 4, 5, 6, 7}};
  EXPECT_TRUE(frame.use_extended_id);
  EXPECT_TRUE(frame.remote_transmission_request);
  EXPECT_EQ(frame.can_id, 0x1234567);
  EXPECT_EQ(frame.can_data_length_code, 8);
  for (uint8_t i = 0; i < 8; i++)
    EXPECT_EQ(frame.data[i], i);
  EXPECT_FALSE(frame.fd);
  EXPECT_FALSE(frame.bit_rate_switch);
  EXPECT_FALSE(frame.error_state_indicator);
}

TEST(CanFrameTest, FullFdPayloadAndMetadataCopy) {
  CanFrame frame{};
  frame.can_id = 0x123;
  frame.can_data_length_code = 15;
  frame.fd = true;
  frame.bit_rate_switch = true;
  frame.error_state_indicator = true;
  for (size_t i = 0; i < sizeof(frame.data); i++)
    frame.data[i] = i;

  CanFrame copy = frame;
  EXPECT_EQ(copy.can_id, 0x123);
  EXPECT_EQ(copy.can_data_length_code, 15);
  EXPECT_TRUE(copy.fd);
  EXPECT_TRUE(copy.bit_rate_switch);
  EXPECT_TRUE(copy.error_state_indicator);
  EXPECT_FALSE(copy.remote_transmission_request);
  for (size_t i = 0; i < sizeof(copy.data); i++)
    EXPECT_EQ(copy.data[i], i);

  copy.can_data_length_code = 8;
  copy.bit_rate_switch = false;
  EXPECT_TRUE(copy.fd);
}

class TestCanbus : public Canbus {
 public:
  CanFrame sent_frame{};
  CanFrame received_frame{};
  bool received{false};
  size_t send_count{0};
  Error send_result{ERROR_OK};

  TestCanbus() {
    this->received_frame.can_id = 0x123;
    this->received_frame.can_data_length_code = CAN_MAX_DATA_LENGTH;
    std::fill_n(this->received_frame.data, CAN_FD_MAX_DATA_LENGTH, 0xAB);
  }

 protected:
  bool setup_internal() override { return true; }
  Error send_message(CanFrame *frame) override {
    this->sent_frame = *frame;
    this->send_count++;
    return this->send_result;
  }
  Error read_message(CanFrame *frame) override {
    if (this->received)
      return ERROR_NOMSG;
    *frame = this->received_frame;
    this->received = true;
    return ERROR_OK;
  }
};

TEST(CanFrameTest, LegacySendRemainsClassic) {
  TestCanbus bus;
  std::vector<uint8_t> payload(8, 0xAB);
  EXPECT_EQ(bus.send_data(0x123, false, false, payload), ERROR_OK);
  EXPECT_EQ(bus.sent_frame.can_id, 0x123);
  EXPECT_EQ(bus.sent_frame.can_data_length_code, 8);
  EXPECT_FALSE(bus.sent_frame.fd);
  EXPECT_FALSE(bus.sent_frame.bit_rate_switch);
  EXPECT_FALSE(bus.sent_frame.error_state_indicator);
  for (uint8_t i = 0; i < 8; i++)
    EXPECT_EQ(bus.sent_frame.data[i], 0xAB);
}

TEST(CanFrameTest, LegacyReceiveCallbackUnchanged) {
  TestCanbus bus;
  size_t calls = 0;
  bus.add_callback([&](uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data) {
    calls++;
    EXPECT_EQ(can_id, 0x123);
    EXPECT_FALSE(extended_id);
    EXPECT_FALSE(rtr);
    EXPECT_EQ(data, (std::vector<uint8_t>(8, 0xAB)));
  });
  bus.loop();
  EXPECT_EQ(calls, 1);
}

TEST(CanFrameTest, DlcConversions) {
  static_assert(can_dlc_to_len(15) == 64);
  static_assert(can_len_to_dlc(9, true) == 9);
  static_assert(can_len_to_dlc(9, false) == CAN_INVALID_DLC);
  constexpr uint8_t lengths[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
  for (uint8_t dlc = 0; dlc < 16; dlc++) {
    EXPECT_EQ(can_dlc_to_len(dlc), lengths[dlc]);
    EXPECT_EQ(can_len_to_dlc(lengths[dlc], false), dlc);
    EXPECT_EQ(can_len_to_dlc(lengths[dlc], true), dlc);
  }
  for (unsigned dlc = 16; dlc <= UINT8_MAX; dlc++)
    EXPECT_EQ(can_dlc_to_len(dlc), CAN_INVALID_DLC);

  for (size_t len = 0; len <= 1024; len++) {
    uint8_t exact = CAN_INVALID_DLC;
    uint8_t padded = CAN_INVALID_DLC;
    for (uint8_t dlc = 0; dlc < 16; dlc++) {
      if (lengths[dlc] == len)
        exact = dlc;
      if (lengths[dlc] >= len && padded == CAN_INVALID_DLC)
        padded = dlc;
    }
    EXPECT_EQ(can_len_to_dlc(len, false), exact) << "length=" << len;
    EXPECT_EQ(can_len_to_dlc(len, true), padded) << "length=" << len;
  }
  EXPECT_EQ(can_len_to_dlc(SIZE_MAX, true), CAN_INVALID_DLC);
}

TEST(CanFrameTest, ReceiveUsesDecodedLengthWithoutChangingCallback) {
  for (bool fd : {false, true}) {
    for (unsigned dlc = 0; dlc <= UINT8_MAX; dlc++) {
      TestCanbus bus;
      bus.received_frame.fd = fd;
      bus.received_frame.can_data_length_code = dlc;
      size_t calls = 0;
      bus.add_callback([&](uint32_t, bool, bool, const std::vector<uint8_t> &data) {
        calls++;
        const size_t expected = fd ? can_dlc_to_len(dlc) : std::min<unsigned>(dlc, 8);
        EXPECT_EQ(data, (std::vector<uint8_t>(expected, 0xAB))) << "fd=" << fd << " dlc=" << dlc;
      });
      bus.loop();
      EXPECT_EQ(calls, dlc <= 15 ? 1 : 0) << "fd=" << fd << " dlc=" << dlc;
    }
  }
}

TEST(CanFrameTest, LegacySendLengthsRemainClassic) {
  TestCanbus bus;
  std::vector<uint8_t> payload(256);
  for (size_t i = 0; i < payload.size(); i++)
    payload[i] = static_cast<uint8_t>(i);
  for (size_t len = 0; len <= payload.size(); len++) {
    const size_t before = bus.send_count;
    EXPECT_EQ(bus.send_data(0x123, false, false, payload.data(), len), ERROR_OK);
    EXPECT_EQ(bus.send_count, before + 1);
    EXPECT_EQ(bus.sent_frame.can_data_length_code, can_len_to_dlc(std::min<size_t>(len, 8), false));
    EXPECT_FALSE(bus.sent_frame.fd);
    for (size_t i = 0; i < std::min<size_t>(len, 8); i++)
      EXPECT_EQ(bus.sent_frame.data[i], payload[i]);
  }
}

TEST(CanFrameTest, FrameSendRejectsInvalidClassicMetadata) {
  TestCanbus bus;
  CanFrame frame{};
  frame.can_id = 0x123;
  frame.can_data_length_code = 9;
  EXPECT_EQ(bus.send_data(frame), ERROR_FAILTX);
  frame.can_data_length_code = 8;
  frame.bit_rate_switch = true;
  EXPECT_EQ(bus.send_data(frame), ERROR_FAILTX);
  frame.fd = true;
  EXPECT_EQ(bus.send_data(frame), ERROR_FAILTX);
  EXPECT_EQ(bus.send_count, 0);
}

class TestFdCanbus : public TestCanbus {
 public:
  bool supports_fd() const override { return true; }
};

TEST(CanFrameTest, ExplicitFrameValidationAndDriverResult) {
  TestFdCanbus bus;
  CanFrame frame{};
  frame.fd = true;
  frame.can_id = 0x123;
  for (unsigned dlc = 0; dlc <= UINT8_MAX; dlc++) {
    frame.can_data_length_code = dlc;
    const size_t before = bus.send_count;
    EXPECT_EQ(bus.send_data(frame), dlc <= 15 ? ERROR_OK : ERROR_FAILTX);
    EXPECT_EQ(bus.send_count, before + (dlc <= 15 ? 1 : 0));
  }
  frame.can_data_length_code = 8;
  frame.remote_transmission_request = true;
  EXPECT_EQ(bus.send_data(frame), ERROR_FAILTX);
  frame.remote_transmission_request = false;
  bus.send_result = ERROR_ALLTXBUSY;
  EXPECT_EQ(bus.send_data(frame), ERROR_ALLTXBUSY);
}

TEST(CanFrameTest, SendValidatesIdentifierWidth) {
  TestCanbus bus;
  CanFrame frame{};
  for (bool extended : {false, true}) {
    frame.use_extended_id = extended;
    frame.can_id = extended ? 0x1FFFFFFF : 0x7FF;
    EXPECT_EQ(bus.send_data(frame), ERROR_OK);
    frame.can_id++;
    const size_t before = bus.send_count;
    EXPECT_EQ(bus.send_data(frame), ERROR_FAILTX);
    EXPECT_EQ(bus.send_count, before);
  }
}

TEST(CanFrameTest, FdPayloadLengthsAndOptInPadding) {
  TestFdCanbus bus;
  std::vector<uint8_t> payload(256, 0xAB);
  for (bool pad : {false, true}) {
    for (size_t len = 0; len <= payload.size(); len++) {
      const uint8_t dlc = can_len_to_dlc(len, pad);
      const size_t before = bus.send_count;
      EXPECT_EQ(bus.send_data(0x123, false, false, payload.data(), len, true, true, pad),
                dlc == CAN_INVALID_DLC ? ERROR_FAILTX : ERROR_OK);
      if (dlc == CAN_INVALID_DLC) {
        EXPECT_EQ(bus.send_count, before);
        continue;
      }
      EXPECT_EQ(bus.send_count, before + 1);
      EXPECT_EQ(bus.sent_frame.can_data_length_code, dlc);
      EXPECT_TRUE(bus.sent_frame.fd);
      EXPECT_TRUE(bus.sent_frame.bit_rate_switch);
      for (size_t i = 0; i < can_dlc_to_len(dlc); i++)
        EXPECT_EQ(bus.sent_frame.data[i], i < len ? 0xAB : 0);
    }
  }
  EXPECT_EQ(bus.send_data(0x123, false, false, payload.data(), SIZE_MAX, true, true, true), ERROR_FAILTX);
}

TEST(CanFrameTest, PayloadSendRejectsInvalidFlagsAndNullData) {
  TestFdCanbus bus;
  const uint8_t data[9]{};
  EXPECT_EQ(bus.send_data(0x123, false, false, data, 8, false, true), ERROR_FAILTX);
  EXPECT_EQ(bus.send_data(0x123, false, true, data, 8, true, false), ERROR_FAILTX);
  EXPECT_EQ(bus.send_data(0x123, false, false, nullptr, 1), ERROR_FAILTX);
  EXPECT_EQ(bus.send_count, 0);
  EXPECT_EQ(bus.send_data(0x123, false, false, data, 9, false, false, true), ERROR_OK);
  EXPECT_EQ(bus.sent_frame.can_data_length_code, 8);
  EXPECT_EQ(bus.send_data(0x123, false, false, nullptr, 0), ERROR_OK);
  EXPECT_EQ(bus.send_data(0x123, false, true, nullptr, 8), ERROR_OK);
  EXPECT_TRUE(bus.sent_frame.remote_transmission_request);
  EXPECT_EQ(bus.sent_frame.can_data_length_code, 8);
}

TEST(CanFrameTest, DefaultsDoNotChangeLegacyOrExplicitFrameSends) {
  TestFdCanbus bus;
  bus.set_default_fd(true);
  bus.set_default_bit_rate_switch(true);
  const std::vector<uint8_t> data{1, 2, 3};
  EXPECT_EQ(bus.send_data(0x123, false, data), ERROR_OK);
  EXPECT_FALSE(bus.sent_frame.fd);
  EXPECT_FALSE(bus.sent_frame.bit_rate_switch);
  EXPECT_EQ(bus.send_data(0x123, false, true, data), ERROR_OK);
  EXPECT_FALSE(bus.sent_frame.fd);
  EXPECT_TRUE(bus.sent_frame.remote_transmission_request);
  CanFrame frame{};
  frame.can_id = 0x123;
  EXPECT_EQ(bus.send_data(frame), ERROR_OK);
  EXPECT_FALSE(bus.sent_frame.fd);
  EXPECT_FALSE(bus.sent_frame.bit_rate_switch);
}

TEST(CanFrameTest, ActionDefaultsExplicitOverridesAndPadding) {
  TestFdCanbus bus;
  bus.set_can_id(0x123);
  bus.set_default_fd(true);
  bus.set_default_bit_rate_switch(true);
  CanbusSendAction<> action;
  action.set_parent(&bus);
  const uint8_t data[9]{1, 2, 3, 4, 5, 6, 7, 8, 9};
  action.set_data_static(data, sizeof(data));
  action.play();
  EXPECT_EQ(bus.send_count, 0);
  action.set_pad_data([]() -> bool { return true; });
  action.play();
  ASSERT_EQ(bus.send_count, 1);
  EXPECT_TRUE(bus.sent_frame.fd);
  EXPECT_TRUE(bus.sent_frame.bit_rate_switch);
  EXPECT_EQ(bus.sent_frame.can_data_length_code, 9);
  for (size_t i = 0; i < 12; i++)
    EXPECT_EQ(bus.sent_frame.data[i], i < sizeof(data) ? data[i] : 0);

  action.set_data_static(data, 8);
  action.set_fd([]() -> bool { return false; });
  action.play();
  ASSERT_EQ(bus.send_count, 2);
  EXPECT_FALSE(bus.sent_frame.fd);
  EXPECT_FALSE(bus.sent_frame.bit_rate_switch);
  action.set_bit_rate_switch([]() -> bool { return true; });
  action.play();
  EXPECT_EQ(bus.send_count, 2);
  action.set_fd([]() -> bool { return true; });
  action.set_bit_rate_switch([]() -> bool { return false; });
  action.play();
  ASSERT_EQ(bus.send_count, 3);
  EXPECT_TRUE(bus.sent_frame.fd);
  EXPECT_FALSE(bus.sent_frame.bit_rate_switch);
  action.set_remote_transmission_request(true);
  action.play();
  EXPECT_EQ(bus.send_count, 3);
}

TEST(CanFrameTest, ActionInheritsPaddingDefaultAndAllowsOverrides) {
  TestFdCanbus bus;
  bus.set_can_id(0x123);
  bus.set_default_fd(true);
  CanbusSendAction<bool> action;
  action.set_parent(&bus);
  const uint8_t data[9]{1, 2, 3, 4, 5, 6, 7, 8, 9};
  action.set_data_static(data, sizeof(data));
  action.play(false);
  EXPECT_EQ(bus.send_count, 0);
  bus.set_default_pad_data(true);
  action.play(false);
  ASSERT_EQ(bus.send_count, 1);
  EXPECT_EQ(bus.sent_frame.can_data_length_code, 9);
  for (size_t i = 0; i < 12; i++)
    EXPECT_EQ(bus.sent_frame.data[i], i < sizeof(data) ? data[i] : 0);

  action.set_pad_data([](bool) -> bool { return false; });
  action.play(true);
  EXPECT_EQ(bus.send_count, 1);
  action.set_pad_data([](bool pad) -> bool { return pad; });
  action.play(false);
  EXPECT_EQ(bus.send_count, 1);
  action.play(true);
  EXPECT_EQ(bus.send_count, 2);
  bus.set_default_pad_data(false);
  action.play(true);
  EXPECT_EQ(bus.send_count, 3);

  bus.set_default_pad_data(true);
  EXPECT_EQ(bus.send_data(0x123, false, false, data, sizeof(data), true, false), ERROR_FAILTX);
  EXPECT_EQ(bus.send_data(0x123, false, false, data, sizeof(data)), ERROR_OK);
  EXPECT_EQ(bus.send_count, 4);
  EXPECT_EQ(bus.sent_frame.can_data_length_code, 8);
  for (size_t i = 0; i < 8; i++)
    EXPECT_EQ(bus.sent_frame.data[i], data[i]);
}

TEST(CanFrameTest, ActionEvaluatesLambdaFlagsAndPayloadLengths) {
  TestFdCanbus bus;
  bus.set_can_id(0x123);
  CanbusSendAction<bool, unsigned> action;
  action.set_parent(&bus);
  action.set_data_template([](bool, unsigned len) -> std::vector<uint8_t> { return std::vector<uint8_t>(len, 0xAB); });
  action.set_fd([](bool fd, unsigned) -> bool { return fd; });
  action.set_bit_rate_switch([](bool fd, unsigned) -> bool { return fd; });
  action.set_pad_data([](bool, unsigned) -> bool { return true; });
  for (bool fd : {false, true}) {
    for (unsigned len : {0, 8, 9, 12, 64, 65, 256}) {
      const size_t before = bus.send_count;
      action.play(fd, len);
      const bool valid = !fd || len <= 64;
      EXPECT_EQ(bus.send_count, before + (valid ? 1 : 0));
      if (valid) {
        EXPECT_EQ(bus.sent_frame.fd, fd);
        EXPECT_EQ(bus.sent_frame.bit_rate_switch, fd);
        const size_t sent_len = fd ? len : std::min<unsigned>(len, 8);
        EXPECT_EQ(bus.sent_frame.can_data_length_code, can_len_to_dlc(sent_len, true));
        for (size_t i = 0; i < sent_len; i++)
          EXPECT_EQ(bus.sent_frame.data[i], 0xAB);
      }
    }
  }
}

TEST(CanFrameTest, ClassicControllerRejectsFdRegardlessOfConfiguredDefaults) {
  TestCanbus bus;
  bus.set_can_id(0x123);
  bus.set_default_fd(true);
  bus.set_default_bit_rate_switch(true);
  CanbusSendAction<> action;
  action.set_parent(&bus);
  action.set_data_static(nullptr, 0);
  action.play();
  EXPECT_EQ(bus.send_count, 0);
  action.set_fd([]() -> bool { return false; });
  action.play();
  EXPECT_EQ(bus.send_count, 1);
  EXPECT_FALSE(bus.sent_frame.fd);
  EXPECT_FALSE(bus.sent_frame.bit_rate_switch);
}

TEST(CanFrameTest, InvalidFrameDoesNotBlockReceptionAndMetadataIsReset) {
  class MixedCanbus : public TestCanbus {
   protected:
    uint8_t read_count_{0};
    Error read_message(CanFrame *frame) override {
      if (this->read_count_ == 3)
        return ERROR_NOMSG;
      frame->can_id = 0x123;
      std::fill_n(frame->data, CAN_FD_MAX_DATA_LENGTH, 0xAB);
      switch (this->read_count_++) {
        case 0:
          frame->can_data_length_code = 16;
          break;
        case 1:
          frame->can_data_length_code = 15;
          frame->fd = true;
          frame->bit_rate_switch = true;
          frame->error_state_indicator = true;
          break;
        case 2:
          // Classic drivers do not write the FD metadata.
          EXPECT_FALSE(frame->fd);
          EXPECT_FALSE(frame->bit_rate_switch);
          EXPECT_FALSE(frame->error_state_indicator);
          frame->can_data_length_code = 15;
          break;
      }
      return ERROR_OK;
    }
  };
  MixedCanbus bus;
  size_t calls = 0;
  bus.add_callback([&](uint32_t, bool, bool, const std::vector<uint8_t> &data) {
    EXPECT_EQ(data.size(), calls == 0 ? 64 : 8);
    calls++;
  });
  bus.loop();
  EXPECT_EQ(calls, 2);
}

}  // namespace esphome::canbus::testing
