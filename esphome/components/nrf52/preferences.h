#pragma once

#ifdef USE_NRF52

namespace esphome {
namespace nrf52 {

void setup_preferences();
//TODO
// void preferences_prevent_write(bool prevent);

}  // namespace nrf52
}  // namespace esphome

#endif  // USE_RP2040
