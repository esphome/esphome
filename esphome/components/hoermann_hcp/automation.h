#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#include "hoermann_hcp.h"

namespace esphome::hoermann_hcp {

// For the ota on_begin trigger. An ordinary restart announces itself from on_shutdown, but an update is
// written to flash long before that runs, so it has to say so when the transfer starts.
template<typename... Ts> class AnnouncePauseAction final : public Action<Ts...>, public Parented<HoermannHcp> {
 public:
  void play(const Ts &...x) override { this->parent_->announce_pause(); }
};

}  // namespace esphome::hoermann_hcp
