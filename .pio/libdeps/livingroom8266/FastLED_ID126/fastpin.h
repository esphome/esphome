#ifndef __INC_FASTPIN_H
#define __INC_FASTPIN_H

#include "FastLED.h"

#include "led_sysdefs.h"
#include <stddef.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"

///@file fastpin.h
/// Class base definitions for defining fast pin access

FASTLED_NAMESPACE_BEGIN

#define NO_PIN 255

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Pin access class - needs to tune for various platforms (naive fallback solution?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Selectable {
public:
	virtual void select() = 0;
	virtual void release() = 0;
	virtual bool isSelected() = 0;
};

#if !defined(FASTLED_NO_PINMAP)

class Pin : public Selectable {
	volatile RwReg *mPort;
	volatile RoReg *mInPort;
	RwReg mPinMask;
	uint8_t mPin;

	void _init() {
		mPinMask = digitalPinToBitMask(mPin);
		mPort = (volatile RwReg*)portOutputRegister(digitalPinToPort(mPin));
		mInPort = (volatile RoReg*)portInputRegister(digitalPinToPort(mPin));
	}
public:
	Pin(int pin) : mPin(pin) { _init(); }

	typedef volatile RwReg * port_ptr_t;
	typedef RwReg port_t;

	inline void setOutput() { pinMode(mPin, OUTPUT); }
	inline void setInput() { pinMode(mPin, INPUT); }

	inline void hi() __attribute__ ((always_inline)) { *mPort |= mPinMask; }
	inline void lo() __attribute__ ((always_inline)) { *mPort &= ~mPinMask; }

	inline void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }
	inline void toggle() __attribute__ ((always_inline)) { *mInPort = mPinMask; }

	inline void hi(register port_ptr_t port) __attribute__ ((always_inline)) { *port |= mPinMask; }
	inline void lo(register port_ptr_t port) __attribute__ ((always_inline)) { *port &= ~mPinMask; }
	inline void set(register port_t val) __attribute__ ((always_inline)) { *mPort = val; }

	inline void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port  = val; }

	port_t hival() __attribute__ ((always_inline)) { return *mPort | mPinMask;  }
	port_t loval() __attribute__ ((always_inline)) { return *mPort & ~mPinMask; }
	port_ptr_t  port() __attribute__ ((always_inline)) { return mPort; }
	port_t mask() __attribute__ ((always_inline)) { return mPinMask; }

	virtual void select() { hi(); }
	virtual void release() { lo(); }
	virtual bool isSelected() { return (*mPort & mPinMask) == mPinMask; }
};

class OutputPin : public Pin {
public:
	OutputPin(int pin) : Pin(pin) { setOutput(); }
};

class InputPin : public Pin {
public:
	InputPin(int pin) : Pin(pin) { setInput(); }
};

#else
// This is the empty code version of the raw pin class, method bodies should be filled in to Do The Right Thing[tm] when making this
// available on a new platform
class Pin : public Selectable {
	volatile RwReg *mPort;
	volatile RoReg *mInPort;
	RwReg mPinMask;
	uint8_t mPin;

	void _init() {
		// TODO: fill in init on a new platform
		mPinMask = 0;
		mPort = NULL;
		mInPort = NULL;
	}
public:
	Pin(int pin) : mPin(pin) { _init(); }

	void setPin(int pin) { mPin = pin; _init(); }

	typedef volatile RwReg * port_ptr_t;
	typedef RwReg port_t;

	inline void setOutput() { /* TODO: Set pin output */ }
	inline void setInput() { /* TODO: Set pin input */ }

	inline void hi() __attribute__ ((always_inline)) { *mPort |= mPinMask; }
	inline void lo() __attribute__ ((always_inline)) { *mPort &= ~mPinMask; }

	inline void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }
	inline void toggle() __attribute__ ((always_inline)) { *mInPort = mPinMask; }

	inline void hi(register port_ptr_t port) __attribute__ ((always_inline)) { *port |= mPinMask; }
	inline void lo(register port_ptr_t port) __attribute__ ((always_inline)) { *port &= ~mPinMask; }
	inline void set(register port_t val) __attribute__ ((always_inline)) { *mPort = val; }

	inline void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port  = val; }

	port_t hival() __attribute__ ((always_inline)) { return *mPort | mPinMask;  }
	port_t loval() __attribute__ ((always_inline)) { return *mPort & ~mPinMask; }
	port_ptr_t  port() __attribute__ ((always_inline)) { return mPort; }
	port_t mask() __attribute__ ((always_inline)) { return mPinMask; }

	virtual void select() { hi(); }
	virtual void release() { lo(); }
	virtual bool isSelected() { return (*mPort & mPinMask) == mPinMask; }
};

class OutputPin : public Pin {
public:
	OutputPin(int pin) : Pin(pin) { setOutput(); }
};

class InputPin : public Pin {
public:
	InputPin(int pin) : Pin(pin) { setInput(); }
};

#endif

/// The simplest level of Pin class.  This relies on runtime functions durinig initialization to get the port/pin mask for the pin.  Most
/// of the accesses involve references to these static globals that get set up.  This won't be the fastest set of pin operations, but it
/// will provide pin level access on pretty much all arduino environments.  In addition, it includes some methods to help optimize access in
/// various ways.  Namely, the versions of hi, lo, and fastset that take the port register as a passed in register variable (saving a global
/// dereference), since these functions are aggressively inlined, that can help collapse out a lot of extraneous memory loads/dereferences.
///
/// In addition, if, while writing a bunch of data to a pin, you know no other pins will be getting written to, you can get/cache a value of
/// the pin's port register and use that to do a full set to the register.  This results in one being able to simply do a store to the register,
/// vs. the load, and/or, and store that would be done normally.
///
/// There are platform specific instantiations of this class that provide direct i/o register access to pins for much higher speed pin twiddling.
///
/// Note that these classes are all static functions.  So the proper usage is Pin<13>::hi(); or such.  Instantiating objects is not recommended,
/// as passing Pin objects around will likely -not- have the effect you're expecting.
#ifdef FASTLED_FORCE_SOFTWARE_PINS
template<uint8_t PIN> class FastPin {
	static RwReg sPinMask;
	static volatile RwReg *sPort;
	static volatile RoReg *sInPort;
	static void _init() {
#if !defined(FASTLED_NO_PINMAP)
		sPinMask = digitalPinToBitMask(PIN);
		sPort = portOutputRegister(digitalPinToPort(PIN));
		sInPort = portInputRegister(digitalPinToPort(PIN));
#endif
	}
public:
	typedef volatile RwReg * port_ptr_t;
	typedef RwReg port_t;

	inline static void setOutput() { _init(); pinMode(PIN, OUTPUT); }
	inline static void setInput() { _init(); pinMode(PIN, INPUT); }

	inline static void hi() __attribute__ ((always_inline)) { *sPort |= sPinMask; }
	inline static void lo() __attribute__ ((always_inline)) { *sPort &= ~sPinMask; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

	inline static void toggle() __attribute__ ((always_inline)) { *sInPort = sPinMask; }

	inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { *port |= sPinMask; }
	inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { *port &= ~sPinMask; }
	inline static void set(register port_t val) __attribute__ ((always_inline)) { *sPort = val; }

	inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port  = val; }

	static port_t hival() __attribute__ ((always_inline)) { return *sPort | sPinMask;  }
	static port_t loval() __attribute__ ((always_inline)) { return *sPort & ~sPinMask; }
	static port_ptr_t  port() __attribute__ ((always_inline)) { return sPort; }
	static port_t mask() __attribute__ ((always_inline)) { return sPinMask; }
};

template<uint8_t PIN> RwReg FastPin<PIN>::sPinMask;
template<uint8_t PIN> volatile RwReg *FastPin<PIN>::sPort;
template<uint8_t PIN> volatile RoReg *FastPin<PIN>::sInPort;

#else

template<uint8_t PIN> class FastPin {
	constexpr static bool validpin() { return false; }

	static_assert(validpin(), "Invalid pin specified");

	static void _init() {
	}
public:
	typedef volatile RwReg * port_ptr_t;
	typedef RwReg port_t;

	inline static void setOutput() { }
	inline static void setInput() { }

	inline static void hi() __attribute__ ((always_inline)) { }
	inline static void lo() __attribute__ ((always_inline)) { }

	inline static void strobe() __attribute__ ((always_inline)) { }

	inline static void toggle() __attribute__ ((always_inline)) { }

	inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { }
	inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { }
	inline static void set(register port_t val) __attribute__ ((always_inline)) { }

	inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { }

	static port_t hival() __attribute__ ((always_inline)) { return 0; }
	static port_t loval() __attribute__ ((always_inline)) { return 0;}
	static port_ptr_t  port() __attribute__ ((always_inline)) { return NULL; }
	static port_t mask() __attribute__ ((always_inline)) { return 0; }
};

#endif

template<uint8_t PIN> class FastPinBB : public FastPin<PIN> {};

typedef volatile uint32_t & reg32_t;
typedef volatile uint32_t * ptr_reg32_t;

FASTLED_NAMESPACE_END

#pragma GCC diagnostic pop

#endif // __INC_FASTPIN_H
