#pragma once

#include "esphome/core/defines.h"

#ifdef USE_VBUS_TRANSACTIONS
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::vbus {

class VBus;
class VBusTransmitter;

enum class TransactionStatus : uint8_t {
  TRANSACTION_STATUS_SUCCESS,
  TRANSACTION_STATUS_TIMEOUT,
  TRANSACTION_STATUS_BUS_LOST,
  TRANSACTION_STATUS_VERIFY_MISMATCH,
};

class VBusTransactions final : public Component {
 public:
  VBusTransactions(VBus *receiver, VBusTransmitter *transmitter) : receiver_(receiver), transmitter_(transmitter) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  void set_response_timeout(uint32_t timeout) { this->response_timeout_ms_ = timeout; }
  bool read_value(uint32_t grant_id, uint16_t value_id) { return this->start_(grant_id, 0x0300, value_id, 0); }
  bool write_value(uint32_t grant_id, uint16_t value_id, uint32_t value, uint16_t command = 0x0200);
  bool is_busy() const { return this->phase_ != Phase::PHASE_IDLE; }
  template<typename F> void add_on_result_callback(F &&callback) {
    this->result_callback_.add(std::forward<F>(callback));
  }

 protected:
  enum class Phase : uint8_t { PHASE_IDLE, PHASE_READ, PHASE_WRITE_ACK, PHASE_VERIFY_SEND, PHASE_VERIFY_REPLY };
  bool start_(uint32_t grant_id, uint16_t command, uint16_t value_id, uint32_t value);
  void on_datagram_(uint16_t command, uint16_t source, uint16_t destination, uint16_t value_id, uint32_t value);
  void finish_(TransactionStatus status, uint32_t value = 0);

  VBus *receiver_;
  VBusTransmitter *transmitter_;
  uint32_t response_timeout_ms_{100};
  uint32_t phase_at_{0};
  uint32_t grant_id_{0};
  uint32_t requested_value_{0};
  uint16_t command_{0};
  uint16_t value_id_{0};
  Phase phase_{Phase::PHASE_IDLE};
  bool stopped_{false};
  CallbackManager<void(uint32_t, uint16_t, uint16_t, uint32_t, TransactionStatus)> result_callback_;
};

}  // namespace esphome::vbus
#endif
