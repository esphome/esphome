#ifndef __FASTPIN_ARM_STM32_H
#define __FASTPIN_ARM_STM32_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be sloightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

/// Template definition for STM32 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data output, set output, clear output, toggle output, input, and direction

template<uint8_t PIN, uint8_t _BIT, uint32_t _MASK, typename _GPIO> class _ARMPIN {
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

  inline static void hi() __attribute__ ((always_inline)) { _GPIO::r()->BSRR = _MASK; }
  inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BRR = _MASK; }
  // inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BSRR = (_MASK<<16); }
  inline static void set(register port_t val) __attribute__ ((always_inline)) { _GPIO::r()->ODR = val; }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { if(_GPIO::r()->ODR & _MASK) { lo(); } else { hi(); } }

  inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) { return _GPIO::r()->ODR | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return _GPIO::r()->ODR & ~_MASK; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_GPIO::r()->ODR; }
  inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRR; }
  inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPIO::r()->BRR; }
  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

#if defined(STM32F10X_MD)
 #define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline volatile GPIO_TypeDef * r() { return T; } };
 #define _IO32(L) _RD32(GPIO ## L)
#elif defined(__STM32F1__)
 #define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline gpio_reg_map* r() { return T->regs; } };
 #define _IO32(L) _RD32(GPIO ## L)
#else
 #error "Platform not supported"
#endif

#define _R(T) struct __gen_struct_ ## T
#define _DEFPIN_ARM(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, _R(GPIO ## L)> {};

// Actual pin definitions
#if defined(SPARK) // Sparkfun STM32F103 based board

_IO32(A); _IO32(B); _IO32(C); _IO32(D); _IO32(E); _IO32(F); _IO32(G);


#define MAX_PIN 19
_DEFPIN_ARM(0, 7, B);
_DEFPIN_ARM(1, 6, B);
_DEFPIN_ARM(2, 5, B);
_DEFPIN_ARM(3, 4, B);
_DEFPIN_ARM(4, 3, B);
_DEFPIN_ARM(5, 15, A);
_DEFPIN_ARM(6, 14, A);
_DEFPIN_ARM(7, 13, A);
_DEFPIN_ARM(8, 8, A);
_DEFPIN_ARM(9, 9, A);
_DEFPIN_ARM(10, 0, A);
_DEFPIN_ARM(11, 1, A);
_DEFPIN_ARM(12, 4, A);
_DEFPIN_ARM(13, 5, A);
_DEFPIN_ARM(14, 6, A);
_DEFPIN_ARM(15, 7, A);
_DEFPIN_ARM(16, 0, B);
_DEFPIN_ARM(17, 1, B);
_DEFPIN_ARM(18, 3, A);
_DEFPIN_ARM(19, 2, A);


#define SPI_DATA 15
#define SPI_CLOCK 13

#define HAS_HARDWARE_PIN_SUPPORT

#endif // SPARK

#if defined(__STM32F1__) // Generic STM32F103 aka "Blue Pill"

_IO32(A); _IO32(B); _IO32(C);

#define MAX_PIN 46

_DEFPIN_ARM(10, 0, A);	// PA0 - PA7
_DEFPIN_ARM(11, 1, A);
_DEFPIN_ARM(12, 2, A);
_DEFPIN_ARM(13, 3, A);
_DEFPIN_ARM(14, 4, A);
_DEFPIN_ARM(15, 5, A);
_DEFPIN_ARM(16, 6, A);
_DEFPIN_ARM(17, 7, A);
_DEFPIN_ARM(29, 8, A);	// PA8 - PA15
_DEFPIN_ARM(30, 9, A);
_DEFPIN_ARM(31, 10, A);
_DEFPIN_ARM(32, 11, A);
_DEFPIN_ARM(33, 12, A);
_DEFPIN_ARM(34, 13, A);
_DEFPIN_ARM(37, 14, A);
_DEFPIN_ARM(38, 15, A);

_DEFPIN_ARM(18, 0, B);	// PB0 - PB11
_DEFPIN_ARM(19, 1, B);
_DEFPIN_ARM(20, 2, B);
_DEFPIN_ARM(39, 3, B);
_DEFPIN_ARM(40, 4, B);
_DEFPIN_ARM(41, 5, B);
_DEFPIN_ARM(42, 6, B);
_DEFPIN_ARM(43, 7, B);
_DEFPIN_ARM(45, 8, B);
_DEFPIN_ARM(46, 9, B);
_DEFPIN_ARM(21, 10, B);
_DEFPIN_ARM(22, 11, B);

_DEFPIN_ARM(2, 13, C);	// PC13 - PC15
_DEFPIN_ARM(3, 14, C);
_DEFPIN_ARM(4, 15, C);

#define SPI_DATA BOARD_SPI1_MOSI_PIN
#define SPI_CLOCK BOARD_SPI1_SCK_PIN

#define HAS_HARDWARE_PIN_SUPPORT

#endif // __STM32F1__

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END

#endif // __INC_FASTPIN_ARM_STM32
