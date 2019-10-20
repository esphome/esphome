#pragma once

#ifndef ESP32
#define ESP32
#endif

#define FASTLED_ESP32

// Use system millis timer
#define FASTLED_HAS_MILLIS

typedef volatile uint32_t RoReg;
typedef volatile uint32_t RwReg;
typedef unsigned long prog_uint32_t;


// Default to NOT using PROGMEM here
#ifndef FASTLED_USE_PROGMEM
# define FASTLED_USE_PROGMEM 0
#endif

#ifndef FASTLED_ALLOW_INTERRUPTS
# define FASTLED_ALLOW_INTERRUPTS 1
# define INTERRUPT_THRESHOLD 0
#endif

#define NEED_CXX_BITS

// These can be overridden
#   define FASTLED_ESP32_RAW_PIN_ORDER

// #define cli() os_intr_lock();
// #define sei() os_intr_lock();
