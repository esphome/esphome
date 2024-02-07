#pragma once

#include "WVariant.h"

#define PINS_COUNT (48)
#define LED_BUILTIN (64)
#if PINS_COUNT > LED_BUILTIN
#error LED_BUILTIN should be bigger than PINS_COUNT. To ignore settings.
#endif
#define LED_BLUE (LED_BUILTIN)
// TODO other are also needed?
#define USE_LFXO  // Board uses 32khz crystal for LF
#define LED_STATE_ON (1)
#define PIN_SERIAL1_RX (33)  // P1.01
#define PIN_SERIAL1_TX (34)  // P1.02
