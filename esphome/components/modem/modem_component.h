#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#ifdef USE_MODEM_URC
#include "esphome/core/line_reader.h"
#endif
#include "esphome/core/preferences.h"
#include "esphome/core/static_task.h"
#include "esphome/components/network/ip_address.h"
#ifdef USE_MODEM_URC
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <cxx_include/esp_modem_api.hpp>
#include <esp_modem_config.h>

#include <atomic>
#include <memory>
#include <string>
#include <utility>

namespace esphome::modem {

enum class ModemState : uint8_t {
  MODEM_STATE_DISABLED,
  MODEM_STATE_ENABLING,
  MODEM_STATE_SYNCED,
  MODEM_STATE_CONNECTING,
  MODEM_STATE_WAIT_IP,
  MODEM_STATE_CONNECTED,
  MODEM_STATE_DISCONNECTING,
};

// CONNECTING runs its long commands once; the later passes only poll the attachment
enum class ConnectStep : uint8_t {
  CONNECT_STEP_START,
  CONNECT_STEP_RADIO_ON,
  CONNECT_STEP_ATTACH_REQUESTED,
};

enum class SimResult : uint8_t {
  SIM_RESULT_READY,
  SIM_RESULT_RETRY,   // status not readable yet, or a PIN step is in progress
  SIM_RESULT_LOCKED,  // nothing more to try: PUK, another lock, no pin_code or the PIN is not sent again
  SIM_RESULT_HALT,    // stop this attempt; a reboot may still open the SIM, so the reboot timer stays
};

// The step that failed most recently; CONNECTED clears it
enum class ModemFault : uint8_t {
  MODEM_FAULT_NONE,
  MODEM_FAULT_NO_RESPONSE,
  MODEM_FAULT_BAUD,
  MODEM_FAULT_INIT_AT,
  MODEM_FAULT_SIM_NOT_READY,
  MODEM_FAULT_SIM_LOCKED,
  MODEM_FAULT_PIN_NOT_SENT,
  MODEM_FAULT_PIN_RECORD,
  MODEM_FAULT_SETUP,
  MODEM_FAULT_NOT_ATTACHED,
  MODEM_FAULT_PPP,
  MODEM_FAULT_TEARDOWN,
};

#ifdef USE_MODEM_PIN_CODE
enum class PinStep : uint8_t {
  PIN_STEP_IDLE,
  PIN_STEP_SAVING,  // the send is counted and waits for the record to reach flash
  PIN_STEP_SENT,
};

// Written by loop() once it tried to save pin_record_; the task leaves the record alone while PENDING
enum class PinSave : uint8_t {
  PIN_SAVE_DONE,
  PIN_SAVE_PENDING,
  PIN_SAVE_FAILED,
};
#endif

// Every esp_modem factory shares this signature, so codegen picks the one for the model
using DceFactory = std::unique_ptr<esp_modem::DCE> (*)(const esp_modem::dce_config *, std::shared_ptr<esp_modem::DTE>,
                                                       esp_netif_t *);

class ModemComponent : public Component {
 public:
  ModemComponent(InternalGPIOPin *tx_pin, InternalGPIOPin *rx_pin, const char *model, DceFactory factory,
                 const char *apn);
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI + 1; }  // Just before Wi-Fi

  void set_reboot_timeout(uint32_t timeout) { this->timeout_ = timeout; }
  void set_use_address(const char *use_address) { this->use_address_ = use_address; }
  void set_rts_pin(InternalGPIOPin *rts_pin) { this->rts_pin_ = rts_pin; }
  void set_cts_pin(InternalGPIOPin *cts_pin) { this->cts_pin_ = cts_pin; }
  void set_baud_rate(int baud_rate) { this->baud_rate_ = baud_rate; }
#ifdef USE_MODEM_PIN_CODE
  void set_pin_code(const char *pin_code) { this->pin_code_ = pin_code; }
#endif
  void set_tx_buffer_size(uint16_t tx_buffer_size) { this->tx_buffer_size_ = tx_buffer_size; }
  void set_rx_buffer_size(uint16_t rx_buffer_size) { this->rx_buffer_size_ = rx_buffer_size; }
  void set_dte_buffer_size(uint16_t dte_buffer_size) { this->dte_buffer_size_ = dte_buffer_size; }
  void set_init_at_commands(const char *const *cmds, uint8_t count) {
    this->init_at_commands_ = cmds;
    this->init_at_count_ = count;
  }
#ifdef USE_MODEM_URC
  void set_urc_text_sensor(text_sensor::TextSensor *urc_text_sensor) { this->urc_text_sensor_ = urc_text_sensor; }
#endif
  template<typename F> void add_on_state_callback(F &&callback) {
    this->on_state_callback_.add(std::forward<F>(callback));
  }

  bool is_connected() const { return this->component_state_.load() == ModemState::MODEM_STATE_CONNECTED; }
  bool is_disabled() const {
    return this->component_state_.load() == ModemState::MODEM_STATE_DISABLED &&
           this->target_state_.load() == ModemState::MODEM_STATE_DISABLED;
  }
  bool is_enabled() const { return !this->is_disabled(); }
  void enable();
  void disable();
#ifdef USE_MODEM_SEND_AT
  /// Queue an AT command for the modem task, which sends it at its next pass and logs the result and response.
  /// Returns false when a command is still queued or the command is too long.
  bool send_at(const char *cmd, uint32_t timeout = COMMAND_TIMEOUT_MS);
  bool send_at(const std::string &cmd, uint32_t timeout = COMMAND_TIMEOUT_MS);
#endif

  network::IPAddresses get_ip_addresses();
  const char *get_use_address() const { return this->use_address_; }

 protected:
  static constexpr uint32_t COMMAND_TIMEOUT_MS = 1000;
  static constexpr uint32_t RETRY_DELAY_MS = 1000;
  static constexpr size_t URC_LINE_SIZE = 128;
  static constexpr size_t AT_COMMAND_SIZE = 128;

  // esp_modem's calls block until the modem answers or their own timeouts run out, up to a minute for a
  // mode change, so the state machine runs on its own task and leaves the main loop alone
  static void task_entry(void *arg);
  void run_task_();
  void wake_task_();
  ModemState compute_next_state_();
  ModemState handle_state_enabling_();
  ModemState handle_state_synced_();
  ModemState handle_state_connecting_();
  ModemState handle_state_wait_ip_();
  ModemState handle_state_disconnecting_();
  /// Pause the task before the next pass, then go to state
  ModemState retry_(ModemState state, uint32_t delay_ms = RETRY_DELAY_MS);
  void transition_to_(ModemState next_state);
  // What the task left for loop() to do
  enum Pending : uint32_t {
    PENDING_STATUS = 1 << 0,
    PENDING_SAVE_BAUD = 1 << 1,
#ifdef USE_MODEM_PIN_CODE
    PENDING_SAVE_PIN = 1 << 2,
#endif
    PENDING_CONNECT_TIMEOUT = 1 << 3,  // arm or cancel the reboot timer for the current state
    PENDING_ON_SYNC = 1 << 4,          // loop() wakes the task once on_sync ran
    PENDING_ON_CONNECT = 1 << 5,
    PENDING_ON_DISCONNECT = 1 << 6,
  };

  // Main loop only, so arming and cancelling cannot interleave
  void arm_connect_timeout_();
  void cancel_connect_timeout_();
  void fail_setup_(const LogString *reason);
  void set_fault_(ModemFault fault);
  void clear_config_fault_(ModemFault fault);
  void hand_over_(uint32_t pending);
  /// Send an AT command from the task and log the result; blocks for up to the timeout
  esp_modem::command_result at_(const char *cmd, uint32_t timeout = COMMAND_TIMEOUT_MS);
  void publish_status_();
  void not_responding_(int baud);
  void dump_connect_params_();
  SimResult prepare_sim_();
#ifdef USE_MODEM_PIN_CODE
  SimResult send_pin_();
  void save_pin_record_();
#endif

  InternalGPIOPin *tx_pin_;
  InternalGPIOPin *rx_pin_;
  InternalGPIOPin *rts_pin_{nullptr};
  InternalGPIOPin *cts_pin_{nullptr};
  const char *model_;
  DceFactory dce_factory_;
  const char *apn_;
  const char *const *init_at_commands_{nullptr};  // static array from code generation
#ifdef USE_MODEM_URC
  text_sensor::TextSensor *urc_text_sensor_{nullptr};
  const char *urc_line_{nullptr};  // the line urc_pending_ hands to loop(), inside urc_reader_
#endif
  std::unique_ptr<esp_modem::DCE> dce_;
  std::shared_ptr<esp_modem::DTE> dte_;
  esp_netif_t *ppp_netif_{nullptr};
  ESPPreferenceObject pref_;
  LazyCallbackManager<void(ModemState)> on_state_callback_;
  int baud_rate_{0};  // 0 = use the esp_modem default
  int current_baud_rate_{0};
  int restored_baud_{0};
  StaticTask task_;
  uint32_t timeout_{0};
  uint32_t step_delay_ms_{0};
  uint32_t state_started_ms_{0};  // when the IP wait or the PIN unlock wait began
  std::atomic<uint32_t> pending_{0};
#ifdef USE_MODEM_PIN_CODE
  const char *pin_code_{nullptr};
  ESPPreferenceObject pin_pref_;
#endif
  // Kept in flash so reboots cannot keep spending the SIM's attempts on a PIN it never confirmed
  struct PinRecord {
    uint32_t hash;  // pin_code the record is about
    uint8_t sends;  // AT+CPIN= sent without the SIM reporting READY since
  };
#ifdef USE_MODEM_PIN_CODE
  PinRecord pin_record_{};  // the task writes it, loop() saves it while pin_save_state_ is PENDING
#endif
#ifdef USE_MODEM_SEND_AT
  uint32_t at_timeout_{COMMAND_TIMEOUT_MS};
#endif
  uint16_t tx_buffer_size_{1024};
  uint16_t rx_buffer_size_{1024};
  uint16_t dte_buffer_size_{1024};
  // The task owns component_state_; the main loop sets target_state_ and reads both
  std::atomic<ModemState> component_state_{ModemState::MODEM_STATE_DISABLED};
  std::atomic<ModemState> target_state_{ModemState::MODEM_STATE_DISABLED};
  // Written by the task only, read by loop()
  std::atomic<ModemFault> fault_{ModemFault::MODEM_FAULT_NONE};
  // A failed baud_rate or init_at step, shown once fault_ is clear until the step passes again
  std::atomic<ModemFault> config_fault_{ModemFault::MODEM_FAULT_NONE};
  // std::atomic<uint8_t> flags: GCC on Xtensa calls out of line for std::atomic<bool>
  // Written by the IP event task, read by the modem task
  std::atomic<uint8_t> got_ip_{0};
  std::atomic<uint8_t> ppp_error_{0};  // last NETIF_PPP_STATUS error code, 0 = none
  // Doublings of the silent-modem probe delay; enable() and an answer reset it
  std::atomic<uint8_t> probe_backoff_{0};
  // Set by loop() once on_sync ran; the task waits for it, not for any notification
  std::atomic<uint8_t> sync_acked_{0};
#ifdef USE_MODEM_SEND_AT
  // Set by send_at() on the main loop once at_command_ is written, cleared by the task after sending it
  std::atomic<uint8_t> at_pending_{0};
#endif
#ifdef USE_MODEM_URC
  // Set by the UART task with urc_line_; until loop() has published it, new lines are dropped
  std::atomic<uint8_t> urc_pending_{0};
#endif
#ifdef USE_MODEM_PIN_CODE
  std::atomic<PinSave> pin_save_state_{PinSave::PIN_SAVE_DONE};
  PinStep pin_step_{PinStep::PIN_STEP_IDLE};
#endif
  ConnectStep connect_step_{ConnectStep::CONNECT_STEP_START};
  uint8_t init_at_count_{0};
  uint8_t uart_port_{UART_NUM_MAX};
  uint8_t attach_polls_{0};
  bool connect_timeout_armed_{false};
  bool modem_synced_{false};  // a modem that never answered has nothing to tear down
  bool resync_{false};        // the attachment read failed, so CONNECTING syncs again first
#ifdef USE_MODEM_SEND_AT
  char at_command_[AT_COMMAND_SIZE];
#endif
#ifdef USE_MODEM_URC
  // Fed on the UART task
  LineReader<URC_LINE_SIZE> urc_reader_;
#endif

 private:
  // Points at a string literal from code generation, never at dynamic storage
  const char *use_address_{""};
};

extern ModemComponent *global_modem_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::modem

#endif
