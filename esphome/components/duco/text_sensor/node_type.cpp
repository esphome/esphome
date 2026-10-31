#include "node_type.h"

#include "../node_type.h"

namespace esphome::duco {

void DucoNodeType::update() { this->parent_->send(this, FUNCTION_NODE, {0x01, this->address_}); }

void DucoNodeType::receive_response(const DucoMessage &message) {
  if (message.size >= 2)
    this->publish_state(LOG_STR_ARG(node_type_name(message.data[0], message.data[1])));
}

}  // namespace esphome::duco
