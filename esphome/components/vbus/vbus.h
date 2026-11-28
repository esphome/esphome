#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_VBUS_TX
#include "vbus_tx.h"
#endif
#ifdef USE_VBUS_TRANSACTIONS
#include "vbus_transactions.h"
#endif
#ifdef USE_VBUS_POLLING
#include "vbus_polling.h"
#endif
#ifdef USE_VBUS_DATAGRAM
#include "esphome/core/helpers.h"
#endif

namespace esphome::vbus {

class VBus;

class VBusListener {
 public:
  // Called from the hub's dump_config(); not dump_config() so Component children don't print twice
  virtual void dump_child_config() {}
  void set_command(uint16_t command) { this->command_ = command; }
  void set_source(uint16_t source) { this->source_ = source; }
  void set_dest(uint16_t dest) { this->dest_ = dest; }

  void on_message(uint16_t command, uint16_t source, uint16_t dest, std::vector<uint8_t> &message);

 protected:
  uint16_t command_{0xffff};
  uint16_t source_{0xffff};
  uint16_t dest_{0xffff};

  virtual void handle_message(std::vector<uint8_t> &message) = 0;
};

class VBus final : public uart::UARTDevice, public Component {
 public:
  void dump_config() override;
  void loop() override;

  void register_listener(VBusListener *listener) { this->listeners_.push_back(listener); }
#ifdef USE_VBUS_RX_DIAGNOSTICS
  uint32_t get_packet_error_count() const { return this->packet_error_count_; }
#endif
#ifdef USE_VBUS_TX
  void set_transmitter(VBusTransmitter *transmitter) { this->transmitter_ = transmitter; }
#ifdef USE_VBUS_SEND_ACTION
  bool request_datagram(uint16_t command, uint16_t value_id, uint32_t value, bool expect_reply = true);
#endif
#endif
#ifdef USE_VBUS_TRANSACTIONS
  void set_transactions(VBusTransactions *transactions) { this->transactions_ = transactions; }
#ifdef USE_VBUS_TRANSACTION_ACTION
  bool request_read_value(uint16_t value_id) {
    return this->manual_tx_allowed_() && this->transactions_ != nullptr &&
           this->transactions_->request_value(0x0300, value_id, 0);
  }
  bool request_write_value(uint16_t value_id, uint32_t value, uint16_t command = 0x0200) {
    if (command != 0x0200 && command != 0x0400)
      return false;
    return this->manual_tx_allowed_() && this->transactions_ != nullptr &&
           this->transactions_->request_value(command, value_id, value);
  }
#endif
#endif
#ifdef USE_VBUS_POLLING
  void set_polling(VBusPolling *polling) { this->polling_ = polling; }
  void suspend_polling() {
    if (this->polling_ != nullptr)
      this->polling_->suspend();
  }
  void resume_polling() {
    if (this->polling_ != nullptr)
      this->polling_->resume();
  }
  bool queue_read(uint16_t value_id) {
    return this->polling_ != nullptr && this->polling_->accepts_manual_requests() &&
           this->polling_->queue_read(value_id);
  }
  bool queue_write(uint16_t value_id, uint32_t value, uint16_t command = 0x0200) {
    return this->polling_ != nullptr && this->polling_->accepts_manual_requests() &&
           this->polling_->queue_write(value_id, value, command);
  }
#endif
#ifdef USE_VBUS_DATAGRAM
  template<typename F> void add_on_datagram_callback(F &&callback) {
    this->datagram_callback_.add(std::forward<F>(callback));
  }
#endif

 protected:
#if defined(USE_VBUS_SEND_ACTION) || defined(USE_VBUS_TRANSACTION_ACTION)
  bool manual_tx_allowed_() const;
#endif
#ifdef USE_VBUS_TX
  friend class VBusTransmitter;
  VBusTransmitter *transmitter_{nullptr};
#endif
#ifdef USE_VBUS_TRANSACTIONS
  VBusTransactions *transactions_{nullptr};
#endif
#ifdef USE_VBUS_POLLING
  VBusPolling *polling_{nullptr};
#endif
  int state_{0};
#ifdef USE_VBUS_RX_DIAGNOSTICS
  uint32_t packet_error_count_{0};
#endif
  uint32_t last_rx_ms_{0};
  std::vector<uint8_t> buffer_;
  uint8_t protocol_;
  uint16_t source_;
  uint16_t dest_;
  uint16_t command_;
  uint8_t frames_;
  uint8_t cframe_;
  uint8_t fbytes_[6];
  int fbcount_;
  std::vector<VBusListener *> listeners_{};
#ifdef USE_VBUS_DATAGRAM
  CallbackManager<void(uint16_t, uint16_t, uint16_t, uint16_t, uint32_t)> datagram_callback_;
#endif
};

}  // namespace esphome::vbus
