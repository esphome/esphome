#include "vbus_transactions.h"

#ifdef USE_VBUS_TRANSACTIONS
#include "vbus.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome::vbus {

ESPHOME_LOG_TAG(TAG, "vbus.transactions");
static constexpr uint32_t FRAME_TIME_MS = 20;

void VBusTransactions::setup() {
  this->receiver_->add_on_datagram_callback(
      [this](uint16_t command, uint16_t source, uint16_t destination, uint16_t value_id, uint32_t value) {
        this->on_datagram_(command, source, destination, value_id, value);
      });
  this->disable_loop();
}

void VBusTransactions::dump_config() {
  ESP_LOGCONFIG(TAG, "VBus transactions:\n  Response timeout: %" PRIu32 " ms", this->response_timeout_ms_);
}

bool VBusTransactions::write_value(uint32_t grant_id, uint16_t value_id, uint32_t value, uint16_t command) {
  if (command != 0x0200 && command != 0x0400) {
    ESP_LOGW(TAG, "Transaction rejected: unsupported write command");
    return false;
  }
  return this->start_(grant_id, command, value_id, value);
}

bool VBusTransactions::start_(uint32_t grant_id, uint16_t command, uint16_t value_id, uint32_t value) {
  if (this->stopped_ || this->is_busy() || !this->transmitter_->ready_to_send(grant_id)) {
    ESP_LOGW(TAG, "Transaction rejected: pending request or no usable lease");
    return false;
  }
  if (!this->transmitter_->send_datagram(grant_id, command, value_id, value))
    return false;
  this->grant_id_ = grant_id;
  this->command_ = command;
  this->value_id_ = value_id;
  this->requested_value_ = value;
  this->phase_at_ = App.get_loop_component_start_time();
  this->phase_ = command == 0x0300 ? Phase::PHASE_READ : Phase::PHASE_WRITE_ACK;
  this->enable_loop();
  return true;
}

void VBusTransactions::finish_(TransactionStatus status, uint32_t value) {
  if (status != TransactionStatus::TRANSACTION_STATUS_SUCCESS)
    this->transmitter_->release_bus(this->grant_id_);
  this->phase_ = Phase::PHASE_IDLE;
  this->disable_loop();
  // Copy the result before a callback can start the next transaction.
  const uint32_t grant_id = this->grant_id_;
  const uint16_t command = this->command_;
  const uint16_t value_id = this->value_id_;
  this->result_callback_.call(grant_id, command, value_id, value, status);
}

void VBusTransactions::on_datagram_(uint16_t command, uint16_t source, uint16_t destination, uint16_t value_id,
                                    uint32_t value) {
  if (!this->is_busy() || this->phase_ == Phase::PHASE_VERIFY_SEND || command != 0x0100 ||
      source != this->transmitter_->get_destination() || destination != this->transmitter_->get_source() ||
      value_id != this->value_id_ || !this->transmitter_->owns_bus(this->grant_id_))
    return;
  const uint32_t elapsed = App.get_loop_component_start_time() - this->phase_at_;
  if (elapsed >= this->response_timeout_ms_) {
    this->finish_(TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
    return;
  }
  if (elapsed < FRAME_TIME_MS)
    return;
  if (this->phase_ == Phase::PHASE_WRITE_ACK) {
    this->phase_ = Phase::PHASE_VERIFY_SEND;
    this->phase_at_ = App.get_loop_component_start_time();
  } else {
    this->finish_(this->phase_ == Phase::PHASE_VERIFY_REPLY && value != this->requested_value_
                      ? TransactionStatus::TRANSACTION_STATUS_VERIFY_MISMATCH
                      : TransactionStatus::TRANSACTION_STATUS_SUCCESS,
                  value);
  }
}

void VBusTransactions::loop() {
  if (!this->is_busy())
    return;
  if (!this->transmitter_->owns_bus(this->grant_id_)) {
    this->finish_(TransactionStatus::TRANSACTION_STATUS_BUS_LOST);
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->phase_at_ >= this->response_timeout_ms_) {
    this->finish_(TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
    return;
  }
  if (this->phase_ == Phase::PHASE_VERIFY_SEND) {
    if (!this->transmitter_->has_bus()) {
      this->finish_(TransactionStatus::TRANSACTION_STATUS_BUS_LOST);
    } else if (this->transmitter_->ready_to_send(this->grant_id_) &&
               this->transmitter_->send_datagram(this->grant_id_, 0x0300, this->value_id_, 0)) {
      this->phase_ = Phase::PHASE_VERIFY_REPLY;
      this->phase_at_ = now;
    }
  }
}

void VBusTransactions::cancel() {
  if (this->is_busy())
    this->finish_(TransactionStatus::TRANSACTION_STATUS_CANCELLED);
}

void VBusTransactions::on_shutdown() {
  this->stopped_ = true;
  this->phase_ = Phase::PHASE_IDLE;
  this->disable_loop();
}

}  // namespace esphome::vbus
#endif
