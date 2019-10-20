#ifndef __INC_FL_DELAY_H
#define __INC_FL_DELAY_H

#include "FastLED.h"

///@file fastled_delay.h
///Utility functions and classes for managing delaycycles

FASTLED_NAMESPACE_BEGIN

/// Class to ensure that a minimum amount of time has kicked since the last time run - and delay if not enough time has passed yet
/// this should make sure that chipsets that have
template<int WAIT> class CMinWait {
	uint16_t mLastMicros;
public:
	CMinWait() { mLastMicros = 0; }

	void wait() {
		uint16_t diff;
		do {
			diff = (micros() & 0xFFFF) - mLastMicros;
		} while(diff < WAIT);
	}

	void mark() { mLastMicros = micros() & 0xFFFF; }
};


////////////////////////////////////////////////////////////////////////////////////////////
//
// Clock cycle counted delay loop
//
////////////////////////////////////////////////////////////////////////////////////////////

// Default is now just 'nop', with special case for AVR

// ESP32 core has it's own definition of NOP, so undef it first
#ifdef ESP32
#undef NOP
#undef NOP2
#endif

#if defined(__AVR__)
#  define FL_NOP __asm__ __volatile__ ("cp r0,r0\n");
#  define FL_NOP2 __asm__ __volatile__ ("rjmp .+0");
#else
#  define FL_NOP __asm__ __volatile__ ("nop\n");
#  define FL_NOP2 __asm__ __volatile__ ("nop\n\t nop\n");
#endif

// predeclaration to not upset the compiler
template<int CYCLES> inline void delaycycles();
template<int CYCLES> inline void delaycycles_min1() {
  delaycycles<1>();
  delaycycles<CYCLES-1>();
}


// TODO: ARM version of _delaycycles_

// usable definition
#if defined(FASTLED_AVR)
// worker template - this will nop for LOOP * 3 + PAD cycles total
template<int LOOP, int PAD> inline void _delaycycles_AVR() {
	delaycycles<PAD>();
	// the loop below is 3 cycles * LOOP.  the LDI is one cycle,
	// the DEC is 1 cycle, the BRNE is 2 cycles if looping back and
	// 1 if not (the LDI balances out the BRNE being 1 cycle on exit)
	__asm__ __volatile__ (
		"		LDI R16, %0\n"
		"L_%=:  DEC R16\n"
		"		BRNE L_%=\n"
		: /* no outputs */
		: "M" (LOOP)
		: "r16"
		);
}

template<int CYCLES> __attribute__((always_inline)) inline void delaycycles() {
	_delaycycles_AVR<CYCLES / 3, CYCLES % 3>();
}
#else
// template<int LOOP, int PAD> inline void _delaycycles_ARM() {
// 	delaycycles<PAD>();
// 	// the loop below is 3 cycles * LOOP.  the LDI is one cycle,
// 	// the DEC is 1 cycle, the BRNE is 2 cycles if looping back and
// 	// 1 if not (the LDI balances out the BRNE being 1 cycle on exit)
// 	__asm__ __volatile__ (
// 		"		mov.w r9, %0\n"
// 		"L_%=:  subs.w r9, r9, #1\n"
// 		"		bne.n L_%=\n"
// 		: /* no outputs */
// 		: "M" (LOOP)
// 		: "r9"
// 		);
// }


template<int CYCLES> __attribute__((always_inline)) inline void delaycycles() {
	// _delaycycles_ARM<CYCLES / 3, CYCLES % 3>();
	FL_NOP; delaycycles<CYCLES-1>();
}
#endif

// pre-instantiations for values small enough to not need the loop, as well as sanity holders
// for some negative values.
template<> __attribute__((always_inline)) inline void delaycycles<-10>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-9>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-8>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-7>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-6>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-5>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-4>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-3>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-2>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-1>() {}
template<> __attribute__((always_inline)) inline void delaycycles<0>() {}
template<> __attribute__((always_inline)) inline void delaycycles<1>() {FL_NOP;}
template<> __attribute__((always_inline)) inline void delaycycles<2>() {FL_NOP2;}
template<> __attribute__((always_inline)) inline void delaycycles<3>() {FL_NOP;FL_NOP2;}
template<> __attribute__((always_inline)) inline void delaycycles<4>() {FL_NOP2;FL_NOP2;}
template<> __attribute__((always_inline)) inline void delaycycles<5>() {FL_NOP2;FL_NOP2;FL_NOP;}

// Some timing related macros/definitions

// Macro to convert from nano-seconds to clocks and clocks to nano-seconds
// #define NS(_NS) (_NS / (1000 / (F_CPU / 1000000L)))
#define F_CPU_MHZ (F_CPU / 1000000L)

// #define NS(_NS) ( (_NS * (F_CPU / 1000000L))) / 1000
#define NS(_NS) (((_NS * F_CPU_MHZ) + 999) / 1000)
#define CLKS_TO_MICROS(_CLKS) ((long)(_CLKS)) / (F_CPU / 1000000L)

//  Macro for making sure there's enough time available
#define NO_TIME(A, B, C) (NS(A) < 3 || NS(B) < 3 || NS(C) < 6)

FASTLED_NAMESPACE_END

#endif
