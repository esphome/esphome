#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/number/number.h"
#include "../hub.h"

namespace esphome::opentherm42 {

// §5.3.4 Class 4, IDs 27/38/78/79: a value this master pushes to the boiler from its own external
// sensor (outside temperature, relative humidity, ...). Unlike OpenTherm42Number's write-only ids,
// there's no sensible value to invent before the user/automation has ever supplied one -- these ids
// have no boiler-side ownership the way Class 5's pre-defined remote boiler parameters do (see hub.h's
// RequestKind comment), so there's no initial_value and no persistence across reboot, matching
// OpenTherm42TspNumber's on-demand model: nothing is sent until control() is first called, and the
// value doesn't survive a restart. The displayed .state is never touched by this class -- only a
// successful READ, handled entirely on the hub side, ever updates it (see handle_response_()). hub and
// id are both required and never change after construction (see CLAUDE.md's "Constructor parameters vs
// setters" rule).
class OpenTherm42SensorFeedNumber : public number::Number, public Component {
 public:
  OpenTherm42SensorFeedNumber(OpenTherm42Hub *hub, uint8_t id) : hub_(hub), id_(id) {}

  void dump_config() override;

 protected:
  void control(float value) override { this->hub_->set_sensor_feed_write_value(this->id_, value); }

  OpenTherm42Hub *hub_;
  uint8_t id_;
};

}  // namespace esphome::opentherm42
