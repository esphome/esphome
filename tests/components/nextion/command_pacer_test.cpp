#include <gtest/gtest.h>

#include <algorithm>
#include <new>
#include <string>
#include <vector>

#include "esphome/components/nextion/nextion.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::nextion::testing {

// Explicit instantiation skips access checks, which lets the test run the pending sends the way
// Nextion::loop() does, and handle an answer the way an 0x01 from the display does, without making them public
template<void (Nextion::*P)(), void (Nextion::*R)(bool), bool (Nextion::*A)(bool), bool NextionBase::*U>
struct PacerAccess {
  friend void send_pending(Nextion &nextion) { (nextion.*P)(); }
  friend void clear_queue(Nextion &nextion) { (nextion.*R)(false); }
  friend void answer_oldest(Nextion &nextion) { (nextion.*A)(true); }
  friend void mark_ready(Nextion &nextion) { nextion.*U = true; }
};
void send_pending(Nextion &nextion);
void clear_queue(Nextion &nextion);
void answer_oldest(Nextion &nextion);
void mark_ready(Nextion &nextion);
template struct PacerAccess<&Nextion::process_pending_in_queue_, &Nextion::reset_, &Nextion::remove_from_q_,
                            &Nextion::is_setup_>;

// The same for reading and handling what the display sent, as loop() does, and for a value it sends after a query
template<void (Nextion::*S)(), void (Nextion::*H)(), void (Nextion::*V)(int)> struct AnswerAccess {
  friend void read_answers(Nextion &nextion) { (nextion.*S)(); }
  friend void handle_answers(Nextion &nextion) { (nextion.*H)(); }
  friend void answer_with_value(Nextion &nextion, int value) { (nextion.*V)(value); }
};
void read_answers(Nextion &nextion);
void handle_answers(Nextion &nextion);
void answer_with_value(Nextion &nextion, int value);
template struct AnswerAccess<&Nextion::process_serial_, &Nextion::process_nextion_commands_,
                             &Nextion::set_numeric_return_>;

// The same for the waveform step of loop(), which sends the addt command that starts a waveform transfer
template<void (Nextion::*W)()> struct WaveformAccess {
  friend void send_pending_waveform(Nextion &nextion) { (nextion.*W)(); }
};
void send_pending_waveform(Nextion &nextion);
template struct WaveformAccess<&Nextion::check_pending_waveform_>;

/// Sets the loop clock the way the main loop does at the start of a component's loop()
static void start_loop_pass() { LoopBlockingGuard{nullptr, nullptr, millis()}; }

/// The same at a chosen time, for a test that runs only loop passes and no timer
static void start_loop_pass_at(uint32_t now_ms) { LoopBlockingGuard{nullptr, nullptr, now_ms}; }

/// A sensor the component asks for its value with "get"
class QueriedSensor : public NextionComponentBase {
 public:
  NextionQueueType get_queue_type() const override { return NextionQueueType::SENSOR; }
};

/// A sensor that draws a waveform; its values go out after the display has answered the addt command with 0xFE
class WaveformSensor : public QueriedSensor {
 public:
  void add_value(uint8_t value) { this->wave_buffer_.push_back(value); }
};

/// Records each command the component writes, and holds the answers a test makes the display send until they are read
class RecordingUART : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {
    for (size_t i = 0; i < len; i++) {
      if (data[i] != 0xFF) {
        this->current_ += static_cast<char>(data[i]);
      } else if (!this->current_.empty()) {
        // The first of the three 0xFF bytes ends the command
        this->commands.push_back(this->current_);
        this->current_.clear();
      }
    }
  }
  bool peek_byte(uint8_t *data) override {
    if (this->received_.empty())
      return false;
    *data = static_cast<uint8_t>(this->received_[0]);
    return true;
  }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->received_.size() < len)
      return false;
    this->received_.copy(reinterpret_cast<char *>(data), len);
    this->received_.erase(0, len);
    return true;
  }
  size_t available() override { return this->received_.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif  // defined(USE_ESP8266) || defined(USE_ESP32)

  /// The display answers a command with 0x01 (success) and the three 0xFF bytes that end each message
  void send_success_answer() { this->received_.append("\x01\xFF\xFF\xFF", 4); }
  /// The display answers an addt command with 0xFE when it is ready for the waveform values
  void send_ready_for_data() { this->received_.append("\xFE\xFF\xFF\xFF", 4); }
  /// The display answers "connect" with its model and firmware
  void send_connect_answer() {
    this->received_ += "comok 1,30601-0,NX4832T035_011R,52,61488,D264B8204F0E1828,16777216\xFF\xFF\xFF";
  }
  /// The display sends 0x88 when it has started
  void send_display_started() { this->received_.append("\x88\xFF\xFF\xFF", 4); }

  /// How many times the component has written this command
  auto times_sent(const std::string &command) const {
    return std::count(this->commands.begin(), this->commands.end(), command);
  }

  std::vector<std::string> commands;

 protected:
  void check_logger_conflict() override {}

  std::string current_;
  std::string received_;
};

class CommandPacer : public ::testing::Test {
 protected:
  void SetUp() override {
    // A fresh App, so the only timer in its scheduler is the one this test plans
    App.~Application();
    new (&App) Application();
    this->nextion_.set_uart_parent(&this->uart_);
    mark_ready(this->nextion_);
  }

  void TearDown() override {
    clear_queue(this->nextion_);
    App.~Application();
    new (&App) Application();  // drops a timer left by a failed test; later tests use App too
  }

  /// Waits for the planned wake-up and runs only the scheduler, as a main loop pass without component loops does
  static void run_wake_up() {
    App.scheduler.process_to_add();
    const auto wake_in = App.scheduler.next_schedule_in(millis());
    ASSERT_TRUE(wake_in.has_value());
    delay(*wake_in);
    App.scheduler.call(millis());
  }

  static bool wake_up_planned() {
    App.scheduler.process_to_add();
    return App.scheduler.next_schedule_in(millis()).has_value();
  }

  /// At 1000 ms an addt goes out, and 20 queries wait behind it. command_spacing is 1 ms.
  void hold_20_queries_behind_addt_() {
    this->nextion_.set_command_spacing(1);
    this->waveform_.set_component_id(1);
    this->waveform_.set_wave_channel_id(0);
    this->waveform_.add_value(20);
    start_loop_pass_at(1000);
    this->nextion_.add_addt_command_to_queue(&this->waveform_);
    for (int i = 0; i < 20; i++)
      this->nextion_.add_to_get_queue(&this->sensor_);
  }

  /// The display connects and reports its start, and the component sends the setup commands
  void connect_display_() {
    // The first loop pass sends "connect", and a pass more than 500 ms later reads the display's answer
    start_loop_pass_at(1000);
    this->nextion_.loop();
    this->uart_.send_connect_answer();
    start_loop_pass_at(1600);
    this->nextion_.loop();

    // One pass reads the 0x88, the next sends the setup commands
    this->uart_.send_display_started();
    this->nextion_.loop();
    this->nextion_.loop();
  }

  RecordingUART uart_;
  Nextion nextion_;
  QueriedSensor sensor_;     // outlives the queue, which TearDown() clears
  WaveformSensor waveform_;  // the same
};

// command_spacing sets the minimum time between two commands. A command the pacer holds back must go out
// when that time has passed, not only at the next loop pass, which can be up to loop_interval (16 ms) later.
TEST_F(CommandPacer, HeldBackCommandIsSentWhenSpacingHasPassed) {
  this->nextion_.set_command_spacing(100);

  // 0 ms: the first command goes out, the second has to wait 100 ms
  start_loop_pass();
  this->nextion_.send_command("page 1");
  this->nextion_.send_command("vis b0,1");
  EXPECT_EQ(this->uart_.commands, std::vector<std::string>{"page 1"});

  // 10 ms: a loop pass still holds the command back, and the wake-up stays due at 100 ms
  delay(10);
  start_loop_pass();
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands, std::vector<std::string>{"page 1"});

  App.scheduler.process_to_add();
  const auto wake_in = App.scheduler.next_schedule_in(millis());
  ASSERT_TRUE(wake_in.has_value());
  EXPECT_GT(*wake_in, 0u);
  EXPECT_LE(*wake_in, 90u);

  // 100 ms: only the scheduler runs, no component loop, and the wake-up sends the command
  delay(*wake_in);
  App.scheduler.call(millis());
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"page 1", "vis b0,1"}));

  // Nothing waits any more, so no further wake-up is planned
  EXPECT_FALSE(App.scheduler.next_schedule_in(millis()).has_value());
}

// A command the pacer holds back right when it is queued, such as one from Home Assistant, plans the wake-up
// itself and goes out without waiting for a loop pass
TEST_F(CommandPacer, CommandHeldBackWhenQueuedIsSentWithoutLoopPass) {
  this->nextion_.set_command_spacing(10);

  start_loop_pass();
  this->nextion_.send_command("page 1");
  this->nextion_.send_command("vis b0,1");
  EXPECT_EQ(this->uart_.commands, std::vector<std::string>{"page 1"});

  run_wake_up();
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"page 1", "vis b0,1"}));
  EXPECT_FALSE(App.scheduler.next_schedule_in(millis()).has_value());
}

// A wake-up sends one command and plans the next one while more wait
TEST_F(CommandPacer, EachWakeUpSendsOneCommandAndPlansTheNext) {
  this->nextion_.set_command_spacing(10);

  start_loop_pass();
  this->nextion_.send_command("page 1");
  this->nextion_.send_command("vis b0,1");
  this->nextion_.send_command("vis b1,1");
  EXPECT_EQ(this->uart_.commands, std::vector<std::string>{"page 1"});

  run_wake_up();
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"page 1", "vis b0,1"}));

  run_wake_up();
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"page 1", "vis b0,1", "vis b1,1"}));
  EXPECT_FALSE(App.scheduler.next_schedule_in(millis()).has_value());
}

// The addt command that starts a waveform transfer waits for the pacer too, and the wake-up sends it
TEST_F(CommandPacer, HeldBackWaveformIsSentByWakeUp) {
  this->nextion_.set_command_spacing(10);
  this->sensor_.set_component_id(1);
  this->sensor_.set_wave_channel_id(0);

  start_loop_pass();
  this->nextion_.send_command("page 1");
  this->nextion_.add_addt_command_to_queue(&this->sensor_);
  EXPECT_EQ(this->uart_.commands, std::vector<std::string>{"page 1"});

  // Component 1, channel 0, no values yet
  run_wake_up();
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"page 1", "addt 1,0,0"}));
}

// The display takes the bytes after its 0xFE as waveform values. A command queued after the addt waits until the
// values are out, so it cannot end up in the waveform.
TEST_F(CommandPacer, CommandWaitsUntilWaveformValuesAreSent) {
  this->nextion_.set_command_spacing(10);
  this->waveform_.set_component_id(1);
  this->waveform_.set_wave_channel_id(0);
  this->waveform_.add_value(20);
  this->waveform_.add_value(40);

  // The wake-up sends the held-back addt for component 1, channel 0, 2 values
  start_loop_pass();
  this->nextion_.send_command("page 1");
  this->nextion_.add_addt_command_to_queue(&this->waveform_);
  run_wake_up();
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"page 1", "addt 1,0,2"}));

  // 20 ms later the spacing has passed, but the display has not sent 0xFE yet. A loop pass holds the command back
  // and plans no wake-up for it.
  this->nextion_.send_command("vis b0,1");
  delay(20);
  start_loop_pass();
  send_pending(this->nextion_);
  send_pending_waveform(this->nextion_);
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"page 1", "addt 1,0,2"}));
  EXPECT_FALSE(wake_up_planned());

  // The display sends 0xFE: the values 20 and 40 go out, then the command. Only a command ends with 0xFF, so the
  // recorder sees the values and the command as one entry.
  this->uart_.send_ready_for_data();
  read_answers(this->nextion_);
  handle_answers(this->nextion_);
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"page 1", "addt 1,0,2", std::string{20, 40} + "vis b0,1"}));
}

// Sending the addt again while the display may still send 0xFE would put a second addt where it expects the values.
// The addt goes out once; after 1 s without 0xFE the waveform is dropped and the waiting command goes out.
TEST_F(CommandPacer, AddtIsSentOnceAndDroppedAfter1sWithoutAnswer) {
  this->nextion_.set_command_spacing(10);
  this->waveform_.set_component_id(1);
  this->waveform_.set_wave_channel_id(0);
  this->waveform_.add_value(20);

  // At 1000 ms the addt goes out and a command is queued behind it
  start_loop_pass_at(1000);
  this->nextion_.add_addt_command_to_queue(&this->waveform_);
  this->nextion_.send_command("vis b0,1");
  EXPECT_EQ(this->uart_.commands, std::vector<std::string>{"addt 1,0,1"});

  // Loop passes every 10 ms until 1990 ms send neither the addt again nor the command
  for (uint32_t now = 1010; now < 2000; now += 10) {
    start_loop_pass_at(now);
    send_pending(this->nextion_);
    send_pending_waveform(this->nextion_);
  }
  EXPECT_EQ(this->uart_.commands, std::vector<std::string>{"addt 1,0,1"});

  // At 2000 ms the display has not sent 0xFE for 1 s, so the waveform is dropped and the command goes out
  start_loop_pass_at(2000);
  send_pending(this->nextion_);
  send_pending_waveform(this->nextion_);
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"addt 1,0,1", "vis b0,1"}));
}

// The addt can wait in the display's buffer behind commands it is still answering. Like a lost answer, the waveform
// is dropped only once the display has answered nothing for 1 s.
TEST_F(CommandPacer, AddtWaitsWhileDisplayAnswersEarlierCommands) {
  this->nextion_.set_command_spacing(10);
  this->waveform_.set_component_id(1);
  this->waveform_.set_wave_channel_id(0);
  this->waveform_.add_value(20);

  // At 1000 ms "page 1" goes out, at 1010 ms the addt, and a command is queued behind it
  start_loop_pass_at(1000);
  this->nextion_.send_command("page 1");
  start_loop_pass_at(1010);
  this->nextion_.add_addt_command_to_queue(&this->waveform_);
  this->nextion_.send_command("vis b0,1");
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"page 1", "addt 1,0,1"}));

  // At 1800 ms the display answers "page 1"
  start_loop_pass_at(1800);
  answer_oldest(this->nextion_);

  // At 2010 ms the addt went out 1 s ago, but the display answered 0.21 s ago, so the command still waits
  start_loop_pass_at(2010);
  send_pending(this->nextion_);
  send_pending_waveform(this->nextion_);
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"page 1", "addt 1,0,1"}));

  // At 2800 ms the display has answered nothing for 1 s, so the waveform is dropped and the command goes out
  start_loop_pass_at(2800);
  send_pending(this->nextion_);
  send_pending_waveform(this->nextion_);
  EXPECT_EQ(this->uart_.commands, (std::vector<std::string>{"page 1", "addt 1,0,1", "vis b0,1"}));
}

// While the display builds a page it reads nothing, so every command sent then waits in its serial buffer of
// about 1 KB. The wake-ups stop at 16 unanswered commands, and each answer lets one more out.
TEST_F(CommandPacer, BusyDisplayGetsAtMost16UnansweredCommands) {
  this->nextion_.set_command_spacing(1);

  // One loop pass queues 20 commands: the first goes out, 19 wait
  start_loop_pass();
  for (int i = 0; i < 20; i++)
    this->nextion_.send_command("vis b0,1");

  // The wake-ups send 1 ms apart until 16 are unanswered, then plan no further wake-up
  for (int i = 0; i < 20 && wake_up_planned(); i++)
    run_wake_up();
  EXPECT_EQ(this->uart_.commands.size(), 16u);
  EXPECT_FALSE(wake_up_planned());

  // The display answers the oldest command, and the next loop pass sends one more
  answer_oldest(this->nextion_);
  delay(1);
  start_loop_pass();
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands.size(), 17u);
}

// A new command or query also waits while 16 commands are unanswered, even when the pacer would let it out
TEST_F(CommandPacer, NewCommandWaitsWhile16AreUnanswered) {
  this->nextion_.set_command_spacing(1);

  // 16 commands, each in its own loop pass 1 ms after the last, so each goes out right away
  for (int i = 0; i < 16; i++) {
    delay(1);
    start_loop_pass();
    this->nextion_.send_command("vis b0,1");
  }
  EXPECT_EQ(this->uart_.commands.size(), 16u);

  // The display has answered none of them, so the next command and the query wait
  delay(1);
  start_loop_pass();
  this->nextion_.send_command("vis b1,1");
  this->nextion_.add_to_get_queue(&this->sensor_);
  EXPECT_EQ(this->uart_.commands.size(), 16u);

  // The display answers the oldest command, and the next loop pass sends the command that waited longest
  answer_oldest(this->nextion_);
  delay(1);
  start_loop_pass();
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands.size(), 17u);
  EXPECT_EQ(this->uart_.commands.back(), "vis b1,1");
}

// An answer can be lost, for example when the display reports a full buffer (0x24) or restarts on its own.
// Sending then waits until the oldest unanswered command was queued 1 s ago and the display has answered nothing
// for 1 s. After that the command is taken as lost and leaves the queue.
TEST_F(CommandPacer, LostAnswersHoldSendingAtMost1s) {
  this->nextion_.set_command_spacing(1);

  // From 1000 ms on, 16 commands go out 1 ms apart, and two more wait
  for (uint32_t i = 0; i < 16; i++) {
    start_loop_pass_at(1000 + i);
    this->nextion_.send_command("vis b0,1");
  }
  start_loop_pass_at(1016);
  this->nextion_.send_command("vis b1,1");
  this->nextion_.send_command("vis b2,1");
  EXPECT_EQ(this->uart_.commands.size(), 16u);

  // At 1500 ms the display answers the first command and then nothing more, so "vis b1,1" goes out
  start_loop_pass_at(1500);
  answer_oldest(this->nextion_);
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands.size(), 17u);

  // At 2001 ms the second command was queued 1 s ago, but the display answered only 0.5 s ago, so "vis b2,1" waits
  start_loop_pass_at(2001);
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands.size(), 17u);

  // At 2500 ms the display has answered nothing for 1 s, so the second command is taken as lost and "vis b2,1"
  // goes out in its place
  start_loop_pass_at(2500);
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands.size(), 18u);
  EXPECT_EQ(this->uart_.commands.back(), "vis b2,1");
  EXPECT_EQ(this->nextion_.queue_size(), 16u);
}

// A value the display sends after a query is an answer too, so it keeps the next command waiting the same way
TEST_F(CommandPacer, ValueAnswerCountsAsAnswer) {
  this->nextion_.set_command_spacing(1);

  // From 1000 ms on, 16 queries go out 1 ms apart, and two commands wait
  for (uint32_t i = 0; i < 16; i++) {
    start_loop_pass_at(1000 + i);
    this->nextion_.add_to_get_queue(&this->sensor_);
  }
  start_loop_pass_at(1016);
  this->nextion_.send_command("vis b1,1");
  this->nextion_.send_command("vis b2,1");
  EXPECT_EQ(this->uart_.commands.size(), 16u);

  // At 1500 ms the display answers the first query with the value 5 and then nothing more, so "vis b1,1" goes out
  start_loop_pass_at(1500);
  answer_with_value(this->nextion_, 5);
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands.size(), 17u);

  // At 2001 ms the second query was queued 1 s ago, but a value answered 0.5 s ago, so "vis b2,1" waits
  start_loop_pass_at(2001);
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands.size(), 17u);
}

// An ESP that was busy for more than 1 s has not yet read what the display sent meanwhile. No command is taken
// as lost while an answer still waits to be read or handled, because that answer may be its own.
TEST_F(CommandPacer, UnreadAnswerKeepsItsCommand) {
  this->nextion_.set_command_spacing(1);

  // From 1000 ms on, 16 commands go out 1 ms apart, and one more waits
  for (uint32_t i = 0; i < 16; i++) {
    start_loop_pass_at(1000 + i);
    this->nextion_.send_command("vis b0,1");
  }
  start_loop_pass_at(1016);
  this->nextion_.send_command("vis b1,1");

  // The display answered the first command, but at 2500 ms the answer is still in the UART
  this->uart_.send_success_answer();
  start_loop_pass_at(2500);
  send_pending(this->nextion_);
  EXPECT_EQ(this->nextion_.queue_size(), 17u);

  // loop() has read the answer but not handled it yet, so still nothing is taken as lost
  read_answers(this->nextion_);
  send_pending(this->nextion_);
  EXPECT_EQ(this->nextion_.queue_size(), 17u);

  // Handling the answer removes the first command, and "vis b1,1" goes out
  handle_answers(this->nextion_);
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands.size(), 17u);
  EXPECT_EQ(this->nextion_.queue_size(), 16u);
}

// A display without command_spacing, or with 0ms, is not held to 16 unanswered commands, and no command is taken as
// lost. The test build defines USE_NEXTION_COMMAND_SPACING, as a firmware does when another display uses it.
TEST_F(CommandPacer, NoSpacingMeansNoLimitOfUnansweredCommands) {
  // This display has no command_spacing

  // From 1000 ms on, 20 commands go out 1 ms apart and the display answers none of them
  for (uint32_t i = 0; i < 20; i++) {
    start_loop_pass_at(1000 + i);
    this->nextion_.send_command("vis b0,1");
  }
  EXPECT_EQ(this->uart_.commands.size(), 20u);

  // At 3000 ms, 2 s later, a new command and a query go out at once
  start_loop_pass_at(3000);
  this->nextion_.send_command("vis b1,1");
  this->nextion_.add_to_get_queue(&this->sensor_);
  EXPECT_EQ(this->uart_.commands.size(), 22u);

  // A pass over the queue takes none of the 22 as lost and plans no wake-up
  send_pending(this->nextion_);
  EXPECT_EQ(this->nextion_.queue_size(), 22u);
  EXPECT_FALSE(wake_up_planned());
}

// Of two displays in one firmware, only the one with a command_spacing above 0 is held to 16 unanswered commands
TEST_F(CommandPacer, DisplayWithoutSpacingIsNotLimitedByAnotherDisplaysSpacing) {
  RecordingUART other_uart;
  Nextion other;
  other.set_uart_parent(&other_uart);
  mark_ready(other);
  other.set_command_spacing(1);  // The display of the fixture has none

  // From 1000 ms on, both displays get 20 commands 1 ms apart and answer none of them
  for (uint32_t i = 0; i < 20; i++) {
    start_loop_pass_at(1000 + i);
    this->nextion_.send_command("vis b0,1");
    other.send_command("vis b0,1");
  }

  // The display without spacing sends all 20, the one with 1 ms stops at 16
  EXPECT_EQ(this->uart_.commands.size(), 20u);
  EXPECT_EQ(other_uart.commands.size(), 16u);
  clear_queue(other);
}

// set_command_spacing() can switch a running display to 0ms. The commands that wait then all go out.
TEST_F(CommandPacer, SwitchingToNoSpacingLiftsTheLimitForWaitingCommands) {
  this->nextion_.set_command_spacing(1);

  // At 1000 ms one command goes out and 19 wait for the pacer
  start_loop_pass_at(1000);
  for (int i = 0; i < 20; i++)
    this->nextion_.send_command("vis b0,1");
  EXPECT_EQ(this->uart_.commands.size(), 1u);

  // At 1001 ms the display has no spacing, so all 19 go out and 20 commands are unanswered
  this->nextion_.set_command_spacing(0);
  start_loop_pass_at(1001);
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands.size(), 20u);
}

// A slow display answers a long burst one command at a time. While its answers keep coming, no command is taken
// as lost, although the oldest ones were queued more than 1 s ago, so each answer removes its own command.
TEST_F(CommandPacer, SlowAnswersDuringLongBurstRemoveNothing) {
  this->nextion_.set_command_spacing(1);

  // At 1000 ms a page queues 30 commands. Loop passes follow 1 ms apart, so 16 of them go out by 1015 ms.
  start_loop_pass_at(1000);
  for (int i = 0; i < 30; i++)
    this->nextion_.send_command("vis b0,1");

  // The display answers one command every 100 ms, and each answer lets one more out. At 2000 ms 4 commands still
  // wait behind 16 unanswered ones that were queued 1 s ago. The last answer comes at 4000 ms.
  size_t answered = 0;
  for (uint32_t now = 1001; now <= 4000; now++) {
    start_loop_pass_at(now);
    if (now % 100 == 0) {
      answer_oldest(this->nextion_);
      answered++;
    }
    send_pending(this->nextion_);
    // A command leaves the queue only through its answer
    ASSERT_EQ(this->nextion_.queue_size(), 30u - answered) << "at " << now << " ms";
  }
  EXPECT_EQ(this->uart_.commands.size(), 30u);
}

// A page that takes longer than 1 s to go out is answered as usual. A new command during it waits behind the
// page's commands and removes none of them, although the oldest were queued more than 1 s ago.
TEST_F(CommandPacer, NewCommandDuringLongPageRemovesNothing) {
  this->nextion_.set_command_spacing(50);

  // At 1000 ms a page queues 40 commands. One goes out every 50 ms, so the last goes out at 2950 ms.
  start_loop_pass_at(1000);
  for (int i = 0; i < 40; i++)
    this->nextion_.send_command("vis b0,1");

  // Loop passes follow 50 ms apart. At the start of each the display answers the command sent in the pass before.
  // At 2100 ms, 1.1 s after the page was queued, 23 of its commands went out and 17 wait, and a new command arrives.
  size_t answered = 0;
  for (uint32_t now = 1050; now <= 3050; now += 50) {
    start_loop_pass_at(now);
    for (; answered < this->uart_.commands.size(); answered++)
      answer_oldest(this->nextion_);
    send_pending(this->nextion_);
    if (now == 2100)
      this->nextion_.send_command("vis b1,1");
  }

  // All 40 commands of the page went out and the new one after them, and the last answer emptied the queue
  EXPECT_EQ(this->uart_.commands.size(), 41u);
  EXPECT_EQ(this->uart_.commands.back(), "vis b1,1");
  EXPECT_EQ(this->nextion_.queue_size(), 0u);
}

// An answer lost earlier must not switch the limit off for the next page: its entry is taken as lost and removed
TEST_F(CommandPacer, LostAnswerFromEarlierKeepsTheLimit) {
  this->nextion_.set_command_spacing(1);

  // At 1000 ms one command goes out, and its answer is lost
  start_loop_pass_at(1000);
  this->nextion_.send_command("dim=50");

  // At 3000 ms a page queues 30 commands, and the display answers none of them while it builds the page.
  // Loop passes follow 1 ms apart for 300 ms.
  start_loop_pass_at(3000);
  for (int i = 0; i < 30; i++)
    this->nextion_.send_command("vis b0,1");
  for (uint32_t now = 3001; now <= 3300; now++) {
    start_loop_pass_at(now);
    send_pending(this->nextion_);
  }

  // Only 16 of the page's commands went out. The queue holds them and the 14 that wait; the old entry is gone.
  EXPECT_EQ(this->uart_.commands.size(), 1u + 16u);
  EXPECT_EQ(this->nextion_.queue_size(), 30u);
}

// After a full buffer (0x24) the answers to 16 commands are lost. Once the display answers again, a page goes out
// as fast as without the loss instead of waiting 1 s for the lost answers.
TEST_F(CommandPacer, PageAfterLostAnswersIsNotSlowedDown) {
  this->nextion_.set_command_spacing(1);

  // From 1000 ms on, 16 commands go out 1 ms apart, and all their answers are lost
  for (uint32_t i = 0; i < 16; i++) {
    start_loop_pass_at(1000 + i);
    this->nextion_.send_command("vis b0,1");
  }

  // At 3000 ms a page queues 48 commands. Loop passes follow 1 ms apart, and at the start of each the display
  // answers every command sent before it.
  start_loop_pass_at(3000);
  for (int i = 0; i < 48; i++)
    this->nextion_.send_command("vis b1,1");
  size_t answered = 16;  // the 16 lost ones never get an answer
  uint32_t now = 3000;
  while (this->uart_.commands.size() < 16 + 48 && now < 5000) {
    now++;
    start_loop_pass_at(now);
    for (; answered < this->uart_.commands.size(); answered++)
      answer_oldest(this->nextion_);
    send_pending(this->nextion_);
  }

  // One command per loop pass: the last of the 48 goes out by 3048 ms, not 1 s later
  EXPECT_LE(now, 3048u);
}

// A display that stays silent takes at most one command per second as lost, so a backlog queued before the first
// loss does not slide through the limit of 16 while the display answers nothing
TEST_F(CommandPacer, SilentDisplayLosesAtMostOneCommandPerSecond) {
  this->nextion_.set_command_spacing(1);

  // From 1000 ms on, 16 commands go out 1 ms apart. At 1016 ms 20 more are queued and wait. The display answers none.
  for (uint32_t i = 0; i < 16; i++) {
    start_loop_pass_at(1000 + i);
    this->nextion_.send_command("vis b0,1");
  }
  start_loop_pass_at(1016);
  for (int i = 0; i < 20; i++)
    this->nextion_.send_command("vis b1,1");
  EXPECT_EQ(this->uart_.commands.size(), 16u);

  // From 2016 ms on, loop passes follow 1 ms apart. The first takes the oldest command as lost and sends one more.
  // The next loss is due 1 s later, at 3016 ms.
  for (uint32_t now = 2016; now < 3016; now++) {
    start_loop_pass_at(now);
    send_pending(this->nextion_);
  }
  EXPECT_EQ(this->uart_.commands.size(), 17u);

  start_loop_pass_at(3016);
  send_pending(this->nextion_);
  EXPECT_EQ(this->uart_.commands.size(), 18u);
}

// Queries that waited behind an addt were queued long ago, and the display had nothing to answer meanwhile. When they
// go out, it still gets its full second. A query taken as lost earlier would put each later value on the next sensor.
TEST_F(CommandPacer, CommandsBehindAddtGetTheirFullSecond) {
  this->hold_20_queries_behind_addt_();

  // 10 ms before the second runs out the display sends 0xFE, so the values go out and the queries may follow
  start_loop_pass_at(1990);
  this->uart_.send_ready_for_data();
  read_answers(this->nextion_);
  handle_answers(this->nextion_);

  // Loop passes follow 1 ms apart. 16 queries go out by 2005 ms, then sending waits for the first answer.
  for (uint32_t now = 1990; now < 2100; now++) {
    start_loop_pass_at(now);
    send_pending(this->nextion_);
  }

  // The first query went out 0.11 s ago, so all 20 are still queued
  EXPECT_EQ(this->uart_.commands.size(), 1u + 16u);
  EXPECT_EQ(this->nextion_.queue_size(), 20u);
}

// The same when the display never sends 0xFE and the waveform is dropped at 2000 ms
TEST_F(CommandPacer, CommandsBehindDroppedAddtGetTheirFullSecond) {
  this->hold_20_queries_behind_addt_();

  // Loop passes follow 1 ms apart. From 2000 ms on, 16 queries go out, then sending waits for the first answer.
  for (uint32_t now = 1001; now < 2100; now++) {
    start_loop_pass_at(now);
    send_pending(this->nextion_);
    send_pending_waveform(this->nextion_);
  }

  // The first query went out 0.1 s ago, so all 20 are still queued
  EXPECT_EQ(this->uart_.commands.size(), 1u + 16u);
  EXPECT_EQ(this->nextion_.queue_size(), 20u);
}

// A display that restarts alone falls back to its default bkcmd and no longer answers commands that succeed. Its 0x88
// makes loop() send bkcmd=3 again, so the limit of 16 does not hold it at one command per second. The start page goes
// out only once, because a page that sends 0x88 when it opens would otherwise bring the display back here without end.
TEST_F(CommandPacer, DisplayRestartGetsBkcmdAgain) {
  this->nextion_.set_start_up_page(2);
  this->connect_display_();
  EXPECT_EQ(this->uart_.times_sent("bkcmd=3"), 1);
  EXPECT_EQ(this->uart_.times_sent("page 2"), 1);

  // The display restarts while the ESP keeps running and sends the same 0x88
  this->uart_.send_display_started();
  this->nextion_.loop();
  this->nextion_.loop();
  EXPECT_EQ(this->uart_.times_sent("bkcmd=3"), 2);
  EXPECT_EQ(this->uart_.times_sent("page 2"), 1);
}

// The display takes the bytes after its 0xFE as waveform values, so bkcmd=3 waits behind an addt like any other
// command. Sent earlier, it would end up in the waveform and the display would stay without it.
TEST_F(CommandPacer, BkcmdAfterRestartWaitsUntilWaveformValuesAreSent) {
  this->connect_display_();
  this->waveform_.set_component_id(1);
  this->waveform_.set_wave_channel_id(0);
  this->waveform_.add_value(20);
  this->nextion_.add_addt_command_to_queue(&this->waveform_);

  // The display restarts with the addt in its buffer: 0x88 comes first, and bkcmd=3 holds back
  this->uart_.send_display_started();
  this->nextion_.loop();
  this->nextion_.loop();
  EXPECT_EQ(this->uart_.times_sent("bkcmd=3"), 1);

  // The display sends 0xFE: one pass sends the value 20, the next sends bkcmd=3. Only a command ends with 0xFF, so the
  // recorder sees the value and the command as one entry.
  this->uart_.send_ready_for_data();
  this->nextion_.loop();
  this->nextion_.loop();
  EXPECT_EQ(this->uart_.times_sent(std::string{20} + "bkcmd=3"), 1);
}

}  // namespace esphome::nextion::testing
