#ifdef USE_ARDUINO

#include "list_entities.h"
#include "esphome/core/application.h"

#include "web_server.h"

namespace esphome {
namespace web_server {

StatesIterator::StatesIterator(WebServer *web_server) : ListEntitiesIterator::ListEntitiesIterator(web_server) {}

bool StatesIterator::has_connected_client() { return true; }

bool StatesIterator::process(const std::string &s) {
  this->str_ = s;
  return true;
}

optional<std::string> StatesIterator::next() {
  while (this->state_ != IteratorState::MAX) {
    this->str_ = nullopt;
    this->advance();
    if (this->str_.has_value()) {
      return this->str_;
    }
  }
  this->str_ = nullopt;
  return nullopt;
}

}  // namespace web_server
}  // namespace esphome

#endif  // USE_ARDUINO
