#pragma once

#ifdef USE_RP2040

namespace esphome {
namespace rp2040 {

void setup_preferences();
void preferences_prevent_write(bool prevent);

}  // namespace rp2040
}  // namespace esphome

#endif  // USE_RP2040
