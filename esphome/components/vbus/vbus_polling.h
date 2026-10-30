#pragma once

#include "esphome/core/defines.h"

#ifdef USE_VBUS_POLLING
#include "vbus_transactions.h"
#include "vbus_tx.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::vbus {

class VBusPolling final : public PollingComponent {
 public:
  VBusPolling(VBusTransactions *transactions, VBusTransmitter *transmitter)
      : transactions_(transactions), transmitter_(transmitter) {}
#ifdef USE_VBUS_CS_PLUS
  VBusPolling(VBusTransactions *transactions, VBusTransmitter *transmitter, bool automatic)
      : transactions_(transactions), transmitter_(transmitter), automatic_(automatic) {}
#endif
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  void suspend();
  void resume();
  bool is_suspended() const { return this->suspended_; }
  bool accepts_manual_requests() const {
#ifdef USE_VBUS_CS_PLUS
    return this->automatic_;
#else
    return true;
#endif
  }
#ifdef USE_VBUS_CS_PLUS
  void clear_pending_reads();
  void resume_group();
  template<typename F> void add_on_group_end_callback(F &&callback) {
    this->group_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_suspended_callback(F &&callback) {
    this->suspended_callback_.add(std::forward<F>(callback));
  }
#endif
  void set_batch_size(uint8_t size) { this->batch_size_ = size; }
  void register_value(uint16_t value_id, bool rx_only);
  bool queue_read(uint16_t value_id);
  bool queue_write(uint16_t value_id, uint32_t value, uint16_t command = 0x0200);
  template<typename F> void add_on_value_callback(F &&callback) {
    this->value_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_result_callback(F &&callback) {
    this->result_callback_.add(std::forward<F>(callback));
  }

 protected:
  struct Value {
    uint32_t value;
    uint16_t id;
    bool rx_only;
    bool pending;
    bool valid;
  };
  struct Request {
    uint32_t value;
    uint32_t queued_at;
    uint16_t id;
    uint16_t command;
  };
  Value *find_value_(uint16_t value_id);
  bool pending_() const;
  void on_result_(uint32_t grant_id, uint16_t command, uint16_t value_id, uint32_t value, TransactionStatus status);
  void release_(bool group_end = false);

  VBusTransactions *transactions_;
  VBusTransmitter *transmitter_;
  StaticVector<Value, VBUS_POLL_VALUE_COUNT> values_;
  StaticRingBuffer<Request, 16> writes_;
  Request active_{};
  uint32_t grant_id_{0};
  uint32_t waiting_at_{0};
  size_t next_read_{0};
  uint8_t batch_size_{16};
  uint8_t reads_{0};
  bool in_flight_{false};
  bool waiting_{false};
  bool stopped_{false};
  bool suspended_{false};
#ifdef USE_VBUS_CS_PLUS
  bool automatic_{true};
  bool held_{false};
#endif
  CallbackManager<void(uint16_t, uint32_t)> value_callback_;
  CallbackManager<void(uint32_t, uint16_t, uint16_t, uint32_t, TransactionStatus)> result_callback_;
#ifdef USE_VBUS_CS_PLUS
  CallbackManager<void(bool)> suspended_callback_;
  CallbackManager<void()> group_callback_;
#endif
};

}  // namespace esphome::vbus
#endif
