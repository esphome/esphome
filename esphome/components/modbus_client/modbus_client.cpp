#include "modbus_client.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::modbus_client {

static const char *const TAG = "modbus_client";

void ModbusCallbackClient::dump_config() { ESP_LOGCONFIG(TAG, "Modbus Client:\n  Address: 0x%02X", this->address_); }

void ModbusCallbackClient::send(std::span<const uint8_t> pdu, ModbusResponseHandler *handler) {
  // Replies come back on the shared client response path keyed only by the request PDU, so two identical
  // in-flight requests could not be told apart - skip the duplicate rather than mis-route its reply. The
  // skipped send still resolves through its own handler (directly - not via take_pending_(), which would
  // wrongly consume the identical original's slot), keeping the one-outcome-per-send guarantee.
  for (size_t i = 0; i < this->pending_count_; i++) {
    const auto &pending = this->pending_[i];
    if (pending.request.size() == pdu.size() && std::equal(pdu.begin(), pdu.end(), pending.request.data())) {
      ESP_LOGW(TAG, "Identical request already pending; skipping send");
      if (handler != nullptr)
        handler->handle_modbus_not_sent();
      return;
    }
  }
  if (this->pending_count_ >= MAX_PENDING_RESPONSES) {
    ESP_LOGW(TAG, "Too many pending requests (%zu); skipping send", this->pending_count_);
    if (handler != nullptr)
      handler->handle_modbus_not_sent();
    return;
  }
  // Register the pending entry BEFORE sending: send_pdu() can resolve it synchronously (on_not_sent when the
  // tx buffer is full or the frame is refused), and take_pending_() must find it to notify the handler.
  auto &slot = this->pending_[this->pending_count_++];
  slot.request.set(pdu.data(), pdu.size());
  slot.handler = handler;
  this->send_pdu(pdu);
}

ModbusResponseHandler *ModbusCallbackClient::take_pending_(std::span<const uint8_t> request) {
  for (size_t i = 0; i < this->pending_count_; i++) {
    auto &pending = this->pending_[i];
    if (pending.request.size() != request.size() || !std::equal(request.begin(), request.end(), pending.request.data()))
      continue;
    auto *handler = pending.handler;
    // Swap-erase: move the last live entry into this slot, then shrink. Removing before the caller invokes
    // the handler keeps a re-entrant send from the handler safe.
    this->pending_[i] = std::move(this->pending_[this->pending_count_ - 1]);
    this->pending_count_--;
    return handler;
  }
  return nullptr;
}

}  // namespace esphome::modbus_client
