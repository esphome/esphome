// NMT master: NMT commands to other nodes.
#include "canopen.h"

#ifdef USE_CANOPEN_NMT_MASTER

#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");

// NMTCommand uses CANopenNode's values: send_nmt() casts it
static_assert(static_cast<uint8_t>(CO_NMT_ENTER_OPERATIONAL) == static_cast<uint8_t>(NMTCommand::NMT_COMMAND_START));
static_assert(static_cast<uint8_t>(CO_NMT_ENTER_STOPPED) == static_cast<uint8_t>(NMTCommand::NMT_COMMAND_STOP));
static_assert(static_cast<uint8_t>(CO_NMT_ENTER_PRE_OPERATIONAL) ==
              static_cast<uint8_t>(NMTCommand::NMT_COMMAND_PRE_OPERATIONAL));
static_assert(static_cast<uint8_t>(CO_NMT_RESET_NODE) == static_cast<uint8_t>(NMTCommand::NMT_COMMAND_RESET_NODE));
static_assert(static_cast<uint8_t>(CO_NMT_RESET_COMMUNICATION) ==
              static_cast<uint8_t>(NMTCommand::NMT_COMMAND_RESET_COMMUNICATION));

bool CANopenComponent::send_nmt(uint8_t node_id, NMTCommand command) {
  // A log handler inside our own send (canbus logs a frame) calling us again would overwrite the frame being sent
  // before CANopenNode marks its buffer: refused, without logging (we are inside a log call)
  if (this->nmt_sending_)
    return false;
  // no NMT object in client-only mode
  if (this->co_ == nullptr || this->co_->NMT == nullptr || this->is_failed() || node_id > 127) {
    ESP_LOGW(TAG, "NMT command %u to node 0x%02x rejected", (unsigned) command, node_id);
    return false;
  }
  ESP_LOGD(TAG, "NMT command %u to node 0x%02x", (unsigned) command, node_id);
  CO_NMT_t *nmt = this->co_->NMT;
  const bool for_us = node_id == 0 || node_id == this->node_id_;
  // CANopenNode writes the frame (and, for us or all nodes, the command it applies to us) before it checks its TX
  // buffer, and keeps one command for us until the next CO_process(): a command while the previous one waits would
  // replace it
  if (nmt->NMT_TXbuff->bufferFull || (for_us && this->nmt_command_pending_())) {
    ESP_LOGW(TAG, "NMT command %u to node 0x%02x rejected: the previous command is still pending", (unsigned) command,
             node_id);
    return false;
  }
  this->nmt_sending_ = true;
  CO_ReturnError_t err = CO_NMT_sendCommand(nmt, static_cast<CO_NMT_command_t>(command), node_id);
  this->nmt_sending_ = false;
  if (err != CO_ERROR_NO) {
    ESP_LOGW(TAG, "NMT command %u to node 0x%02x failed: %d", (unsigned) command, node_id, err);
    return false;
  }
  if (for_us)
    this->apply_nmt_command_();
  return true;
}

}  // namespace esphome::canopen

#endif  // USE_CANOPEN_NMT_MASTER
