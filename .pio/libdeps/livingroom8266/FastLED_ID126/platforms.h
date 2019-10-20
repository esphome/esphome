#ifndef __INC_PLATFORMS_H
#define __INC_PLATFORMS_H

#include "FastLED.h"

#include "fastled_config.h"

#if defined(NRF51)
#include "platforms/arm/nrf51/fastled_arm_nrf51.h"
#elif defined(NRF52_SERIES)
#include "platforms/arm/nrf52/fastled_arm_nrf52.h"
#elif defined(__MK20DX128__) || defined(__MK20DX256__)
// Include k20/T3 headers
#include "platforms/arm/k20/fastled_arm_k20.h"
#elif defined(__MK66FX1M0__) || defined(__MK64FX512__)
// Include k66/T3.6 headers
#include "platforms/arm/k66/fastled_arm_k66.h"
#elif defined(__MKL26Z64__)
// Include kl26/T-LC headers
#include "platforms/arm/kl26/fastled_arm_kl26.h"
#elif defined(__SAM3X8E__)
// Include sam/due headers
#include "platforms/arm/sam/fastled_arm_sam.h"
#elif defined(STM32F10X_MD) || defined(__STM32F1__)
#include "platforms/arm/stm32/fastled_arm_stm32.h"
#elif defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || defined(__SAMD21E18A__)
#include "platforms/arm/d21/fastled_arm_d21.h"
#elif defined(__SAMD51G19A__) || defined(__SAMD51J19A__)
#include "platforms/arm/d51/fastled_arm_d51.h"
#elif defined(ESP8266)
#include "platforms/esp/8266/fastled_esp8266.h"
#elif defined(ESP32)
#include "platforms/esp/32/fastled_esp32.h"
#else
// AVR platforms
#include "platforms/avr/fastled_avr.h"
#endif

#endif
