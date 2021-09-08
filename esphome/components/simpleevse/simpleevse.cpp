#include "simplesevse.h"
#include "esphome/core/log.h"

namespace esphome {
namespace simpleevse {

const char * const TAG = "SimpleEVSE";

void SimpleEvseComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SimpleEvse:");
  this->check_uart_settings(BAUD_RATE, STOP_BITS, uart::UARTParityOptions::UART_CONFIG_PARITY_NONE, PARITY_BITS);
  LOG_UPDATE_INTERVAL(this);
}

void SimpleEvseComponent::add_transaction(std::unique_ptr<ModbusTransaction> &&transaction) {
  if (this->running_) {
    this->transactions_.push(std::move(transaction));
  } else {
    ESP_LOGW(TAG, "Cancelled transaction because connection is not running.");
    transaction->cancel();
  }
}

void SimpleEvseComponent::on_status_received_(ModbusTransactionResult result, const std::vector<uint16_t> &reg) {
  if (result == ModbusTransactionResult::SUCCESS) {
    if (reg.size() == COUNT_STATUS_REGISTER) {
      std::copy(reg.cbegin(), reg.cend(), this->status_register_.begin());
      this->running_ = true;
      this->status_clear_error();
      this->process_triggers_();
    } else {
      ESP_LOGW(TAG, "Invalid number of register: expected %d, got %zu", COUNT_STATUS_REGISTER, reg.size());
      this->running_ = false;
      this->status_set_error();
    }
  } else {
    this->status_set_error();
    if (this->running_) {
      this->running_ = false;

      ESP_LOGW(TAG, "No connection to EVSE - reset pending transactions (size=%zu).", this->transactions_.size());
      while (!this->transactions_.empty()) {
        auto transaction = std::move(this->transactions_.front());
        this->transactions_.pop();
        transaction->cancel();
      }
    }
  }

  for (auto it = std::begin(this->observer_); it != std::end(this->observer_); ++it) {
    (*it)->update(this->running_, this->status_register_);
  }
}

void SimpleEvseComponent::process_triggers_() {
  uint16_t vehicle_state = this->status_register_[REGISTER_VEHICLE_STATE];

  if (!this->was_plugged_ &&
      (vehicle_state == VehicleState::VEHICLE_EV_PRESENT || vehicle_state == VehicleState::VEHICLE_CHARGING ||
       vehicle_state == VehicleState::VEHICLE_CHARGING_WITH_VENT)) {
    // vehicle is now plugged
    this->was_plugged_ = true;

    if (this->unplugged_trigger_ && this->unplugged_trigger_->is_action_running()) {
      this->unplugged_trigger_->stop_action();
    }

    if (this->plugged_trigger_) {
      this->plugged_trigger_->trigger();
    }
  } else if (this->was_plugged_ && vehicle_state == VehicleState::VEHICLE_READY) {
    // vehicle is now unplugged
    this->was_plugged_ = false;

    if (this->plugged_trigger_ && this->plugged_trigger_->is_action_running()) {
      this->plugged_trigger_->stop_action();
    }

    if (this->unplugged_trigger_) {
      this->unplugged_trigger_->trigger();
    }
  }
}

void SimpleEvseComponent::idle() {
  const uint32_t now = millis();

  // take next queued transaction and execute it
  if (!transactions_.empty()) {
    ESP_LOGD(TAG, "Executed next queued transaction (size=%zu).", this->transactions_.size());
    auto transaction = std::move(this->transactions_.front());
    transactions_.pop();
    this->execute_(std::move(transaction));

    // force update of status because the transaction might change it
    this->force_update_ = true;
    return;
  }

  // poll status
  if (this->force_update_ || (now - this->last_state_udpate_) >= this->update_interval_) {
    this->force_update_ = false;
    this->last_state_udpate_ = now;
    auto status = make_unique<ModbusReadHoldingRegistersTransaction>(
        FIRST_STATUS_REGISTER, COUNT_STATUS_REGISTER,
        std::bind(&SimpleEvseComponent::on_status_received_, this, std::placeholders::_1, std::placeholders::_2));

    this->execute_(std::move(status));
    return;
  }
}

}  // namespace simpleevse
}  // namespace esphome
