#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"

namespace esphome {
namespace datetime {

class DateTimeBase : public EntityBase {
 public:
  /// Return whether this Datetime has gotten a full state yet.
  bool has_state() const { return has_state_; }

 protected:
  bool has_state_{false};
};

}  // namespace datetime
}  // namespace esphome
