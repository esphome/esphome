#ifndef __INC_FASTPIN_ARM_D51_H
#define __INC_FASTPIN_ARM_D51_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

/// Template definition for STM32 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data output, set output, clear output, toggle output, input, and direction

template<uint8_t PIN, uint8_t _BIT, uint32_t _MASK, int _GRP> class _ARMPIN {
public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  #if 0
  inline static void setOutput() {
    if(_BIT<8) {
      _CRL::r() = (_CRL::r() & (0xF << (_BIT*4)) | (0x1 << (_BIT*4));
    } else {
      _CRH::r() = (_CRH::r() & (0xF << ((_BIT-8)*4))) | (0x1 << ((_BIT-8)*4));
    }
  }
  inline static void setInput() { /* TODO */ } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }
  #endif

  inline static void setOutput() { pinMode(PIN, OUTPUT); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
  inline static void setInput() { pinMode(PIN, INPUT); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

  inline static void hi() __attribute__ ((always_inline)) { PORT->Group[_GRP].OUTSET.reg = _MASK; }
  inline static void lo() __attribute__ ((always_inline)) { PORT->Group[_GRP].OUTCLR.reg = _MASK; }
  inline static void set(register port_t val) __attribute__ ((always_inline)) { PORT->Group[_GRP].OUT.reg = val; }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { PORT->Group[_GRP].OUTTGL.reg = _MASK; }

  inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) { return PORT->Group[_GRP].OUT.reg | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return PORT->Group[_GRP].OUT.reg & ~_MASK; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return &PORT->Group[_GRP].OUT.reg; }
  inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &PORT->Group[_GRP].OUTSET.reg; }
  inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &PORT->Group[_GRP].OUTCLR.reg; }
  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

#define _R(T) struct __gen_struct_ ## T
#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline volatile PortGroup * r() { return T; } };

#define _IO32(L) _RD32(GPIO ## L)

#define _DEFPIN_ARM(PIN, L, BIT) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, L> {};

// Actual pin definitions
#if defined(ADAFRUIT_ITSYBITSY_M4_EXPRESS)

#define MAX_PIN 19
// D0-D13, including D6+D8 (DotStar CLK + DATA)
_DEFPIN_ARM( 0, 0, 16); _DEFPIN_ARM( 1, 0, 17); _DEFPIN_ARM( 2, 0,  7); _DEFPIN_ARM( 3, 1, 22);
_DEFPIN_ARM( 4, 0, 14); _DEFPIN_ARM( 5, 0, 15); _DEFPIN_ARM( 6, 1,  2); _DEFPIN_ARM( 7, 0, 18);
_DEFPIN_ARM( 8, 1,  3); _DEFPIN_ARM( 9, 0, 19); _DEFPIN_ARM(10, 0, 20); _DEFPIN_ARM(11, 0, 21);
_DEFPIN_ARM(12, 0, 23); _DEFPIN_ARM(13, 0, 22);
// A0-A5
_DEFPIN_ARM(14, 0,  2); _DEFPIN_ARM(15, 0,  5); _DEFPIN_ARM(16, 1,  8); _DEFPIN_ARM(17, 1,  9);
_DEFPIN_ARM(18, 0,  4); _DEFPIN_ARM(19, 0,  6); /* A6 is present in variant.h but couldn't find it on the schematic */
// SDA/SCL
_DEFPIN_ARM(21, 0, 12); _DEFPIN_ARM(22, 0, 13);

// 23..25  MISO/SCK/MOSI
_DEFPIN_ARM(23, 1, 23); _DEFPIN_ARM(24, 0,  1); _DEFPIN_ARM(25, 0,  0);

#define SPI_DATA 25
#define SPI_CLOCK 24

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ADAFRUIT_FEATHER_M4_EXPRESS)

#define MAX_PIN 19
// D0-D13, including D8 (neopixel)  no pins 2 3
_DEFPIN_ARM( 0, 1, 17); _DEFPIN_ARM( 1, 1, 16);
_DEFPIN_ARM( 4, 0, 14); _DEFPIN_ARM( 5, 0, 16); _DEFPIN_ARM( 6, 0,  18);
_DEFPIN_ARM( 8, 1,  3); _DEFPIN_ARM( 9, 0, 19); _DEFPIN_ARM(10, 0, 20); _DEFPIN_ARM(11, 0, 21);
_DEFPIN_ARM(12, 0, 22); _DEFPIN_ARM(13, 0, 23);
// A0-A5
_DEFPIN_ARM(14, 0,  2); _DEFPIN_ARM(15, 0,  5); _DEFPIN_ARM(16, 1,  8); _DEFPIN_ARM(17, 1,  9);
_DEFPIN_ARM(18, 0,  4); _DEFPIN_ARM(19, 0,  6); /* A6 is present in variant.h but couldn't find it on the schematic */
// SDA/SCL
_DEFPIN_ARM(21, 0, 12); _DEFPIN_ARM(22, 0, 13);
// 23..25  MISO/MOSI/SCK
_DEFPIN_ARM(23, 1, 22); _DEFPIN_ARM(24, 1,  23); _DEFPIN_ARM(25, 0,  17);

#define SPI_DATA 24
#define SPI_CLOCK 25

#define HAS_HARDWARE_PIN_SUPPORT 1
#endif



#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END


#endif // __INC_FASTPIN_ARM_D51_H
