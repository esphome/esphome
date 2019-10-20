#ifndef __INC_FASTPIN_AVR_H
#define __INC_FASTPIN_AVR_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

#define AVR_PIN_CYCLES(_PIN) ((((int)FastPin<_PIN>::port())-0x20 < 64) ? 1 : 2)

/// Class definition for a Pin where we know the port registers at compile time for said pin.  This allows us to make
/// a lot of optimizations, as the inlined hi/lo methods will devolve to a single io register write/bitset.
template<uint8_t PIN, uint8_t _MASK, typename _PORT, typename _DDR, typename _PIN> class _AVRPIN {
public:
	typedef volatile uint8_t * port_ptr_t;
	typedef uint8_t port_t;

	inline static void setOutput() { _DDR::r() |= _MASK; }
	inline static void setInput() { _DDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { _PORT::r() |= _MASK; }
	inline static void lo() __attribute__ ((always_inline)) { _PORT::r() &= ~_MASK; }
	inline static void set(register uint8_t val) __attribute__ ((always_inline)) { _PORT::r() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

	inline static void toggle() __attribute__ ((always_inline)) { _PIN::r() = _MASK; }

	inline static void hi(register port_ptr_t /*port*/) __attribute__ ((always_inline)) { hi(); }
	inline static void lo(register port_ptr_t /*port*/) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(register port_ptr_t /*port*/, register uint8_t val) __attribute__ ((always_inline)) { set(val); }

	inline static port_t hival() __attribute__ ((always_inline)) { return _PORT::r() | _MASK; }
	inline static port_t loval() __attribute__ ((always_inline)) { return _PORT::r() & ~_MASK; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_PORT::r(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};



/// AVR definitions for pins.  Getting around  the fact that I can't pass GPIO register addresses in as template arguments by instead creating
/// a custom type for each GPIO register with a single, static, aggressively inlined function that returns that specific GPIO register.  A similar
/// trick is used a bit further below for the ARM GPIO registers (of which there are far more than on AVR!)
typedef volatile uint8_t & reg8_t;
#define _R(T) struct __gen_struct_ ## T
#define _RD8(T) struct __gen_struct_ ## T { static inline reg8_t r() { return T; }};
#define _IO(L) _RD8(DDR ## L); _RD8(PORT ## L); _RD8(PIN ## L);
#define _DEFPIN_AVR(_PIN, MASK, L) template<> class FastPin<_PIN> : public _AVRPIN<_PIN, MASK, _R(PORT ## L), _R(DDR ## L), _R(PIN ## L)> {};

#if defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny25__)
_IO(B);

#if defined(__AVR_ATtiny25__)
#pragma message "ATtiny25 has very limited storage. This library could use up to more than 100% of its flash size"
#endif

#define MAX_PIN 5

_DEFPIN_AVR(0, 0x01, B); _DEFPIN_AVR(1, 0x02, B); _DEFPIN_AVR(2, 0x04, B); _DEFPIN_AVR(3, 0x08, B);
_DEFPIN_AVR(4, 0x10, B); _DEFPIN_AVR(5, 0x20, B);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny441__)
#define MAX_PIN 11
_IO(A); _IO(B);

_DEFPIN_AVR(0, 0x01, B); _DEFPIN_AVR(1, 0x02, B); _DEFPIN_AVR(2, 0x04, B);
_DEFPIN_AVR(3, 0x80, A); _DEFPIN_AVR(4, 0x40, A); _DEFPIN_AVR(5, 0x20, A);
_DEFPIN_AVR(6, 0x10, A); _DEFPIN_AVR(7, 0x08, A); _DEFPIN_AVR(8, 0x04, A);
_DEFPIN_AVR(9, 0x02, A); _DEFPIN_AVR(10, 0x01, A); _DEFPIN_AVR(11, 0x08, B);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AVR_DIGISPARK) // digispark pin layout
#define MAX_PIN 5
#define HAS_HARDWARE_PIN_SUPPORT 1
_IO(A); _IO(B);

_DEFPIN_AVR(0, 0x01, B); _DEFPIN_AVR(1, 0x02, B); _DEFPIN_AVR(2, 0x04, B);
_DEFPIN_AVR(3, 0x80, A); _DEFPIN_AVR(4, 0x40, A); _DEFPIN_AVR(5, 0x20, A);

#elif defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) 
_IO(A); _IO(B);

#define MAX_PIN 10

_DEFPIN_AVR(0, 0x01, A); _DEFPIN_AVR(1, 0x02, A); _DEFPIN_AVR(2, 0x04, A); _DEFPIN_AVR(3, 0x08, A);
_DEFPIN_AVR(4, 0x10, A); _DEFPIN_AVR(5, 0x20, A); _DEFPIN_AVR(6, 0x40, A); _DEFPIN_AVR(7, 0x80, A);
_DEFPIN_AVR(8, 0x04, B); _DEFPIN_AVR(9, 0x02, B); _DEFPIN_AVR(10, 0x01, B);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AVR_DIGISPARKPRO)

_IO(A); _IO(B);
#define MAX_PIN 12

_DEFPIN_AVR(0, 0x01, B); _DEFPIN_AVR(1, 0x02, B); _DEFPIN_AVR(2, 0x04, B); _DEFPIN_AVR(3, 0x20, B);
_DEFPIN_AVR(4, 0x08, B); _DEFPIN_AVR(5, 0x80, A); _DEFPIN_AVR(6, 0x01, A); _DEFPIN_AVR(7, 0x02, A);
_DEFPIN_AVR(8, 0x04, A); _DEFPIN_AVR(9, 0x08, A); _DEFPIN_AVR(10, 0x10, A); _DEFPIN_AVR(11, 0x20, A);
_DEFPIN_AVR(12, 0x40, A);

#elif defined(__AVR_ATtiny167__) || defined(__AVR_ATtiny87__)
_IO(A); _IO(B);

#define MAX_PIN 15

_DEFPIN_AVR(0, 0x01, A);  _DEFPIN_AVR(1, 0x02, A);   _DEFPIN_AVR(2, 0x04, A);  _DEFPIN_AVR(3, 0x08, A);
_DEFPIN_AVR(4, 0x10, A);  _DEFPIN_AVR(5, 0x20, A);   _DEFPIN_AVR(6, 0x40, A);  _DEFPIN_AVR(7, 0x80, A);
_DEFPIN_AVR(8, 0x01, B);  _DEFPIN_AVR(9, 0x02, B);   _DEFPIN_AVR(10, 0x04, B); _DEFPIN_AVR(11, 0x08, B);
_DEFPIN_AVR(12, 0x10, B); _DEFPIN_AVR(13, 0x20, B); _DEFPIN_AVR(14, 0x40, B); _DEFPIN_AVR(15, 0x80, B);

#define SPI_DATA 4
#define SPI_CLOCK 5
#define AVR_HARDWARE_SPI 1

#define HAS_HARDWARE_PIN_SUPPORT 1
#elif defined(ARDUINO_HOODLOADER2) && (defined(__AVR_ATmega32U2__) || defined(__AVR_ATmega16U2__) || defined(__AVR_ATmega8U2__)) || defined(__AVR_AT90USB82__) || defined(__AVR_AT90USB162__)

_IO(D); _IO(B); _IO(C);

#define MAX_PIN 20

_DEFPIN_AVR( 0, 0x01, B); _DEFPIN_AVR( 1, 0x02, B); _DEFPIN_AVR( 2, 0x04, B); _DEFPIN_AVR( 3, 0x08, B);
_DEFPIN_AVR( 4, 0x10, B); _DEFPIN_AVR( 5, 0x20, B); _DEFPIN_AVR( 6, 0x40, B); _DEFPIN_AVR( 7, 0x80, B);

_DEFPIN_AVR( 8, 0x80, C); _DEFPIN_AVR( 9, 0x40, C); _DEFPIN_AVR( 10, 0x20,C); _DEFPIN_AVR( 11, 0x10, C);
_DEFPIN_AVR( 12, 0x04, C); _DEFPIN_AVR( 13, 0x01, D); _DEFPIN_AVR( 14, 0x02, D); _DEFPIN_AVR(15, 0x04, D);
_DEFPIN_AVR( 16, 0x08, D); _DEFPIN_AVR( 17, 0x10, D); _DEFPIN_AVR( 18, 0x20, D); _DEFPIN_AVR( 19, 0x40, D);
_DEFPIN_AVR( 20, 0x80, D);

#define HAS_HARDWARE_PIN_SUPPORT 1
// #define SPI_DATA 2
// #define SPI_CLOCK 1
// #define AVR_HARDWARE_SPI 1

#elif defined(IS_BEAN)

// Accelerated port definitions for arduino avrs
_IO(D); _IO(B); _IO(C);

#define MAX_PIN 19
_DEFPIN_AVR( 0, 0x40, D); _DEFPIN_AVR( 1, 0x02, B); _DEFPIN_AVR( 2, 0x04, B); _DEFPIN_AVR( 3, 0x08, B);
_DEFPIN_AVR( 4, 0x10, B); _DEFPIN_AVR( 5, 0x20, B); _DEFPIN_AVR( 6, 0x01, D); _DEFPIN_AVR( 7, 0x80, D);
_DEFPIN_AVR( 8, 0x01, B); _DEFPIN_AVR( 9, 0x02, D); _DEFPIN_AVR(10, 0x04, D); _DEFPIN_AVR(11, 0x08, D);
_DEFPIN_AVR(12, 0x10, D); _DEFPIN_AVR(13, 0x20, D); _DEFPIN_AVR(14, 0x01, C); _DEFPIN_AVR(15, 0x02, C);
_DEFPIN_AVR(16, 0x04, C); _DEFPIN_AVR(17, 0x08, C); _DEFPIN_AVR(18, 0x10, C); _DEFPIN_AVR(19, 0x20, C);

#define SPI_DATA 3
#define SPI_CLOCK 5
#define SPI_SELECT 2
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#ifndef __AVR_ATmega8__
#define SPI_UART0_DATA 9
#define SPI_UART0_CLOCK 12
#endif

#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328PB__) || defined(__AVR_ATmega328__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega168P__) || defined(__AVR_ATmega8__)
// Accelerated port definitions for arduino avrs
_IO(D); _IO(B); _IO(C);

#define MAX_PIN 19
_DEFPIN_AVR( 0, 0x01, D); _DEFPIN_AVR( 1, 0x02, D); _DEFPIN_AVR( 2, 0x04, D); _DEFPIN_AVR( 3, 0x08, D);
_DEFPIN_AVR( 4, 0x10, D); _DEFPIN_AVR( 5, 0x20, D); _DEFPIN_AVR( 6, 0x40, D); _DEFPIN_AVR( 7, 0x80, D);
_DEFPIN_AVR( 8, 0x01, B); _DEFPIN_AVR( 9, 0x02, B); _DEFPIN_AVR(10, 0x04, B); _DEFPIN_AVR(11, 0x08, B);
_DEFPIN_AVR(12, 0x10, B); _DEFPIN_AVR(13, 0x20, B); _DEFPIN_AVR(14, 0x01, C); _DEFPIN_AVR(15, 0x02, C);
_DEFPIN_AVR(16, 0x04, C); _DEFPIN_AVR(17, 0x08, C); _DEFPIN_AVR(18, 0x10, C); _DEFPIN_AVR(19, 0x20, C);

#define SPI_DATA 11
#define SPI_CLOCK 13
#define SPI_SELECT 10
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#ifndef __AVR_ATmega8__
#define SPI_UART0_DATA 1
#define SPI_UART0_CLOCK 4
#endif

#elif defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega644P__)

_IO(A); _IO(B); _IO(C); _IO(D);

#define MAX_PIN 31
_DEFPIN_AVR(0, 1<<0, B); _DEFPIN_AVR(1, 1<<1, B); _DEFPIN_AVR(2, 1<<2, B); _DEFPIN_AVR(3, 1<<3, B);
_DEFPIN_AVR(4, 1<<4, B); _DEFPIN_AVR(5, 1<<5, B); _DEFPIN_AVR(6, 1<<6, B); _DEFPIN_AVR(7, 1<<7, B);
_DEFPIN_AVR(8, 1<<0, D); _DEFPIN_AVR(9, 1<<1, D); _DEFPIN_AVR(10, 1<<2, D); _DEFPIN_AVR(11, 1<<3, D);
_DEFPIN_AVR(12, 1<<4, D); _DEFPIN_AVR(13, 1<<5, D); _DEFPIN_AVR(14, 1<<6, D); _DEFPIN_AVR(15, 1<<7, D);
_DEFPIN_AVR(16, 1<<0, C); _DEFPIN_AVR(17, 1<<1, C); _DEFPIN_AVR(18, 1<<2, C); _DEFPIN_AVR(19, 1<<3, C);
_DEFPIN_AVR(20, 1<<4, C); _DEFPIN_AVR(21, 1<<5, C); _DEFPIN_AVR(22, 1<<6, C); _DEFPIN_AVR(23, 1<<7, C);
_DEFPIN_AVR(24, 1<<0, A); _DEFPIN_AVR(25, 1<<1, A); _DEFPIN_AVR(26, 1<<2, A); _DEFPIN_AVR(27, 1<<3, A);
_DEFPIN_AVR(28, 1<<4, A); _DEFPIN_AVR(29, 1<<5, A); _DEFPIN_AVR(30, 1<<6, A); _DEFPIN_AVR(31, 1<<7, A);

#define SPI_DATA 5
#define SPI_CLOCK 7
#define SPI_SELECT 4
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#elif  defined(__AVR_ATmega128RFA1__) || defined(__AVR_ATmega256RFR2__)

// AKA the Pinoccio

_IO(A); _IO(B); _IO(C); _IO(D); _IO(E); _IO(F);

_DEFPIN_AVR( 0, 1<<0, E); _DEFPIN_AVR( 1, 1<<1, E); _DEFPIN_AVR( 2, 1<<7, B); _DEFPIN_AVR( 3, 1<<3, E);
_DEFPIN_AVR( 4, 1<<4, E); _DEFPIN_AVR( 5, 1<<5, E); _DEFPIN_AVR( 6, 1<<2, E); _DEFPIN_AVR( 7, 1<<6, E);
_DEFPIN_AVR( 8, 1<<5, D); _DEFPIN_AVR( 9, 1<<0, B); _DEFPIN_AVR(10, 1<<2, B); _DEFPIN_AVR(11, 1<<3, B);
_DEFPIN_AVR(12, 1<<1, B); _DEFPIN_AVR(13, 1<<2, D); _DEFPIN_AVR(14, 1<<3, D); _DEFPIN_AVR(15, 1<<0, D);
_DEFPIN_AVR(16, 1<<1, D); _DEFPIN_AVR(17, 1<<4, D); _DEFPIN_AVR(18, 1<<7, E); _DEFPIN_AVR(19, 1<<6, D);
_DEFPIN_AVR(20, 1<<7, D); _DEFPIN_AVR(21, 1<<4, B); _DEFPIN_AVR(22, 1<<5, B); _DEFPIN_AVR(23, 1<<6, B);
_DEFPIN_AVR(24, 1<<0, F); _DEFPIN_AVR(25, 1<<1, F); _DEFPIN_AVR(26, 1<<2, F); _DEFPIN_AVR(27, 1<<3, F);
_DEFPIN_AVR(28, 1<<4, F); _DEFPIN_AVR(29, 1<<5, F); _DEFPIN_AVR(30, 1<<6, F); _DEFPIN_AVR(31, 1<<7, F);

#define SPI_DATA 10
#define SPI_CLOCK 12
#define SPI_SELECT 9

#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
// megas

_IO(A); _IO(B); _IO(C); _IO(D); _IO(E); _IO(F); _IO(G); _IO(H); _IO(J); _IO(K); _IO(L);

#define MAX_PIN 69
_DEFPIN_AVR(0, 1, E); _DEFPIN_AVR(1, 2, E); _DEFPIN_AVR(2, 16, E); _DEFPIN_AVR(3, 32, E);
_DEFPIN_AVR(4, 32, G); _DEFPIN_AVR(5, 8, E); _DEFPIN_AVR(6, 8, H); _DEFPIN_AVR(7, 16, H);
_DEFPIN_AVR(8, 32, H); _DEFPIN_AVR(9, 64, H); _DEFPIN_AVR(10, 16, B); _DEFPIN_AVR(11, 32, B);
_DEFPIN_AVR(12, 64, B); _DEFPIN_AVR(13, 128, B); _DEFPIN_AVR(14, 2, J); _DEFPIN_AVR(15, 1, J);
_DEFPIN_AVR(16, 2, H); _DEFPIN_AVR(17, 1, H); _DEFPIN_AVR(18, 8, D); _DEFPIN_AVR(19, 4, D);
_DEFPIN_AVR(20, 2, D); _DEFPIN_AVR(21, 1, D); _DEFPIN_AVR(22, 1, A); _DEFPIN_AVR(23, 2, A);
_DEFPIN_AVR(24, 4, A); _DEFPIN_AVR(25, 8, A); _DEFPIN_AVR(26, 16, A); _DEFPIN_AVR(27, 32, A);
_DEFPIN_AVR(28, 64, A); _DEFPIN_AVR(29, 128, A); _DEFPIN_AVR(30, 128, C); _DEFPIN_AVR(31, 64, C);
_DEFPIN_AVR(32, 32, C); _DEFPIN_AVR(33, 16, C); _DEFPIN_AVR(34, 8, C); _DEFPIN_AVR(35, 4, C);
_DEFPIN_AVR(36, 2, C); _DEFPIN_AVR(37, 1, C); _DEFPIN_AVR(38, 128, D); _DEFPIN_AVR(39, 4, G);
_DEFPIN_AVR(40, 2, G); _DEFPIN_AVR(41, 1, G); _DEFPIN_AVR(42, 128, L); _DEFPIN_AVR(43, 64, L);
_DEFPIN_AVR(44, 32, L); _DEFPIN_AVR(45, 16, L); _DEFPIN_AVR(46, 8, L); _DEFPIN_AVR(47, 4, L);
_DEFPIN_AVR(48, 2, L); _DEFPIN_AVR(49, 1, L); _DEFPIN_AVR(50, 8, B); _DEFPIN_AVR(51, 4, B);
_DEFPIN_AVR(52, 2, B); _DEFPIN_AVR(53, 1, B); _DEFPIN_AVR(54, 1, F); _DEFPIN_AVR(55, 2, F);
_DEFPIN_AVR(56, 4, F); _DEFPIN_AVR(57, 8, F); _DEFPIN_AVR(58, 16, F); _DEFPIN_AVR(59, 32, F);
_DEFPIN_AVR(60, 64, F); _DEFPIN_AVR(61, 128, F); _DEFPIN_AVR(62, 1, K); _DEFPIN_AVR(63, 2, K);
_DEFPIN_AVR(64, 4, K); _DEFPIN_AVR(65, 8, K); _DEFPIN_AVR(66, 16, K); _DEFPIN_AVR(67, 32, K);
_DEFPIN_AVR(68, 64, K); _DEFPIN_AVR(69, 128, K);

#define SPI_DATA 51
#define SPI_CLOCK 52
#define SPI_SELECT 53
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

// Leonardo, teensy, blinkm
#elif defined(__AVR_ATmega32U4__) && defined(CORE_TEENSY)

// teensy defs
_IO(B); _IO(C); _IO(D); _IO(E); _IO(F);

#define MAX_PIN 23
_DEFPIN_AVR(0, 1, B); _DEFPIN_AVR(1, 2, B); _DEFPIN_AVR(2, 4, B); _DEFPIN_AVR(3, 8, B);
_DEFPIN_AVR(4, 128, B); _DEFPIN_AVR(5, 1, D); _DEFPIN_AVR(6, 2, D); _DEFPIN_AVR(7, 4, D);
_DEFPIN_AVR(8, 8, D); _DEFPIN_AVR(9, 64, C); _DEFPIN_AVR(10, 128, C); _DEFPIN_AVR(11, 64, D);
_DEFPIN_AVR(12, 128, D); _DEFPIN_AVR(13, 16, B); _DEFPIN_AVR(14, 32, B); _DEFPIN_AVR(15, 64, B);
_DEFPIN_AVR(16, 128, F); _DEFPIN_AVR(17, 64, F); _DEFPIN_AVR(18, 32, F); _DEFPIN_AVR(19, 16, F);
_DEFPIN_AVR(20, 2, F); _DEFPIN_AVR(21, 1, F); _DEFPIN_AVR(22, 16, D); _DEFPIN_AVR(23, 32, D);

#define SPI_DATA 2
#define SPI_CLOCK 1
#define SPI_SELECT 0
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

// PD3/PD5
#define SPI_UART1_DATA 8
#define SPI_UART1_CLOCK 23

#elif defined(__AVR_AT90USB646__) || defined(__AVR_AT90USB1286__)
// teensy++ 2 defs

_IO(A); _IO(B); _IO(C); _IO(D); _IO(E); _IO(F);

#define MAX_PIN 45
_DEFPIN_AVR(0, 1, D); _DEFPIN_AVR(1, 2, D); _DEFPIN_AVR(2, 4, D); _DEFPIN_AVR(3, 8, D);
_DEFPIN_AVR(4, 16, D); _DEFPIN_AVR(5, 32, D); _DEFPIN_AVR(6, 64, D); _DEFPIN_AVR(7, 128, D);
_DEFPIN_AVR(8, 1, E); _DEFPIN_AVR(9, 2, E); _DEFPIN_AVR(10, 1, C); _DEFPIN_AVR(11, 2, C);
_DEFPIN_AVR(12, 4, C); _DEFPIN_AVR(13, 8, C); _DEFPIN_AVR(14, 16, C); _DEFPIN_AVR(15, 32, C);
_DEFPIN_AVR(16, 64, C); _DEFPIN_AVR(17, 128, C); _DEFPIN_AVR(18, 64, E); _DEFPIN_AVR(19, 128, E);
_DEFPIN_AVR(20, 1, B); _DEFPIN_AVR(21, 2, B); _DEFPIN_AVR(22, 4, B); _DEFPIN_AVR(23, 8, B);
_DEFPIN_AVR(24, 16, B); _DEFPIN_AVR(25, 32, B); _DEFPIN_AVR(26, 64, B); _DEFPIN_AVR(27, 128, B);
_DEFPIN_AVR(28, 1, A); _DEFPIN_AVR(29, 2, A); _DEFPIN_AVR(30, 4, A); _DEFPIN_AVR(31, 8, A);
_DEFPIN_AVR(32, 16, A); _DEFPIN_AVR(33, 32, A); _DEFPIN_AVR(34, 64, A); _DEFPIN_AVR(35, 128, A);
_DEFPIN_AVR(36, 16, E); _DEFPIN_AVR(37, 32, E); _DEFPIN_AVR(38, 1, F); _DEFPIN_AVR(39, 2, F);
_DEFPIN_AVR(40, 4, F); _DEFPIN_AVR(41, 8, F); _DEFPIN_AVR(42, 16, F); _DEFPIN_AVR(43, 32, F);
_DEFPIN_AVR(44, 64, F); _DEFPIN_AVR(45, 128, F);

#define SPI_DATA 22
#define SPI_CLOCK 21
#define SPI_SELECT 20
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

// PD3/PD5
#define SPI_UART1_DATA 3
#define SPI_UART1_CLOCK 5


#elif defined(__AVR_ATmega32U4__)

// leonard defs
_IO(B); _IO(C); _IO(D); _IO(E); _IO(F);

#define MAX_PIN 30
_DEFPIN_AVR(0, 4, D); _DEFPIN_AVR(1, 8, D); _DEFPIN_AVR(2, 2, D); _DEFPIN_AVR(3, 1, D);
_DEFPIN_AVR(4, 16, D); _DEFPIN_AVR(5, 64, C); _DEFPIN_AVR(6, 128, D); _DEFPIN_AVR(7, 64, E);
_DEFPIN_AVR(8, 16, B); _DEFPIN_AVR(9, 32, B); _DEFPIN_AVR(10, 64, B); _DEFPIN_AVR(11, 128, B);
_DEFPIN_AVR(12, 64, D); _DEFPIN_AVR(13, 128, C); _DEFPIN_AVR(14, 8, B); _DEFPIN_AVR(15, 2, B);
_DEFPIN_AVR(16, 4, B); _DEFPIN_AVR(17, 1, B); _DEFPIN_AVR(18, 128, F); _DEFPIN_AVR(19, 64, F);
_DEFPIN_AVR(20, 32, F); _DEFPIN_AVR(21, 16, F); _DEFPIN_AVR(22, 2, F); _DEFPIN_AVR(23, 1, F);
_DEFPIN_AVR(24, 16, D); _DEFPIN_AVR(25, 128, D); _DEFPIN_AVR(26, 16, B); _DEFPIN_AVR(27, 32, B);
_DEFPIN_AVR(28, 64, B); _DEFPIN_AVR(29, 64, D); _DEFPIN_AVR(30, 32, D);

#define SPI_DATA 16
#define SPI_CLOCK 15
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

// PD3/PD5
#define SPI_UART1_DATA 1
#define SPI_UART1_CLOCK 30


#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END

#endif // __INC_FASTPIN_AVR_H
