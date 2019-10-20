#ifndef __INC_FASTPIN_ARM_SAM_H
#define __INC_FASTPIN_ARM_SAM_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be sloightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else


/// Template definition for arduino due style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data register, set output register, clear output register, set data direction register
template<uint8_t PIN, uint32_t _MASK, typename _PDOR, typename _PSOR, typename _PCOR, typename _PDDR> class _DUEPIN {
public:
	typedef volatile uint32_t * port_ptr_t;
	typedef uint32_t port_t;

	inline static void setOutput() { pinMode(PIN, OUTPUT); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
	inline static void setInput() { pinMode(PIN, INPUT); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { _PSOR::r() = _MASK; }
	inline static void lo() __attribute__ ((always_inline)) { _PCOR::r() = _MASK; }
	inline static void set(register port_t val) __attribute__ ((always_inline)) { _PDOR::r() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle();  }

	inline static void toggle() __attribute__ ((always_inline)) { _PDOR::r() ^= _MASK; }

	inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
	inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

	inline static port_t hival() __attribute__ ((always_inline)) { return _PDOR::r() | _MASK; }
	inline static port_t loval() __attribute__ ((always_inline)) { return _PDOR::r() & ~_MASK; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_PDOR::r(); }
	inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_PSOR::r(); }
	inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_PCOR::r(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};


/// Template definition for DUE  style ARM pins using bit banding, providing direct access to the various GPIO registers.  GCC
/// does a poor job of optimizing around these accesses so they are not being used just yet.
template<uint8_t PIN, uint32_t _BIT, typename _PDOR, typename _PSOR, typename _PCOR, typename _PDDR> class _DUEPIN_BITBAND {
public:
	typedef volatile uint32_t * port_ptr_t;
	typedef uint32_t port_t;

	inline static void setOutput() { pinMode(PIN, OUTPUT); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
	inline static void setInput() { pinMode(PIN, INPUT); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = 1; }
	inline static void lo() __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = 0; }
	inline static void set(register port_t val) __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

	inline static void toggle() __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() ^= 1; }

	inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi();  }
	inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

	inline static port_t hival() __attribute__ ((always_inline)) { return 1; }
	inline static port_t loval() __attribute__ ((always_inline)) { return 0; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return _PDOR::template rx<_BIT>(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return 1; }
};

#define GPIO_BITBAND_ADDR(reg, bit) (((uint32_t)&(reg) - 0x40000000) * 32 + (bit) * 4 + 0x42000000)
#define GPIO_BITBAND_PTR(reg, bit) ((uint32_t *)GPIO_BITBAND_ADDR((reg), (bit)))

#define _R(T) struct __gen_struct_ ## T
#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline reg32_t r() { return T; } \
	template<int BIT> static __attribute__((always_inline)) inline ptr_reg32_t rx() { return GPIO_BITBAND_PTR(T, BIT); } };
#define DUE_IO32(L) _RD32(REG_PIO ## L ## _ODSR); _RD32(REG_PIO ## L ## _SODR); _RD32(REG_PIO ## L ## _CODR); _RD32(REG_PIO ## L ## _OER);

#define _DEFPIN_DUE(PIN, BIT, L) template<> class FastPin<PIN> : public _DUEPIN<PIN, 1 << BIT, _R(REG_PIO ## L ## _ODSR), _R(REG_PIO ## L ## _SODR), _R(REG_PIO ## L ## _CODR), \
  																			_R(GPIO ## L ## _OER)> {}; \
  								   template<> class FastPinBB<PIN> : public _DUEPIN_BITBAND<PIN, BIT, _R(REG_PIO ## L ## _ODSR), _R(REG_PIO ## L ## _SODR), _R(REG_PIO ## L ## _CODR), \
  																			_R(GPIO ## L ## _OER)> {};

#if defined(__SAM3X8E__)

DUE_IO32(A);
DUE_IO32(B);
DUE_IO32(C);
DUE_IO32(D);

#define MAX_PIN 78
_DEFPIN_DUE(0, 8, A); _DEFPIN_DUE(1, 9, A); _DEFPIN_DUE(2, 25, B); _DEFPIN_DUE(3, 28, C);
_DEFPIN_DUE(4, 26, C); _DEFPIN_DUE(5, 25, C); _DEFPIN_DUE(6, 24, C); _DEFPIN_DUE(7, 23, C);
_DEFPIN_DUE(8, 22, C); _DEFPIN_DUE(9, 21, C); _DEFPIN_DUE(10, 29, C); _DEFPIN_DUE(11, 7, D);
_DEFPIN_DUE(12, 8, D); _DEFPIN_DUE(13, 27, B); _DEFPIN_DUE(14, 4, D); _DEFPIN_DUE(15, 5, D);
_DEFPIN_DUE(16, 13, A); _DEFPIN_DUE(17, 12, A); _DEFPIN_DUE(18, 11, A); _DEFPIN_DUE(19, 10, A);
_DEFPIN_DUE(20, 12, B); _DEFPIN_DUE(21, 13, B); _DEFPIN_DUE(22, 26, B); _DEFPIN_DUE(23, 14, A);
_DEFPIN_DUE(24, 15, A); _DEFPIN_DUE(25, 0, D); _DEFPIN_DUE(26, 1, D); _DEFPIN_DUE(27, 2, D);
_DEFPIN_DUE(28, 3, D); _DEFPIN_DUE(29, 6, D); _DEFPIN_DUE(30, 9, D); _DEFPIN_DUE(31, 7, A);
_DEFPIN_DUE(32, 10, D); _DEFPIN_DUE(33, 1, C); _DEFPIN_DUE(34, 2, C); _DEFPIN_DUE(35, 3, C);
_DEFPIN_DUE(36, 4, C); _DEFPIN_DUE(37, 5, C); _DEFPIN_DUE(38, 6, C); _DEFPIN_DUE(39, 7, C);
_DEFPIN_DUE(40, 8, C); _DEFPIN_DUE(41, 9, C); _DEFPIN_DUE(42, 19, A); _DEFPIN_DUE(43, 20, A);
_DEFPIN_DUE(44, 19, C); _DEFPIN_DUE(45, 18, C); _DEFPIN_DUE(46, 17, C); _DEFPIN_DUE(47, 16, C);
_DEFPIN_DUE(48, 15, C); _DEFPIN_DUE(49, 14, C); _DEFPIN_DUE(50, 13, C); _DEFPIN_DUE(51, 12, C);
_DEFPIN_DUE(52, 21, B); _DEFPIN_DUE(53, 14, B); _DEFPIN_DUE(54, 16, A); _DEFPIN_DUE(55, 24, A);
_DEFPIN_DUE(56, 23, A); _DEFPIN_DUE(57, 22, A); _DEFPIN_DUE(58, 6, A); _DEFPIN_DUE(59, 4, A);
_DEFPIN_DUE(60, 3, A); _DEFPIN_DUE(61, 2, A); _DEFPIN_DUE(62, 17, B); _DEFPIN_DUE(63, 18, B);
_DEFPIN_DUE(64, 19, B); _DEFPIN_DUE(65, 20, B); _DEFPIN_DUE(66, 15, B); _DEFPIN_DUE(67, 16, B);
_DEFPIN_DUE(68, 1, A); _DEFPIN_DUE(69, 0, A); _DEFPIN_DUE(70, 17, A); _DEFPIN_DUE(71, 18, A);
_DEFPIN_DUE(72, 30, C); _DEFPIN_DUE(73, 21, A); _DEFPIN_DUE(74, 25, A); _DEFPIN_DUE(75, 26, A);
_DEFPIN_DUE(76, 27, A); _DEFPIN_DUE(77, 28, A); _DEFPIN_DUE(78, 23, B);

// digix pins
_DEFPIN_DUE(90, 0, B); _DEFPIN_DUE(91, 1, B); _DEFPIN_DUE(92, 2, B); _DEFPIN_DUE(93, 3, B);
_DEFPIN_DUE(94, 4, B); _DEFPIN_DUE(95, 5, B); _DEFPIN_DUE(96, 6, B); _DEFPIN_DUE(97, 7, B);
_DEFPIN_DUE(98, 8, B); _DEFPIN_DUE(99, 9, B); _DEFPIN_DUE(100, 5, A); _DEFPIN_DUE(101, 22, B);
_DEFPIN_DUE(102, 23, B); _DEFPIN_DUE(103, 24, B); _DEFPIN_DUE(104, 27, C); _DEFPIN_DUE(105, 20, C);
_DEFPIN_DUE(106, 11, C); _DEFPIN_DUE(107, 10, C); _DEFPIN_DUE(108, 21, A); _DEFPIN_DUE(109, 30, C);
_DEFPIN_DUE(110, 29, B); _DEFPIN_DUE(111, 30, B); _DEFPIN_DUE(112, 31, B); _DEFPIN_DUE(113, 28, B);

#define SPI_DATA 75
#define SPI_CLOCK 76
#define ARM_HARDWARE_SPI
#define HAS_HARDWARE_PIN_SUPPORT

#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END


#endif // __INC_FASTPIN_ARM_SAM_H
