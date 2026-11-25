// Raw frames for other components: frame listeners, send_frame(), heartbeat / EMCY callbacks for every node.
#include "canopen.h"

#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");

void CANopenComponent::add_frame_listener(FrameListener &listener) {
  if ((listener.can_id & listener.mask) > canbus::CAN_STANDARD_ID_MAX) {
    ESP_LOGW(TAG, "frame listener 0x%x/0x%x matches no 11-bit CAN ID, not added", listener.can_id, listener.mask);
    return;
  }
  // adding it twice would link it to itself
  for (const FrameListener *l = this->frame_listeners_; l != nullptr; l = l->next) {
    if (l == &listener)
      return;
  }
  // the bits above 0x7FF are 0 in both the frame's id and can_id & mask, so they can go
  listener.can_id &= canbus::CAN_STANDARD_ID_MAX;
  listener.mask &= canbus::CAN_STANDARD_ID_MAX;
  listener.next = this->frame_listeners_;
  this->frame_listeners_ = &listener;
  if (this->co_ != nullptr)
    this->check_frame_listener_(listener.can_id, listener.mask);
}

// A listener on an ID CANopenNode receives itself (NMT, heartbeats it consumes, ...) is almost always a mistake.
void CANopenComponent::check_frame_listener_(uint16_t can_id, uint16_t mask) {
  CO_CANmodule_t *mod = this->co_->CANmodule;
  for (uint16_t i = 0; i < mod->rxSize; i++) {
    const CO_CANrx_t *rx = &mod->rxArray[i];
    if (rx->CANrx_callback == nullptr || (rx->ident & 0x800))
      continue;
    if (((can_id ^ rx->ident) & mask & rx->mask) == 0) {
      ESP_LOGW(TAG, "frame listener 0x%03x/0x%03x overlaps CAN ID 0x%03x used by CANopenNode", can_id, mask,
               rx->ident & 0x7FF);
    }
  }
}

bool CANopenComponent::send_frame(uint32_t can_id, const uint8_t *data, size_t len) {
  if (can_id > canbus::CAN_STANDARD_ID_MAX || len > canbus::CAN_MAX_DATA_LENGTH)
    return false;
  return this->transmit(can_id, data, static_cast<uint8_t>(len));
}

void CANopenComponent::dispatch_frame_(const CO_CANrxMsg_t &msg) {
  if (msg.ident & 0x800)
    return;  // RTR
  for (const FrameListener *l = this->frame_listeners_; l != nullptr; l = l->next) {
    if (((msg.ident ^ l->can_id) & l->mask) == 0)
      l->callback(l->context, msg.ident, msg.data, msg.DLC);
  }
  uint8_t node;
#ifdef USE_CANOPEN_HEARTBEAT
  NMTState state;
  if (decode_heartbeat(msg.ident, msg.data, msg.DLC, node, state)) {
    this->heartbeat_callback_.call(node, state);
    return;
  }
#endif
  Emergency emcy;
  if (!decode_emcy(msg.ident, msg.data, msg.DLC, node, emcy))
    return;
  // code 0x0000: an error cleared; the register shows the errors still active
  if (emcy.error_code == 0) {
    ESP_LOGI(TAG, "node 0x%02x EMCY: error reset, register 0x%02x", node, emcy.error_register);
  } else {
    ESP_LOGW(TAG, "node 0x%02x EMCY: error code 0x%04x, register 0x%02x", node, emcy.error_code, emcy.error_register);
  }
#ifdef USE_CANOPEN_EMCY
  this->emcy_callback_.call(node, emcy);
#endif
}

}  // namespace esphome::canopen
