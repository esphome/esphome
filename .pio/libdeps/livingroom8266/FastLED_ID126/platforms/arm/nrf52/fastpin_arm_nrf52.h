#ifndef __FASTPIN_ARM_NRF52_H
#define __FASTPIN_ARM_NRF52_H

    
/*
//
// Background:
// ===========
// the nRF52 has more than 32 ports, and thus must support
// two distinct GPIO port registers.  
//
// For the nRF52 series, the structure to control the port is
// `NRF_GPIO_Type`, with separate addresses mapped for set, clear, etc.
// The two ports are defined as NRF_P0 and NRF_P1.
// An example declaration for the ports is:
//     #define NRF_P0_BASE   0x50000000UL
//     #define NRF_P1_BASE   0x50000300UL
//     #define NRF_P0        ((NRF_GPIO_Type*)NRF_P0_BASE)
//     #define NRF_P1        ((NRF_GPIO_Type*)NRF_P1_BASE)
//
// Therefore, ideally, the _DEFPIN_ARM() macro would simply
// conditionally pass either NRF_P0 or NRF_P1 to the underlying
// FastPin<> template class class.
//
// The "pin" provided to the FastLED<> template (and which
// the _DEFPIN_ARM() macro specializes for valid pins) is NOT
// the microcontroller port.pin, but the Arduino digital pin.
// Some boards have an identity mapping (e.g., nRF52832 Feather)
// but most do not.  Therefore, the _DEFPIN_ARM() macro
// must translate the Arduino pin to the mcu port.pin.
//
// 
// Difficulties:
// =============
// The goal is to avoid any such lookups, using compile-time
// optimized functions for speed, in line with FastLED's
// overall design goals. This means constexpr, compile-time
// and aggressive inlining of functions....
//
// Right away, this precludes the use of g_ADigitalPinMap,
// which is not constexpr, and thus not available for
// preprocessor/compile-time optimizations.  Therefore,
// we have to specialize FastPin<uint8_t PIN>, given a
// compile-time value for PIN, into at least a PORT and
// a BITMASK for the port.
//
// Arduino compiles using C++11 for at least Feather nRF52840 Express.
// C++11 is very restrictive about template parameters.
// Template parameters can only be:
// 1. a type (as most people expect)
// 2. a template
// 3. a constexpr native integer type
//
// Therefore, attempts to use `NRF_GPIO_Type *` as a
// template parameter will fail....
//
// Solution:
// =========
// The solution chosen is to define a unique structure for each port,
// whose SOLE purpose is to have a static inline function that
// returns the `NRF_GPIO_Type *` that is needed.
//
// Thus, while it's illegal to pass `NRF_P0` as a template
// parameter, it's perfectly legal to pass `__generated_struct_NRF_P0`,
// and have the template call a well-known `static inline` function
// that returns `NRF_P0` ... which is itself a compile-time constant.
//
// Note that additional magic can be applied that will automatically
// generate the structures.  If you want to add that to this platform,
// check out the KL26 platform files for a starting point.
//
*/

// manually define two structures, to avoid fighting with preprocessor macros
struct __generated_struct_NRF_P0 {
    FASTLED_NRF52_INLINE_ATTRIBUTE constexpr static NRF_GPIO_Type * r() {
        return NRF_P0;
    }
};
struct __generated_struct_NRF_P1 {
    FASTLED_NRF52_INLINE_ATTRIBUTE constexpr static NRF_GPIO_Type * r() {
        return NRF_P1;
    }
};


// The actual class template can then use a typename, for what is essentially a constexpr NRF_GPIO_Type*
template <uint32_t _MASK, typename _PORT, uint8_t _PORT_NUMBER, uint8_t _PIN_NUMBER> class _ARMPIN  {
public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  FASTLED_NRF52_INLINE_ATTRIBUTE static void       setOutput() {
    // OK for this to be more than one instruction, as unusual to quickly switch input/output modes
    nrf_gpio_cfg(
        nrf_pin(),
        NRF_GPIO_PIN_DIR_OUTPUT,        // set pin as output
        NRF_GPIO_PIN_INPUT_DISCONNECT,  // disconnect the input buffering
        NRF_GPIO_PIN_NOPULL,            // neither pull-up nor pull-down resistors enabled
        NRF_GPIO_PIN_H0H1,              // high drive mode required for faster speeds
        NRF_GPIO_PIN_NOSENSE            // pin sense level disabled
        );
  }
  FASTLED_NRF52_INLINE_ATTRIBUTE static void       setInput()  {
    // OK for this to be more than one instruction, as unusual to quickly switch input/output modes
    nrf_gpio_cfg(
        nrf_pin(),
        NRF_GPIO_PIN_DIR_INPUT,         // set pin as input
        NRF_GPIO_PIN_INPUT_DISCONNECT,  // disconnect the input buffering
        NRF_GPIO_PIN_NOPULL,            // neither pull-up nor pull-down resistors enabled
        NRF_GPIO_PIN_H0H1,              // high drive mode required for faster speeds
        NRF_GPIO_PIN_NOSENSE            // pin sense level disabled
        );
  }
  FASTLED_NRF52_INLINE_ATTRIBUTE static void       hi()        { _PORT::r()->OUTSET = _MASK;            } // sets _MASK in the SET   OUTPUT register (output set high)
  FASTLED_NRF52_INLINE_ATTRIBUTE static void       lo()        { _PORT::r()->OUTCLR = _MASK;            } // sets _MASK in the CLEAR OUTPUT register (output set low)
  FASTLED_NRF52_INLINE_ATTRIBUTE static void       toggle()    { _PORT::r()->OUT ^= _MASK;              } // toggles _MASK bits in the OUTPUT GPIO port directly
  FASTLED_NRF52_INLINE_ATTRIBUTE static void       strobe()    { toggle();     toggle();                } // BUGBUG -- Is this used by FastLED?  Without knowing (for example) SPI Speed?
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     hival()     { return _PORT::r()->OUT | _MASK;        } // sets all _MASK bit(s) in the OUTPUT GPIO port to 1
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     loval()     { return _PORT::r()->OUT & ~_MASK;       } // sets all _MASK bit(s) in the OUTPUT GPIO port to 0
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t port()      { return &(_PORT::r()->OUT);             } // gets raw pointer to OUTPUT          GPIO port
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t cport()     { return &(_PORT::r()->OUTCLR);          } // gets raw pointer to SET   DIRECTION GPIO port
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t sport()     { return &(_PORT::r()->OUTSET);          } // gets raw pointer to CLEAR DIRECTION GPIO port
  FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     mask()      { return _MASK;                          } // gets the value of _MASK
  FASTLED_NRF52_INLINE_ATTRIBUTE static void hi (register port_ptr_t port) { hi();                      } // sets _MASK in the SET   OUTPUT register (output set high)
  FASTLED_NRF52_INLINE_ATTRIBUTE static void lo (register port_ptr_t port) { lo();                      } // sets _MASK in the CLEAR OUTPUT register (output set low)
  FASTLED_NRF52_INLINE_ATTRIBUTE static void set(register port_t     val ) { _PORT::r()->OUT = val;     } // sets entire port's value (optimization used by FastLED)
  FASTLED_NRF52_INLINE_ATTRIBUTE static void fastset(register port_ptr_t port, register port_t val) { *port = val; }
  constexpr                      static uint32_t   nrf_pin2() { return NRF_GPIO_PIN_MAP(_PORT_NUMBER, _PIN_NUMBER); }
  constexpr                      static bool       LowSpeedOnlyRecommended() {
    // only allow one function body.
    #undef _FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT

    // unique cases for each board / processor package / module?
    #if defined(NRF52810_XXAA) && defined(NRF52810_PACKAGE_QFN48)
        #if defined(_FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT)
            #error "Multiple board match"
        #endif
        #define _FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT 1
        static_assert(_PORT_NUMBER == 0, "nRF52810 only has one port");
        return (
            (_PIN_NUMBER == 25) ||
            (_PIN_NUMBER == 26) ||
            (_PIN_NUMBER == 27) ||
            (_PIN_NUMBER == 28) ||
            (_PIN_NUMBER == 29)
            );
    #endif
    #if defined(NRF52810_XXAA) && defined(NRF52810_PACKAGE_QFN32)
        #if defined(_FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT)
            #error "Multiple board match"
        #endif
        #define _FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT 1
        static_assert(_PORT_NUMBER == 0, "nRF52810 only has one port");
        if (_PORT_NUMBER == 0) {
            if (
                (_PIN_NUMBER == 26) ||
                (_PIN_NUMBER == 27)
                ) {
                return true;
            }
        }
        return false;
    #endif
    #if defined(NRF52832_XXAA) || defined(NRF52832_XXAB)
        #if defined(_FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT)
            #error "Multiple board match"
        #endif
        #define _FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT 1
        static_assert(_PORT_NUMBER == 0, "nRF52832 only has one port");
        // data sheets shows the same pins in both QFN48 and WLCSP package
        // are recommended as low-speed only:
        return (
            (_PIN_NUMBER == 22) ||
            (_PIN_NUMBER == 23) ||
            (_PIN_NUMBER == 24) ||
            (_PIN_NUMBER == 25) ||
            (_PIN_NUMBER == 26) ||
            (_PIN_NUMBER == 27) ||
            (_PIN_NUMBER == 28) ||
            (_PIN_NUMBER == 29) ||
            (_PIN_NUMBER == 30) ||
            (_PIN_NUMBER == 31)
            );
    #endif
    #if defined(NRF52840_XXAA) && defined(NRF52840_PACKAGE_aQFN73)
        #if defined(_FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT)
            #error "Multiple board match"
        #endif
        #define _FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT 1
        static_assert(_PORT_NUMBER == 0 || _PORT_NUMBER == 1, "nRF52840 only has two ports");
        return
            (
                (
                    (_PORT_NUMBER == 0) &&
                    (
                        (_PIN_NUMBER ==  2) ||
                        (_PIN_NUMBER ==  3) ||
                        (_PIN_NUMBER ==  9) ||
                        (_PIN_NUMBER == 10) ||
                        (_PIN_NUMBER == 11) ||
                        (_PIN_NUMBER == 12) ||
                        (_PIN_NUMBER == 14) ||
                        (_PIN_NUMBER == 28) ||
                        (_PIN_NUMBER == 29) ||
                        (_PIN_NUMBER == 30) ||
                        (_PIN_NUMBER == 31)
                    )
                )
                ||
                (
                    (_PORT_NUMBER == 1) &&
                    (
                        (_PIN_NUMBER ==  2) ||
                        (_PIN_NUMBER ==  3) ||
                        (_PIN_NUMBER ==  4) ||
                        (_PIN_NUMBER ==  5) ||
                        (_PIN_NUMBER ==  6) ||
                        (_PIN_NUMBER ==  7) ||
                        (_PIN_NUMBER == 10) ||
                        (_PIN_NUMBER == 13) ||
                        (_PIN_NUMBER == 15)
                    )
                )
            );
    #endif
    #if false && defined(NRF52840_XXAA) && (defined(NRF52840_PACKAGE_aQFN73) || defined(ARDUINO_NRF52840_FEATHER))
        // Adafruit nRF52840 feather uses RAYTAC MDBT50Q module, which is aQFN73
        // See https://cdn-learn.adafruit.com/assets/assets/000/068/544/original/Raytac_MDBT50Q.pdf
        #if defined(_FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT)
            #error "Multiple board match"
        #endif
        #define _FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT 1
        static_assert(_PORT_NUMBER == 0 || _PORT_NUMBER == 1, "nRF52840 only has two ports");
        return
            (
                (
                    (_PORT_NUMBER == 0) &&
                    (
                        (_PIN_NUMBER ==  2) ||
                        (_PIN_NUMBER ==  3) ||
                        (_PIN_NUMBER ==  9) ||
                        (_PIN_NUMBER == 10) ||
                        (_PIN_NUMBER == 28) ||
                        (_PIN_NUMBER == 29) ||
                        (_PIN_NUMBER == 30) ||
                        (_PIN_NUMBER == 31)
                    )
                )
                ||
                (
                    (_PORT_NUMBER == 1) &&
                    (
                        (_PIN_NUMBER ==  1) ||
                        (_PIN_NUMBER ==  2) ||
                        (_PIN_NUMBER ==  3) ||
                        (_PIN_NUMBER ==  4) ||
                        (_PIN_NUMBER ==  5) ||
                        (_PIN_NUMBER ==  6) ||
                        (_PIN_NUMBER ==  7) ||
                        (_PIN_NUMBER == 10) ||
                        (_PIN_NUMBER == 11) ||
                        (_PIN_NUMBER == 12) ||
                        (_PIN_NUMBER == 13) ||
                        (_PIN_NUMBER == 14) ||
                        (_PIN_NUMBER == 15)
                    )
                )
            );
    #endif
    #if !defined(_FASTLED_NRF52_LOW_SPEED_ONLY_BOARD_DETECT)
        #warning "Unknown board / package, ... caller must pins support high-speed"
        return false; // choosing default to be FALSE, to allow users to ATTEMPT to use high-speed on pins where support is not known
    #endif
  }
  // Expose the nrf pin (port/pin combined), port, and pin as properties (e.g., for setting up SPI)

  FASTLED_NRF52_INLINE_ATTRIBUTE static uint32_t   nrf_pin()  { return NRF_GPIO_PIN_MAP(_PORT_NUMBER, _PIN_NUMBER); }
};

//
// BOARD_PIN can be either the pin portion of a port.pin, or the combined NRF_GPIO_PIN_MAP() number.
// For example both the following two defines refer to P1.15 (pin 47) as Arduino pin 3:
//     _DEFPIN_ARM(3, 1, 15);
//     _DEFPIN_ARM(3, 1, 47);
//
// Similarly, the following defines are all equivalent:
//     _DEFPIN_ARM_IDENTITY_P1(47);
//     _DEFPIN_ARM(47, 1, 15);
//     _DEFPIN_ARM(47, 1, 47);
//

#define _DEFPIN_ARM(ARDUINO_PIN, BOARD_PORT, BOARD_PIN)  \
    template<> class FastPin<ARDUINO_PIN> :              \
    public _ARMPIN<                                      \
        1u << (BOARD_PIN & 31u),                         \
        __generated_struct_NRF_P ## BOARD_PORT,          \
        (BOARD_PIN / 32),                                \
        BOARD_PIN & 31u                                  \
        >                                                \
    {}

#define _DEFPIN_ARM_IDENTITY_P0(ARDUINO_PIN)      \
    template<> class FastPin<ARDUINO_PIN> :       \
    public _ARMPIN<                               \
        1u << (ARDUINO_PIN & 31u),                \
        __generated_struct_NRF_P0,                \
        0,                                        \
        (ARDUINO_PIN & 31u) + 0                   \
        >                                         \
    {}

#define _DEFPIN_ARM_IDENTITY_P1(ARDUINO_PIN)      \
    template<> class FastPin<ARDUINO_PIN> :       \
    public _ARMPIN<                               \
        1u << (ARDUINO_PIN & 31u),                \
        __generated_struct_NRF_P1,                \
        1,                                        \
        (ARDUINO_PIN & 31u) + 32                  \
        >                                         \
    {}

// The actual pin definitions are in a separate header file...
#include "fastpin_arm_nrf52_variants.h"

#define HAS_HARDWARE_PIN_SUPPORT

#endif // #ifndef __FASTPIN_ARM_NRF52_H
