#pragma once

#ifdef USE_ARDUINO

#include "list_entities.h"
#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
#include "esphome/core/defines.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace web_server {

class WebServer;

class StatesIterator : public ListEntitiesIterator {
 public:
  StatesIterator(WebServer *web_server);
  optional<std::string> next();

 protected:
  // WebServer *web_server_;
  optional<std::string> str_;
  
  bool has_connected_client() override;
  bool process(const std::string &s) override;
};

}  // namespace web_server
}  // namespace esphome

#endif  // USE_ARDUINO
