#include "vbus_tx.h"

#ifdef USE_VBUS_TX
#include "vbus.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome::vbus {

ESPHOME_LOG_TAG(TAG, "vbus.tx");
static constexpr uint32_t MAX_RX_AGE_MS = 50;
static constexpr uint32_t CONTENTION_MS = 100;
static constexpr uint32_t GRANT_WAIT_MS = 10000;
// A 16-byte datagram takes 16.7 ms at 9600 baud; reserve two frames for release.
static constexpr uint32_t FRAME_TIME_MS = 20;
static constexpr uint32_t RELEASE_MARGIN_MS = 2 * FRAME_TIME_MS;

static bool requires_reply(uint16_t command) { return command == 0x0200 || command == 0x0300 || command == 0x0400; }

void VBusTransmitter::dump_config() {
  ESP_LOGCONFIG(TAG, "VBus TX:\n  Source: 0x%04x\n  Controller: 0x%04x\n  Lease: %" PRIu32 " ms", this->source_,
                this->destination_, this->lease_duration_ms_);
}

bool VBusTransmitter::request_bus() {
  if (this->stopped_ || this->state_ != State::STATE_IDLE)
    return false;
  this->requested_at_ = App.get_loop_component_start_time();
  if (this->receiver_->available() != 0 || this->receiver_->state_ != 0) {
    this->rx_fresh_ = false;
    this->packet_fresh_ = false;
  }
  this->state_ = State::STATE_WAIT_GRANT;
  return true;
}

bool VBusTransmitter::has_bus() const {
  const uint32_t now = App.get_loop_component_start_time();
  return this->state_ == State::STATE_OWNED && !this->release_pending_ && this->observed_ &&
         now - this->observed_at_ <= MAX_RX_AGE_MS &&
         now - this->granted_at_ < this->lease_duration_ms_ - RELEASE_MARGIN_MS;
}

bool VBusTransmitter::owns_bus(uint32_t grant_id) const {
  const uint32_t now = App.get_loop_component_start_time();
  return grant_id == this->grant_id_ && this->state_ == State::STATE_OWNED && this->observed_ &&
         now - this->observed_at_ <= MAX_RX_AGE_MS &&
         now - this->granted_at_ < this->lease_duration_ms_ - FRAME_TIME_MS;
}

bool VBusTransmitter::can_send_() const {
  const uint32_t now = App.get_loop_component_start_time();
  return this->observed_ && now - this->observed_at_ <= MAX_RX_AGE_MS && this->rx_fresh_ && !this->packet_pending_ &&
         !this->awaiting_reply_ && this->receiver_->state_ == 0 && !this->receiver_->available() &&
         (!this->sent_ || now - this->sent_at_ >= FRAME_TIME_MS) && this->parent_->is_connected() &&
         this->parent_->available_for_write() >= 16;
}

bool VBusTransmitter::send_datagram(uint32_t grant_id, uint16_t command, uint16_t value_id, uint32_t value,
                                    bool expect_reply) {
  if ((!expect_reply && requires_reply(command)) || grant_id != this->grant_id_ || (command & 0x8080) != 0 ||
      command == 0x0500 || command == 0x0600 || !this->has_bus() || !this->can_send_()) {
    ESP_LOGW(TAG, "Datagram not sent: invalid command/grant/reply mode, no lease or busy bus");
    return false;
  }
  this->send_(command, value_id, value, expect_reply);
  return true;
}

void VBusTransmitter::send_(uint16_t command, uint16_t value_id, uint32_t value, bool expect_reply) {
  std::array<uint8_t, 16> bytes{0xaa,
                                uint8_t(this->destination_),
                                uint8_t(this->destination_ >> 8),
                                uint8_t(this->source_),
                                uint8_t(this->source_ >> 8),
                                0x20,
                                uint8_t(command),
                                uint8_t(command >> 8),
                                uint8_t(value_id),
                                uint8_t(value_id >> 8),
                                uint8_t(value),
                                uint8_t(value >> 8),
                                uint8_t(value >> 16),
                                uint8_t(value >> 24),
                                0,
                                0};
  for (size_t i = 0; i < 6; ++i) {
    bytes[14] |= (bytes[8 + i] >> 7) << i;
    bytes[8 + i] &= 0x7f;
  }
  uint8_t sum = 0x7f;
  for (size_t i = 1; i < 15; ++i)
    sum = (sum - bytes[i]) & 0x7f;
  bytes[15] = sum;
  this->sent_at_ = App.get_loop_component_start_time();
  this->sent_command_ = command;
  this->sent_value_id_ = value_id;
  this->sent_value_ = value;
  this->sent_ = true;
  this->awaiting_reply_ = expect_reply;
  this->write_array(bytes);
}

void VBusTransmitter::stop_() {
  this->state_ = State::STATE_IDLE;
  this->release_pending_ = false;
  this->awaiting_reply_ = false;
}

void VBusTransmitter::release_bus(uint32_t grant_id) {
  if (this->state_ == State::STATE_OWNED && grant_id == this->grant_id_) {
    this->release_pending_ = true;
  } else if (this->state_ != State::STATE_OWNED && grant_id == 0) {
    this->stop_();
  }
}

void VBusTransmitter::on_shutdown() {
  this->stopped_ = true;
  this->stop_();
}

void VBusTransmitter::observe_rx(size_t available) {
  const uint32_t now = App.get_loop_component_start_time();
  const bool fresh = this->observed_ && now - this->observed_at_ <= MAX_RX_AGE_MS && available <= 32;
  if (!fresh) {
    this->rx_fresh_ = false;
    this->packet_fresh_ = false;
    if (this->state_ == State::STATE_OWNED || this->state_ == State::STATE_CONTEND)
      this->stop_();
  } else if (available == 0) {
    // A backlog stays untrusted until a subsequent pass sees an empty RX queue.
    this->rx_fresh_ = true;
  }
  this->observed_ = true;
  this->observed_at_ = now;
  this->rx_unread_ = available;
}

void VBusTransmitter::observe_byte(uint8_t byte) {
  if (this->rx_unread_ != 0)
    --this->rx_unread_;
  if (this->state_ == State::STATE_CONTEND)
    this->stop_();
  if (byte == 0xaa) {
    this->packet_at_ = App.get_loop_component_start_time();
    this->packet_fresh_ = this->rx_fresh_ && this->state_ == State::STATE_WAIT_GRANT;
    this->packet_pending_ = true;
  } else if ((byte & 0x80) != 0 || this->receiver_->state_ == 0) {
    if (this->state_ == State::STATE_OWNED)
      this->stop_();
  }
}

void VBusTransmitter::on_header(uint8_t protocol, uint16_t source, uint16_t destination) {
  if (this->state_ == State::STATE_WAIT_GRANT && source == this->source_)
    this->stop_();
  if (this->state_ != State::STATE_OWNED)
    return;
  const bool reply = source == this->destination_ && destination == this->source_;
  const bool echo = source == this->source_ && destination == this->destination_;
  if (protocol != 0x20 || (!reply && !echo))
    this->stop_();
}

void VBusTransmitter::on_datagram(uint16_t command, uint16_t source, uint16_t destination, uint16_t value_id,
                                  uint32_t value) {
  const uint32_t now = App.get_loop_component_start_time();
  this->packet_pending_ = false;
  if (this->state_ == State::STATE_WAIT_GRANT && command == 0x0500 && source == this->destination_ &&
      (destination == 0 || destination == this->source_) && value_id == 0 && value == 0 && this->packet_fresh_ &&
      now - this->packet_at_ <= MAX_RX_AGE_MS && now - this->requested_at_ < GRANT_WAIT_MS &&
      !this->receiver_->available()) {
    this->granted_at_ = now;
    this->sent_ = false;
    this->state_ = State::STATE_CONTEND;
  } else if (this->state_ == State::STATE_OWNED &&
             ((source == this->source_ && (!this->sent_ || command != this->sent_command_ ||
                                           value_id != this->sent_value_id_ || value != this->sent_value_)) ||
              command == 0x0500 || command == 0x0600)) {
    this->stop_();
  } else if (this->state_ == State::STATE_OWNED && source == this->destination_ && destination == this->source_ &&
             this->sent_ && now - this->sent_at_ >= FRAME_TIME_MS &&
             (!requires_reply(this->sent_command_) || value_id == this->sent_value_id_)) {
    this->awaiting_reply_ = false;
  }
}

void VBusTransmitter::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->rx_unread_ != 0) {
    this->rx_fresh_ = false;
    this->packet_fresh_ = false;
    if (this->state_ == State::STATE_OWNED || this->state_ == State::STATE_CONTEND)
      this->stop_();
  }
  if (this->state_ == State::STATE_WAIT_GRANT && now - this->requested_at_ >= GRANT_WAIT_MS) {
    ESP_LOGW(TAG, "Bus grant timed out");
    this->stop_();
  }
  if (this->state_ != State::STATE_CONTEND && this->state_ != State::STATE_OWNED)
    return;
  if (now - this->granted_at_ >= this->lease_duration_ms_ - FRAME_TIME_MS || now - this->observed_at_ > MAX_RX_AGE_MS) {
    this->stop_();
    return;
  }
  if (this->state_ == State::STATE_CONTEND) {
    if (now - this->granted_at_ < CONTENTION_MS)
      return;
    if (now - this->granted_at_ > CONTENTION_MS + MAX_RX_AGE_MS || !this->can_send_()) {
      this->stop_();
      return;
    }
    this->state_ = State::STATE_OWNED;
    if (++this->grant_id_ == 0)
      ++this->grant_id_;
    this->grant_callback_.call(this->grant_id_);
  }
  if (this->state_ == State::STATE_OWNED &&
      (this->release_pending_ || now - this->granted_at_ >= this->lease_duration_ms_ - RELEASE_MARGIN_MS) &&
      this->can_send_()) {
    this->send_(0x0600, 0, 0);
    this->stop_();
  }
}

}  // namespace esphome::vbus
#endif
