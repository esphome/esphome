#pragma once

#include "esphome/components/ds248x_base/ds248x_base.h"

namespace esphome {
namespace ds2484 {

/// DS2484 single-channel I2C-to-1-Wire bridge
/// This is a simple wrapper around the DS248x base class with no additional functionality
class DS2484OneWireBus : public ds248x_base::DS248xOneWireBusBase {
  // No additional members or methods needed - everything is inherited from base class
};

}  // namespace ds2484
}  // namespace esphome
