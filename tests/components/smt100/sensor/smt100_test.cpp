#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/smt100/smt100.h"
#include "esphome/core/application.h"
#include "../common.h"

namespace esphome::smt100::testing {

namespace {

constexpr const char *const REQUEST = "GetAllMeasurements!\r";
// Captured from an SMT100 LV with firmware V1.52, which answers about 400 ms after the request
constexpr const char *const ANSWER = "12202,17.02,30.59,22.945,5.10\r\n";
constexpr uint32_t ANSWER_TIME_MS = 400;
constexpr uint32_t TIMEOUT_MS = 1000;

// Sets the loop clock the component reads through App.get_loop_component_start_time().
void set_loop_time(uint32_t now) { LoopBlockingGuard clock(nullptr, LOG_STR("test"), now); }

class SMT100Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sut_.set_uart_parent(&this->uart_);
    this->sut_.set_counts_sensor(&this->counts_);
    this->sut_.set_permittivity_sensor(&this->permittivity_);
    this->sut_.set_moisture_sensor(&this->moisture_);
    this->sut_.set_temperature_sensor(&this->temperature_);
    this->sut_.set_voltage_sensor(&this->voltage_);
    this->moisture_.add_on_state_callback([this](float state) { this->readings_++; });
    this->sut_.setup();
  }

  // Advances the clock and runs loop() the way the core does: only while the component has its loop enabled.
  void loop_after_(uint32_t ms) {
    this->now_ += ms;
    set_loop_time(this->now_);
    if (this->sut_.is_in_loop_state())
      this->sut_.loop();
  }

  // Runs update() and the loop pass that sends the request.
  void request_() {
    this->sut_.update();
    this->loop_after_(16);
  }

  // Sends a request and lets the sensor answer it with the given text.
  void read_(const std::string &answer) {
    this->request_();
    this->uart_.receive(answer);
    this->loop_after_(ANSWER_TIME_MS);
  }

  bool all_unknown_() const {
    return std::isnan(this->counts_.state) && std::isnan(this->permittivity_.state) &&
           std::isnan(this->moisture_.state) && std::isnan(this->temperature_.state) &&
           std::isnan(this->voltage_.state);
  }

  FakeUART uart_;
  SMT100Component sut_;
  sensor::Sensor counts_;
  sensor::Sensor permittivity_;
  sensor::Sensor moisture_;
  sensor::Sensor temperature_;
  sensor::Sensor voltage_;
  uint32_t now_{1000};
  int readings_{0};
};

}  // namespace

TEST(SMT100Parse, ReadsAnAnswerOfTheSensor) {
  float values[FIELD_COUNT];
  ASSERT_TRUE(parse_measurements("12202,17.02,30.59,22.945,5.10", values));
  EXPECT_FLOAT_EQ(values[FIELD_COUNTS], 12202.0f);
  EXPECT_FLOAT_EQ(values[FIELD_PERMITTIVITY], 17.02f);
  EXPECT_FLOAT_EQ(values[FIELD_MOISTURE], 30.59f);
  EXPECT_FLOAT_EQ(values[FIELD_TEMPERATURE], 22.945f);
  EXPECT_FLOAT_EQ(values[FIELD_VOLTAGE], 5.10f);
}

TEST(SMT100Parse, ReadsTheLongestAnswer) {
  float values[FIELD_COUNT];
  ASSERT_TRUE(parse_measurements("65535,100.00,100.00,-20.000,15.00", values));
  EXPECT_FLOAT_EQ(values[FIELD_COUNTS], 65535.0f);
  EXPECT_FLOAT_EQ(values[FIELD_TEMPERATURE], -20.0f);
  EXPECT_FLOAT_EQ(values[FIELD_VOLTAGE], 15.0f);
}

TEST(SMT100Parse, RejectsWhatIsNotAnAnswer) {
  static constexpr const char *const LINES[] = {
      "",
      ",",
      ",,,,",
      // Fewer values than expected: the tail of an answer cut off by a reset or by noise
      "22.945,5.10",
      "12202,17.02,30.59,22.945",
      "12202,17.02,30.59,22.945,",
      // More values than expected: two answers that lost the line end between them
      "12202,17.02,30.59,22.945,5.10,12202",
      "12202,17.02,30.59,22.945,5.1012202,17.02,30.59,22.945,5.10",
      // Not numbers
      "counts,permittivity,moisture,temperature,voltage",
      "12202,17.02,30.59,22.945,5.10V",
      "12202,17.02,,22.945,5.10",
      "12202,17.02,30.59,nan,5.10",
      "12202,17.02,30.59,inf,5.10",
      "12202,17.02,30.59,22.945,1e99",
      // A value longer than any the sensor sends
      "12202,17.02,30.59,22.94500000000,5.10",
      // The answer to another command
      "V1.52 LV",
  };
  for (const char *line : LINES) {
    float values[FIELD_COUNT];
    EXPECT_FALSE(parse_measurements(line, values)) << "'" << line << "'";
  }
}

TEST_F(SMT100Test, SendsNothingUntilAnUpdate) {
  this->loop_after_(16);
  EXPECT_EQ(this->uart_.tx, "");
}

TEST_F(SMT100Test, LoopIsOffWhileNothingIsRequested) {
  EXPECT_FALSE(this->sut_.is_in_loop_state());
  this->read_(ANSWER);
  EXPECT_FALSE(this->sut_.is_in_loop_state());
}

TEST(SMT100Setup, SendsARequestMadeBeforeSetup) {
  // A lambda can call update() before setup() has run; the request must not be left without a loop to send it
  FakeUART uart;
  SMT100Component sut;
  sut.set_uart_parent(&uart);
  sut.update();
  sut.setup();

  ASSERT_TRUE(sut.is_in_loop_state());
  set_loop_time(1000);
  sut.loop();
  EXPECT_EQ(uart.tx, REQUEST);
}

TEST_F(SMT100Test, SendsARequestMadeFromAStateCallback) {
  // on_value automations run while the answer is published
  bool again = true;
  this->voltage_.add_on_state_callback([this, &again](float state) {
    if (again) {
      again = false;
      this->sut_.update();
    }
  });

  this->read_(ANSWER);
  this->loop_after_(16);

  EXPECT_EQ(this->uart_.tx, std::string(REQUEST) + REQUEST);
}

TEST_F(SMT100Test, PublishesTheAnswer) {
  this->read_(ANSWER);

  EXPECT_EQ(this->uart_.tx, REQUEST);
  EXPECT_FLOAT_EQ(this->counts_.state, 12202.0f);
  EXPECT_FLOAT_EQ(this->permittivity_.state, 17.02f);
  EXPECT_FLOAT_EQ(this->moisture_.state, 30.59f);
  EXPECT_FLOAT_EQ(this->temperature_.state, 22.945f);
  EXPECT_FLOAT_EQ(this->voltage_.state, 5.10f);
  EXPECT_FALSE(this->sut_.status_has_warning());
}

TEST_F(SMT100Test, CollectsAnAnswerThatArrivesInPieces) {
  this->request_();
  // NUL bytes, as read while the sensor powers up, and an empty line are not part of the answer
  this->uart_.receive(std::string("\0\0", 2) + "\r\n12202,17.");
  this->loop_after_(ANSWER_TIME_MS);
  EXPECT_EQ(this->readings_, 0);
  this->uart_.receive("02,30.59,22.945,5.10\r");
  this->loop_after_(16);
  EXPECT_EQ(this->readings_, 0);
  this->uart_.receive("\n");
  this->loop_after_(16);

  EXPECT_EQ(this->readings_, 1);
  EXPECT_FLOAT_EQ(this->permittivity_.state, 17.02f);
  EXPECT_FLOAT_EQ(this->moisture_.state, 30.59f);
}

TEST_F(SMT100Test, WaitsForTheAnswerBeforeTheNextRequest) {
  this->request_();
  this->sut_.update();
  this->loop_after_(16);
  EXPECT_EQ(this->uart_.tx, REQUEST);

  this->uart_.receive(ANSWER);
  this->loop_after_(ANSWER_TIME_MS);
  this->request_();
  EXPECT_EQ(this->uart_.tx, std::string(REQUEST) + REQUEST);
}

TEST_F(SMT100Test, MissingAnswerMakesTheEntitiesUnknown) {
  this->read_(ANSWER);

  this->request_();
  this->loop_after_(TIMEOUT_MS - 1);
  EXPECT_FLOAT_EQ(this->moisture_.state, 30.59f);
  EXPECT_FALSE(this->sut_.status_has_warning());
  this->loop_after_(1);

  EXPECT_TRUE(this->all_unknown_());
  EXPECT_TRUE(this->sut_.status_has_warning());
}

TEST_F(SMT100Test, RecoversWhenTheSensorAnswersAgain) {
  this->request_();
  this->loop_after_(TIMEOUT_MS);
  ASSERT_TRUE(this->sut_.status_has_warning());

  this->read_(ANSWER);

  EXPECT_FLOAT_EQ(this->moisture_.state, 30.59f);
  EXPECT_FALSE(this->sut_.status_has_warning());
}

TEST_F(SMT100Test, MalformedAnswerMakesTheEntitiesUnknown) {
  this->read_(ANSWER);

  // An answer cut short crashed releases before 2026.3
  this->read_("22.945,5.10\r\n");

  EXPECT_TRUE(this->all_unknown_());
  EXPECT_TRUE(this->sut_.status_has_warning());
}

TEST_F(SMT100Test, OverlongLineIsNotAnAnswer) {
  this->request_();
  this->uart_.receive(std::string(200, '7') + "\r\n");
  this->loop_after_(ANSWER_TIME_MS);
  EXPECT_EQ(this->readings_, 0);

  this->uart_.receive(ANSWER);
  this->loop_after_(16);
  EXPECT_FLOAT_EQ(this->moisture_.state, 30.59f);
}

TEST_F(SMT100Test, DropsWhatArrivedBeforeTheRequest) {
  // A late answer to a request that already timed out, and noise without a line end
  this->request_();
  this->loop_after_(TIMEOUT_MS);
  this->uart_.receive("11111,11.11,11.11,11.111,1.11\r\n99999,9");

  this->read_(ANSWER);

  EXPECT_FLOAT_EQ(this->counts_.state, 12202.0f);
  EXPECT_FLOAT_EQ(this->moisture_.state, 30.59f);
}

}  // namespace esphome::smt100::testing
