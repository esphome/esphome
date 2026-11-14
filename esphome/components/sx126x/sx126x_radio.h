#pragma once

#include "radio_profile.h"
#include "sx126x_commands.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <span>
#include <vector>

namespace esphome::sx126x {

enum class SX126xError {
  SX126X_ERROR_NONE = 0,
  SX126X_ERROR_TIMEOUT,
  SX126X_ERROR_INVALID_PARAMS,
  SX126X_ERROR_NOT_READY,
  SX126X_ERROR_BUSY_TIMEOUT,
  SX126X_ERROR_BUSY,
  SX126X_ERROR_CANCELLED
};

class SX126xTransmitListener {
 public:
  virtual void on_transmit_complete(SX126xError result) = 0;
};

// Who may use the radio between operations. This is driver state, not the chip's mode.
enum class RadioState : uint8_t {
  // Startup, cold sleep, or a failed configuration, mode command or restoration. Only
  // configure() is accepted; sends return NOT_READY and the loop sleeps. SX126x configures
  // again every second after a failure, and on the next mode command after cold sleep.
  RADIO_STATE_NEEDS_CONFIGURATION,
  // Available. The chip is receiving or sleeping, as configured, or after the TX mode command
  // sending whatever the FIFO holds. That send is not tracked: a send or mode command may cut
  // it short, and poll() clears its TX_DONE as a flag without a packet.
  RADIO_STATE_IDLE,
  // A send is on air. SPI is free between polls; only polling and cancellation are accepted.
  RADIO_STATE_TRANSMITTING,
  // Standby with a received packet left in the FIFO, found when a send entered standby.
  // The send was rejected with BUSY; the next poll() delivers the packet and restores the
  // radio. configure() may discard it to recover from repeated read failures.
  RADIO_STATE_HOLDING_RX,
};

enum class ModeCommand : uint8_t {
  MODE_COMMAND_RX,
  MODE_COMMAND_TX,
  MODE_COMMAND_STANDBY,
  MODE_COMMAND_SLEEP,
  MODE_COMMAND_COLD_SLEEP,
  MODE_COMMAND_IMAGE_CAL,
};

// What a radio operation leaves for its caller to do. The radio has already committed its
// state; the caller reports status first, then calls the listener or delivers the packet.
struct RadioEffects {
  SX126xError result{SX126xError::SX126X_ERROR_NONE};
  // The completion to notify when finished is set; null for a blocking send.
  SX126xTransmitListener *listener{nullptr};
  // With report set: the warning to set, or null to clear it.
  const LogString *warning{nullptr};
  float rssi{0};
  float snr{0};
  bool report{false};
  // A send ended, and result is its outcome.
  bool finished{false};
  // The packet buffer holds a packet to deliver.
  bool received{false};
  // Nothing to poll until the next DIO1 edge or configuration.
  bool sleep_loop{false};
};

// Owns the radio state and every SPI command sequence.
//
// Status reports, completions and packet listeners can run user code, which may call back
// into the driver. Every operation therefore does its SPI work with busy() set, commits the
// next state, and returns RadioEffects; SX126x::apply_() then reports status and calls the
// listeners. This class has no access to component status or listeners, so callbacks only
// ever see a committed state. Do not log here either: log listeners are user code. busy()
// covers re-entry during a sequence, for example from a GPIO pin that logs.
//
// Each sequence starts with bus().begin(); after a BUSY timeout the rest is skipped and
// bus().ok() is false. A failed send preparation restores the radio in a new sequence. A
// failed restoration needs configuration, and its result overrides the send result. A
// successful send start reports nothing, so no user code runs before the caller has the
// result. Every send ends in finish_transmit_(), exactly once.
//
// A failed receive read keeps the packet and retries after RECEIVE_RETRY_INTERVAL_MS, so a
// stuck BUSY line does not cost a 20 ms wait on every loop.
class SX126xRadio {
 public:
  SX126xRadio() = default;
  SX126xRadio(const SX126xRadio &) = delete;
  SX126xRadio &operator=(const SX126xRadio &) = delete;
  SX126xSpiDevice &bus() { return this->commands_.bus(); }
  void set_dio1_pin(GPIOPin *pin) { this->dio1_pin_ = pin; }
  void set_rst_pin(GPIOPin *pin) { this->rst_pin_ = pin; }
  GPIOPin *get_dio1_pin() const { return this->dio1_pin_; }
  GPIOPin *get_rst_pin() const { return this->rst_pin_; }
  RadioState state() const { return this->state_; }
  bool busy() const { return this->busy_; }
  bool is_transmitting() const { return this->state_ == RadioState::RADIO_STATE_TRANSMITTING; }
  // A held packet may be discarded: explicit reconfiguration is the recovery path.
  bool can_configure() const { return !this->busy_ && !this->is_transmitting(); }
  bool can_change_mode() const { return !this->busy_ && this->state_ == RadioState::RADIO_STATE_IDLE; }
  size_t max_packet_size() const { return this->applied_.max_packet_size(); }

  // Requires can_configure(). Resets the chip and applies the requested profile.
  RadioEffects configure(const RadioProfile &requested, std::span<char, 16> version);
  // Requires can_change_mode(). Cold sleep and failed commands need configuration afterwards.
  RadioEffects change_mode(ModeCommand command, uint8_t arg);
  // Result SX126X_ERROR_NONE starts a send that finishes exactly once, through poll_transmit() or
  // cancel_transmit(). Any other result owes no completion.
  RadioEffects start_transmit(SX126xTransmitListener *listener, const uint8_t *data, size_t len, uint32_t timeout_ms);
  // Sets finished only when it ended the current send.
  RadioEffects poll_transmit(uint32_t now);
  // Sets finished only if listener owns the current send.
  RadioEffects cancel_transmit(SX126xTransmitListener *listener);
  // One loop step: polls a send, or reads a received packet into packet.
  RadioEffects poll(uint32_t now, std::vector<uint8_t> &packet);

 private:
  struct TxOperation {
    SX126xTransmitListener *listener{nullptr};
    uint32_t started_ms{0};
    uint32_t timeout_ms{0};
  };
  void commit_(RadioState state) {
    this->state_ = state;
    this->busy_ = false;
  }
  static RadioEffects comm_report(bool ok);
  const LogString *apply_configuration_(const RadioProfile &requested, std::span<char, 16> version);
  bool restore_();
  RadioEffects finish_transmit_(SX126xError result);
  RadioEffects receive_(bool holding, uint32_t now, std::vector<uint8_t> &packet);
  SX126xCommands commands_;
  AppliedProfile applied_;
  TxOperation tx_;
  uint32_t rx_retry_started_ms_{0};
  uint16_t pending_irq_{0};  // flags of a received packet whose read failed after they were cleared
  GPIOPin *dio1_pin_{nullptr};
  GPIOPin *rst_pin_{nullptr};
  RadioState state_{RadioState::RADIO_STATE_NEEDS_CONFIGURATION};
  bool busy_{false};
  bool rx_retry_pending_{false};
};

}  // namespace esphome::sx126x
