#pragma once

#include <cinttypes>
#include <memory>
#include <vector>

#include "tuya.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/time.h"
#endif

namespace esphome::tuya {

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

#ifdef USE_TUYA_MCU_WATCHDOG
namespace testing {
class TuyaWatchdogTest;
}  // namespace testing
#endif

/// Tuya MCU controller connected over UART (the standard Tuya serial protocol).
class TuyaUART final : public Tuya, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_status_pin(InternalGPIOPin *status_pin) { this->status_pin_ = status_pin; }
  void set_heartbeat(bool heartbeat) { this->heartbeat_ = heartbeat; }
  void set_datapoint_ack(bool datapoint_ack) { this->datapoint_ack_ = datapoint_ack; }
#ifdef USE_TUYA_MCU_WATCHDOG
  void set_mcu_watchdog(uint8_t missed_heartbeats) { this->watchdog_missed_heartbeats_ = missed_heartbeats; }
#endif
  TuyaInitState get_init_state();
#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
  void set_force_time_sync(bool force_time_sync) { this->force_time_sync_ = force_time_sync; }
#endif

 protected:
  void dump_mcu_config_();
  void handle_char_(uint8_t c);
  void handle_datapoints_(const uint8_t *buffer, size_t len, bool ack);
  bool validate_message_();

  void handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
  void send_raw_command_(TuyaCommand command);
  void process_command_queue_();
  void send_command_(TuyaCommand command);
  void send_empty_command_(TuyaCommandType command);
  void send_datapoint_command(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint8_t *data,
                              size_t len) override;
  void set_status_pin_();
  void set_heartbeat_interval_(uint32_t interval);
  void retry_init_();
  void log_init_failure_();
  void start_network_status_();
  void send_wifi_status_();
  uint8_t get_wifi_status_code_();
  uint8_t get_wifi_rssi_();

#ifdef USE_TUYA_MCU_WATCHDOG
  // clang-tidy compiles the host tests without USE_HOST, so the friend is unconditional
  friend class testing::TuyaWatchdogTest;
  void note_missed_heartbeat_();
  uint8_t watchdog_missed_heartbeats_{0};
  uint8_t heartbeat_misses_{0};        // unanswered heartbeats, less one per two answered in a row at the first send
  uint8_t heartbeat_answers_{0};       // heartbeats in a row answered at the first send, 0 or 1
  bool unarmed_misses_logged_{false};  // count reached before the working mode reply, logged once
#else
  bool heartbeat_missed_{false};  // unanswered heartbeat logged, until the next reply
#endif
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
  bool init_failure_logged_{false};
  TuyaInitState init_failure_state_{TuyaInitState::INIT_HEARTBEAT};  // step of the last logged failure
  uint8_t wifi_status_retries_{0};  // heartbeat: false, WiFi status sends without a reply
  uint8_t protocol_version_ = -1;
  InternalGPIOPin *status_pin_{nullptr};
  int status_pin_reported_ = -1;
  int reset_pin_reported_ = -1;
  uint32_t last_command_timestamp_ = 0;
  uint32_t last_rx_char_timestamp_ = 0;
  std::unique_ptr<char[]> product_;
  std::vector<uint8_t> rx_message_;
  std::vector<TuyaCommand> command_queue_;
  optional<TuyaCommandType> expected_response_{};
  uint8_t wifi_status_ = -1;
  bool product_invalid_{false};
  bool working_mode_received_{false};
  bool resent_{false};
  bool heartbeat_{true};
  bool datapoint_ack_{false};
};

}  // namespace esphome::tuya
