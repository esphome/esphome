#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include "hoermann_hcp.h"

namespace esphome::hoermann_hcp {

// Tells the bus controller the accessory is about to go quiet. An ordinary restart announces itself during
// teardown; an update has to say so when the transfer starts, because by the time teardown runs the image
// has been written and the controller has been unanswered throughout.
template<typename... Ts> class AnnouncePauseAction final : public Action<Ts...>, public Parented<HoermannHcp> {
 public:
  void play(const Ts &...x) override { this->parent_->announce_pause(); }
};

}  // namespace esphome::hoermann_hcp
