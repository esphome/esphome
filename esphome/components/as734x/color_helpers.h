#pragma once
#include <cstdint>

namespace esphome::as734x {

float clamp(float val, float min_val, float max_val);
float gamma_correct(float channel);
void tristimulus_to_hex(float x, float y, float z, uint8_t &ro, uint8_t &go, uint8_t &bo);
void tristimulus_to_cct(float x, float y, float z, float &cct);

}  // namespace esphome::as734x
