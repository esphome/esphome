#pragma once

#include "esphome/core/component.h"
#include "esphome/core/component_iterator.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace api {

class APIConnection;

class ListEntitiesIterator : public ComponentIterator {
 public:
  ListEntitiesIterator(APIConnection *client);
  bool on_service(UserServiceDescriptor *service) override;
#ifdef USE_ESP32_CAMERA
  bool on_camera(esp32_camera::ESP32Camera *camera) override;
#endif
  bool on_end() override;

 protected:
  APIConnection *client_;
};

}  // namespace api
}  // namespace esphome
