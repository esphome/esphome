#ifndef __INC_LED_SYSDEFS_AVR_H
#define __INC_LED_SYSDEFS_AVR_H

#define FASTLED_AVR

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 2
#endif

#define FASTLED_SPI_BYTE_ONLY

#include <avr/io.h>
#include <avr/interrupt.h> // for cli/se definitions

// Define the register types
typedef volatile       uint8_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile       uint8_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */


// Default to disallowing interrupts (may want to gate this on teensy2 vs. other arm platforms, since the
// teensy2 has a good, fast millis interrupt implementation)
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif


// Default to using PROGMEM here
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 1
#endif

#if defined(ARDUINO_AVR_DIGISPARK) || defined(ARDUINO_AVR_DIGISPARKPRO)
#ifndef NO_CORRECTION
#define NO_CORRECTION 1
#endif
#endif

extern "C" {
#  if defined(CORE_TEENSY) || defined(TEENSYDUINO)
extern volatile unsigned long timer0_millis_count;
#    define MS_COUNTER timer0_millis_count
#  elif defined(ATTINY_CORE)
extern volatile unsigned long millis_timer_millis;
#    define MS_COUNTER millis_timer_millis
#  else
extern volatile unsigned long timer0_millis;
#    define MS_COUNTER timer0_millis
#  endif
};

// special defs for the tiny environments
#if defined(__AVR_ATmega32U2__) || defined(__AVR_ATmega16U2__) || defined(__AVR_ATmega8U2__) || defined(__AVR_AT90USB162__) || defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny167__) || defined(__AVR_ATtiny87__) || defined(__AVR_ATtinyX41__) || defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny441__)
#define LIB8_ATTINY 1
#define FASTLED_NEEDS_YIELD
#endif

#if defined(ARDUINO) && (ARDUINO > 150) && !defined(IS_BEAN) && !defined (ARDUINO_AVR_DIGISPARK) && !defined (LIB8_TINY) && !defined (ARDUINO_AVR_LARDU_328E)
// don't need YIELD defined by the library 
#else 
#define FASTLED_NEEDS_YIELD
extern "C" void yield();
#endif
#endif
