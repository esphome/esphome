// NMT master: NMT commands to other nodes.
#include "canopen.h"

#ifdef USE_CANOPEN_NMT_MASTER

#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");

bool CANopenComponent::send_nmt(uint8_t node_id, NMTCommand command) {
  // no NMT object in client-only mode
  if (this->co_ == nullptr || this->co_->NMT == nullptr || this->is_failed() || node_id > 127) {
    ESP_LOGW(TAG, "NMT command %u to node 0x%02x rejected", (unsigned) command, node_id);
    return false;
  }
  ESP_LOGD(TAG, "NMT command %u to node 0x%02x", (unsigned) command, node_id);
  CO_NMT_t *nmt = this->co_->NMT;
  // CANopenNode writes the frame (and, for us or all nodes, the command it applies to us) before it checks its TX
  // buffer, and keeps one command for us until the next CO_process(): a command while the previous one waits would
  // replace it
  if (nmt->NMT_TXbuff->bufferFull ||
      ((node_id == 0 || node_id == this->node_id_) && nmt->internalCommand != CO_NMT_NO_COMMAND)) {
    ESP_LOGW(TAG, "NMT command %u to node 0x%02x rejected: the previous command is still pending", (unsigned) command,
             node_id);
    return false;
  }
  CO_ReturnError_t err = CO_NMT_sendCommand(nmt, static_cast<CO_NMT_command_t>(command), node_id);
  if (err != CO_ERROR_NO) {
    ESP_LOGW(TAG, "NMT command %u to node 0x%02x failed: %d", (unsigned) command, node_id, err);
    return false;
  }
  return true;
}

}  // namespace esphome::canopen

#endif  // USE_CANOPEN_NMT_MASTER
