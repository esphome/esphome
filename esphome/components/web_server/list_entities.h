#pragma once

#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace web_server {

class WebServer;

class ListEntitiesIterator : public ComponentIterator {
 public:
  ListEntitiesIterator(WebServer *web_server);

 protected:
  WebServer *web_server_;
};

}  // namespace web_server
}  // namespace esphome
