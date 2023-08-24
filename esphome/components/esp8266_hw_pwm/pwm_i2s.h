#pragma once

#ifdef USE_ESP8266
extern "C" {

bool pwm_begin(uint32_t rate);
void pwm_set_rate(uint32_t rate);
float pwm_get_rate();
void pwm_set_level(float level);
uint32_t pwm_levels();

}

#endif  // USE_ESP8266
