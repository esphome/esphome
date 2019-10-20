#ifndef __INC_LED_SYSDEFS_ARM_K66_H
#define __INC_LED_SYSDEFS_ARM_K66_H

#define FASTLED_TEENSY3
#define FASTLED_ARM

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 1
#endif

// Default to allowing interrupts
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 1
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

#if (F_CPU == 192000000)
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


// Default to using PROGMEM, since TEENSY3 provides it
// even though all it does is ignore it.  Just being
// conservative here in case TEENSY3 changes.
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 1
#endif


#endif
