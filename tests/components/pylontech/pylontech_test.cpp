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
#include "esphome/core/application.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::pylontech::testing {

namespace {

class MockUARTComponent : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;
  std::string tx;  // everything the hub wrote, in order

  void push_rx(std::initializer_list<std::string> lines) {
    for (const std::string &line : lines) {
      this->rx.insert(this->rx.end(), line.begin(), line.end());
    }
  }

  void write_array(const uint8_t *data, size_t len) override {
    this->tx.append(reinterpret_cast<const char *>(data), len);
  }
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

struct PublishedCells {
  uint8_t bat_num;
  cell_mask_t read;
  PylontechListener::CellArray cells;
};
// Every cell record any listener gets, in publish order
std::vector<PublishedCells> published_cells;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class TestListener final : public PylontechListener {
 public:
  explicit TestListener(uint8_t bat_num) : PylontechListener(bat_num) {}
  void on_line_read(LineContents *line) override {
    EXPECT_EQ(line->bat_num, this->bat_num_);
    published.push_back(*line);
  }
  void on_cells_read(const CellArray &cells, cell_mask_t read) override {
    published_cells.push_back({this->bat_num_, read, cells});
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
constexpr const char *REPLY_END = "Command completed successfully\r\n$$\r\n";
constexpr const char *CELL_HEADER =
    "Battery  Volt     Curr     Tempr    Base State   Volt. State  Curr. State  Temp. State  SOC          Coulomb      BAL\r\n";
// clang-format on

// A line of the "bat N" reply for cell cell with volt (mV) and tempr (m°C); the record keeps 0.01 °C
std::string cell_line(int cell, int volt, int tempr) {
  char buf[160];
  snprintf(buf, sizeof(buf),
           "%d        %d     -914     %d    Dischg       Normal       Normal       Normal       70%%          "
           "34015 mAH    N\r\n",
           cell, volt, tempr);
  return buf;
}

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

// The cells of batteries 1 and 3 are read
class PylontechCellTest : public PylontechTest {
 protected:
  void SetUp() override {
    PylontechTest::SetUp();
    published_cells.clear();
    this->hub_.add_cell_battery(1);
    this->hub_.add_cell_battery(3);
  }

  // Lets the hub drain what was pushed and publish everything pending
  void drain_and_publish_() {
    for (int i = 0; i < 8; i++) {
      this->hub_.loop();
    }
  }

  // Lets the pause before a bat command pass: moves the loop clock on and runs one idle loop, which also
  // publishes one pending record
  void let_pause_pass_() {
    this->now_ += 50;
    LoopBlockingGuard{nullptr, nullptr, this->now_};
    this->hub_.loop();
  }

  uint32_t now_{1000};
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

TEST_F(PylontechCellTest, CellsAreReadAfterThePwrReplyOneBatteryAtATime) {
  // Each pause's idle loop publishes one pending record: the battery record, then one battery's cells
  this->uart_.push_rx({"pwr\r\n@\r\n", HEADER, BAT1, REPLY_END});
  this->hub_.loop();
  EXPECT_TRUE(this->uart_.tx.empty());  // the pack needs a pause after its reply
  EXPECT_TRUE(this->published_().empty());
  this->let_pause_pass_();
  EXPECT_EQ(this->uart_.tx, "bat 1\n");
  ASSERT_EQ(this->published_().size(), 1u);
  EXPECT_EQ(this->published_()[0].bat_num, 1);

  this->uart_.push_rx({"pylon>bat 1\r\n@\r\n", CELL_HEADER, cell_line(0, 3308, 18300), cell_line(1, 3310, 18400),
                       cell_line(14, 3299, 17900), REPLY_END});
  this->hub_.loop();
  EXPECT_TRUE(published_cells.empty());
  this->let_pause_pass_();
  EXPECT_EQ(this->uart_.tx, "bat 1\nbat 3\n");
  ASSERT_EQ(published_cells.size(), 1u);

  this->uart_.push_rx({"pylon>bat 3\r\n@\r\n", CELL_HEADER, cell_line(5, 3250, 16000), REPLY_END});
  this->hub_.loop();
  this->let_pause_pass_();
  EXPECT_EQ(this->uart_.tx, "bat 1\nbat 3\n");
  ASSERT_EQ(published_cells.size(), 2u);
  this->hub_.loop();
  ASSERT_EQ(published_cells.size(), 2u);

  const PublishedCells &b1 = published_cells[0];
  EXPECT_EQ(b1.bat_num, 1);
  EXPECT_EQ(b1.read, (1u << 0) | (1u << 1) | (1u << 14));
  EXPECT_EQ(b1.cells[0].volt, 3308);
  EXPECT_EQ(b1.cells[0].tempr, 1830);
  EXPECT_EQ(b1.cells[1].volt, 3310);
  EXPECT_EQ(b1.cells[14].volt, 3299);
  EXPECT_EQ(b1.cells[14].tempr, 1790);
  const PublishedCells &b3 = published_cells[1];
  EXPECT_EQ(b3.bat_num, 3);
  EXPECT_EQ(b3.read, 1u << 5);
  EXPECT_EQ(b3.cells[5].volt, 3250);
  EXPECT_EQ(b3.cells[5].tempr, 1600);

  // The next pwr reply starts the cycle again
  this->uart_.push_rx({HEADER, BAT1, REPLY_END});
  this->hub_.loop();
  this->let_pause_pass_();
  EXPECT_EQ(this->uart_.tx, "bat 1\nbat 3\nbat 1\n");
}

TEST_F(PylontechCellTest, CellPollingCanBeTurnedOffAtAnyReplyEnd) {
  this->hub_.set_cell_polling_enabled(false);
  this->uart_.push_rx({HEADER, BAT1, REPLY_END});
  this->drain_and_publish_();
  this->let_pause_pass_();
  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_EQ(this->published_().size(), 1u);

  this->hub_.set_cell_polling_enabled(true);
  this->uart_.push_rx({HEADER, BAT1, REPLY_END});
  this->hub_.loop();
  this->let_pause_pass_();
  EXPECT_EQ(this->uart_.tx, "bat 1\n");

  // Turned off while bat 1 is answering: its cells are still published, bat 3 is not sent
  this->hub_.set_cell_polling_enabled(false);
  this->uart_.push_rx({CELL_HEADER, cell_line(0, 3308, 18300), REPLY_END});
  this->drain_and_publish_();
  this->let_pause_pass_();
  EXPECT_EQ(this->uart_.tx, "bat 1\n");
  ASSERT_EQ(published_cells.size(), 1u);
  EXPECT_EQ(published_cells[0].bat_num, 1);
}

TEST_F(PylontechCellTest, UpdateAbandonsAnUnansweredBatReply) {
  this->uart_.push_rx({HEADER, BAT1, REPLY_END});
  this->hub_.loop();
  this->let_pause_pass_();
  EXPECT_EQ(this->uart_.tx, "bat 1\n");

  // No reply ever comes; the next pwr lines are battery lines again
  this->hub_.update();
  this->uart_.push_rx({HEADER, BAT2, REPLY_END});
  this->drain_and_publish_();
  this->let_pause_pass_();
  EXPECT_EQ(this->uart_.tx, "bat 1\npwr\nbat 1\n");
  ASSERT_EQ(this->published_().size(), 2u);
  EXPECT_EQ(this->published_()[1].bat_num, 2);
  EXPECT_TRUE(published_cells.empty());
}

TEST_F(PylontechCellTest, UpdateCancelsABatCommandThatWasNotSentYet) {
  this->uart_.push_rx({HEADER, BAT1, REPLY_END});
  this->hub_.loop();
  this->hub_.update();
  this->let_pause_pass_();
  EXPECT_EQ(this->uart_.tx, "pwr\n");
}

TEST_F(PylontechCellTest, LateBatReplyAfterUpdateIsNotMistakenForThePwrReply) {
  this->uart_.push_rx({HEADER, BAT1, REPLY_END});
  this->hub_.loop();
  this->let_pause_pass_();
  EXPECT_EQ(this->uart_.tx, "bat 1\n");

  // The pack answers the abandoned bat 1 only after the next pwr went out, then answers pwr
  this->hub_.update();
  this->uart_.push_rx({CELL_HEADER, cell_line(0, 3308, 18300), REPLY_END});
  this->uart_.push_rx({"pwr\r\n@\r\n", HEADER, BAT2, REPLY_END});
  this->drain_and_publish_();
  this->let_pause_pass_();

  // The late cells are dropped, its "$$" sends nothing, and the pwr reply starts the cycle once
  EXPECT_EQ(this->uart_.tx, "bat 1\npwr\nbat 1\n");
  ASSERT_EQ(this->published_().size(), 2u);
  EXPECT_EQ(this->published_()[1].bat_num, 2);
  EXPECT_EQ(this->published_()[1].volt, 46012);
  EXPECT_TRUE(published_cells.empty());
}

TEST_F(PylontechCellTest, BadCellLinesAreIgnoredAndTheNextReplyStartsEmpty) {
  this->uart_.push_rx({HEADER, BAT1, REPLY_END});
  this->hub_.loop();
  this->let_pause_pass_();
  this->uart_.push_rx({CELL_HEADER, cell_line(0, 3308, 18300), "0        abc      -914     18300    Dischg\r\n",
                       cell_line(15, 3300, 18000), cell_line(14, 70000, 18000), cell_line(13, 3300, 400000),
                       REPLY_END});
  this->hub_.loop();
  this->let_pause_pass_();
  this->uart_.push_rx({CELL_HEADER, cell_line(5, 3250, 16000), cell_line(5, 3251, 16100), REPLY_END});
  this->drain_and_publish_();

  // The bad line for cell 0 leaves the good one; cell 15 does not exist; 70000 mV and 400 °C are out of
  // range; the second cell 5 line replaces the first
  ASSERT_EQ(published_cells.size(), 2u);
  EXPECT_EQ(published_cells[0].read, 1u << 0);
  EXPECT_EQ(published_cells[0].cells[0].volt, 3308);
  EXPECT_EQ(published_cells[1].bat_num, 3);
  EXPECT_EQ(published_cells[1].read, 1u << 5);
  EXPECT_EQ(published_cells[1].cells[5].volt, 3251);

  // A reply that read nothing publishes an empty record, not the old cells
  this->uart_.push_rx({HEADER, BAT1, REPLY_END});
  this->hub_.loop();
  this->let_pause_pass_();
  this->uart_.push_rx({CELL_HEADER, REPLY_END});
  this->drain_and_publish_();
  ASSERT_EQ(published_cells.size(), 3u);
  EXPECT_EQ(published_cells[2].bat_num, 1);
  EXPECT_EQ(published_cells[2].read, 0u);
}

TEST_F(PylontechCellTest, ANewReplyUnmarksTheUnpublishedRecordOfItsBattery) {
  // A whole cycle arrives with only the idle loops of the pauses: the battery record and battery 1's
  // cells are published, battery 3's cells stay pending
  this->uart_.push_rx({HEADER, BAT1, REPLY_END});
  this->hub_.loop();
  this->let_pause_pass_();
  this->uart_.push_rx({CELL_HEADER, cell_line(0, 3308, 18300), REPLY_END});
  this->hub_.loop();
  this->let_pause_pass_();
  this->uart_.push_rx({CELL_HEADER, cell_line(5, 3250, 16000), REPLY_END});
  this->hub_.loop();
  this->uart_.push_rx({HEADER, BAT1, REPLY_END});
  this->hub_.loop();
  this->let_pause_pass_();
  ASSERT_EQ(published_cells.size(), 1u);
  EXPECT_EQ(published_cells[0].bat_num, 1);

  // The next bat 1 reply is cut short by a gap: battery 3's cells go out, battery 1's wait for "$$"
  this->uart_.push_rx({CELL_HEADER, cell_line(0, 3400, 19000)});
  this->hub_.loop();
  this->hub_.loop();
  this->hub_.loop();
  ASSERT_EQ(published_cells.size(), 2u);
  EXPECT_EQ(published_cells[1].bat_num, 3);
  this->uart_.push_rx({REPLY_END});
  this->drain_and_publish_();
  ASSERT_EQ(published_cells.size(), 3u);
  EXPECT_EQ(published_cells[2].bat_num, 1);
  EXPECT_EQ(published_cells[2].cells[0].volt, 3400);
}

}  // namespace esphome::pylontech::testing
