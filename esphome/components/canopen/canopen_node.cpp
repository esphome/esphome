#include "canopen_node.h"

namespace esphome::canopen {

CANopenNode::CANopenNode(CANopenComponent *parent, uint8_t node_id) : canopen_(parent), node_id_(node_id) {
  // the hooks are only called from the hub's processing, after construction
  parent->add_on_node_event_callback([this](uint8_t node_id, NodeEvent event, NMTState state) {
    if (node_id != this->node_id_)
      return;
    switch (event) {
      case NodeEvent::NODE_EVENT_ONLINE:
        this->on_node_online();
        break;
      case NodeEvent::NODE_EVENT_OFFLINE:
        this->on_node_offline();
        break;
      case NodeEvent::NODE_EVENT_NMT_CHANGED:
        this->on_node_nmt_changed(state);
        break;
      case NodeEvent::NODE_EVENT_RESET:
        this->on_node_reset();
        break;
    }
  });
#ifdef USE_CANOPEN_EMCY  // set by register_node(); this file is compiled even without a CANopenNode subclass
  parent->add_on_emcy_callback([this](uint8_t node_id, const Emergency &emergency) {
    if (node_id == this->node_id_)
      this->on_node_emcy(emergency);
  });
#endif
}

}  // namespace esphome::canopen
