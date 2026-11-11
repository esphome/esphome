#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "esphome/components/tuya/tuya_uart.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "common.h"

namespace esphome::tuya::testing {

namespace {

constexpr uint8_t HEARTBEAT = 0x00;
constexpr uint8_t PRODUCT_QUERY = 0x01;
constexpr uint8_t CONF_QUERY = 0x02;
constexpr uint8_t DATAPOINT_REPORT = 0x07;
constexpr uint8_t DATAPOINT_QUERY = 0x08;
constexpr uint8_t WEATHER_ENABLE = 0x20;
constexpr uint8_t WEATHER_DATA = 0x21;
constexpr uint8_t EXTENDED_SERVICES = 0x34;
constexpr uint8_t REQUEST_WEATHER = 0x03;
// just past the 5 s in which a request counts as answered by the last frame
constexpr uint32_t PAST_DEBOUNCE_MS = 5100;
// just past the 300 ms the component waits for a reply
constexpr int PAST_RECEIVE_TIMEOUT_MS = 400;
constexpr int MAX_RETRIES = 10;
constexpr uint32_t UPDATE_INTERVAL_MS = 2000;
constexpr uint32_t PAST_HALF_UPDATE_INTERVAL_MS = 1100;

using Bytes = std::vector<uint8_t>;

TuyaWeatherValue int_value(const char *key, int32_t value) { return TuyaWeatherValue{key, false, value, {}}; }
TuyaWeatherValue string_value(const char *key, std::string value) {
  return TuyaWeatherValue{key, true, 0, std::move(value)};
}

// One entry of a 0x21 payload: key length, key, type, value length, value
void put_entry(Bytes &payload, const char *key, uint8_t type, const Bytes &value) {
  payload.push_back(static_cast<uint8_t>(strlen(key)));
  payload.insert(payload.end(), key, key + strlen(key));
  payload.push_back(type);
  payload.push_back(static_cast<uint8_t>(value.size()));
  payload.insert(payload.end(), value.begin(), value.end());
}

}  // namespace

// Friend of TuyaUART: the timer callbacks are called directly, the host scheduler runs on the real clock.
// The weather rules read the loop clock, which the fixture sets; the reply timeout runs on the real clock.
class TuyaWeatherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->now_ = millis() | 1;
    this->sut_.set_uart_parent(&this->uart_);
    this->mcu_(HEARTBEAT, {0x01});
    this->sent_(PRODUCT_QUERY);
    this->finish_init_();
  }

  // The timers capture &sut_; drop them before the fixture dies
  void TearDown() override {
    App.scheduler.cancel_timeout(&this->sut_, DATAPOINT_DUMP_TIMEOUT_ID);
    App.scheduler.cancel_interval(&this->sut_, WEATHER_INTERVAL_ID);
    App.scheduler.cancel_interval(&this->sut_, WEATHER_RETRY_INTERVAL_ID);
    App.scheduler.call(millis());
    App.scheduler.trim_freelist();
  }

  // Product, working mode with a status pin, first datapoint report
  void finish_init_() {
    this->mcu_(PRODUCT_QUERY, {'p'});
    this->sent_(CONF_QUERY);
    this->mcu_(CONF_QUERY, {0x0E, 0x00});
    this->sent_(DATAPOINT_QUERY);
    this->mcu_(DATAPOINT_REPORT, {0x01, 0x01, 0x00, 0x01, 0x01});
    ASSERT_EQ(this->sut_.get_init_state(), TuyaInitState::INIT_DONE);
  }

  // 0x34/0x03 from the MCU, returns the result byte
  uint8_t request_() {
    this->mcu_(EXTENDED_SERVICES, {REQUEST_WEATHER});
    Bytes result = this->sent_(EXTENDED_SERVICES);
    if (result.size() != 2 || result[0] != REQUEST_WEATHER) {
      ADD_FAILURE() << "no result of the weather request";
      return 0xFF;
    }
    return result[1];
  }

  // A frame from the MCU
  void mcu_(uint8_t cmd, const Bytes &payload) {
    Bytes frame{0x55, 0xAA, 0x03, cmd, static_cast<uint8_t>(payload.size() >> 8), static_cast<uint8_t>(payload.size())};
    frame.insert(frame.end(), payload.begin(), payload.end());
    uint8_t checksum = 0;
    for (uint8_t b : frame)
      checksum += b;
    frame.push_back(checksum);
    this->uart_.rx.insert(this->uart_.rx.end(), frame.begin(), frame.end());
    this->loop_();
  }

  // One loop of the component at the fixture's loop clock
  void loop_() {
    LoopBlockingGuard{nullptr, nullptr, this->now_};
    this->sut_.loop();
  }

  // Moves the loop clock
  void advance_(uint32_t ms) { this->now_ += ms; }

  // Runs the component until it has written a frame, returns the raw frame
  Bytes sent_raw_() {
    for (int i = 0; i < 500; i++) {
      this->loop_();
      Bytes &tx = this->uart_.tx;
      if (tx.size() >= 7) {
        size_t size = 7 + ((tx[4] << 8) | tx[5]);
        if (tx.size() >= size) {
          Bytes frame(tx.begin(), tx.begin() + size);
          tx.erase(tx.begin(), tx.begin() + size);
          return frame;
        }
      }
      delay(1);
    }
    ADD_FAILURE() << "no frame written";
    return {};
  }

  // Same, checks header, command and checksum and returns the payload
  Bytes sent_(uint8_t cmd) {
    Bytes frame = this->sent_raw_();
    if (frame.empty())
      return {};
    uint8_t checksum = 0;
    for (size_t i = 0; i + 1 < frame.size(); i++)
      checksum += frame[i];
    EXPECT_EQ(frame[0], 0x55);
    EXPECT_EQ(frame[1], 0xAA);
    EXPECT_EQ(frame[2], 0x00);
    EXPECT_EQ(frame[3], cmd);
    EXPECT_EQ(frame.back(), checksum);
    return Bytes(frame.begin() + 6, frame.end() - 1);
  }

  // The MCU enables the service with a key list; nothing is configured, so no 0x21 follows
  void enable_(const std::vector<const char *> &keys) {
    Bytes payload;
    for (const char *key : keys) {
      payload.push_back(static_cast<uint8_t>(strlen(key)));
      payload.insert(payload.end(), key, key + strlen(key));
    }
    this->mcu_(WEATHER_ENABLE, payload);
    EXPECT_EQ(this->sent_(WEATHER_ENABLE), (Bytes{0x01, 0x00}));
  }

  // Payload of the next 0x21 frame; the MCU acknowledges it
  Bytes weather_frame_() {
    Bytes payload = this->sent_(WEATHER_DATA);
    this->mcu_(WEATHER_DATA, {});
    return payload;
  }

  void expect_nothing_sent_() {
    this->pump_(20);
    EXPECT_TRUE(this->uart_.tx.empty());
  }

  // Runs the component for a time on the real clock
  void pump_(int ms) {
    for (int i = 0; i < ms; i++) {
      this->loop_();
      delay(1);
    }
  }

  // One tick of the retry timer
  void retry_() {
    LoopBlockingGuard{nullptr, nullptr, this->now_};
    this->sut_.retry_weather_();
  }

  // One tick of the update_interval timer
  void update_() {
    LoopBlockingGuard{nullptr, nullptr, this->now_};
    this->sut_.update_weather_();
  }

  // True when the retry timer was still set; it is cancelled either way
  bool stop_retry_() { return App.scheduler.cancel_interval(&this->sut_, WEATHER_RETRY_INTERVAL_ID); }

  // The frame that waits for its ack gets none and is dropped
  void lose_frame_() {
    this->pump_(PAST_RECEIVE_TIMEOUT_MS);
    EXPECT_TRUE(this->uart_.tx.empty());
  }

  uint32_t now_{1};
  MockUARTComponent uart_;
  TuyaUART sut_;
};

TEST_F(TuyaWeatherTest, IntegerIsBigEndianInt32) {
  this->enable_({"w.temp"});
  EXPECT_TRUE(this->sut_.send_weather({int_value("w.temp", -2)}));
  const Bytes expected{0x55, 0xAA, 0x00, 0x21, 0x00, 0x0E, 0x01, 0x06, 'w',  '.', 't',
                       'e',  'm',  'p',  0x00, 0x04, 0xFF, 0xFF, 0xFF, 0xFE, 0x8F};
  EXPECT_EQ(this->sent_raw_(), expected);
}

TEST_F(TuyaWeatherTest, PositiveIntegerByteOrder) {
  this->enable_({"w.pressure"});
  EXPECT_TRUE(this->sut_.send_weather({int_value("w.pressure", 0x01020304)}));
  Bytes expected{0x01};
  put_entry(expected, "w.pressure", 0x00, {0x01, 0x02, 0x03, 0x04});
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, KeyNotRequestedIsSkipped) {
  this->enable_({"w.temp"});
  EXPECT_TRUE(this->sut_.send_weather({int_value("w.humidity", 50), int_value("w.temp", 21)}));
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, EmptyKeyListTakesEveryKey) {
  this->enable_({});
  EXPECT_TRUE(this->sut_.send_weather({int_value("w.humidity", 50), string_value("w.windDir", "NE")}));
  Bytes expected{0x01};
  put_entry(expected, "w.humidity", 0x00, {0x00, 0x00, 0x00, 0x32});
  put_entry(expected, "w.windDir", 0x01, {'N', 'E'});
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, ForecastRequestSendsTodayAsDayZero) {
  this->enable_({"w.temp", "w.conditionNum", "w.date.3"});
  EXPECT_TRUE(this->sut_.send_weather({int_value("w.temp", 21), string_value("w.conditionNum", "120")}));
  Bytes expected{0x01};
  put_entry(expected, "w.temp.0", 0x00, {0x00, 0x00, 0x00, 0x15});
  put_entry(expected, "w.conditionNum.0", 0x01, {'1', '2', '0'});
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, DayZeroKeyReplacesPlainKey) {
  this->enable_({"w.temp", "w.date.3"});
  EXPECT_TRUE(this->sut_.send_weather({int_value("w.temp", 5), int_value("w.temp.0", 7)}));
  Bytes expected{0x01};
  put_entry(expected, "w.temp.0", 0x00, {0x00, 0x00, 0x00, 0x07});
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, ForecastDayBound) {
  this->enable_({"w.temp", "w.date.2"});
  EXPECT_TRUE(this->sut_.send_weather({int_value("w.temp.1", 3), int_value("w.temp.2", 4)}));
  Bytes expected{0x01};
  put_entry(expected, "w.temp.1", 0x00, {0x00, 0x00, 0x00, 0x03});
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, ForecastKeyWithoutForecastRequestIsNotSent) {
  this->enable_({"w.temp"});
  EXPECT_FALSE(this->sut_.send_weather({int_value("w.temp.1", 3)}));
  this->expect_nothing_sent_();
}

TEST_F(TuyaWeatherTest, StringLimitIs99Bytes) {
  this->enable_({"w.conditionNum", "w.windDir"});
  EXPECT_TRUE(this->sut_.send_weather(
      {string_value("w.conditionNum", std::string(100, 'a')), string_value("w.windDir", std::string(99, 'b'))}));
  Bytes expected{0x01};
  put_entry(expected, "w.windDir", 0x01, Bytes(99, 'b'));
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, NotSentBeforeTheMcuEnablesTheService) {
  EXPECT_FALSE(this->sut_.send_weather({int_value("w.temp", 21)}));
  this->expect_nothing_sent_();
}

// 0x34/0x03: the result byte goes out first, then the values the request callback collected
TEST_F(TuyaWeatherTest, OnDemandRequestIsAcknowledgedBeforeTheFrame) {
  this->sut_.add_on_weather_request_callback([this](const std::vector<std::string> &keys) {
    EXPECT_TRUE(keys.empty());
    EXPECT_TRUE(this->sut_.send_weather({int_value("w.temp", 21)}));
  });
  this->mcu_(EXTENDED_SERVICES, {REQUEST_WEATHER});
  EXPECT_EQ(this->sent_(EXTENDED_SERVICES), (Bytes{REQUEST_WEATHER, 0x00}));
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, OnDemandRequestWithoutValuesGetsFailureResult) {
  EXPECT_EQ(this->request_(), 0x01);
  this->expect_nothing_sent_();
}

// The MCU sends 0x20 and 0x34/0x03 back to back: one frame answers both
TEST_F(TuyaWeatherTest, RequestRightAfterEnableGetsNoSecondFrame) {
  this->sut_.add_on_weather_request_callback(
      [this](const std::vector<std::string> &) { this->sut_.send_weather({int_value("w.temp", 21)}); });
  this->enable_({"w.temp"});
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  EXPECT_EQ(this->weather_frame_(), expected);
  EXPECT_EQ(this->request_(), 0x00);
  this->expect_nothing_sent_();
}

TEST_F(TuyaWeatherTest, RequestWhileTheFrameAwaitsItsAckGetsNoSecondFrame) {
  this->sut_.add_on_weather_request_callback(
      [this](const std::vector<std::string> &) { this->sut_.send_weather({int_value("w.temp", 21)}); });
  this->enable_({"w.temp"});
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  EXPECT_EQ(this->sent_(WEATHER_DATA), expected);
  EXPECT_EQ(this->request_(), 0x00);
  this->expect_nothing_sent_();
}

TEST_F(TuyaWeatherTest, IncompleteAnswerDoesNotStartTheRequestLimit) {
  this->sut_.add_on_weather_request_callback([this](const std::vector<std::string> &) {
    this->sut_.send_weather({string_value("w.conditionNum", std::string(100, 'a')), int_value("w.temp", 21)});
  });
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_EQ(this->weather_frame_(), expected);
  this->advance_(PAST_DEBOUNCE_MS);
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, SecondRequestIsRefusedUntilTheMcuRestarts) {
  this->sut_.add_on_weather_request_callback(
      [this](const std::vector<std::string> &) { this->sut_.send_weather({int_value("w.temp", 21)}); });
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_EQ(this->weather_frame_(), expected);
  this->advance_(PAST_DEBOUNCE_MS);
  EXPECT_EQ(this->request_(), 0x01);
  this->expect_nothing_sent_();
  // heartbeat 0x00: the MCU restarted
  this->mcu_(HEARTBEAT, {0x00});
  this->sent_(PRODUCT_QUERY);
  this->finish_init_();
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, QueuedFramesAreDroppedWhenTheMcuRestarts) {
  this->enable_({});
  EXPECT_TRUE(this->sut_.send_weather({int_value("w.temp", 21)}));
  EXPECT_TRUE(this->sut_.send_weather({int_value("w.humidity", 50)}));
  this->mcu_(HEARTBEAT, {0x00});
  Bytes frame = this->sent_raw_();
  // the first frame may have left before the restart, the second one waits behind it
  if (frame.size() > 3 && frame[3] == WEATHER_DATA)
    frame = this->sent_raw_();
  ASSERT_GT(frame.size(), 3u);
  EXPECT_EQ(frame[3], PRODUCT_QUERY);
  this->finish_init_();
  this->expect_nothing_sent_();
}

// 0x20 that finds no value: the request callback runs again on every retry tick, ten times in all
TEST_F(TuyaWeatherTest, EnableWithoutValuesIsRetriedTenTimes) {
  int calls = 0;
  this->sut_.add_on_weather_request_callback([&calls](const std::vector<std::string> &) { calls++; });
  this->enable_({"w.temp"});
  // the timer runs first after a short offset: that tick does nothing
  this->retry_();
  EXPECT_EQ(calls, 1);
  for (int i = 0; i < MAX_RETRIES - 1; i++)
    this->retry_();
  EXPECT_EQ(calls, MAX_RETRIES);
  EXPECT_TRUE(this->stop_retry_());
  this->sut_.add_on_weather_request_callback([](const std::vector<std::string> &) {});
  this->enable_({"w.temp"});
  this->retry_();
  for (int i = 0; i < MAX_RETRIES; i++)
    this->retry_();
  EXPECT_FALSE(this->stop_retry_());
  this->expect_nothing_sent_();
}

TEST_F(TuyaWeatherTest, RetryEndsWithTheFirstFrame) {
  bool ready = false;
  this->sut_.add_on_weather_request_callback([this, &ready](const std::vector<std::string> &) {
    if (ready)
      this->sut_.send_weather({int_value("w.temp", 21)});
  });
  this->enable_({"w.temp"});
  this->retry_();
  this->retry_();
  this->expect_nothing_sent_();
  ready = true;
  this->retry_();
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  EXPECT_EQ(this->weather_frame_(), expected);
  EXPECT_FALSE(this->stop_retry_());
}

// Values of one key do not answer the request for the others
TEST_F(TuyaWeatherTest, SendOutsideARequestKeepsTheRetry) {
  this->sut_.add_on_weather_request_callback([](const std::vector<std::string> &) {});
  this->enable_({"w.temp", "w.humidity"});
  EXPECT_TRUE(this->sut_.send_weather({int_value("w.temp", 21)}));
  EXPECT_TRUE(this->stop_retry_());
}

// update_interval: no frame when the MCU acknowledged an answer less than half an interval ago
TEST_F(TuyaWeatherTest, UpdateIsSkippedAfterARecentAnswer) {
  this->sut_.set_weather_update_interval(UPDATE_INTERVAL_MS);
  this->sut_.add_on_weather_request_callback(
      [this](const std::vector<std::string> &) { this->sut_.send_weather({int_value("w.temp", 21)}); });
  this->enable_({"w.temp"});
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  EXPECT_EQ(this->weather_frame_(), expected);
  // the first tick comes after a short offset and does nothing, the second one finds the recent answer
  this->update_();
  this->update_();
  this->expect_nothing_sent_();
  this->advance_(PAST_HALF_UPDATE_INTERVAL_MS);
  this->update_();
  EXPECT_EQ(this->weather_frame_(), expected);
}

// An answer the MCU did not acknowledge does not start the 20 min limit and is built again by the retry
TEST_F(TuyaWeatherTest, UnacknowledgedAnswerIsBuiltAgain) {
  this->sut_.add_on_weather_request_callback(
      [this](const std::vector<std::string> &) { this->sut_.send_weather({int_value("w.temp", 21)}); });
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_EQ(this->sent_(WEATHER_DATA), expected);
  this->lose_frame_();
  this->retry_();
  this->retry_();
  EXPECT_EQ(this->sent_(WEATHER_DATA), expected);
  this->lose_frame_();
  EXPECT_TRUE(this->stop_retry_());
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_EQ(this->weather_frame_(), expected);
}

// An ack that arrives after the frame was given up does not count as an answer
TEST_F(TuyaWeatherTest, LateAckIsIgnored) {
  this->sut_.add_on_weather_request_callback(
      [this](const std::vector<std::string> &) { this->sut_.send_weather({int_value("w.temp", 21)}); });
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_EQ(this->sent_(WEATHER_DATA), expected);
  this->lose_frame_();
  this->mcu_(WEATHER_DATA, {});
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_EQ(this->weather_frame_(), expected);
}

// A frame of a send_weather() call outside a request is not the answer to a request
TEST_F(TuyaWeatherTest, FrameOutsideARequestDoesNotAnswerARequest) {
  this->enable_({});
  EXPECT_TRUE(this->sut_.send_weather({int_value("w.humidity", 50)}));
  this->sent_(WEATHER_DATA);
  EXPECT_EQ(this->request_(), 0x01);
}

// 9600 baud: a long frame is on the wire for longer than the 300 ms the component waits for a reply
TEST_F(TuyaWeatherTest, LongFrameIsNotGivenUpWhileItIsOnTheWire) {
  this->uart_.set_baud_rate(9600);
  this->uart_.set_data_bits(8);
  this->uart_.set_stop_bits(1);
  this->sut_.add_on_weather_request_callback([this](const std::vector<std::string> &) {
    this->sut_.send_weather(
        {string_value("w.conditionNum", std::string(99, 'a')), string_value("w.condition", std::string(99, 'b')),
         string_value("w.tips", std::string(99, 'c')), string_value("w.windDir", std::string(99, 'd')),
         string_value("w.sunrise", std::string(99, 'e')), string_value("w.sunset", std::string(99, 'f'))});
  });
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_GT(this->sent_(WEATHER_DATA).size(), 600u);
  this->pump_(PAST_RECEIVE_TIMEOUT_MS);
  EXPECT_TRUE(this->uart_.tx.empty());
  // the ack still counts: the next request is answered by this frame
  this->mcu_(WEATHER_DATA, {});
  EXPECT_EQ(this->request_(), 0x00);
  this->expect_nothing_sent_();
}

TEST_F(TuyaWeatherTest, EnableWithAnEmptyKeyIsRefused) {
  this->mcu_(WEATHER_ENABLE, {0x00});
  EXPECT_EQ(this->sent_(WEATHER_ENABLE), (Bytes{0x00, 0x01}));
  EXPECT_FALSE(this->sut_.send_weather({int_value("w.temp", 21)}));
  this->expect_nothing_sent_();
}

TEST_F(TuyaWeatherTest, EnableWithTooManyKeysIsRefused) {
  Bytes payload;
  for (int i = 0; i < 65; i++) {
    payload.push_back(0x01);
    payload.push_back('k');
  }
  this->mcu_(WEATHER_ENABLE, payload);
  EXPECT_EQ(this->sent_(WEATHER_ENABLE), (Bytes{0x00, 0x01}));
  EXPECT_FALSE(this->sut_.send_weather({int_value("k", 21)}));
  this->expect_nothing_sent_();
}

// 0x34/0x03 after a restart of the module alone: no key list, configured forecast keys make it a forecast
TEST_F(TuyaWeatherTest, RequestWithoutEnableSendsConfiguredForecastWithDays) {
  sensor::Sensor today, tomorrow;
  this->sut_.init_weather_sources(2, 0);
  this->sut_.add_weather_sensor("w.temp", &today);
  this->sut_.add_weather_sensor("w.temp.1", &tomorrow);
  today.publish_state(21.0f);
  tomorrow.publish_state(19.0f);
  EXPECT_EQ(this->request_(), 0x00);
  Bytes expected{0x01};
  put_entry(expected, "w.temp.0", 0x00, {0x00, 0x00, 0x00, 0x15});
  put_entry(expected, "w.temp.1", 0x00, {0x00, 0x00, 0x00, 0x13});
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, MalformedEnableTurnsTheServiceOff) {
  this->enable_({"w.temp"});
  // key length 5, one byte of key
  this->mcu_(WEATHER_ENABLE, {0x05, 'w'});
  EXPECT_EQ(this->sent_(WEATHER_ENABLE), (Bytes{0x00, 0x01}));
  EXPECT_FALSE(this->sut_.send_weather({int_value("w.temp", 21)}));
  this->expect_nothing_sent_();
}

// Configured sources: a sensor is rounded to an integer, the wind speed is a string with one decimal
TEST_F(TuyaWeatherTest, SensorSourcesAreRoundedAndWindSpeedIsAString) {
  sensor::Sensor temp, wind;
  this->sut_.init_weather_sources(2, 0);
  this->sut_.add_weather_sensor("w.temp", &temp);
  this->sut_.add_weather_sensor("w.windSpeed", &wind);
  temp.publish_state(-2.6f);
  wind.publish_state(3.5f);
  EXPECT_EQ(this->request_(), 0x00);
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0xFF, 0xFF, 0xFF, 0xFD});
  put_entry(expected, "w.windSpeed", 0x01, {'3', '.', '5'});
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, TextSensorSourceIsSentAsAString) {
  text_sensor::TextSensor dir;
  this->sut_.init_weather_sources(0, 1);
  this->sut_.add_weather_text_sensor("w.windDir", &dir);
  dir.publish_state("NE");
  EXPECT_EQ(this->request_(), 0x00);
  Bytes expected{0x01};
  put_entry(expected, "w.windDir", 0x01, {'N', 'E'});
  EXPECT_EQ(this->weather_frame_(), expected);
}

// A source without a state is left out and the answer does not start the request limit
TEST_F(TuyaWeatherTest, SourceWithoutAStateIsLeftOut) {
  sensor::Sensor temp, humidity;
  text_sensor::TextSensor dir;
  this->sut_.init_weather_sources(2, 1);
  this->sut_.add_weather_sensor("w.temp", &temp);
  this->sut_.add_weather_sensor("w.humidity", &humidity);
  this->sut_.add_weather_text_sensor("w.windDir", &dir);
  temp.publish_state(21.0f);
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_EQ(this->weather_frame_(), expected);
  this->advance_(PAST_DEBOUNCE_MS);
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, NoSourceWithAStateGetsFailureResult) {
  sensor::Sensor temp;
  text_sensor::TextSensor dir;
  this->sut_.init_weather_sources(1, 1);
  this->sut_.add_weather_sensor("w.temp", &temp);
  this->sut_.add_weather_text_sensor("w.windDir", &dir);
  EXPECT_EQ(this->request_(), 0x01);
  this->expect_nothing_sent_();
}

TEST_F(TuyaWeatherTest, SensorOutOfTheInt32RangeIsLeftOut) {
  sensor::Sensor pressure, humidity;
  this->sut_.init_weather_sources(2, 0);
  this->sut_.add_weather_sensor("w.pressure", &pressure);
  this->sut_.add_weather_sensor("w.humidity", &humidity);
  pressure.publish_state(3e9f);
  humidity.publish_state(49.5f);
  EXPECT_EQ(this->request_(), 0x00);
  Bytes expected{0x01};
  put_entry(expected, "w.humidity", 0x00, {0x00, 0x00, 0x00, 0x32});
  EXPECT_EQ(this->weather_frame_(), expected);
}

TEST_F(TuyaWeatherTest, WindSpeedThatDoesNotFitIsLeftOut) {
  sensor::Sensor wind, humidity;
  this->sut_.init_weather_sources(2, 0);
  this->sut_.add_weather_sensor("w.windSpeed", &wind);
  this->sut_.add_weather_sensor("w.humidity", &humidity);
  wind.publish_state(1e20f);
  humidity.publish_state(50.0f);
  EXPECT_EQ(this->request_(), 0x00);
  Bytes expected{0x01};
  put_entry(expected, "w.humidity", 0x00, {0x00, 0x00, 0x00, 0x32});
  EXPECT_EQ(this->weather_frame_(), expected);
}

// A value of the automation wins over the source for the same key
TEST_F(TuyaWeatherTest, AutomationValueReplacesTheSource) {
  sensor::Sensor temp, humidity;
  this->sut_.init_weather_sources(2, 0);
  this->sut_.add_weather_sensor("w.temp", &temp);
  this->sut_.add_weather_sensor("w.humidity", &humidity);
  temp.publish_state(21.0f);
  humidity.publish_state(50.0f);
  this->sut_.add_on_weather_request_callback(
      [this](const std::vector<std::string> &) { this->sut_.send_weather({int_value("w.temp", 5)}); });
  EXPECT_EQ(this->request_(), 0x00);
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x05});
  put_entry(expected, "w.humidity", 0x00, {0x00, 0x00, 0x00, 0x32});
  EXPECT_EQ(this->weather_frame_(), expected);
}

// A source without a state for a key the MCU did not ask for leaves the answer complete: the limit starts
TEST_F(TuyaWeatherTest, UnrequestedSourceWithoutAStateKeepsTheAnswerComplete) {
  sensor::Sensor temp, humidity;
  this->sut_.init_weather_sources(2, 0);
  this->sut_.add_weather_sensor("w.temp", &temp);
  this->sut_.add_weather_sensor("w.humidity", &humidity);
  temp.publish_state(21.0f);
  Bytes expected{0x01};
  put_entry(expected, "w.temp", 0x00, {0x00, 0x00, 0x00, 0x15});
  this->enable_({"w.temp"});
  EXPECT_EQ(this->weather_frame_(), expected);
  this->advance_(PAST_DEBOUNCE_MS);
  EXPECT_EQ(this->request_(), 0x00);
  EXPECT_EQ(this->weather_frame_(), expected);
  this->advance_(PAST_DEBOUNCE_MS);
  EXPECT_EQ(this->request_(), 0x01);
  this->expect_nothing_sent_();
}

}  // namespace esphome::tuya::testing
