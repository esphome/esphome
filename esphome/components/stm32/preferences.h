#pragma once

#ifdef USE_STM32

namespace esphome {
namespace stm32 {

void setup_preferences();
void preferences_prevent_write(bool prevent);

}  // namespace stm32
}  // namespace esphome

#endif  // USE_STM32
