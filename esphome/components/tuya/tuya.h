#pragma once

#include <cinttypes>
#include <memory>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_TUYA_LOW_POWER
#include "tuya_low_power.h"
#include "tuya_low_power_product.h"
#include "tuya_low_power_wire_transactions.h"
#endif  // USE_TUYA_LOW_POWER

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/time.h"
#endif

namespace esphome::tuya {

enum class TuyaDatapointType : uint8_t {
  RAW = 0x00,      // variable length
  BOOLEAN = 0x01,  // 1 byte (0/1)
  INTEGER = 0x02,  // 4 byte
  STRING = 0x03,   // variable length
  ENUM = 0x04,     // 1 byte
  BITMASK = 0x05,  // 1/2/4 bytes
};

struct TuyaDatapoint {
  uint8_t id;
  TuyaDatapointType type;
  size_t len;
  union {
    bool value_bool;
    int value_int;
    uint32_t value_uint;
    uint8_t value_enum;
    uint32_t value_bitmask;
  };
  std::string value_string;
  std::vector<uint8_t> value_raw;

  /// Log a warning when this datapoint is not of type `expected`, and return it.
  const TuyaDatapoint &expect_type(TuyaDatapointType expected) const;
};

struct TuyaDatapointListener {
  uint8_t datapoint_id;
  Callback<void(const TuyaDatapoint &)> on_datapoint;
};

#ifdef USE_TUYA_LOW_POWER
enum class TuyaProtocol : uint8_t {
  TUYA_PROTOCOL_STANDARD = 0,
  TUYA_PROTOCOL_LOW_POWER = 1,
};
#endif

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
  DATAPOINT_REPORT_SYNC = 0x22,
  DATAPOINT_REPORT_ACK = 0x23,
  WIFI_RSSI = 0x24,
  VACUUM_MAP_UPLOAD = 0x28,
  GET_NETWORK_STATUS = 0x2B,
  EXTENDED_SERVICES = 0x34,
};

enum class TuyaExtendedServicesCommandType : uint8_t {
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

class Tuya final : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  template<typename F> void register_listener(uint8_t datapoint_id, F &&func) {
    this->register_listener_(datapoint_id, Callback<void(const TuyaDatapoint &)>::create(std::forward<F>(func)));
  }
  void set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
  void set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void set_status_pin(InternalGPIOPin *status_pin) { this->status_pin_ = status_pin; }
#ifdef USE_TUYA_LOW_POWER
  void set_protocol(TuyaProtocol protocol) { this->protocol_ = protocol; }
  void set_mcu_wake_pin(GPIOPin *pin) { this->mcu_wake_pin_ = pin; }
  /// Called once after boot, when the MCU's state is known and it takes writes.
  template<typename F> void add_on_mcu_ready_callback(F &&callback) {
    this->mcu_ready_callback_.add(std::forward<F>(callback));
  }
#endif  // USE_TUYA_LOW_POWER
  void set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void set_string_datapoint_value(uint8_t datapoint_id, const char *value);
  void set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
  void set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length);
  void force_set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void force_set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
  void force_set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void force_set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void force_set_string_datapoint_value(uint8_t datapoint_id, const char *value);
  void force_set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
  void force_set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length);
  TuyaInitState get_init_state();
#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
  void set_force_time_sync(bool force_time_sync) { this->force_time_sync_ = force_time_sync; }
#endif
  void add_ignore_mcu_update_on_datapoints(uint8_t ignore_mcu_update_on_datapoints) {
    this->ignore_mcu_update_on_datapoints_.push_back(ignore_mcu_update_on_datapoints);
  }
  template<typename F> void add_on_initialized_callback(F &&callback) {
    this->initialized_callback_.add(std::forward<F>(callback));
  }

 protected:
  void register_listener_(uint8_t datapoint_id, Callback<void(const TuyaDatapoint &)> func);
  void handle_char_(uint8_t c);
  void handle_datapoints_(const uint8_t *buffer, size_t len);
  optional<TuyaDatapoint> get_datapoint_(uint8_t datapoint_id);
  bool validate_message_();

  void handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
#ifdef USE_TUYA_LOW_POWER
  bool is_low_power_() const { return this->protocol_ == TuyaProtocol::TUYA_PROTOCOL_LOW_POWER; }
  // Entry points called from the standard Tuya code paths in tuya.cpp.
  void setup_low_power_();
  void loop_low_power_();
  void handle_low_power_frame_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
  void send_low_power_raw_command_(const TuyaCommand &command);
  void send_low_power_datapoint_(uint8_t datapoint_id, TuyaDatapointType datapoint_type,
                                 const std::vector<uint8_t> &data);
  void handle_low_power_command_(uint8_t command, uint8_t version, const uint8_t *data, size_t len);
  void send_low_power_direct_(uint8_t command, const std::vector<uint8_t> &payload);
  void send_low_power_time_(uint8_t command);
  void send_low_power_network_();
  void send_low_power_product_query_(bool direct_probe);
  void send_product_query_now_(bool direct_probe);
  void begin_mcu_wake_pulse_();
  bool mcu_wake_pulse_running_();
  void poll_low_power_v3_status_();
  void process_low_power_queue_();
  void begin_low_power_product_retry_(uint32_t now);
  void poll_low_power_product_recovery_();
  void finish_low_power_sync_();
  void notify_mcu_ready_() {
    if (this->mcu_ready_)
      return;
    this->mcu_ready_ = true;
    this->mcu_ready_callback_.call();
  }
#endif  // USE_TUYA_LOW_POWER
  void send_raw_command_(TuyaCommand command);
  void process_command_queue_();
  void send_command_(const TuyaCommand &command);
  void send_empty_command_(TuyaCommandType command);
  void set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, uint32_t value,
                                    uint8_t length, bool forced);
  void set_string_datapoint_value_(uint8_t datapoint_id, const char *value, size_t len, bool forced);
  void set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced);
  void send_datapoint_command_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, std::vector<uint8_t> data);
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
#ifdef USE_TUYA_LOW_POWER
  TuyaProtocol protocol_{TuyaProtocol::TUYA_PROTOCOL_STANDARD};
  GPIOPin *mcu_wake_pin_{nullptr};
  TuyaLowPowerSession lp_session_{};
  TuyaLowPowerWireTransactions lp_wire_{};
  TuyaLowPowerProductInfo lp_product_{};
  CallbackManager<void()> mcu_ready_callback_{};
  uint32_t lp_product_retry_at_{0};
  uint8_t lp_product_retries_{0};  // Fast CMD01 retries left before the slow ones
  bool lp_product_retry_active_{false};
  bool mcu_ready_{false};
  // The MCU frames with protocol version 3: status via CMD08/CMD07, writes via CMD06.
  bool lp_v3_{false};
  bool lp_v3_mode_acked_{false};
  // The MCU sent a report nobody asked for: it pushes changes, so the status poll is not needed.
  bool lp_v3_pushes_{false};
  uint32_t lp_v3_last_poll_ms_{0};
  uint32_t lp_mcu_wake_pulse_until_{0};
  uint32_t lp_mcu_woken_ms_{0};  // Last wake pulse before a queued command
  bool lp_mcu_wake_pulse_active_{false};
  uint8_t lp_network_status_{0xFF};
#endif  // USE_TUYA_LOW_POWER
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
  std::vector<TuyaDatapointListener> listeners_;
  std::vector<TuyaDatapoint> datapoints_;
  std::vector<uint8_t> rx_message_;
  std::vector<uint8_t> ignore_mcu_update_on_datapoints_{};
  std::vector<TuyaCommand> command_queue_;
  optional<TuyaCommandType> expected_response_{};
  uint8_t wifi_status_ = -1;
  bool product_invalid_{false};
  CallbackManager<void()> initialized_callback_{};
};

}  // namespace esphome::tuya
