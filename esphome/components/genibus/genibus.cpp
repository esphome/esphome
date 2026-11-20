#include "genibus.h"

#include <algorithm>
#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::genibus {

ESPHOME_LOG_TAG(TAG, "genibus");

static constexpr size_t MAX_LOG_BYTES = 64;

// GenibusError, in order
PROGMEM_STRING_TABLE(GenibusErrorStrings, "none", "no answer", "CRC error", "malformed answer", "write failed",
                     "disconnected");

void GenibusBus::dump_config() {
  ESP_LOGCONFIG(TAG,
                "GENIbus:\n"
                "  Master address: 0x%02X\n"
                "  Response timeout: %" PRIu32 " ms\n"
                "  Quiet period: %" PRIu32 " ms",
                this->master_address_, RESPONSE_TIMEOUT_MS, QUIET_MS);
}

bool GenibusBus::send(GenibusDevice *device, std::span<const uint8_t> apdus) {
  if (apdus.empty() || apdus.size() > MAX_REQUEST_SIZE) {
    ESP_LOGW(TAG, "Request of %u bytes not sent", static_cast<unsigned>(apdus.size()));
    return false;
  }
  if (!this->is_connected() || this->active_ == device)
    return false;
  for (auto &pending : this->pending_) {
    if (pending.device != device)
      continue;
    if (pending.size != 0)
      return false;
    std::copy(apdus.begin(), apdus.end(), pending.apdus.begin());
    pending.size = static_cast<uint8_t>(apdus.size());
    this->enable_loop();
    return true;
  }
  ESP_LOGW(TAG, "Device 0x%02X is not registered", device->get_address());
  return false;
}

bool GenibusBus::is_busy(const GenibusDevice *device) const {
  if (this->active_ == device)
    return true;
  for (const auto &pending : this->pending_) {
    if (pending.device == device)
      return pending.size != 0;
  }
  return false;
}

void GenibusBus::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->active_ != nullptr) {
    if (now - this->sent_at_ < RESPONSE_TIMEOUT_MS)
      return;
    this->rx_.reset();
    this->finish_(GenibusError::GENIBUS_ERROR_TIMEOUT);
  }
  if (this->quiet_) {
    if (now - this->quiet_since_ < QUIET_MS)
      return;
    this->quiet_ = false;
  }
  this->transmit_next_();
}

void GenibusBus::transmit_next_() {
  if (!this->is_connected()) {
    this->connection_lost_();
    this->disable_loop();
    return;
  }
  // Wrap by comparison, not modulo: with no devices count is 0
  const size_t count = this->pending_.size();
  size_t slot = this->next_slot_;
  for (size_t i = 0; i < count; i++, slot++) {
    if (slot >= count)
      slot = 0;
    auto &pending = this->pending_[slot];
    if (pending.size == 0)
      continue;
    std::array<uint8_t, MAX_REQUEST_SIZE + 2 + FRAME_OVERHEAD> frame;
    const size_t size = build_request(frame, pending.device->get_address(), this->master_address_,
                                      {pending.apdus.data(), pending.size});
    pending.size = 0;
    this->next_slot_ = static_cast<uint8_t>(slot + 1);
    this->active_ = pending.device;
    this->active_address_ = pending.device->get_address();
    this->sent_at_ = App.get_loop_component_start_time();
    this->rx_.reset();
    ESP_LOGV_HEX(TAG, frame.data(), size, MAX_LOG_BYTES, "TX %s");
    // build_request() cannot fail here: send() limits the request to MAX_REQUEST_SIZE
    if (size == 0 || !this->write_frame({frame.data(), size}))
      this->finish_(GenibusError::GENIBUS_ERROR_WRITE_FAILED);
    return;
  }
  this->disable_loop();
}

void GenibusBus::receive_bytes(std::span<const uint8_t> data) {
  for (uint8_t byte : data) {
    if (this->rx_.feed(byte)) {
      this->handle_frame_();
      this->rx_.reset();
    }
  }
}

void GenibusBus::handle_frame_() {
  const auto frame = this->rx_.frame();
  ESP_LOGV_HEX(TAG, frame.data(), frame.size(), MAX_LOG_BYTES, "RX %s");
  if (this->active_ == nullptr) {
    ESP_LOGV(TAG, "Frame dropped: %s", this->quiet_ ? LOG_STR_LITERAL("late reply") : LOG_STR_LITERAL("no request"));
    return;
  }
  // Fine for BLE with one pump; a shared RS-485 bus would need the address check first
  if (!this->rx_.crc_ok()) {
    this->finish_(GenibusError::GENIBUS_ERROR_CRC);
    return;
  }
  // Another node's traffic on a shared bus, or our own echo. Exact addresses: a request to the broadcast or
  // connection request address would never match.
  if (frame[0] != START_REPLY || frame[2] != this->master_address_ || frame[3] != this->active_address_)
    return;
  ApduReader reader(this->rx_.apdus());
  Apdu apdu;
  while (reader.next(apdu)) {
  }
  if (!reader.at_end()) {
    this->finish_(GenibusError::GENIBUS_ERROR_MALFORMED);
    return;
  }
  this->finish_(GenibusError::GENIBUS_ERROR_NONE, this->rx_.apdus());
}

void GenibusBus::finish_(GenibusError error, std::span<const uint8_t> apdus) {
  GenibusDevice *device = this->active_;
  this->active_ = nullptr;
  this->enable_loop();
  // A lost connection is the transport's to report
  if (error != GenibusError::GENIBUS_ERROR_DISCONNECTED) {
    const bool ok = error == GenibusError::GENIBUS_ERROR_NONE;
    // After a timeout, a corrupt reply or a frame that went out only in part, the device may still be sending
    if (!ok) {
      this->quiet_ = true;
      this->quiet_since_ = App.get_loop_component_start_time();
    }
    const LogString *text = GenibusErrorStrings::get_log_str(static_cast<uint8_t>(error), 0);
    this->status_comm_result(ok, text);
    if (!ok) {
      ESP_LOGD(TAG, "Request to 0x%02X: %s", this->active_address_, LOG_STR_ARG(text));
    }
  }
  if (device != nullptr)
    device->on_response({error, apdus});
}

void GenibusBus::connection_lost_() {
  this->rx_.reset();
  if (this->active_ != nullptr)
    this->finish_(GenibusError::GENIBUS_ERROR_DISCONNECTED);
  for (auto &pending : this->pending_) {
    if (pending.size == 0)
      continue;
    pending.size = 0;
    pending.device->on_response({GenibusError::GENIBUS_ERROR_DISCONNECTED, {}});
  }
  if (this->down_notified_)
    return;
  this->down_notified_ = true;
  for (auto &pending : this->pending_)
    pending.device->on_disconnected();
}

void GenibusBus::connection_established_() {
  this->down_notified_ = false;
  for (auto &pending : this->pending_)
    pending.device->on_connected();
}

bool GenibusDevice::request_object_(uint8_t object, uint16_t sub_id) {
  uint8_t apdu[5];
  put_get_object(apdu, object, sub_id);
  return this->parent_->send(this, apdu);
}

}  // namespace esphome::genibus
