#pragma once

#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
#include "esphome/core/controller.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace api {

class APIConnection;

class InitialStateIterator : public ComponentIterator {
 public:
  InitialStateIterator(APIConnection *client);

 protected:
  APIConnection *client_;
};

}  // namespace api
}  // namespace esphome
