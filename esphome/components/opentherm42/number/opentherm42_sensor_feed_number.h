#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/number/number.h"
#include "../hub.h"

namespace esphome::opentherm42 {

// §5.3.4 Class 4, IDs 24/37 and IDs 27/38/78/79: a value this master pushes to the boiler from its own
// external sensor (room temperature, TrCH2, outside temperature, relative humidity, ...). Unlike
// OpenTherm42Number's write-only ids, there's no sensible value to invent before the user/automation
// has ever supplied one -- these ids have no boiler-side ownership the way Class 5's pre-defined
// remote boiler parameters do (see hub.h's RequestKind comment), so there's no initial_value and no
// persistence across reboot, matching OpenTherm42TspNumber's on-demand model: nothing is sent until
// control() is first called, and the value doesn't survive a restart.
//
// control() publishes .state optimistically, same as OpenTherm42Number, since it's exactly what was
// just commanded (never an untrustworthy WRITE-ACK echo -- see hub.cpp's handle_response_(), which
// never republishes on a successful write for any id in this component). For IDs 24/37, which have no
// READ-DATA counterpart at all, that's the only thing that ever touches .state. For 27/38/78/79, which
// do have one, a successful READ still updates .state afterwards and remains the authoritative source
// -- control()'s publish is just there so the entity shows something between being set and the next
// scheduled read, rather than sitting on Unknown.
//
// hub and id are both required and never change after construction (see CLAUDE.md's "Constructor
// parameters vs setters" rule).
class OpenTherm42SensorFeedNumber : public number::Number, public Component {
 public:
  OpenTherm42SensorFeedNumber(OpenTherm42Hub *hub, uint8_t id) : hub_(hub), id_(id) {}

  void dump_config() override;

 protected:
  void control(float value) override {
    this->hub_->set_sensor_feed_write_value(this->id_, value);
    this->publish_state(value);
  }

  OpenTherm42Hub *hub_;
  uint8_t id_;
};

}  // namespace esphome::opentherm42
