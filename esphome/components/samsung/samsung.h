#pragma once

#include "esphome/components/climate/climate_mode.h"
#include "esphome/components/climate_ir/climate_ir.h"

#include <array>

namespace esphome::samsung {

static constexpr uint8_t SAMSUNG_AC_TEMP_MIN = 16;
static constexpr uint8_t SAMSUNG_AC_TEMP_MAX = 30;
// A frame is made of 7 byte sections; the normal state is two sections and a power change adds a timer section
static constexpr uint8_t SAMSUNG_AC_SECTION_LENGTH = 7;
static constexpr uint8_t SAMSUNG_AC_STATE_LENGTH = 2 * SAMSUNG_AC_SECTION_LENGTH;

class SamsungClimate : public climate_ir::ClimateIR {
 public:
  SamsungClimate();

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  /// Send the given sections as one frame, each with its checksum filled in
  void send_sections_(const uint8_t *const *sections, uint8_t count);

  // The last state sent or received; its power bits say whether the next frame has to switch power
  std::array<uint8_t, SAMSUNG_AC_STATE_LENGTH> state_{};
};

}  // namespace esphome::samsung
