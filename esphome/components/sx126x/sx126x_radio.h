#pragma once

#include "radio_profile.h"
#include "sx126x_commands.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <span>
#include <vector>

namespace esphome::sx126x {

enum class SX126xError {
  NONE = 0,
  TIMEOUT,
  INVALID_PARAMS,
  SX126X_ERROR_NOT_READY,
  SX126X_ERROR_BUSY_TIMEOUT,
  SX126X_ERROR_BUSY,
  SX126X_ERROR_CANCELLED
};

class SX126xTransmitListener {
 public:
  virtual void on_transmit_complete(SX126xError result) = 0;
};

enum class RadioState : uint8_t {
  RADIO_STATE_NEEDS_CONFIGURATION,
  RADIO_STATE_IDLE,
  RADIO_STATE_TRANSMITTING,
  // Standby with a received packet left in the FIFO for the next poll() to deliver.
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
  SX126xError result{SX126xError::NONE};
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

// Owns the radio state and every SPI command sequence. It has no access to component status
// or to listeners, so no user code can run while a sequence or state change is in progress;
// each operation returns RadioEffects instead. Do not log here either: log listeners are user
// code. busy() is true only during a sequence, for re-entry from GPIO pin implementations.
class Sx126xRadio {
 public:
  Sx126xRadio() = default;
  Sx126xRadio(const Sx126xRadio &) = delete;
  Sx126xRadio &operator=(const Sx126xRadio &) = delete;
  Sx126xSpiDevice &bus() { return this->commands_.bus(); }
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
  // Result NONE starts a send that finishes exactly once, through poll_transmit() or
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
  static RadioEffects comm_report_(bool ok);
  const LogString *apply_configuration_(const RadioProfile &requested, std::span<char, 16> version);
  bool restore_();
  RadioEffects finish_transmit_(SX126xError result);
  RadioEffects receive_(bool holding, uint32_t now, std::vector<uint8_t> &packet);
  Sx126xCommands commands_;
  AppliedProfile applied_;
  TxOperation tx_;
  uint32_t rx_retry_started_ms_{0};
  GPIOPin *dio1_pin_{nullptr};
  GPIOPin *rst_pin_{nullptr};
  RadioState state_{RadioState::RADIO_STATE_NEEDS_CONFIGURATION};
  bool busy_{false};
  bool rx_retry_pending_{false};
};

}  // namespace esphome::sx126x
