#include "subscribe_state_attributes.h"
#include "api_connection.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

bool InitialStateAttributesIterator::on_entity(EntityBase *entity) {
  return this->client_->send_state_attributes(entity, entity->get_state_attributes());
}

InitialStateAttributesIterator::InitialStateAttributesIterator(APIConnection *client) : client_(client) {}

}  // namespace api
}  // namespace esphome
