#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "esphome/components/pylontech/pylontech.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::pylontech::testing {

namespace {

class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;

  void push_rx(std::initializer_list<std::string> lines) {
    for (const std::string &line : lines) {
      this->rx.insert(this->rx.end(), line.begin(), line.end());
    }
  }

  void write_array(const uint8_t *data, size_t len) override {}
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len) {
      return false;
    }
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }

  MOCK_METHOD(bool, peek_byte, (uint8_t * data), (override));
  MOCK_METHOD(uart::UARTFlushResult, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

// Every record any listener gets, in publish order
std::vector<PylontechListener::LineContents> published;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class TestListener final : public PylontechListener {
 public:
  explicit TestListener(uint8_t bat_num) : PylontechListener(bat_num) {}
  void on_line_read(LineContents *line) override {
    EXPECT_EQ(line->bat_num, this->bat_num_);
    published.push_back(*line);
  }
  void dump_config() override {}
};

// clang-format off
constexpr const char *HEADER =
    "Power Volt   Curr   Tempr  Tlow   Thigh  Vlow   Vhigh  Base.St  Volt.St  Curr.St  Temp.St  Coulomb  Time"
    "                 B.V.St   B.T.St   MosTempr M.T.St\r\n";
constexpr const char *BAT1 =
    "1     50548  8910   25000  24200  25000  3368   3371   Charge   Normal   Normal   Normal   97%      "
    "2021-06-30 20:49:45  Normal  Normal  22700    Normal\r\n";
constexpr const char *BAT2 =
    "2     46012  -1255  9100   5300   5500   3047   3091   SysError Low      Normal   Normal   4%       "
    "2025-11-28 17:56:33  Low      Normal  7800     Normal\r\n";
constexpr const char *NEW_HEADER =
    "Power Volt Curr Tempr Tlow Tlow.Id Thigh Thigh.Id Vlow Vlow.Id Vhigh Vhigh.Id Base.St Volt.St Curr.St Temp.St "
    "Coulomb Time                B.V.St B.T.St MosTempr M.T.St SysAlarm.St\r\n";
constexpr const char *NEW_BAT1 =
    "1     49405 0   17600 13700 8      14500 0        3293 2       3294   0       Idle    Normal  Normal  Normal  "
    "60%     2025-12-05 00:53:41 Normal Normal 16600    Normal Normal\r\n";
// clang-format on

// An older firmware line for battery bat with pack voltage volt (mV)
std::string bat_line(int bat, int volt) {
  char buf[192];
  snprintf(buf, sizeof(buf),
           "%d     %d  0      17000  16000  18000  3200   3300   Idle     Normal   Normal   Normal   50%%      "
           "2025-11-28 17:56:33  Normal   Normal  16000    Normal\r\n",
           bat, volt);
  return buf;
}

class PylontechTest : public ::testing::Test {
 protected:
  void SetUp() override {
    published.clear();
    this->hub_.set_uart_parent(&this->uart_);
    for (TestListener &listener : this->listeners_) {
      this->hub_.register_listener(&listener);
    }
  }

  std::vector<PylontechListener::LineContents> &published_() { return published; }

  MockUARTComponent uart_;
  PylontechComponent hub_;
  TestListener listeners_[3]{TestListener(1), TestListener(2), TestListener(3)};
};

}  // namespace

TEST_F(PylontechTest, BurstIsPublishedAfterTheDrainOnePerLoop) {
  this->uart_.push_rx({HEADER, BAT1, BAT2});
  this->hub_.loop();
  EXPECT_TRUE(this->uart_.rx.empty());
  EXPECT_TRUE(this->published_().empty());

  this->hub_.loop();
  ASSERT_EQ(this->published_().size(), 1u);
  this->hub_.loop();
  ASSERT_EQ(this->published_().size(), 2u);
  this->hub_.loop();
  EXPECT_EQ(this->published_().size(), 2u);

  const auto &b1 = this->published_()[0];
  EXPECT_EQ(b1.bat_num, 1);
  EXPECT_EQ(b1.volt, 50548);
  EXPECT_EQ(b1.curr, 8910);
  EXPECT_EQ(b1.tempr, 25000);
  EXPECT_EQ(b1.tlow, 24200);
  EXPECT_EQ(b1.thigh, 25000);
  EXPECT_EQ(b1.vlow, 3368);
  EXPECT_EQ(b1.vhigh, 3371);
  EXPECT_STREQ(b1.base_st, "Charge");
  EXPECT_STREQ(b1.volt_st, "Normal");
  EXPECT_EQ(b1.coulomb, 97);
  EXPECT_EQ(b1.mostempr, 22700);

  const auto &b2 = this->published_()[1];
  EXPECT_EQ(b2.bat_num, 2);
  EXPECT_EQ(b2.curr, -1255);
  EXPECT_STREQ(b2.base_st, "SysError");
  EXPECT_STREQ(b2.volt_st, "Low");
  EXPECT_EQ(b2.coulomb, 4);
  EXPECT_EQ(b2.mostempr, 7800);
}

TEST_F(PylontechTest, NothingIsPublishedWhileBytesKeepArriving) {
  this->uart_.push_rx({HEADER, BAT1});
  this->hub_.loop();
  this->uart_.push_rx({BAT2});
  this->hub_.loop();
  EXPECT_TRUE(this->published_().empty());
  this->hub_.loop();
  this->hub_.loop();
  EXPECT_EQ(this->published_().size(), 2u);
}

TEST_F(PylontechTest, NewerFirmwareSkipsIdColumns) {
  this->uart_.push_rx({NEW_HEADER, NEW_BAT1});
  this->hub_.loop();
  this->hub_.loop();
  ASSERT_EQ(this->published_().size(), 1u);
  const auto &b1 = this->published_()[0];
  EXPECT_EQ(b1.volt, 49405);
  EXPECT_EQ(b1.tlow, 13700);
  EXPECT_EQ(b1.thigh, 14500);
  EXPECT_EQ(b1.vlow, 3293);
  EXPECT_EQ(b1.vhigh, 3294);
  EXPECT_STREQ(b1.base_st, "Idle");
  EXPECT_EQ(b1.coulomb, 60);
  EXPECT_EQ(b1.mostempr, 16600);
}

TEST_F(PylontechTest, OverlongHeaderStillSetsTheColumnLayout) {
  // Past the 199 character line limit: the line is dropped but its start still selects the layout
  const std::string pad(40, ' ');
  std::string new_header(NEW_HEADER);
  new_header.insert(new_header.size() - 2, pad + "Extra.St");
  ASSERT_GT(new_header.size(), 201u);
  this->uart_.push_rx({new_header, NEW_BAT1});
  this->hub_.loop();
  this->hub_.loop();
  ASSERT_EQ(this->published_().size(), 1u);
  EXPECT_EQ(this->published_()[0].thigh, 14500);
  EXPECT_EQ(this->published_()[0].mostempr, 16600);

  // An overlong older header switches back
  std::string old_header(HEADER);
  old_header.insert(old_header.size() - 2, pad + pad + "Extra.St");
  ASSERT_GT(old_header.size(), 201u);
  this->uart_.push_rx({old_header, BAT1});
  this->hub_.loop();
  this->hub_.loop();
  ASSERT_EQ(this->published_().size(), 2u);
  EXPECT_EQ(this->published_()[1].thigh, 25000);
  EXPECT_EQ(this->published_()[1].mostempr, 22700);
}

TEST_F(PylontechTest, BatteryWithoutListenerIsIgnored) {
  this->uart_.push_rx(
      {HEADER, bat_line(5, 49000), bat_line(17, 49000), bat_line(0, 49000), bat_line(INT32_MIN, 49000), BAT1, BAT2});
  for (int i = 0; i < 4; i++) {
    this->hub_.loop();
  }
  ASSERT_EQ(this->published_().size(), 2u);
  EXPECT_EQ(this->published_()[0].bat_num, 1);
  EXPECT_EQ(this->published_()[1].bat_num, 2);
}

TEST_F(PylontechTest, OutOfRangeValueRejectsTheLine) {
  this->uart_.push_rx({HEADER, bat_line(1, -1), bat_line(1, 70000), BAT2});
  for (int i = 0; i < 3; i++) {
    this->hub_.loop();
  }
  ASSERT_EQ(this->published_().size(), 1u);
  EXPECT_EQ(this->published_()[0].bat_num, 2);
}

TEST_F(PylontechTest, InvalidAndUnknownLinesAreSkipped) {
  this->uart_.push_rx({"pwr\r\n@\r\n", HEADER, "1     abc\r\n", BAT2, "$$\r\n"});
  for (int i = 0; i < 3; i++) {
    this->hub_.loop();
  }
  ASSERT_EQ(this->published_().size(), 1u);
  EXPECT_EQ(this->published_()[0].bat_num, 2);
}

TEST_F(PylontechTest, RepeatedBatteryLineOverwrites) {
  this->uart_.push_rx({HEADER, bat_line(3, 49000), BAT1, bat_line(3, 49500)});
  for (int i = 0; i < 4; i++) {
    this->hub_.loop();
  }
  ASSERT_EQ(this->published_().size(), 2u);
  EXPECT_EQ(this->published_()[0].bat_num, 1);
  EXPECT_EQ(this->published_()[1].bat_num, 3);
  EXPECT_EQ(this->published_()[1].volt, 49500);

  // The next reply fills the slot again
  this->uart_.push_rx({HEADER, bat_line(3, 48000)});
  this->hub_.loop();
  this->hub_.loop();
  ASSERT_EQ(this->published_().size(), 3u);
  EXPECT_EQ(this->published_()[2].volt, 48000);
}

TEST_F(PylontechTest, BadLineClearsTheBatteryPendingRecord) {
  this->uart_.push_rx({HEADER, BAT1, "1     abc\r\n", BAT2});
  for (int i = 0; i < 3; i++) {
    this->hub_.loop();
  }
  ASSERT_EQ(this->published_().size(), 1u);
  EXPECT_EQ(this->published_()[0].bat_num, 2);
}

}  // namespace esphome::pylontech::testing
