#pragma once

#include <cinttypes>
#include <memory>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_TUYA_LOW_POWER
#include <array>

#include "esphome/core/automation.h"
#include "tuya_low_power.h"
#include "tuya_low_power_product.h"
#include "tuya_low_power_autobaud.h"
#include "tuya_low_power_sleep_window.h"
#include "tuya_low_power_product_retry.h"
#include "tuya_low_power_standby_state.h"
#include "tuya_low_power_wire_transactions.h"
#include "tuya_low_power_uart_errors.h"
#include "tuya_low_power_host.h"
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
  void set_auto_baud(bool auto_baud) { this->auto_baud_ = auto_baud; }
  void set_mcu_wake_pin(GPIOPin *pin) { this->mcu_wake_pin_ = pin; }
  /// Only an MCU that reports this product ID (CMD01 "p") is accepted; empty accepts any.
  void set_product_id(const char *product_id) { this->product_id_ = product_id; }
  void set_low_power_host(TuyaLowPowerHost *host) { this->lp_host_ = host; }
  bool request_low_power_refresh();
  /// Diagnostics: sends only the start of an MCU firmware update (CMD0A, or CMD0D for version-0 frames) for an
  /// image of `image_size` bytes and logs the MCU's answer. No data follows; the running MCU firmware is untouched.
  bool low_power_probe_mcu_update(uint32_t image_size);
  /// Sends an MCU firmware image with the Tuya MCU update commands (start, then packets of offset and data, each
  /// acknowledged, then an empty packet at the end). The MCU checks the image itself before it installs it. `image`
  /// must stay valid until the transfer ends. Restarts the module afterwards, so it detects the MCU anew.
  bool low_power_update_mcu(const uint8_t *image, size_t size);
  /// Firmware version from the MCU's product info ("v"), empty until it was reported.
  const std::string &low_power_mcu_version() const { return this->lp_product_.version; }
  // Called by the platform's power management bridge (not by CMD38 alone).
  bool notify_low_power_transition(bool sleeping);
  /// Standby steps, called by the platform's power management bridge.
  bool low_power_prepare_for_standby();
  void low_power_standby_cancelled();
  void low_power_standby_resumed(bool expected_source, bool wake_gpio);
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
  /// Called once after boot, when the MCU takes commands: after the initialization of the standard protocol,
  /// and for protocol low_power once the MCU reports its changes on its own.
  template<typename F> void add_on_mcu_ready_callback(F &&callback) {
    this->mcu_ready_callback_.add(std::forward<F>(callback));
  }

 protected:
  void notify_mcu_ready_() {
    if (this->mcu_ready_)
      return;
    this->mcu_ready_ = true;
    this->mcu_ready_callback_.call();
  }
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
  bool check_low_power_uart_rx_errors_();
  uint32_t low_power_uart_rx_error_count_() const;
  void dump_low_power_config_();
  void on_low_power_rx_byte_();
  void preempt_low_power_sleep_();
  void handle_low_power_frame_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
  void send_low_power_raw_command_(const TuyaCommand &command);
  bool begin_low_power_command_(const TuyaCommand &command);
  void send_low_power_datapoint_(uint8_t datapoint_id, TuyaDatapointType datapoint_type,
                                 const std::vector<uint8_t> &data);
  void handle_low_power_command_(uint8_t command, uint8_t version, const uint8_t *data, size_t len);
  void send_low_power_direct_(uint8_t command, const std::vector<uint8_t> &payload);
  void send_low_power_time_(uint8_t command);
  void send_low_power_network_();
  void update_low_power_sleep_lock_();
  void poll_low_power_autobaud_();
  void send_low_power_product_query_(bool direct_probe);
  void begin_mcu_wake_pulse_();
  bool mcu_wake_pulse_running_();
  void poll_low_power_v3_status_();
  void process_low_power_queue_();
  void poll_low_power_product_recovery_();
  void invalidate_low_power_mode_epoch_();
  bool low_power_ready_for_sleep_(bool in_cmd38_handler = false) const;
  void cancel_low_power_standby_();
  void poll_low_power_sleep_preflight_();
  void cancel_low_power_preflight_(bool notify_mcu);
  void send_low_power_awake_notification_();
  void send_low_power_awake_frame_();
  void configure_low_power_wake_lines_();
  bool low_power_v3_sleep_supported_() const;
  void poll_low_power_v3_sleep_();
  void handle_low_power_v3_sleep_(const uint8_t *data, size_t len);
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
  bool auto_baud_{false};
  GPIOPin *mcu_wake_pin_{nullptr};
  TuyaLowPowerHost *lp_host_{nullptr};
  const char *product_id_{""};
  TuyaLowPowerAutoBaud lp_autobaud_{};
  TuyaLowPowerUartErrors lp_uart_rx_errors_{};
  TuyaLowPowerSession lp_session_{};
  TuyaLowPowerProductInfo lp_product_{};
  // Bounded CMD01 JSON copy for diagnostics, no extra heap allocation.
  std::array<char, 513> lp_product_text_{};
  TuyaLowPowerSleepWindow lp_sleep_window_{};
  TuyaLowPowerProductRetry lp_product_retry_{};
  bool lp_mode_requery_pending_{false};
  bool lp_initialized_callback_called_{false};
  // The MCU frames with protocol version 3: status via CMD08/CMD07, writes via CMD06.
  bool lp_v3_{false};
  uint32_t lp_v3_last_poll_ms_{0};
  bool lp_v3_mode_acked_{false};
  // The MCU sent a report nobody asked for: it pushes changes, so the status poll is not needed.
  bool lp_v3_pushes_{false};
  // sm=2: the module offers sleep with 0x91 after a quiet period, as the original T3-U firmware does.
  bool lp_v3_offer_pending_{false};
  // Diagnostics: logged only when they change, so a waiting module does not fill the log.
  const LogString *lp_v3_sleep_blocked_{nullptr};
  uint8_t lp_v3_last_answer_{0xFF};
  uint32_t lp_v3_offer_deadline_ms_{0};
  uint32_t lp_v3_idle_since_ms_{0};
  bool lp_wake_lines_configured_{false};
  enum class UpdateProbe : uint8_t { UPDATE_PROBE_IDLE, UPDATE_PROBE_PENDING, UPDATE_PROBE_SENT };
  UpdateProbe lp_update_probe_{UpdateProbe::UPDATE_PROBE_IDLE};
  uint32_t lp_update_probe_size_{0};
  uint32_t lp_update_probe_sent_ms_{0};
  uint8_t low_power_update_start_cmd_() const { return this->lp_v3_ ? 0x0A : 0x0D; }
  uint8_t low_power_update_data_cmd_() const { return this->lp_v3_ ? 0x0B : 0x0E; }
  // Version-0 MCUs (HC32) clear the staging area only on a separate erase command (0x0C, subcommand 0),
  // which blocks for the whole erase without an answer. Version-3 MCUs erase on the start command.
  static constexpr uint8_t LOW_POWER_UPDATE_ERASE_CMD = 0x0C;
  static constexpr uint32_t LOW_POWER_UPDATE_ERASE_MS = 1000;
  void poll_low_power_update_probe_();
  void wake_low_power_mcu_();
  enum class McuUpdate : uint8_t {
    MCU_UPDATE_IDLE,
    MCU_UPDATE_START_PENDING,
    MCU_UPDATE_START_SENT,
    MCU_UPDATE_ERASE_WAIT,
    MCU_UPDATE_SEND_PACKET,
    MCU_UPDATE_PACKET_SENT,
    MCU_UPDATE_FINAL_SENT,
  };
  void poll_low_power_mcu_update_();
  bool handle_low_power_mcu_update_(uint8_t command, const uint8_t *data, size_t len);
  void send_low_power_update_packet_();
  void fail_low_power_mcu_update_(const char *reason);
  McuUpdate lp_mcu_update_{McuUpdate::MCU_UPDATE_IDLE};
  const uint8_t *lp_mcu_update_image_{nullptr};
  uint32_t lp_mcu_update_size_{0};
  uint32_t lp_mcu_update_offset_{0};
  uint16_t lp_mcu_update_packet_{256};
  uint16_t lp_mcu_update_chunk_{0};
  uint8_t lp_mcu_update_retries_{0};
  uint8_t lp_mcu_update_percent_{0};
  uint32_t lp_mcu_update_sent_ms_{0};
  bool lp_mcu_wake_from_host_{false};  // The wake output belongs to the host, not to the configuration
  int lp_wake_input_{-1};              // wk mapped to this module's GPIO numbering
  uint32_t lp_mode_requery_after_ms_{0};
  uint32_t lp_wake_guard_until_{0};
  uint32_t lp_mcu_wake_pulse_until_{0};
  uint32_t lp_mcu_woken_ms_{0};  // Last wake pulse before a queued command
  bool lp_mcu_wake_pulse_active_{false};
  bool lp_awake_notification_pending_{false};
  uint32_t lp_last_resync_ms_{0};
  TuyaLowPowerStandbyState lp_standby_{};
  TuyaLowPowerWireTransactions lp_wire_{};
  bool lp_cmd38_pending_{false};
  uint32_t lp_cmd38_deadline_ms_{0};
  uint32_t lp_cmd38_cooldown_until_{0};
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
  CallbackManager<void()> mcu_ready_callback_{};
  bool mcu_ready_{false};
};

#ifdef USE_TUYA_LOW_POWER
// Explicit DP106 refresh for automation; no unsolicited boot refresh.
template<typename... Ts> class TuyaRefreshAction : public Action<Ts...> {
 public:
  explicit TuyaRefreshAction(Tuya *parent) : parent_(parent) {}

 protected:
  void play(const Ts &...x) override { (void) this->parent_->request_low_power_refresh(); }

  Tuya *parent_;
};
#endif

}  // namespace esphome::tuya
