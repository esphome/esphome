#ifndef __INC_FASTSPI_ARM_H
#define __INC_FASTSPI_ARM_H

//
// copied from k20 code
// changed SPI1 define to KINETISK_SPI1
// TODO: add third alternative MOSI pin (28) and CLOCK pin (27)
// TODO: add alternative pins for SPI1
// TODO: add SPI2 output
//

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_TEENSY3) && defined(CORE_TEENSY)

// Version 1.20 renamed SPI_t to KINETISK_SPI_t
#if TEENSYDUINO >= 120
#define SPI_t KINETISK_SPI_t
#endif

#ifndef KINETISK_SPI0
#define KINETISK_SPI0 SPI0
#endif

#ifndef SPI_PUSHR_CONT
#define SPI_PUSHR_CONT SPIX.PUSHR_CONT
#define SPI_PUSHR_CTAS(X) SPIX.PUSHR_CTAS(X)
#define SPI_PUSHR_EOQ SPIX.PUSHR_EOQ
#define SPI_PUSHR_CTCNT SPIX.PUSHR_CTCNT
#define SPI_PUSHR_PCS(X) SPIX.PUSHR_PCS(X)
#endif

// Template function that, on compilation, expands to a constant representing the highest bit set in a byte.  Right now,
// if no bits are set (value is 0), it returns 0, which is also the value returned if the lowest bit is the only bit
// set (the zero-th bit).  Unclear if I  will want this to change at some point.
template<int VAL, int BIT> class BitWork {
public:
	static int highestBit() __attribute__((always_inline)) { return (VAL & 1 << BIT) ? BIT : BitWork<VAL, BIT-1>::highestBit(); }
};
template<int VAL> class BitWork<VAL, 0> {
public:
	static int highestBit() __attribute__((always_inline)) { return 0; }
};

#define MAX(A, B) (( (A) > (B) ) ? (A) : (B))

#define USE_CONT 0
// intra-frame backup data
struct SPIState {
	uint32_t _ctar0,_ctar1;
	uint32_t pins[4];
};

// extern SPIState gState;


// Templated function to translate a clock divider value into the prescalar, scalar, and clock doubling setting for the world.
template <int VAL> void getScalars(uint32_t & preScalar, uint32_t & scalar, uint32_t & dbl) {
	switch(VAL) {
		// Handle the dbl clock cases
		case 0: case 1:
		case 2: preScalar = 0; scalar = 0; dbl = 1; break;
		case 3: preScalar = 1; scalar = 0; dbl = 1; break;
		case 5: preScalar = 2; scalar = 0; dbl = 1; break;
		case 7: preScalar = 3; scalar = 0; dbl = 1; break;

		// Handle the scalar value 6 cases (since it's not a power of two, it won't get caught
		// below)
		case 9: preScalar = 1; scalar = 2; dbl = 1; break;
		case 18: case 19: preScalar = 1; scalar = 2; dbl = 0; break;

		case 15: preScalar = 2; scalar = 2; dbl = 1; break;
		case 30: case 31: preScalar = 2; scalar = 2; dbl = 0; break;

		case 21: case 22: case 23: preScalar = 3; scalar = 2; dbl = 1; break;
		case 42: case 43: case 44: case 45: case 46: case 47: preScalar = 3; scalar = 2; dbl = 0; break;
		default: {
			int p2 = BitWork<VAL/2, 15>::highestBit();
			int p3 = BitWork<VAL/3, 15>::highestBit();
			int p5 = BitWork<VAL/5, 15>::highestBit();
			int p7 = BitWork<VAL/7, 15>::highestBit();

			int w2 = 2 * (1 << p2);
			int w3 = (VAL/3) > 0 ? 3 * (1 << p3) : 0;
			int w5 = (VAL/5) > 0 ? 5 * (1 << p5) : 0;
			int w7 = (VAL/7) > 0 ? 7 * (1 << p7) : 0;

			int maxval = MAX(MAX(w2, w3), MAX(w5, w7));

			if(w2 == maxval) { preScalar = 0; scalar = p2; }
			else if(w3 == maxval) { preScalar = 1; scalar = p3; }
			else if(w5 == maxval) { preScalar = 2; scalar = p5; }
			else if(w7 == maxval) { preScalar = 3; scalar = p7; }

			dbl = 0;
			if(scalar == 0) { dbl = 1; }
			else if(scalar < 3) { scalar--; }
		}
	}
	return;
}

#define SPIX (*(SPI_t*)pSPIX)

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER, uint32_t pSPIX>
class ARMHardwareSPIOutput {
	Selectable *m_pSelect;
	SPIState gState;

	// Borrowed from the teensy3 SPSR emulation code -- note, enabling pin 7 disables pin 11 (and vice versa),
	// and likewise enabling pin 14 disables pin 13 (and vice versa)
	inline void enable_pins(void) __attribute__((always_inline)) {
		//serial_print("enable_pins\n");
		switch(_DATA_PIN) {
			case 7:
				CORE_PIN7_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
				CORE_PIN11_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
				break;
			case 11:
				CORE_PIN11_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
				CORE_PIN7_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
				break;
		}

		switch(_CLOCK_PIN) {
			case 13:
				CORE_PIN13_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
				CORE_PIN14_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
				break;
			case 14:
				CORE_PIN14_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
				CORE_PIN13_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1);
				break;
		}
	}

	// Borrowed from the teensy3 SPSR emulation code.  We disable the pins that we're using, and restore the state on the pins that we aren't using
	inline void disable_pins(void) __attribute__((always_inline)) {
		switch(_DATA_PIN) {
			case 7: CORE_PIN7_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1); CORE_PIN11_CONFIG = gState.pins[1]; break;
			case 11: CORE_PIN11_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1); CORE_PIN7_CONFIG = gState.pins[0]; break;
		}

		switch(_CLOCK_PIN) {
			case 13: CORE_PIN13_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1); CORE_PIN14_CONFIG = gState.pins[3]; break;
			case 14: CORE_PIN14_CONFIG = PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1); CORE_PIN13_CONFIG = gState.pins[2]; break;
		}
	}

	static inline void update_ctars(uint32_t ctar0, uint32_t ctar1) __attribute__((always_inline)) {
		if(SPIX.CTAR0 == ctar0 && SPIX.CTAR1 == ctar1) return;
		uint32_t mcr = SPIX.MCR;
		if(mcr & SPI_MCR_MDIS) {
			SPIX.CTAR0 = ctar0;
			SPIX.CTAR1 = ctar1;
		} else {
			SPIX.MCR = mcr | SPI_MCR_MDIS | SPI_MCR_HALT;
			SPIX.CTAR0 = ctar0;
			SPIX.CTAR1 = ctar1;
			SPIX.MCR = mcr;
		}
	}

	static inline void update_ctar0(uint32_t ctar) __attribute__((always_inline)) {
		if (SPIX.CTAR0 == ctar) return;
		uint32_t mcr = SPIX.MCR;
		if (mcr & SPI_MCR_MDIS) {
			SPIX.CTAR0 = ctar;
		} else {
			SPIX.MCR = mcr | SPI_MCR_MDIS | SPI_MCR_HALT;
			SPIX.CTAR0 = ctar;

			SPIX.MCR = mcr;
		}
	}

	static inline void update_ctar1(uint32_t ctar) __attribute__((always_inline)) {
		if (SPIX.CTAR1 == ctar) return;
		uint32_t mcr = SPIX.MCR;
		if (mcr & SPI_MCR_MDIS) {
			SPIX.CTAR1 = ctar;
		} else {
			SPIX.MCR = mcr | SPI_MCR_MDIS | SPI_MCR_HALT;
			SPIX.CTAR1 = ctar;
			SPIX.MCR = mcr;

		}
	}

	void setSPIRate() {
		// Configure CTAR0, defaulting to 8 bits and CTAR1, defaulting to 16 bits
		uint32_t _PBR = 0;
		uint32_t _BR = 0;
		uint32_t _CSSCK = 0;
		uint32_t _DBR = 0;

		// if(_SPI_CLOCK_DIVIDER >= 256) 		{ _PBR = 0; _BR = _CSSCK = 7; _DBR = 0; } // osc/256
		// else if(_SPI_CLOCK_DIVIDER >= 128) 	{ _PBR = 0; _BR = _CSSCK = 6; _DBR = 0; } // osc/128
		// else if(_SPI_CLOCK_DIVIDER >= 64) 	{ _PBR = 0; _BR = _CSSCK = 5; _DBR = 0; } // osc/64
		// else if(_SPI_CLOCK_DIVIDER >= 32) 	{ _PBR = 0; _BR = _CSSCK = 4; _DBR = 0; } // osc/32
		// else if(_SPI_CLOCK_DIVIDER >= 16) 	{ _PBR = 0; _BR = _CSSCK = 3; _DBR = 0; } // osc/16
		// else if(_SPI_CLOCK_DIVIDER >= 8) 	{ _PBR = 0; _BR = _CSSCK = 1; _DBR = 0; } // osc/8
		// else if(_SPI_CLOCK_DIVIDER >= 7) 	{ _PBR = 3; _BR = _CSSCK = 0; _DBR = 1; } // osc/7
		// else if(_SPI_CLOCK_DIVIDER >= 5) 	{ _PBR = 2; _BR = _CSSCK = 0; _DBR = 1; } // osc/5
		// else if(_SPI_CLOCK_DIVIDER >= 4) 	{ _PBR = 0; _BR = _CSSCK = 0; _DBR = 0; } // osc/4
		// else if(_SPI_CLOCK_DIVIDER >= 3) 	{ _PBR = 1; _BR = _CSSCK = 0; _DBR = 1; } // osc/3
		// else                                { _PBR = 0; _BR = _CSSCK = 0; _DBR = 1; } // osc/2

		getScalars<_SPI_CLOCK_DIVIDER>(_PBR, _BR, _DBR);
		_CSSCK = _BR;

		uint32_t ctar0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(_PBR) | SPI_CTAR_BR(_BR) | SPI_CTAR_CSSCK(_CSSCK);
		uint32_t ctar1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(_PBR) | SPI_CTAR_BR(_BR) | SPI_CTAR_CSSCK(_CSSCK);

		#if USE_CONT == 1
		ctar0 |= SPI_CTAR_CPHA | SPI_CTAR_CPOL;
		ctar1 |= SPI_CTAR_CPHA | SPI_CTAR_CPOL;
		#endif

		if(_DBR) {
			ctar0 |= SPI_CTAR_DBR;
			ctar1 |= SPI_CTAR_DBR;
		}

		update_ctars(ctar0,ctar1);
	}

	void inline save_spi_state() __attribute__ ((always_inline)) {
		// save ctar data
		gState._ctar0 = SPIX.CTAR0;
		gState._ctar1 = SPIX.CTAR1;

		// save data for the not-us pins
		gState.pins[0] = CORE_PIN7_CONFIG;
		gState.pins[1] = CORE_PIN11_CONFIG;
		gState.pins[2] = CORE_PIN13_CONFIG;
		gState.pins[3] = CORE_PIN14_CONFIG;
	}

	void inline restore_spi_state() __attribute__ ((always_inline)) {
		// restore ctar data
		update_ctars(gState._ctar0,gState._ctar1);

		// restore data for the not-us pins (not necessary because disable_pins will do this)
		// CORE_PIN7_CONFIG = gState.pins[0];
		// CORE_PIN11_CONFIG = gState.pins[1];
		// CORE_PIN13_CONFIG = gState.pins[2];
		// CORE_PIN14_CONFIG = gState.pins[3];
	}


public:
	ARMHardwareSPIOutput() { m_pSelect = NULL; }
	ARMHardwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }


	void init() {
		// set the pins to output
		FastPin<_DATA_PIN>::setOutput();
		FastPin<_CLOCK_PIN>::setOutput();

		// Enable SPI0 clock
		uint32_t sim6 = SIM_SCGC6;
		if((SPI_t*)pSPIX == &KINETISK_SPI0) {
			if (!(sim6 & SIM_SCGC6_SPI0)) {
				//serial_print("init1\n");
				SIM_SCGC6 = sim6 | SIM_SCGC6_SPI0;
				SPIX.CTAR0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(1) | SPI_CTAR_BR(1);
			}
		} else if((SPI_t*)pSPIX == &KINETISK_SPI1) {
			if (!(sim6 & SIM_SCGC6_SPI1)) {
				//serial_print("init1\n");
				SIM_SCGC6 = sim6 | SIM_SCGC6_SPI1;
				SPIX.CTAR0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(1) | SPI_CTAR_BR(1);
			}
		}

		// Configure SPI as the master and enable
		SPIX.MCR |= SPI_MCR_MSTR; // | SPI_MCR_CONT_SCKE);
		SPIX.MCR &= ~(SPI_MCR_MDIS | SPI_MCR_HALT);

		// pin/spi configuration happens on select
	}

	static void waitFully() __attribute__((always_inline)) {
		// Wait for the last byte to get shifted into the register
		bool empty = false;

		do {
			cli();
			if ((SPIX.SR & 0xF000) > 0) {
				// reset the TCF flag
				SPIX.SR |= SPI_SR_TCF;
			} else {
				empty = true;
			}
			sei();
		} while (!empty);

		// wait for the TCF flag to get set
		while (!(SPIX.SR & SPI_SR_TCF));
		SPIX.SR |= (SPI_SR_TCF | SPI_SR_EOQF);
	}

	static bool needwait() __attribute__((always_inline)) { return (SPIX.SR & 0x4000); }
	static void wait() __attribute__((always_inline)) { while( (SPIX.SR & 0x4000) );  }
	static void wait1() __attribute__((always_inline)) { while( (SPIX.SR & 0xF000) >= 0x2000);  }

	enum ECont { CONT, NOCONT };
	enum EWait { PRE, POST, NONE };
	enum ELast { NOTLAST, LAST };

	#if USE_CONT == 1
	#define CM CONT
	#else
	#define CM NOCONT
	#endif
	#define WM PRE

	template<ECont CONT_STATE, EWait WAIT_STATE, ELast LAST_STATE> class Write {
	public:
		static void writeWord(uint16_t w) __attribute__((always_inline)) {
			if(WAIT_STATE == PRE) { wait(); }
			SPIX.PUSHR = ((LAST_STATE == LAST) ? SPI_PUSHR_EOQ : 0) |
			((CONT_STATE == CONT) ? SPI_PUSHR_CONT : 0) |
			SPI_PUSHR_CTAS(1) | (w & 0xFFFF);
			SPIX.SR |= SPI_SR_TCF;
			if(WAIT_STATE == POST) { wait(); }
		}

		static void writeByte(uint8_t b) __attribute__((always_inline)) {
			if(WAIT_STATE == PRE) { wait(); }
			SPIX.PUSHR = ((LAST_STATE == LAST) ? SPI_PUSHR_EOQ : 0) |
			((CONT_STATE == CONT) ? SPI_PUSHR_CONT : 0) |
			SPI_PUSHR_CTAS(0) | (b & 0xFF);
			SPIX.SR |= SPI_SR_TCF;
			if(WAIT_STATE == POST) { wait(); }
		}
	};

	static void writeWord(uint16_t w) __attribute__((always_inline)) { wait(); SPIX.PUSHR = SPI_PUSHR_CTAS(1) | (w & 0xFFFF); SPIX.SR |= SPI_SR_TCF;}
	static void writeWordNoWait(uint16_t w) __attribute__((always_inline)) { SPIX.PUSHR = SPI_PUSHR_CTAS(1) | (w & 0xFFFF); SPIX.SR |= SPI_SR_TCF;}

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); SPIX.PUSHR = SPI_PUSHR_CTAS(0) | (b & 0xFF); SPIX.SR |= SPI_SR_TCF;}
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { SPIX.PUSHR = SPI_PUSHR_CTAS(0) | (b & 0xFF);SPIX.SR |= SPI_SR_TCF; wait(); }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { SPIX.PUSHR = SPI_PUSHR_CTAS(0) | (b & 0xFF); SPIX.SR |= SPI_SR_TCF;}

	static void writeWordCont(uint16_t w) __attribute__((always_inline)) { wait(); SPIX.PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(1) | (w & 0xFFFF); SPIX.SR |= SPI_SR_TCF;}
	static void writeWordContNoWait(uint16_t w) __attribute__((always_inline)) { SPIX.PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(1) | (w & 0xFFFF); SPIX.SR |= SPI_SR_TCF;}

	static void writeByteCont(uint8_t b) __attribute__((always_inline)) { wait(); SPIX.PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0) | (b & 0xFF); SPIX.SR |= SPI_SR_TCF;}
	static void writeByteContPostWait(uint8_t b) __attribute__((always_inline)) { SPIX.PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0) | (b & 0xFF); SPIX.SR |= SPI_SR_TCF;wait(); }
	static void writeByteContNoWait(uint8_t b) __attribute__((always_inline)) { SPIX.PUSHR = SPI_PUSHR_CONT | SPI_PUSHR_CTAS(0) | (b & 0xFF); SPIX.SR |= SPI_SR_TCF;}

	// not the most efficient mechanism in the world - but should be enough for sm16716 and friends
	template <uint8_t BIT> inline static void writeBit(uint8_t b) {
		uint32_t ctar1_save = SPIX.CTAR1;

		// Clear out the FMSZ bits, reset them for 1 bit transferd for the start bit
		uint32_t ctar1 = (ctar1_save & (~SPI_CTAR_FMSZ(15))) | SPI_CTAR_FMSZ(0);
		update_ctar1(ctar1);

		writeWord( (b & (1 << BIT)) != 0);

		update_ctar1(ctar1_save);
	}

	void inline select() __attribute__((always_inline)) {
		save_spi_state();
		if(m_pSelect != NULL) { m_pSelect->select(); }
		setSPIRate();
		enable_pins();
	}

	void inline release() __attribute__((always_inline)) {
		disable_pins();
		if(m_pSelect != NULL) { m_pSelect->release(); }
		restore_spi_state();
	}

	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) { Write<CM, WM, NOTLAST>::writeByte(value); }
	}

	void writeBytesValue(uint8_t value, int len) {
		select();
		while(len--) {
			writeByte(value);
		}
		waitFully();
		release();
	}

	// Write a block of n uint8_ts out
	template <class D> void writeBytes(register uint8_t *data, int len) {
		uint8_t *end = data + len;
		select();
		// could be optimized to write 16bit words out instead of 8bit bytes
		while(data != end) {
			writeByte(D::adjust(*data++));
		}
		D::postBlock(len);
		waitFully();
		release();
	}

	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) {
		select();
		int len = pixels.mLen;

		// Setup the pixel controller
		if((FLAGS & FLAG_START_BIT) == 0) {
			//If no start bit stupiditiy, write out as many 16-bit blocks as we can
			while(pixels.has(2)) {
				// Load and write out the first two bytes
				if(WM == NONE) { wait1(); }
				Write<CM, WM, NOTLAST>::writeWord(D::adjust(pixels.loadAndScale0()) << 8 | D::adjust(pixels.loadAndScale1()));

				// Load and write out the next two bytes (step dithering, advance data in between since we
				// cross pixels here)
				Write<CM, WM, NOTLAST>::writeWord(D::adjust(pixels.loadAndScale2()) << 8 | D::adjust(pixels.stepAdvanceAndLoadAndScale0()));

				// Load and write out the next two bytes
				Write<CM, WM, NOTLAST>::writeWord(D::adjust(pixels.loadAndScale1()) << 8 | D::adjust(pixels.loadAndScale2()));
				pixels.stepDithering();
				pixels.advanceData();
			}

			if(pixels.has(1)) {
				if(WM == NONE) { wait1(); }
				// write out the rest as alternating 16/8-bit blocks (likely to be just one)
				Write<CM, WM, NOTLAST>::writeWord(D::adjust(pixels.loadAndScale0()) << 8 | D::adjust(pixels.loadAndScale1()));
				Write<CM, WM, NOTLAST>::writeByte(D::adjust(pixels.loadAndScale2()));
			}

			D::postBlock(len);
			waitFully();
		} else if(FLAGS & FLAG_START_BIT) {
			uint32_t ctar1_save = SPIX.CTAR1;

			// Clear out the FMSZ bits, reset them for 9 bits transferd for the start bit
			uint32_t ctar1 = (ctar1_save & (~SPI_CTAR_FMSZ(15))) | SPI_CTAR_FMSZ(8);
			update_ctar1(ctar1);

			while(pixels.has(1)) {
				writeWord( 0x100 | D::adjust(pixels.loadAndScale0()));
				writeByte(D::adjust(pixels.loadAndScale1()));
				writeByte(D::adjust(pixels.loadAndScale2()));
				pixels.advanceData();
				pixels.stepDithering();
			}
			D::postBlock(len);
			waitFully();

			// restore ctar1
			update_ctar1(ctar1_save);
		}
		release();
	}
};
#endif

FASTLED_NAMESPACE_END

#endif
