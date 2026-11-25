// CANopenNode's CAN driver (301/CO_driver.h) on canbus, all on the main loop; CANptr is the CANopenComponent.
#include "canopen.h"

#include <cstring>

#include "esphome/core/application.h"

namespace esphome::canopen {

void discard_pending_tx(CO_CANmodule_t *mod, CO_CANtx_t *buf) {
  if (buf->bufferFull) {
    buf->bufferFull = false;
    mod->CANtxCount--;
  }
}

static bool transmit_buffer(CO_CANmodule_t *mod, CO_CANtx_t *buf) {
  return static_cast<CANopenComponent *>(mod->CANptr)->transmit(buf->ident, buf->data, buf->DLC);
}

void CANopenComponent::on_frame_(const canbus::CanFrame &frame) {
  // CANopen uses classic CAN with 11-bit ids
#ifdef USE_CANBUS_FD
  if (frame.fd)
    return;
#endif
  const uint8_t len = frame.payload_length();
  if (frame.use_extended_id || len > 8)
    return;
  CO_CANmodule_t *mod = this->co_->CANmodule;
  CO_CANrxMsg_t msg{};
  msg.ident = (frame.can_id & 0x7FF) | (frame.remote_transmission_request ? 0x800 : 0);
  msg.DLC = len;
  memcpy(msg.data, frame.data, len);
  for (uint16_t i = 0; i < mod->rxSize; i++) {
    CO_CANrx_t *rx = &mod->rxArray[i];
    if (rx->CANrx_callback == nullptr || ((msg.ident ^ rx->ident) & rx->mask) != 0)
      continue;
    if (rx->object == this->co_->SDOclient && this->sdo_reply_is_stale_(frame.data, len))
      continue;
    rx->CANrx_callback(rx->object, &msg);
  }
  this->dispatch_frame_(msg);
  // CANopenNode restarts its response timer on a reply, so loop() charges time from here.
  if (CO_FLAG_READ(this->co_->SDOclient->CANrxNew)) {
    this->process_sdo_(0);
    this->last_ms_ = App.get_loop_component_start_time();
  }
}

bool CANopenComponent::transmit(uint16_t ident, const uint8_t *data, uint8_t len) {
  return this->canbus_->send_data(ident & 0x7FF, false, ident & 0x800, data, len) == canbus::ERROR_OK;
}

}  // namespace esphome::canopen

using esphome::canopen::discard_pending_tx;
using esphome::canopen::transmit_buffer;

extern "C" {

// names and signatures fixed by CO_driver.h
// NOLINTBEGIN(readability-identifier-naming,readability-non-const-parameter)

void CO_CANsetConfigurationMode(void *CANptr) {}

void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule) { CANmodule->CANnormal = true; }

CO_ReturnError_t CO_CANmodule_init(CO_CANmodule_t *CANmodule, void *CANptr, CO_CANrx_t rxArray[], uint16_t rxSize,
                                   CO_CANtx_t txArray[], uint16_t txSize, uint16_t CANbitRate) {
  CANmodule->CANptr = CANptr;
  CANmodule->rxArray = rxArray;
  CANmodule->rxSize = rxSize;
  CANmodule->txArray = txArray;
  CANmodule->txSize = txSize;
  CANmodule->CANerrorStatus = 0;
  CANmodule->CANnormal = false;
  CANmodule->useCANrxFilters = false;
  CANmodule->bufferInhibitFlag = false;
  CANmodule->firstCANtxMessage = true;
  CANmodule->CANtxCount = 0;
  CANmodule->errOld = 0;
  memset(rxArray, 0, sizeof(CO_CANrx_t) * rxSize);
  memset(txArray, 0, sizeof(CO_CANtx_t) * txSize);
  return CO_ERROR_NO;
}

void CO_CANmodule_disable(CO_CANmodule_t *CANmodule) {
  if (CANmodule != nullptr)
    CANmodule->CANnormal = false;
}

CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, uint16_t mask,
                                    bool_t rtr, void *object, void (*CANrx_callback)(void *object, void *message)) {
  if (index >= CANmodule->rxSize)
    return CO_ERROR_ILLEGAL_ARGUMENT;
  CO_CANrx_t *rx = &CANmodule->rxArray[index];
  rx->object = object;
  rx->CANrx_callback = CANrx_callback;
  rx->ident = (ident & 0x7FF) | (rtr ? 0x800 : 0);
  rx->mask = (mask & 0x7FF) | 0x800;
  return CO_ERROR_NO;
}

CO_CANtx_t *CO_CANtxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, bool_t rtr, uint8_t noOfBytes,
                               bool_t syncFlag) {
  if (index >= CANmodule->txSize)
    return nullptr;
  CO_CANtx_t *tx = &CANmodule->txArray[index];
  // the SDO client sets its buffer up again for every request
  discard_pending_tx(CANmodule, tx);
  tx->ident = (ident & 0x7FF) | (rtr ? 0x800 : 0);
  tx->DLC = noOfBytes;
  tx->syncFlag = syncFlag;
  return tx;
}

CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer) {
  if (buffer->bufferFull) {
    if (!CANmodule->firstCANtxMessage)
      CANmodule->CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
    return CO_ERROR_TX_OVERFLOW;
  }
  if (CANmodule->CANtxCount == 0 && transmit_buffer(CANmodule, buffer)) {
    CANmodule->firstCANtxMessage = false;
    return CO_ERROR_NO;
  }
  // queued behind the others; CO_CANmodule_process() resends it from loop()
  buffer->bufferFull = true;
  CANmodule->CANtxCount++;
  static_cast<esphome::canopen::CANopenComponent *>(CANmodule->CANptr)->enable_loop();
  return CO_ERROR_NO;
}

void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule) {
  for (uint16_t i = 0; i < CANmodule->txSize; i++) {
    CO_CANtx_t *tx = &CANmodule->txArray[i];
    if (tx->bufferFull && tx->syncFlag) {
      discard_pending_tx(CANmodule, tx);
      CANmodule->CANerrorStatus |= CO_CAN_ERRTX_PDO_LATE;
    }
  }
}

void CO_CANmodule_process(CO_CANmodule_t *CANmodule) {
  for (uint16_t i = 0; i < CANmodule->txSize && CANmodule->CANtxCount > 0; i++) {
    CO_CANtx_t *tx = &CANmodule->txArray[i];
    if (!tx->bufferFull)
      continue;
    if (!transmit_buffer(CANmodule, tx))
      break;
    tx->bufferFull = false;
    CANmodule->CANtxCount--;
    CANmodule->firstCANtxMessage = false;
  }
}

// NOLINTEND(readability-identifier-naming,readability-non-const-parameter)

}  // extern "C"
