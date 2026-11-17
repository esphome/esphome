#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/pipsolar/pipsolar.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/helpers.h"

namespace esphome::pipsolar::testing {

namespace {

class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;

  void write_array(const uint8_t *data, size_t len) override {}
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len)
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }

  MOCK_METHOD(bool, peek_byte, (uint8_t *data), (override));
  MOCK_METHOD(uart::UARTFlushResult, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

// Payload plus the inverter's CRC (bytes 0x28, 0x0D and 0x0A bumped by one) and CR
std::vector<uint8_t> frame(const std::string &payload, bool corrupt_crc = false) {
  std::vector<uint8_t> out(payload.begin(), payload.end());
  uint16_t crc = crc16be(out.data(), out.size());
  uint8_t hi = crc >> 8, lo = crc & 0xFF;
  for (uint8_t *b : {&hi, &lo}) {
    if (*b == 0x28 || *b == 0x0D || *b == 0x0A)
      (*b)++;
  }
  if (corrupt_crc)
    lo ^= 0x01;
  out.insert(out.end(), {hi, lo, 0x0D});
  return out;
}

class PipsolarTest : public ::testing::Test {
 protected:
  void SetUp() override { this->sut_.set_uart_parent(&this->uart_); }

  // Runs one poll of the single enabled command and decodes the answer
  void poll_(const std::vector<uint8_t> &answer) {
    this->sut_.setup();
    this->sut_.update();
    this->sut_.loop();  // sends the poll
    this->uart_.rx.insert(this->uart_.rx.end(), answer.begin(), answer.end());
    this->sut_.loop();  // reads the answer
    this->sut_.loop();  // checks the CRC
    this->sut_.loop();  // decodes
  }

  MockUARTComponent uart_;
  Pipsolar sut_;
};

constexpr const char *QPIRI_PREFIX = "(230.0 21.7 230.0 50.0 21.7 5000 4000 48.0 46.0 42.0 56.4 54.0 1 10 010 ";
constexpr const char *QPIRI_GOOD_TAIL = "1 2 1 9 01 0 0 54.0 0 1";
// Ends just before the fan offset field
constexpr const char *QPIGS_PREFIX =
    "(230.0 50.0 230.0 50.0 0161 0105 003 397 52.70 000 100 0035 0000 000.0 00.00 00000 00010101";

class PipsolarQpiriTest : public PipsolarTest {
 protected:
  void SetUp() override {
    PipsolarTest::SetUp();
    this->sut_.set_battery_type(&this->battery_type_);
    this->sut_.set_input_voltage_range(&this->input_voltage_range_);
    this->sut_.set_output_source_priority(&this->output_source_priority_);
    this->sut_.set_pv_ok_condition_for_parallel(&this->pv_ok_condition_for_parallel_);
    this->sut_.set_pv_power_balance(&this->pv_power_balance_);
  }

  // The four QPIRI fields that used to publish INT_MIN when missing
  void expect_optional_fields_nan_() {
    EXPECT_THAT(this->input_voltage_range_.state, ::testing::IsNan());
    EXPECT_THAT(this->output_source_priority_.state, ::testing::IsNan());
    EXPECT_THAT(this->pv_ok_condition_for_parallel_.state, ::testing::IsNan());
    EXPECT_THAT(this->pv_power_balance_.state, ::testing::IsNan());
  }

  sensor::Sensor battery_type_;
  sensor::Sensor input_voltage_range_;
  sensor::Sensor output_source_priority_;
  sensor::Sensor pv_ok_condition_for_parallel_;
  sensor::Sensor pv_power_balance_;
};

class PipsolarQpigsTest : public PipsolarTest {
 protected:
  void SetUp() override {
    PipsolarTest::SetUp();
    this->sut_.set_battery_voltage_offset_for_fans_on(&this->offset_);
  }

  sensor::Sensor offset_;
};

class PipsolarQpiwsTest : public PipsolarTest {
 protected:
  void SetUp() override {
    PipsolarTest::SetUp();
    this->sut_.set_fault_code(&this->fault_code_);
  }

  binary_sensor::BinarySensor fault_code_;
};

}  // namespace

TEST_F(PipsolarQpiriTest, IntegerFieldsPublishValues) {
  this->poll_(frame(std::string(QPIRI_PREFIX) + QPIRI_GOOD_TAIL));
  EXPECT_FLOAT_EQ(this->battery_type_.state, 1.0f);
  EXPECT_FLOAT_EQ(this->input_voltage_range_.state, 1.0f);
  EXPECT_FLOAT_EQ(this->output_source_priority_.state, 2.0f);
  EXPECT_FLOAT_EQ(this->pv_ok_condition_for_parallel_.state, 0.0f);
  EXPECT_FLOAT_EQ(this->pv_power_balance_.state, 1.0f);
}

TEST_F(PipsolarQpiriTest, UnparseableFieldsPublishNan) {
  this->poll_(frame(std::string(QPIRI_PREFIX) + "x x 1 9 01 0 0 54.0 x x"));
  EXPECT_FLOAT_EQ(this->battery_type_.state, 1.0f);
  this->expect_optional_fields_nan_();
}

// A failed poll decodes an empty answer, so every field is missing
TEST_F(PipsolarQpiriTest, FailedPollPublishesNan) {
  this->poll_(frame(std::string(QPIRI_PREFIX) + QPIRI_GOOD_TAIL, true));
  EXPECT_THAT(this->battery_type_.state, ::testing::IsNan());
  this->expect_optional_fields_nan_();
}

TEST_F(PipsolarQpigsTest, FanOffsetPublishesValue) {
  this->poll_(frame(std::string(QPIGS_PREFIX) + " 03 00 00000 010"));
  EXPECT_FLOAT_EQ(this->offset_.state, 0.3f);
}

TEST_F(PipsolarQpigsTest, MissingFanOffsetPublishesNan) {
  this->poll_(frame(QPIGS_PREFIX));
  EXPECT_THAT(this->offset_.state, ::testing::IsNan());
}

TEST_F(PipsolarQpiwsTest, FaultCodePublishesOn) {
  this->poll_(frame("(0000000000000000000000000000000050000"));
  EXPECT_TRUE(this->fault_code_.has_state());
  EXPECT_TRUE(this->fault_code_.state);
}

TEST_F(PipsolarQpiwsTest, ZeroFaultCodePublishesOff) {
  this->poll_(frame("(0000000000000000000000000000000000000"));
  EXPECT_TRUE(this->fault_code_.has_state());
  EXPECT_FALSE(this->fault_code_.state);
}

TEST_F(PipsolarQpiwsTest, MissingFaultCodePublishesUnknown) {
  this->poll_(frame("(0000000000000000000000000000000"));
  EXPECT_FALSE(this->fault_code_.has_state());
}

TEST_F(PipsolarQpiwsTest, UnparseableFaultCodePublishesUnknown) {
  this->poll_(frame("(0000000000000000000000000000000xx0000"));
  EXPECT_FALSE(this->fault_code_.has_state());
}

}  // namespace esphome::pipsolar::testing
