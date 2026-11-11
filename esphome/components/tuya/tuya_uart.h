#pragma once

#include <cinttypes>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "tuya.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/time.h"
#endif
#ifdef USE_TUYA_WEATHER
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#endif

namespace esphome::tuya {

#ifdef USE_TUYA_WEATHER
static constexpr size_t MAX_FRAME_SIZE = 512;
#else
// Longest frame accepted, as in Tasmota; keeps the reader's positions 8 bit
static constexpr size_t MAX_FRAME_SIZE = 255;
#endif

// 55 AA, version, command, payload length (2, BE), payload, checksum
using TuyaLength = PayloadLengthU16BE<LengthField{.offset = 4, .trailer = 1}>;
void warn_too_long(size_t len);
using TuyaFrame = FrameFormat<FrameHeader<0x55, 0xAA>, WarnIfLonger<TuyaLength, MAX_FRAME_SIZE, warn_too_long>>;

// Scheduler ids of the weather service, after the ids in tuya_uart.cpp
inline constexpr uint32_t WEATHER_INTERVAL_ID = 4;
inline constexpr uint32_t WEATHER_RETRY_INTERVAL_ID = 5;

enum class TuyaCommandType : uint8_t {
  HEARTBEAT = 0x00,
  PRODUCT_QUERY = 0x01,
  CONF_QUERY = 0x02,
  WIFI_STATE = 0x03,
  WIFI_RESET = 0x04,
  WIFI_SELECT = 0x05,
  DATAPOINT_DELIVER = 0x06,
  DATAPOINT_REPORT_ASYNC = 0x07,
  DATAPOINT_QUERY = 0x08,
  GMT_TIME_QUERY = 0x0C,
  WIFI_TEST = 0x0E,
  LOCAL_TIME_QUERY = 0x1C,
  WEATHER_ENABLE = 0x20,  // MCU asks for the weather service with a list of keys
  WEATHER_DATA = 0x21,    // module sends weather values, MCU acks with an empty 0x21
  DATAPOINT_REPORT_SYNC = 0x22,
  DATAPOINT_REPORT_ACK = 0x23,
  WIFI_RSSI = 0x24,
  VACUUM_MAP_UPLOAD = 0x28,
  GET_NETWORK_STATUS = 0x2B,
  EXTENDED_SERVICES = 0x34,
};

enum class TuyaExtendedServicesCommandType : uint8_t {
  REQUEST_WEATHER = 0x03,  // MCU asks for weather data now (at most once per 20 min)
  RESET_NOTIFICATION = 0x04,
  MODULE_RESET = 0x05,
  UPDATE_IN_PROGRESS = 0x0A,
};

enum class TuyaInitState : uint8_t {
  INIT_HEARTBEAT = 0x00,
  INIT_PRODUCT,
  INIT_CONF,
  INIT_WIFI,
  INIT_DATAPOINT,
  INIT_DONE,
};

struct TuyaCommand {
  TuyaCommandType cmd;
  std::vector<uint8_t> payload;
};

#ifdef USE_TUYA_WEATHER
struct TuyaWeatherValue {
  std::string key;
  bool is_string;
  int32_t value_int;
  std::string value_string;
};

enum class TuyaWeatherFrame : uint8_t {
  VALUES,    // values of a send_weather() call outside a request
  ANSWER,    // collected answer that left a value out
  COMPLETE,  // collected answer with every value
  DEMAND,    // complete answer that serves a 0x34/0x03 request: its ack starts the 20 min limit
};

namespace testing {
class TuyaWeatherTest;
}  // namespace testing
#endif

/// Tuya MCU controller connected over UART (the standard Tuya serial protocol).
class TuyaUART final : public Tuya, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_status_pin(InternalGPIOPin *status_pin) { this->status_pin_ = status_pin; }
  TuyaInitState get_init_state();
#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
  void set_force_time_sync(bool force_time_sync) { this->force_time_sync_ = force_time_sync; }
#endif

#ifdef USE_TUYA_WEATHER
  void set_weather_update_interval(uint32_t interval_ms) { this->weather_update_interval_ = interval_ms; }
  void init_weather_sources([[maybe_unused]] size_t sensors, [[maybe_unused]] size_t text_sensors) {
#ifdef USE_SENSOR
    this->weather_sensors_.init(sensors);
#endif
#ifdef USE_TEXT_SENSOR
    this->weather_text_sensors_.init(text_sensors);
#endif
  }
#ifdef USE_SENSOR
  void add_weather_sensor(const char *key, sensor::Sensor *source) { this->weather_sensors_.emplace_back(key, source); }
#endif
#ifdef USE_TEXT_SENSOR
  void add_weather_text_sensor(const char *key, text_sensor::TextSensor *source) {
    this->weather_text_sensors_.emplace_back(key, source);
  }
#endif
  template<typename F> void add_on_weather_request_callback(F &&callback) {
    this->weather_request_callback_.add(std::forward<F>(callback));
  }
  bool send_weather(const std::vector<TuyaWeatherValue> &values);
#endif

 protected:
  void dump_mcu_config_();
  /// Returns false for a frame that fails the checksum
  bool handle_frame_(const uint8_t *frame, size_t len);
  void handle_datapoints_(const uint8_t *buffer, size_t len);

  void handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
  void send_raw_command_(TuyaCommand command);
  void process_command_queue_();
  void send_command_(TuyaCommand command);
  void send_empty_command_(TuyaCommandType command);
  void send_datapoint_command(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint8_t *data,
                              size_t len) override;
  void set_status_pin_();
  void send_wifi_status_();
  uint8_t get_wifi_status_code_();
  uint8_t get_wifi_rssi_();

#ifdef USE_TIME
  void register_local_time_sync_callback_();
  void check_force_time_sync_();
  void send_local_time_(bool periodic = false);
  void send_gmt_time_();
  time::RealTimeClock *time_id_{nullptr};
  bool force_time_sync_{false};
  uint8_t last_force_time_sync_minute_{0xFF};  // 0xFF: no minute seen yet
  bool time_sync_callback_registered_{false};
  bool gmt_time_sync_callback_registered_{false};
#endif
  TuyaInitState init_state_ = TuyaInitState::INIT_HEARTBEAT;
  bool init_failed_{false};
  int init_retries_{0};
  uint8_t protocol_version_ = -1;
  InternalGPIOPin *status_pin_{nullptr};
  int status_pin_reported_ = -1;
  int reset_pin_reported_ = -1;
  uint32_t last_command_timestamp_ = 0;
  uint32_t last_rx_char_timestamp_ = 0;
  std::unique_ptr<char[]> product_;
  std::vector<TuyaCommand> command_queue_;
  optional<TuyaCommandType> expected_response_{};
  uint8_t wifi_status_ = -1;
  bool product_invalid_{false};
  // Holds commands back while a received frame is handled
  bool handling_frame_{false};
  LengthFrameReader<MAX_FRAME_SIZE, TuyaFrame> reader_;

#ifdef USE_TUYA_WEATHER
  friend class testing::TuyaWeatherTest;
  void reset_weather_();
  void handle_weather_enable_(const uint8_t *buffer, size_t len);
  void answer_weather_enable_();
  void handle_weather_request_();
  void start_weather_interval_();
  void update_weather_();
  void start_weather_retry_();
  void retry_weather_();
  void handle_weather_ack_();
  void handle_weather_lost_();
  bool request_weather_();
  bool collect_weather_(std::vector<uint8_t> &payload);
  bool build_weather_payload_(std::span<const TuyaWeatherValue> values, std::vector<uint8_t> &payload, bool &dropped);
  void queue_weather_frame_(std::vector<uint8_t> &&payload, TuyaWeatherFrame kind);
  bool weather_answer_queued_() const;
  bool weather_key_requested_(StringRef key) const;
  bool weather_source_skipped_(const char *key) const;
  uint8_t configured_weather_days_() const;
  TuyaWeatherFrame weather_answer_kind_() const;
  void add_pending_weather_(StringRef key, bool is_string, int32_t value_int, StringRef value_string);
  std::vector<uint8_t> take_weather_buffer_();
  void keep_weather_buffer_(std::vector<uint8_t> &buffer);
  void continue_weather_tx_();
  bool finish_weather_tx_();
  std::span<const TuyaWeatherValue> pending_weather_values_() const {
    return {this->pending_weather_.data(), this->pending_weather_count_};
  }
  uint32_t weather_update_interval_ = 0;  // 0 = only on the MCU's requests
  bool weather_enabled_ = false;          // MCU sent 0x20 (or 0x34/0x03)
  bool collecting_weather_ = false;       // inside collect_weather_: send_weather() collects into pending_weather_
  bool weather_incomplete_ = false;       // last answer left out a value (no state, out of range, too long)
  std::vector<std::string> weather_keys_;
  uint8_t weather_forecast_days_ = 0;  // w.date.N in the enable list: values w.<key>.<day> for day < N
  std::vector<TuyaWeatherValue> pending_weather_;
  size_t pending_weather_count_ = 0;
  std::vector<uint8_t> weather_buffer_;           // payload of the frame on the wire, then memory for the next one
  size_t weather_tx_offset_ = 0;                  // its bytes written so far, the checksum counts as one
  uint32_t weather_tx_last_ = 0;                  // loop time of its last write
  uint32_t weather_tx_end_ = 0;                   // earliest time all of it has left the UART
  bool weather_tx_active_ = false;                // a 0x21 frame is written in parts, nothing else goes out
  bool weather_demand_pending_ = false;           // a 0x34/0x03 request waits for the ack of a complete answer
  bool weather_last_complete_ = false;            // the answer acknowledged last had every value
  std::vector<TuyaWeatherFrame> weather_frames_;  // kinds of the queued 0x21 frames, in queue order
  bool weather_in_flight_ = false;                // the first of them is written and waits for its ack
  bool skip_weather_retry_ = false;               // the retry timer has not run yet
  bool skip_weather_update_ = false;              // the update_interval timer has not run yet
  bool weather_update_empty_ = false;             // the last tick or request found no value, warned once
  uint8_t weather_retries_ = 0;                   // tries since the last acknowledged answer
  uint32_t last_weather_sent_ = 0;                // last answer the MCU acknowledged
  uint32_t last_weather_demand_ = 0;              // last acknowledged answer that counts against the 20 min limit
  CallbackManager<void(const std::vector<std::string> &)> weather_request_callback_{};
#ifdef USE_SENSOR
  FixedVector<std::pair<const char *, sensor::Sensor *>> weather_sensors_;
#endif
#ifdef USE_TEXT_SENSOR
  FixedVector<std::pair<const char *, text_sensor::TextSensor *>> weather_text_sensors_;
#endif
#endif
};

}  // namespace esphome::tuya
