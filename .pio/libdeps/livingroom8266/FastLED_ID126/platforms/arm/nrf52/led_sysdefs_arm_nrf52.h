#ifndef __LED_SYSDEFS_ARM_NRF52
#define __LED_SYSDEFS_ARM_NRF52

#define FASTLED_ARM

#ifndef F_CPU
    #define F_CPU 64000000 // the NRF52 series has a 64MHz CPU
#endif

// even though CPU is at 64MHz, use the 8MHz-defined timings because...
// PWM module   runs at 16MHz
// SPI0..2      runs at  8MHz
#define CLOCKLESS_FREQUENCY 16000000 // the NRF52 has EasyDMA for PWM module at 16MHz

#ifndef F_TIMER
    #define F_TIMER 16000000 // the NRF52 timer is 16MHz, even though CPU is 64MHz
#endif

#if !defined(FASTLED_USE_PROGMEM)
    #define FASTLED_USE_PROGMEM 0 // nRF52 series have flat memory model
#endif

#if !defined(FASTLED_ALLOW_INTERRUPTS)
    #define FASTLED_ALLOW_INTERRUPTS 1
#endif

// Use PWM instance 0
// See clockless_arm_nrf52.h and (in root of library) platforms.cpp
#define FASTLED_NRF52_ENABLE_PWM_INSTANCE0

#if defined(FASTLED_NRF52_NEVER_INLINE)
    #define FASTLED_NRF52_INLINE_ATTRIBUTE __attribute__((always_inline)) inline
#else     
    #define FASTLED_NRF52_INLINE_ATTRIBUTE __attribute__((always_inline)) inline
#endif    



#include <nrf.h>
#include <nrf_spim.h>   // for FastSPI
#include <nrf_pwm.h>    // for Clockless
#include <nrf_nvic.h>   // for Clockless / anything else using interrupts
typedef __I  uint32_t RoReg;
typedef __IO uint32_t RwReg;

#define cli()  __disable_irq()
#define sei()  __enable_irq()

#define FASTLED_NRF52_DEBUGPRINT(format, ...)
//#define FASTLED_NRF52_DEBUGPRINT(format, ...)\
//    do {\
//        FastLED_NRF52_DebugPrint(format, ##__VA_ARGS__);\
//    } while(0);




#endif // __LED_SYSDEFS_ARM_NRF52
