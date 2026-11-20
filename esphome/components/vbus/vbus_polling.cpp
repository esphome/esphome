#include "vbus_polling.h"

#ifdef USE_VBUS_POLLING
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome::vbus {

ESPHOME_LOG_TAG(TAG, "vbus.polling");
static constexpr uint32_t QUEUE_TIMEOUT_MS = 10000;

void VBusPolling::register_value(uint16_t value_id, bool rx_only) {
  if (this->find_value_(value_id) == nullptr)
    this->values_.push_back({0, value_id, rx_only, false, false});
}

VBusPolling::Value *VBusPolling::find_value_(uint16_t value_id) {
  for (auto &entry : this->values_) {
    if (entry.id == value_id)
      return &entry;
  }
  return nullptr;
}

void VBusPolling::setup() {
  this->transmitter_->add_on_bus_grant_callback([this](uint32_t grant_id) {
    if (!this->stopped_ && this->waiting_) {
      this->grant_id_ = grant_id;
      this->reads_ = 0;
      this->waiting_ = false;
    }
  });
  this->transactions_->add_on_result_callback(
      [this](uint32_t grant_id, uint16_t command, uint16_t value_id, uint32_t value, TransactionStatus status) {
        this->on_result_(grant_id, command, value_id, value, status);
      });
  this->disable_loop();
  this->update();
}

void VBusPolling::dump_config() {
  ESP_LOGCONFIG(TAG, "VBus polling:\n  Read batch: %u\n  Write queue: 16", this->batch_size_);
  LOG_UPDATE_INTERVAL(this);
}

void VBusPolling::update() {
  if (this->stopped_ || this->suspended_ || !this->accepts_manual_requests())
    return;
  for (auto &entry : this->values_) {
    if (!entry.rx_only && (!this->in_flight_ || this->active_.command != 0x0300 || this->active_.id != entry.id))
      entry.pending = true;
  }
  if (this->pending_())
    this->enable_loop();
}

bool VBusPolling::queue_read(uint16_t value_id) {
  auto *entry = this->find_value_(value_id);
  if (this->stopped_ || this->suspended_ || entry == nullptr || entry->rx_only) {
    ESP_LOGW(TAG, "Read %u rejected: stopped/suspended, unregistered or RX-only value", value_id);
    return false;
  }
  if (!this->in_flight_ || this->active_.command != 0x0300 || this->active_.id != value_id)
    entry->pending = true;
  this->enable_loop();
  return true;
}

bool VBusPolling::queue_write(uint16_t value_id, uint32_t value, uint16_t command) {
  if (this->stopped_ || this->suspended_ || (command != 0x0200 && command != 0x0400) ||
      !this->writes_.push({value, App.get_loop_component_start_time(), value_id, command})) {
    ESP_LOGW(TAG, "Write rejected: stopped/suspended, invalid command or full queue");
    return false;
  }
  this->enable_loop();
  return true;
}

bool VBusPolling::pending_() const {
  return !this->writes_.empty() ||
         std::any_of(this->values_.begin(), this->values_.end(), [](const Value &entry) { return entry.pending; });
}

void VBusPolling::release_([[maybe_unused]] bool group_end) {
#ifdef USE_VBUS_CS_PLUS
  const bool notify = group_end && !this->automatic_ && this->grant_id_ != 0;
#endif
  if (this->grant_id_ != 0)
    this->transmitter_->release_bus(this->grant_id_);
  this->grant_id_ = 0;
#ifdef USE_VBUS_CS_PLUS
  if (notify) {
    this->held_ = true;
    this->group_callback_.call();
  }
#endif
}

void VBusPolling::on_result_(uint32_t grant_id, uint16_t command, uint16_t value_id, uint32_t value,
                             TransactionStatus status) {
  if (this->stopped_ || !this->in_flight_ || grant_id != this->grant_id_ || command != this->active_.command ||
      value_id != this->active_.id)
    return;
  this->in_flight_ = false;
  auto *entry = this->find_value_(value_id);
  const bool changed = status == TransactionStatus::TRANSACTION_STATUS_SUCCESS && entry != nullptr && !entry->rx_only &&
                       (!entry->valid || entry->value != value);
  if (status == TransactionStatus::TRANSACTION_STATUS_SUCCESS && entry != nullptr) {
    entry->value = value;
    entry->valid = true;
    // A verified write also satisfies a pending refresh of that value.
    entry->pending = false;
  }
  if (status != TransactionStatus::TRANSACTION_STATUS_SUCCESS)
    this->release_();
  if (changed)
    this->value_callback_.call(value_id, value);
  if (!this->stopped_)
    this->result_callback_.call(grant_id, command, value_id, value, status);
}

void VBusPolling::loop() {
  if (this->stopped_ || this->suspended_ || this->in_flight_)
    return;
#ifdef USE_VBUS_CS_PLUS
  if (this->held_)
    return;
#endif
  const uint32_t now = App.get_loop_component_start_time();
  // Expire one unsent write per pass, before callbacks can enqueue more work.
  if (!this->writes_.empty() && now - this->writes_.front().queued_at >= QUEUE_TIMEOUT_MS) {
    const auto request = this->writes_.front();
    this->writes_.pop();
    this->result_callback_.call(0, request.command, request.id, 0, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
    return;
  }
  if (!this->pending_()) {
    this->release_(true);
    this->waiting_ = false;
    this->transmitter_->release_bus(0);
    this->disable_loop();
    return;
  }
  if (this->grant_id_ != 0 && !this->transmitter_->owns_bus(this->grant_id_))
    this->grant_id_ = 0;
  if (this->grant_id_ == 0) {
    if (!this->waiting_) {
      this->waiting_at_ = now;
      this->waiting_ = true;
    }
    if (now - this->waiting_at_ >= QUEUE_TIMEOUT_MS) {
      StaticVector<uint16_t, VBUS_POLL_VALUE_COUNT> expired;
      for (auto &entry : this->values_) {
        if (entry.pending)
          expired.push_back(entry.id);
        entry.pending = false;
      }
      // Clear pending work before callbacks can queue another read of the same value.
      this->waiting_ = false;
      this->transmitter_->release_bus(0);
      ESP_LOGW(TAG, "Bus grant timeout: %u pending reads", static_cast<unsigned>(expired.size()));
      for (uint16_t id : expired) {
        if (this->stopped_)
          break;
        this->result_callback_.call(0, 0x0300, id, 0, TransactionStatus::TRANSACTION_STATUS_TIMEOUT);
      }
      return;
    }
    this->transmitter_->request_bus();
    return;
  }
  if (this->transactions_->is_busy())
    return;
  const bool write = !this->writes_.empty();
  const uint32_t budget = this->transactions_->get_response_timeout() * (write ? 3 : 1);
  if ((!write && this->reads_ >= this->batch_size_) || !this->transmitter_->has_time(this->grant_id_, budget)) {
    this->release_(true);
    return;
  }
  if (!this->transmitter_->ready_to_send(this->grant_id_))
    return;
  if (write) {
    const auto request = this->writes_.front();
    if (!this->transactions_->write_value(this->grant_id_, request.id, request.value, request.command))
      return;
    this->active_ = request;
    this->writes_.pop();
  } else {
    for (size_t i = 0; i < this->values_.size(); ++i) {
      const size_t index = (this->next_read_ + i) % this->values_.size();
      auto &entry = this->values_[index];
      if (!entry.pending)
        continue;
      if (!this->transactions_->read_value(this->grant_id_, entry.id))
        return;
      this->active_ = {0, now, entry.id, 0x0300};
      entry.pending = false;
      this->next_read_ = (index + 1) % this->values_.size();
      ++this->reads_;
      break;
    }
  }
  this->in_flight_ = true;
}

void VBusPolling::suspend() {
  if (this->stopped_ || this->suspended_)
    return;
  const bool active = this->in_flight_;
  const auto request = this->active_;
  const uint32_t grant_id = this->grant_id_;
  StaticVector<Request, 16> cancelled;
  for (const auto &pending : this->writes_)
    cancelled.push_back(pending);
  // Clear ownership and pending work before cancellation callbacks can reenter.
  this->suspended_ = true;
  this->in_flight_ = false;
  this->waiting_ = false;
  this->grant_id_ = 0;
  this->writes_.clear();
  for (auto &entry : this->values_)
    entry.pending = false;
  this->disable_loop();
  this->transmitter_->cancel_bus();
#ifdef USE_VBUS_CS_PLUS
  this->suspended_callback_.call(true);
#endif
  this->transactions_->cancel();
  if (active && !this->stopped_) {
    this->result_callback_.call(grant_id, request.command, request.id, 0,
                                TransactionStatus::TRANSACTION_STATUS_CANCELLED);
  }
  for (const auto &pending : cancelled) {
    if (this->stopped_)
      break;
    this->result_callback_.call(0, pending.command, pending.id, 0, TransactionStatus::TRANSACTION_STATUS_CANCELLED);
  }
}

void VBusPolling::resume() {
  if (!this->stopped_ && this->suspended_) {
    this->suspended_ = false;
    this->update();
#ifdef USE_VBUS_CS_PLUS
    this->suspended_callback_.call(false);
#endif
  }
}

#ifdef USE_VBUS_CS_PLUS
void VBusPolling::clear_pending_reads() {
  for (auto &entry : this->values_)
    entry.pending = false;
}

void VBusPolling::resume_group() {
  if (!this->stopped_ && !this->suspended_) {
    this->held_ = false;
    if (this->pending_())
      this->enable_loop();
  }
}
#endif

void VBusPolling::on_shutdown() {
  this->stopped_ = true;
  this->in_flight_ = false;
  this->writes_.clear();
  for (auto &entry : this->values_)
    entry.pending = false;
  this->stop_poller();
  this->transactions_->on_shutdown();
  this->transmitter_->on_shutdown();
  this->grant_id_ = 0;
  this->disable_loop();
}

}  // namespace esphome::vbus
#endif
