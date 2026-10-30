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

using message_parser_t = std::function<float(std::vector<uint8_t> &)>;

class VBus;

class VBusListener {
 public:
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
#ifdef USE_VBUS_TX
  void set_transmitter(VBusTransmitter *transmitter) { this->transmitter_ = transmitter; }
  bool request_bus() {
#ifdef USE_VBUS_POLLING
    if (this->polling_ != nullptr)
      return false;
#endif
    return this->transmitter_ != nullptr && this->transmitter_->request_bus();
  }
  bool has_bus() const { return this->transmitter_ != nullptr && this->transmitter_->has_bus(); }
  bool send_datagram(uint32_t grant_id, uint16_t command, uint16_t value_id, uint32_t value, bool expect_reply = true) {
#ifdef USE_VBUS_TRANSACTIONS
    if (this->transactions_ != nullptr && this->transactions_->is_busy())
      return false;
#endif
#ifdef USE_VBUS_POLLING
    if (this->polling_ != nullptr)
      return false;
#endif
    return this->transmitter_ != nullptr &&
           this->transmitter_->send_datagram(grant_id, command, value_id, value, expect_reply);
  }
  void release_bus(uint32_t grant_id) {
#ifdef USE_VBUS_POLLING
    if (this->polling_ != nullptr)
      return;
#endif
    if (this->transmitter_ != nullptr)
      this->transmitter_->release_bus(grant_id);
  }
#endif
#ifdef USE_VBUS_TRANSACTIONS
  void set_transactions(VBusTransactions *transactions) { this->transactions_ = transactions; }
  bool read_value(uint32_t grant_id, uint16_t value_id) {
#ifdef USE_VBUS_POLLING
    if (this->polling_ != nullptr)
      return false;
#endif
    return this->transactions_ != nullptr && this->transactions_->read_value(grant_id, value_id);
  }
  bool write_value(uint32_t grant_id, uint16_t value_id, uint32_t value, uint16_t command = 0x0200) {
#ifdef USE_VBUS_POLLING
    if (this->polling_ != nullptr)
      return false;
#endif
    return this->transactions_ != nullptr && this->transactions_->write_value(grant_id, value_id, value, command);
  }
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
