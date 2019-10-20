#ifndef __INC_LED_SYSDEFS_ARM_SAM_H
#define __INC_LED_SYSDEFS_ARM_SAM_H

#if defined(STM32F10X_MD)

 #include <application.h>

 #define FASTLED_NAMESPACE_BEGIN namespace NSFastLED {
 #define FASTLED_NAMESPACE_END }
 #define FASTLED_USING_NAMESPACE using namespace NSFastLED;

 // reusing/abusing cli/sei defs for due
 #define cli()  __disable_irq(); __disable_fault_irq();
 #define sei() __enable_irq(); __enable_fault_irq();

#elif defined (__STM32F1__)

 #include "cm3_regs.h"

 #define cli() nvic_globalirq_disable()
 #define sei() nvic_globalirq_enable()

#else
 #error "Platform not supported"
#endif

#define FASTLED_ARM

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 1
#endif

// Default to allowing interrupts
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

// pgmspace definitions
#define PROGMEM
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define pgm_read_dword_near(addr) pgm_read_dword(addr)

// Default to NOT using PROGMEM here
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

// data type defs
typedef volatile       uint8_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile       uint8_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */

#define FASTLED_NO_PINMAP

#ifndef F_CPU
 #define F_CPU 72000000
#endif
#endif
