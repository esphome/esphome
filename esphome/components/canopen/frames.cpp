// Raw frames for other components: frame listeners, send_frame(), heartbeat / EMCY callbacks for every node.
#include "canopen.h"

#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");

void CANopenComponent::add_frame_listener(uint32_t can_id, uint32_t mask, FrameCallback callback, void *context) {
  if ((can_id & mask) > 0x7FF) {
    ESP_LOGW(TAG, "frame listener 0x%" PRIx32 "/0x%" PRIx32 " matches no 11-bit CAN ID, not added", can_id, mask);
    return;
  }
  // the bits above 0x7FF are 0 in both the frame's id and can_id & mask, so they can go
  this->frame_listeners_.push_back(FrameListener{can_id & 0x7FF, mask & 0x7FF, callback, context});
  if (this->co_ != nullptr)
    this->check_frame_listener_(can_id & 0x7FF, mask & 0x7FF);
}

void CANopenComponent::init_frame_listeners_() {
  for (const auto &l : this->frame_listeners_)
    this->check_frame_listener_(l.can_id, l.mask);
}

// A listener on an ID CANopenNode receives itself (NMT, heartbeats it consumes, ...) is almost always a mistake.
void CANopenComponent::check_frame_listener_(uint32_t can_id, uint32_t mask) {
  CO_CANmodule_t *mod = this->co_->CANmodule;
  for (uint16_t i = 0; i < mod->rxSize; i++) {
    const CO_CANrx_t *rx = &mod->rxArray[i];
    if (rx->CANrx_callback == nullptr || (rx->ident & 0x800))
      continue;
    if (((can_id ^ rx->ident) & mask & rx->mask & 0x7FF) == 0) {
      ESP_LOGW(TAG, "frame listener 0x%03" PRIx32 "/0x%03" PRIx32 " overlaps CAN ID 0x%03x used by CANopenNode", can_id,
               mask, rx->ident & 0x7FF);
    }
  }
}

bool CANopenComponent::send_frame(uint32_t can_id, const uint8_t *data, uint8_t len) {
  if (can_id > 0x7FF || len > 8)
    return false;
  return this->transmit(can_id, data, len);
}

void CANopenComponent::dispatch_frame_(const CO_CANrxMsg_t &msg) {
  if (msg.ident & 0x800)
    return;  // RTR
  // by index, on a copy, up to the count before the first call: a callback may add listeners (push_back() may move
  // the vector), so no range-for
  for (size_t i = 0, n = this->frame_listeners_.size(); i < n; i++) {  // NOLINT(modernize-loop-convert)
    const FrameListener l = this->frame_listeners_[i];
    if (((msg.ident ^ l.can_id) & l.mask) == 0)
      l.callback(l.context, msg.ident, msg.data, msg.DLC);
  }
  uint8_t node;
  NMTState state;
  Emergency emcy;
  if (decode_heartbeat(msg.ident, msg.data, msg.DLC, node, state)) {
    this->heartbeat_callback_.call(node, state);
  } else if (decode_emcy(msg.ident, msg.data, msg.DLC, node, emcy)) {
    // code 0x0000: an error cleared; the register shows the errors still active
    if (emcy.error_code == 0) {
      ESP_LOGI(TAG, "node 0x%02x EMCY: error reset, register 0x%02x", node, emcy.error_register);
    } else {
      ESP_LOGW(TAG, "node 0x%02x EMCY: error code 0x%04x, register 0x%02x", node, emcy.error_code, emcy.error_register);
    }
    this->emcy_callback_.call(node, emcy);
  }
}

}  // namespace esphome::canopen
