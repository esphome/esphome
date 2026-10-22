#pragma once
#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/preferences.h"
#include "esphome/core/static_task.h"
#include "esphome/components/network/ip_address.h"
#ifdef USE_MODEM_URC
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <cxx_include/esp_modem_api.hpp>
#include <esp_modem_config.h>

#include <array>
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
  SIM_RESULT_RETRY,            // status not readable yet, or the PIN was just sent
  SIM_RESULT_FATAL,            // locked with no usable PIN, or PUK locked
  SIM_RESULT_PIN_REJECTED,     // pin_code was refused, now or on an earlier boot
  SIM_RESULT_PIN_UNCONFIRMED,  // the PIN went out but the SIM stayed locked; nothing said it was wrong
};

enum class SimFault : uint8_t {
  SIM_FAULT_NONE,
  SIM_FAULT_LOCKED,
  SIM_FAULT_PIN_REJECTED,
  SIM_FAULT_PIN_UNCONFIRMED,
};

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
  void set_pin_code(const char *pin_code) { this->pin_code_ = pin_code; }
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
  /// Queue an AT command for the modem task, which sends it at its next pass and logs the result and response.
  /// Returns false when a command is still queued or the command is too long.
  bool send_at(const char *cmd, uint32_t timeout = COMMAND_TIMEOUT_MS);
  bool send_at(const std::string &cmd, uint32_t timeout = COMMAND_TIMEOUT_MS);

  network::IPAddresses get_ip_addresses();
  const char *get_use_address() const { return this->use_address_; }

 protected:
  static constexpr uint32_t COMMAND_TIMEOUT_MS = 1000;
  static constexpr uint32_t RETRY_DELAY_MS = 1000;
  // A silent modem is not going to answer the next second, so its probes are spaced out
  static constexpr uint32_t PROBE_RETRY_DELAY_MS = 5000;
  // How long the task waits for the main loop to run on_sync before attaching
  static constexpr uint32_t SYNC_ACK_TIMEOUT_MS = 5000;
  static constexpr uint32_t SYNC_ACK_POLL_MS = 50;
  // AT+CPIN= can take a few seconds; the SIM may need longer still before AT+CPIN? says READY
  static constexpr uint32_t PIN_TIMEOUT_MS = 5000;
  static constexpr uint32_t PIN_UNLOCK_WAIT_MS = 10000;
  // Sends of one pin_code the SIM never confirmed, across reboots, before it is not sent again
  static constexpr uint8_t PIN_MAX_UNCONFIRMED_SENDS = 2;
  // A rejected PIN that could not be written to flash is retried a few times
  static constexpr uint32_t PIN_SAVE_RETRY_MS = 5000;
  static constexpr uint8_t PIN_SAVE_RETRIES = 3;
  static constexpr size_t URC_LINE_SIZE = 128;
  static constexpr size_t AT_COMMAND_SIZE = 128;
  // State changes the task made since loop() last ran; a burst is a handful of quick steps
  static constexpr uint8_t STATE_QUEUE_SIZE = 8;
  // esp_modem's command strings and the log lines are built on this stack
  static constexpr uint32_t TASK_STACK_SIZE = 5120;

  // esp_modem's calls block until the modem answers or their own timeouts run out, up to a minute for a
  // mode change, so the state machine runs on its own task and leaves the main loop alone
  static void task_entry(void *arg);
  void run_task_();
  void wake_task_();
  ModemState compute_next_state_();
  ModemState handle_state_enabling_();
  ModemState handle_state_connecting_();
  ModemState handle_state_wait_ip_();
  ModemState handle_state_disconnecting_();
  /// Pause the task before the next pass, then go to state
  ModemState retry_(ModemState state, uint32_t delay_ms = RETRY_DELAY_MS);
  void transition_to_(ModemState next_state);
  // One bit per fault source, so a recovery in one path cannot hide another path's fault
  enum Warning : uint8_t {
    WARN_INIT_AT = 1 << 0,
    WARN_BAUD = 1 << 1,
    WARN_TEARDOWN = 1 << 2,
    WARN_NO_RESPONSE = 1 << 3,
    WARN_NOT_ATTACHED = 1 << 4,
    WARN_PPP = 1 << 5,    // PPP did not start or gave no address
    WARN_SIM = 1 << 6,    // no SIM, or its state cannot be read
    WARN_SETUP = 1 << 7,  // the APN or the radio was refused
  };
  // What the task left for loop() to publish
  enum Pending : uint32_t {
    PENDING_STATUS = 1 << 0,
    PENDING_SAVE_BAUD = 1 << 1,
    PENDING_SAVE_PIN = 1 << 2,
    PENDING_CONNECT_TIMEOUT = 1 << 3,  // arm or cancel the reboot timer for the current state
  };

  // Main loop only, so arming and cancelling cannot interleave
  void arm_connect_timeout_();
  void cancel_connect_timeout_();
  void fail_setup_(const LogString *reason);
  void set_warning_(uint8_t bit);
  void clear_warning_(uint8_t bit);
  void set_sim_fault_(SimFault fault);
  void hand_over_(uint32_t pending);
  /// Send an AT command from the task and log the result; blocks for up to the timeout
  bool at_(const char *cmd, uint32_t timeout = COMMAND_TIMEOUT_MS);
  /// Send cmd and keep the whole reply in output, ERROR lines included (dce_->at() drops those)
  esp_modem::command_result command_raw_(const char *cmd, std::string &output, uint32_t timeout);
  void publish_status_();
  void not_responding_(int baud);
  void dump_connect_params_();
  SimResult prepare_sim_();

  InternalGPIOPin *tx_pin_;
  InternalGPIOPin *rx_pin_;
  InternalGPIOPin *rts_pin_{nullptr};
  InternalGPIOPin *cts_pin_{nullptr};
  const char *model_;
  DceFactory dce_factory_;
  const char *apn_;
  const char *pin_code_{nullptr};
  const char *const *init_at_commands_{nullptr};  // static array from code generation
#ifdef USE_MODEM_URC
  text_sensor::TextSensor *urc_text_sensor_{nullptr};
#endif
  std::unique_ptr<esp_modem::DCE> dce_;
  std::shared_ptr<esp_modem::DTE> dte_;
  esp_netif_t *ppp_netif_{nullptr};
  ESPPreferenceObject pref_;
  ESPPreferenceObject pin_pref_;
  LazyCallbackManager<void(ModemState)> on_state_callback_;
  // The task writes a slot, then advances the head; loop() owns the tail
  std::array<ModemState, STATE_QUEUE_SIZE> state_queue_{};
  std::atomic<uint8_t> state_head_{0};
  std::atomic<uint8_t> state_tail_{0};
  int baud_rate_{0};  // 0 = use the esp_modem default
  int current_baud_rate_{0};
  int restored_baud_{0};
  StaticTask task_;
  uint32_t timeout_{0};
  uint32_t step_delay_ms_{0};
  uint32_t wait_ip_start_{0};
  uint32_t synced_at_{0};
  uint32_t pin_sent_at_{0};
  // What happened to the configured pin_code, kept in flash so a reboot cannot spend another SIM attempt
  struct PinRecord {
    uint32_t hash;  // pin_code the record is about
    uint8_t sends;  // AT+CPIN= sent without the SIM reporting READY since
    bool rejected;  // the SIM refused it
  } pin_record_{};
  uint32_t at_timeout_{COMMAND_TIMEOUT_MS};
  std::atomic<uint32_t> pending_{0};
  uint16_t tx_buffer_size_{1024};
  uint16_t rx_buffer_size_{1024};
  uint16_t dte_buffer_size_{1024};
  // The task owns component_state_; the main loop sets target_state_ and reads both
  std::atomic<ModemState> component_state_{ModemState::MODEM_STATE_DISABLED};
  std::atomic<ModemState> target_state_{ModemState::MODEM_STATE_DISABLED};
  std::atomic<SimFault> sim_fault_{SimFault::SIM_FAULT_NONE};
  // Written by the task only; loop() picks the published message from it
  std::atomic<uint8_t> warnings_{0};
  // std::atomic<uint8_t> flags: GCC on Xtensa calls out of line for std::atomic<bool>
  // Set by send_at() on the main loop once at_command_ is written, cleared by the task after sending it
  std::atomic<uint8_t> at_pending_{0};
  // Written by the IP event task, read by the modem task
  std::atomic<uint8_t> got_ip_{0};
  std::atomic<uint8_t> ppp_error_{0};  // last NETIF_PPP_STATUS error code, 0 = none
  ConnectStep connect_step_{ConnectStep::CONNECT_STEP_START};
  uint8_t init_at_count_{0};
  uint8_t uart_port_{UART_NUM_MAX};
  uint8_t attach_polls_{0};
  uint8_t pin_save_retries_{0};
  // One PIN attempt per connection attempt: a second wrong PIN could PUK-lock the SIM
  bool pin_sent_{false};
  bool connect_timeout_armed_{false};
  bool modem_synced_{false};  // a modem that never answered has nothing to tear down
  bool cme_numeric_{false};   // AT+CMEE=1 accepted, so a wrong PIN answers +CME ERROR: 16
  char at_command_[AT_COMMAND_SIZE];
#ifdef USE_MODEM_URC
  // The UART task assembles a line in urc_buf_ and hands it to loop() by setting urc_pending_; until loop()
  // has published it, new lines are dropped
  std::atomic<uint8_t> urc_pending_{0};
  uint8_t urc_len_{0};
  bool urc_discard_{false};  // drop the rest of the current line
  char urc_buf_[URC_LINE_SIZE];
#endif

 private:
  // Points at a string literal from code generation, never at dynamic storage
  const char *use_address_{""};
};

extern ModemComponent *global_modem_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::modem

#endif
