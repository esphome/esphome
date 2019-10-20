#ifndef __INC_FASTSPI_H
#define __INC_FASTSPI_H

#include "FastLED.h"

#include "controller.h"
#include "lib8tion.h"

#include "fastspi_bitbang.h"

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_TEENSY3) && (F_CPU > 48000000)
#define DATA_RATE_MHZ(X) (((48000000L / 1000000L) / X))
#define DATA_RATE_KHZ(X) (((48000000L / 1000L) / X))
#else
#define DATA_RATE_MHZ(X) ((F_CPU / 1000000L) / X)
#define DATA_RATE_KHZ(X) ((F_CPU / 1000L) / X)
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// External SPI template definition with partial instantiation(s) to map to hardware SPI ports on platforms/builds where the pin
// mappings are known at compile time.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(FASTLED_ALL_PINS_HARDWARE_SPI)
template<uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class SPIOutput : public AVRSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

template<uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class SoftwareSPIOutput : public AVRSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#ifndef FASTLED_FORCE_SOFTWARE_SPI

#if defined(NRF51) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)
template<uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class SPIOutput : public NRF51SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

#if defined(NRF52_SERIES) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)
template<uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class SPIOutput : public NRF52SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

#if defined(SPI_DATA) && defined(SPI_CLOCK)

#if defined(FASTLED_TEENSY3) && defined(ARM_HARDWARE_SPI)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED, 0x4002C000> {};

#if defined(SPI2_DATA)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI2_DATA, SPI2_CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<SPI2_DATA, SPI2_CLOCK, SPI_SPEED, 0x4002C000> {};

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI2_CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<SPI_DATA, SPI2_CLOCK, SPI_SPEED, 0x4002C000> {};

template<uint8_t SPI_SPEED>
class SPIOutput<SPI2_DATA, SPI_CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<SPI2_DATA, SPI_CLOCK, SPI_SPEED, 0x4002C000> {};
#endif

#elif defined(FASTLED_TEENSYLC) && defined(ARM_HARDWARE_SPI)

#define DECLARE_SPI0(__DATA,__CLOCK) template<uint8_t SPI_SPEED>\
 class SPIOutput<__DATA, __CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<__DATA, __CLOCK, SPI_SPEED, 0x40076000> {};
 #define DECLARE_SPI1(__DATA,__CLOCK) template<uint8_t SPI_SPEED>\
  class SPIOutput<__DATA, __CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<__DATA, __CLOCK, SPI_SPEED, 0x40077000> {};

DECLARE_SPI0(7,13);
DECLARE_SPI0(8,13);
DECLARE_SPI0(11,13);
DECLARE_SPI0(12,13);
DECLARE_SPI0(7,14);
DECLARE_SPI0(8,14);
DECLARE_SPI0(11,14);
DECLARE_SPI0(12,14);
DECLARE_SPI1(0,20);
DECLARE_SPI1(1,20);
DECLARE_SPI1(21,20);

#elif defined(__SAM3X8E__)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public SAMHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> {};

#elif defined(AVR_HARDWARE_SPI)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public AVRHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> {};

#if defined(SPI_UART0_DATA)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_UART0_DATA, SPI_UART0_CLOCK, SPI_SPEED> : public AVRUSART0SPIOutput<SPI_UART0_DATA, SPI_UART0_CLOCK, SPI_SPEED> {};

#endif

#if defined(SPI_UART1_DATA)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_UART1_DATA, SPI_UART1_CLOCK, SPI_SPEED> : public AVRUSART1SPIOutput<SPI_UART1_DATA, SPI_UART1_CLOCK, SPI_SPEED> {};

#endif

#endif

#else
#  if !defined(FASTLED_INTERNAL) && !defined(FASTLED_ALL_PINS_HARDWARE_SPI)
#    ifdef FASTLED_HAS_PRAGMA_MESSAGE
#      pragma message "No hardware SPI pins defined.  All SPI access will default to bitbanged output"
#    else
#      warning "No hardware SPI pins defined.  All SPI access will default to bitbanged output"
#    endif
#  endif
#endif

// #if defined(USART_DATA) && defined(USART_CLOCK)
// template<uint8_t SPI_SPEED>
// class AVRSPIOutput<USART_DATA, USART_CLOCK, SPI_SPEED> : public AVRUSARTSPIOutput<USART_DATA, USART_CLOCK, SPI_SPEED> {};
// #endif

#else
#  if !defined(FASTLED_INTERNAL) && !defined(FASTLED_ALL_PINS_HARDWARE_SPI)
#    ifdef FASTLED_HAS_PRAGMA_MESSAGE
#      pragma message "Forcing software SPI - no hardware SPI for you!"
#    else
#      warning "Forcing software SPI - no hardware SPI for you!"
#    endif
#  endif
#endif

FASTLED_NAMESPACE_END

#endif
