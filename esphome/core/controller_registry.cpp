#include "esphome/core/controller_registry.h"

#ifdef USE_CONTROLLER_REGISTRY

namespace esphome {

StaticVector<Controller *, CONTROLLER_REGISTRY_MAX> ControllerRegistry::controllers;

}  // namespace esphome

#endif  // USE_CONTROLLER_REGISTRY
