// NMT master: NMT commands to other nodes.
#include "canopen.h"

#ifdef USE_CANOPEN_NMT_MASTER

#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");

bool CANopenComponent::send_nmt(uint8_t node_id, NMTCommand command) {
  // no NMT object in client-only mode
  if (this->co_ == nullptr || this->co_->NMT == nullptr || this->is_failed() || node_id > 127)
    return false;
  ESP_LOGD(TAG, "NMT command %u to node 0x%02x", (unsigned) command, node_id);
  return CO_NMT_sendCommand(this->co_->NMT, static_cast<CO_NMT_command_t>(command), node_id) == CO_ERROR_NO;
}

}  // namespace esphome::canopen

#endif  // USE_CANOPEN_NMT_MASTER
