#pragma once

#include "tuya.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/gpio.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/time.h"
#endif

namespace esphome {
namespace tuya {

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

class TuyaUART : public Tuya, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void set_status_pin(InternalGPIOPin *status_pin) { this->status_pin_ = status_pin; }
  TuyaInitState get_init_state();
#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
#endif

 protected:
  void handle_char_(uint8_t c);
  bool validate_message_();
  void handle_datapoints_(const uint8_t *buffer, size_t len);
  void handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
  void send_raw_command_(TuyaCommand command);
  void process_command_queue_();
  void send_command_(const TuyaCommand &command);
  void send_empty_command_(TuyaCommandType command);
  void send_datapoint_command(uint8_t datapoint_id, TuyaDatapointType datapoint_type,
                              const std::vector<uint8_t> &data) override;
  void set_status_pin_();
  void send_wifi_status_();
  uint8_t get_wifi_status_code_();
  uint8_t get_wifi_rssi_();
#ifdef USE_TIME
  void send_local_time_();
  time::RealTimeClock *time_id_{nullptr};
  bool time_sync_callback_registered_{false};
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
  std::string product_ = "";
  std::vector<uint8_t> rx_message_;
  std::vector<TuyaCommand> command_queue_;
  optional<TuyaCommandType> expected_response_{};
  uint8_t wifi_status_ = -1;
};

}  // namespace tuya
}  // namespace esphome
