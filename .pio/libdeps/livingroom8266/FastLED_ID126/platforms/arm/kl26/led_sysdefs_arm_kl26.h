#ifndef __INC_LED_SYSDEFS_ARM_KL26_H
#define __INC_LED_SYSDEFS_ARM_KL26_H

#define FASTLED_TEENSYLC
#define FASTLED_ARM
#define FASTLED_ARM_M0_PLUS

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 1
#endif

#define FASTLED_SPI_BYTE_ONLY

// Default to allowing interrupts
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 1
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

#if (F_CPU == 96000000)
#define CLK_DBL 1
#endif

// Get some system include files
#include <avr/io.h>
#include <avr/interrupt.h> // for cli/se definitions

// Define the register types
#if defined(ARDUINO) // && ARDUINO < 150
typedef volatile       uint8_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile       uint8_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */
#endif

extern volatile uint32_t systick_millis_count;
#  define MS_COUNTER systick_millis_count

// Default to using PROGMEM since TEENSYLC provides it
// even though all it does is ignore it.  Just being
// conservative here in case TEENSYLC changes.
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 1
#endif

#endif
