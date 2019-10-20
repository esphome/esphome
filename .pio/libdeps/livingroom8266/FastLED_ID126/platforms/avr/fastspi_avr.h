#ifndef __INC_FASTSPI_AVR_H
#define __INC_FASTSPI_AVR_H

FASTLED_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware SPI support using USART registers and friends
//
// TODO: Complete/test implementation - right now this doesn't work
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// uno/mini/duemilanove
#if defined(AVR_HARDWARE_SPI)

#if defined(UBRR1)

#ifndef UCPHA1
#define UCPHA1 1
#endif

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class AVRUSART1SPIOutput {
	Selectable *m_pSelect;

public:
	AVRUSART1SPIOutput() { m_pSelect = NULL; }
	AVRUSART1SPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	void init() {
		UBRR1 = 0;

		/* Set MSPI mode of operation and SPI data mode 0. */
		UCSR1C = (1<<UMSEL11)|(1<<UMSEL10)|(0<<UCPHA1)|(0<<UCPOL1);
		/* Enable receiver and transmitter. */
		UCSR1B = (1<<RXEN1)|(1<<TXEN1);

		FastPin<_CLOCK_PIN>::setOutput();
		FastPin<_DATA_PIN>::setOutput();

		// must be done last, see page 206
		setSPIRate();
	}

	void setSPIRate() {
		if(_SPI_CLOCK_DIVIDER > 2) {
			UBRR1 = (_SPI_CLOCK_DIVIDER/2)-1;
		} else {
			UBRR1 = 0;
		}
	}


	static void stop() {
		// TODO: stop the uart spi output
	}

	static bool shouldWait(bool wait = false) __attribute__((always_inline)) {
		static bool sWait=false;
		if(sWait) {
			sWait = wait; return true;
		} else {
			sWait = wait; return false;
		}
		// return true;
	}
	static void wait() __attribute__((always_inline)) {
		if(shouldWait()) {
			while(!(UCSR1A & (1<<UDRE1)));
		}
	}
	static void waitFully() __attribute__((always_inline)) { wait(); }

	static void writeWord(uint16_t w) __attribute__((always_inline)) { writeByte(w>>8); writeByte(w&0xFF); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); UDR1=b;  shouldWait(true); }
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { UDR1=b; shouldWait(true); wait(); }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { UDR1=b; shouldWait(true); }


	template <uint8_t BIT> inline static void writeBit(uint8_t b) {
		if(b && (1 << BIT)) {
			FastPin<_DATA_PIN>::hi();
		} else {
			FastPin<_DATA_PIN>::lo();
		}

		FastPin<_CLOCK_PIN>::hi();
		FastPin<_CLOCK_PIN>::lo();
	}

	void enable_pins() { }
	void disable_pins() { }

	void select() {
		if(m_pSelect != NULL) {
			m_pSelect->select();
		}
		enable_pins();
		setSPIRate();
	}

	void release() {
		if(m_pSelect != NULL) {
			m_pSelect->release();
		}
		disable_pins();
	}

	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) {
			writeByte(value);
		}
	}

	void writeBytesValue(uint8_t value, int len) {
		//setSPIRate();
		select();
		while(len--) {
			writeByte(value);
		}
		release();
	}

	// Write a block of n uint8_ts out
	template <class D> void writeBytes(register uint8_t *data, int len) {
		//setSPIRate();
		uint8_t *end = data + len;
		select();
		while(data != end) {
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			writeByte(D::adjust(*data++)); delaycycles<3>();
		}
		release();
	}

	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) {
		//setSPIRate();
		int len = pixels.mLen;

		select();
		while(pixels.has(1)) {
			if(FLAGS & FLAG_START_BIT) {
				writeBit<0>(1);
				writeBytePostWait(D::adjust(pixels.loadAndScale0()));
				writeBytePostWait(D::adjust(pixels.loadAndScale1()));
				writeBytePostWait(D::adjust(pixels.loadAndScale2()));
			} else {
				writeByte(D::adjust(pixels.loadAndScale0()));
				writeByte(D::adjust(pixels.loadAndScale1()));
				writeByte(D::adjust(pixels.loadAndScale2()));
			}

			pixels.advanceData();
			pixels.stepDithering();
		}
		D::postBlock(len);
		release();
	}
};
#endif

#if defined(UBRR0)
template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class AVRUSART0SPIOutput {
	Selectable *m_pSelect;

public:
	AVRUSART0SPIOutput() { m_pSelect = NULL; }
	AVRUSART0SPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	void init() {
		UBRR0 = 0;

		/* Set MSPI mode of operation and SPI data mode 0. */
		UCSR0C = (1<<UMSEL01)|(1<<UMSEL00)/*|(0<<UCPHA0)*/|(0<<UCPOL0);
		/* Enable receiver and transmitter. */
		UCSR0B = (1<<RXEN0)|(1<<TXEN0);

		FastPin<_CLOCK_PIN>::setOutput();
		FastPin<_DATA_PIN>::setOutput();


		// must be done last, see page 206
		setSPIRate();
	}

	void setSPIRate() {
		if(_SPI_CLOCK_DIVIDER > 2) {
			UBRR0 = (_SPI_CLOCK_DIVIDER/2)-1;
		} else {
			UBRR0 = 0;
		}
	}

	static void stop() {
		// TODO: stop the uart spi output
	}

	static bool shouldWait(bool wait = false) __attribute__((always_inline)) {
		static bool sWait=false;
		if(sWait) {
			sWait = wait; return true;
		} else {
			sWait = wait; return false;
		}
		// return true;
	}
	static void wait() __attribute__((always_inline)) {
		if(shouldWait()) {
			while(!(UCSR0A & (1<<UDRE0)));
		}
	}
	static void waitFully() __attribute__((always_inline)) { wait(); }

	static void writeWord(uint16_t w) __attribute__((always_inline)) { writeByte(w>>8); writeByte(w&0xFF); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); UDR0=b;  shouldWait(true); }
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { UDR0=b; shouldWait(true); wait(); }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { UDR0=b; shouldWait(true); }


	template <uint8_t BIT> inline static void writeBit(uint8_t b) {
		if(b && (1 << BIT)) {
			FastPin<_DATA_PIN>::hi();
		} else {
			FastPin<_DATA_PIN>::lo();
		}

		FastPin<_CLOCK_PIN>::hi();
		FastPin<_CLOCK_PIN>::lo();
	}

	void enable_pins() { }
	void disable_pins() { }

	void select() {
		if(m_pSelect != NULL) {
			m_pSelect->select();
		}
		enable_pins();
		setSPIRate();
	}

		void release() {
			if(m_pSelect != NULL) {
				m_pSelect->release();
			}
			disable_pins();
		}

	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) {
			writeByte(value);
		}
	}

	void writeBytesValue(uint8_t value, int len) {
		//setSPIRate();
		select();
		while(len--) {
			writeByte(value);
		}
		release();
	}

	// Write a block of n uint8_ts out
	template <class D> void writeBytes(register uint8_t *data, int len) {
		//setSPIRate();
		uint8_t *end = data + len;
		select();
		while(data != end) {
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			writeByte(D::adjust(*data++)); delaycycles<3>();
		}
		release();
	}

	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) {
		//setSPIRate();
		int len = pixels.mLen;

		select();
		while(pixels.has(1)) {
			if(FLAGS & FLAG_START_BIT) {
				writeBit<0>(1);
				writeBytePostWait(D::adjust(pixels.loadAndScale0()));
				writeBytePostWait(D::adjust(pixels.loadAndScale1()));
				writeBytePostWait(D::adjust(pixels.loadAndScale2()));
			} else {
				writeByte(D::adjust(pixels.loadAndScale0()));
				writeByte(D::adjust(pixels.loadAndScale1()));
				writeByte(D::adjust(pixels.loadAndScale2()));
			}

			pixels.advanceData();
			pixels.stepDithering();
		}
		D::postBlock(len);
		waitFully();
		release();
	}
};

#endif


#if defined(SPSR)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware SPI support using SPDR registers and friends
//
// Technically speaking, this uses the AVR SPI registers.  This will work on the Teensy 3.0 because Paul made a set of compatability
// classes that map the AVR SPI registers to ARM's, however this caps the performance of output.
//
// TODO: implement ARMHardwareSPIOutput
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class AVRHardwareSPIOutput {
	Selectable *m_pSelect;
	bool mWait;
public:
	AVRHardwareSPIOutput() { m_pSelect = NULL; mWait = false;}
	AVRHardwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	void setSPIRate() {
		SPCR &= ~ ( (1<<SPR1) | (1<<SPR0) ); 	// clear out the prescalar bits

	    bool b2x = false;

	    if(_SPI_CLOCK_DIVIDER >= 128) { SPCR |= (1<<SPR1); SPCR |= (1<<SPR0); }
	    else if(_SPI_CLOCK_DIVIDER >= 64) { SPCR |= (1<<SPR1);}
	    else if(_SPI_CLOCK_DIVIDER >= 32) { SPCR |= (1<<SPR1); b2x = true;  }
	    else if(_SPI_CLOCK_DIVIDER >= 16) { SPCR |= (1<<SPR0); }
	    else if(_SPI_CLOCK_DIVIDER >= 8) { SPCR |= (1<<SPR0); b2x = true; }
	    else if(_SPI_CLOCK_DIVIDER >= 4) { /* do nothing - default rate */ }
	    else { b2x = true; }

	    if(b2x) { SPSR |= (1<<SPI2X); }
	    else { SPSR &= ~ (1<<SPI2X); }
	}

	void init() {
		volatile uint8_t clr;

		// set the pins to output
		FastPin<_DATA_PIN>::setOutput();
		FastPin<_CLOCK_PIN>::setOutput();
#ifdef SPI_SELECT
		// Make sure the slave select line is set to output, or arduino will block us
		FastPin<SPI_SELECT>::setOutput();
		FastPin<SPI_SELECT>::lo();
#endif

		SPCR |= ((1<<SPE) | (1<<MSTR) ); 		// enable SPI as master
		SPCR &= ~ ( (1<<SPR1) | (1<<SPR0) ); 	// clear out the prescalar bits

		clr = SPSR; // clear SPI status register
		clr = SPDR; // clear SPI data register
		clr;

	    bool b2x = false;

	    if(_SPI_CLOCK_DIVIDER >= 128) { SPCR |= (1<<SPR1); SPCR |= (1<<SPR0); }
	    else if(_SPI_CLOCK_DIVIDER >= 64) { SPCR |= (1<<SPR1);}
	    else if(_SPI_CLOCK_DIVIDER >= 32) { SPCR |= (1<<SPR1); b2x = true;  }
	    else if(_SPI_CLOCK_DIVIDER >= 16) { SPCR |= (1<<SPR0); }
	    else if(_SPI_CLOCK_DIVIDER >= 8) { SPCR |= (1<<SPR0); b2x = true; }
	    else if(_SPI_CLOCK_DIVIDER >= 4) { /* do nothing - default rate */ }
	    else { b2x = true; }

	    if(b2x) { SPSR |= (1<<SPI2X); }
	    else { SPSR &= ~ (1<<SPI2X); }

	    SPDR=0;
	    shouldWait(false);
			release();
		}

	static bool shouldWait(bool wait = false) __attribute__((always_inline)) {
		static bool sWait=false;
		if(sWait) { sWait = wait; return true; } else { sWait = wait; return false; }
		// return true;
	}
	static void wait() __attribute__((always_inline)) { if(shouldWait()) { while(!(SPSR & (1<<SPIF))); } }
	static void waitFully() __attribute__((always_inline)) { wait(); }

	static void writeWord(uint16_t w) __attribute__((always_inline)) { writeByte(w>>8); writeByte(w&0xFF); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); SPDR=b;  shouldWait(true); }
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { SPDR=b; shouldWait(true); wait(); }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { SPDR=b; shouldWait(true); }

	template <uint8_t BIT> inline static void writeBit(uint8_t b) {
		SPCR &= ~(1 << SPE);
		if(b & (1 << BIT)) {
			FastPin<_DATA_PIN>::hi();
		} else {
			FastPin<_DATA_PIN>::lo();
		}

		FastPin<_CLOCK_PIN>::hi();
		FastPin<_CLOCK_PIN>::lo();
		SPCR |= 1 << SPE;
		shouldWait(false);
	}

	void enable_pins() {
		SPCR |= ((1<<SPE) | (1<<MSTR) ); 		// enable SPI as master
	}

	void disable_pins() {
		SPCR &= ~(((1<<SPE) | (1<<MSTR) )); // disable SPI
	}

	void select() {
		if(m_pSelect != NULL) { m_pSelect->select(); }
		enable_pins();
		setSPIRate();
	}

	void release() {
		if(m_pSelect != NULL) { m_pSelect->release(); }
		disable_pins();
	}

	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) { writeByte(value); }
	}

	void writeBytesValue(uint8_t value, int len) {
		//setSPIRate();
		select();
		while(len--) {
			writeByte(value);
		}
		release();
	}

	// Write a block of n uint8_ts out
	template <class D> void writeBytes(register uint8_t *data, int len) {
		//setSPIRate();
		uint8_t *end = data + len;
		select();
		while(data != end) {
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			writeByte(D::adjust(*data++)); delaycycles<3>();
		}
		release();
	}

	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) {
		//setSPIRate();
		int len = pixels.mLen;

		select();
		while(pixels.has(1)) {
			if(FLAGS & FLAG_START_BIT) {
				writeBit<0>(1);
				writeBytePostWait(D::adjust(pixels.loadAndScale0()));
				writeBytePostWait(D::adjust(pixels.loadAndScale1()));
				writeBytePostWait(D::adjust(pixels.loadAndScale2()));
			} else {
				writeByte(D::adjust(pixels.loadAndScale0()));
				writeByte(D::adjust(pixels.loadAndScale1()));
				writeByte(D::adjust(pixels.loadAndScale2()));
			}

			pixels.advanceData();
			pixels.stepDithering();
		}
		D::postBlock(len);
		waitFully();
		release();
	}
};
#elif defined(SPSR0)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware SPI support using SPDR0 registers and friends
//
// Technically speaking, this uses the AVR SPI registers.  This will work on the Teensy 3.0 because Paul made a set of compatability
// classes that map the AVR SPI registers to ARM's, however this caps the performance of output.
//
// TODO: implement ARMHardwareSPIOutput
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class AVRHardwareSPIOutput {
	Selectable *m_pSelect;
	bool mWait;
public:
	AVRHardwareSPIOutput() { m_pSelect = NULL; mWait = false;}
	AVRHardwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	void setSPIRate() {
		SPCR0 &= ~ ( (1<<SPR10) | (1<<SPR0) ); 	// clear out the prescalar bits

	    bool b2x = false;

	    if(_SPI_CLOCK_DIVIDER >= 128) { SPCR0 |= (1<<SPR1); SPCR0 |= (1<<SPR0); }
	    else if(_SPI_CLOCK_DIVIDER >= 64) { SPCR0 |= (1<<SPR1);}
	    else if(_SPI_CLOCK_DIVIDER >= 32) { SPCR0 |= (1<<SPR1); b2x = true;  }
	    else if(_SPI_CLOCK_DIVIDER >= 16) { SPCR0 |= (1<<SPR0); }
	    else if(_SPI_CLOCK_DIVIDER >= 8) { SPCR0 |= (1<<SPR0); b2x = true; }
	    else if(_SPI_CLOCK_DIVIDER >= 4) { /* do nothing - default rate */ }
	    else { b2x = true; }

	    if(b2x) { SPSR0 |= (1<<SPI2X); }
	    else { SPSR0 &= ~ (1<<SPI2X); }
	}

	void init() {
		volatile uint8_t clr;

		// set the pins to output
		FastPin<_DATA_PIN>::setOutput();
		FastPin<_CLOCK_PIN>::setOutput();
#ifdef SPI_SELECT
		// Make sure the slave select line is set to output, or arduino will block us
		FastPin<SPI_SELECT>::setOutput();
		FastPin<SPI_SELECT>::lo();
#endif

		SPCR0 |= ((1<<SPE) | (1<<MSTR) ); 		// enable SPI as master
		SPCR0 &= ~ ( (1<<SPR1) | (1<<SPR0) ); 	// clear out the prescalar bits

		clr = SPSR0; // clear SPI status register
		clr = SPDR0; // clear SPI data register
		clr;

	    bool b2x = false;

	    if(_SPI_CLOCK_DIVIDER >= 128) { SPCR0 |= (1<<SPR1); SPCR0 |= (1<<SPR0); }
	    else if(_SPI_CLOCK_DIVIDER >= 64) { SPCR0 |= (1<<SPR1);}
	    else if(_SPI_CLOCK_DIVIDER >= 32) { SPCR0 |= (1<<SPR1); b2x = true;  }
	    else if(_SPI_CLOCK_DIVIDER >= 16) { SPCR0 |= (1<<SPR0); }
	    else if(_SPI_CLOCK_DIVIDER >= 8) { SPCR0 |= (1<<SPR0); b2x = true; }
	    else if(_SPI_CLOCK_DIVIDER >= 4) { /* do nothing - default rate */ }
	    else { b2x = true; }

	    if(b2x) { SPSR0 |= (1<<SPI2X); }
	    else { SPSR0 &= ~ (1<<SPI2X); }

	    SPDR0=0;
	    shouldWait(false);
			release();
		}

	static bool shouldWait(bool wait = false) __attribute__((always_inline)) {
		static bool sWait=false;
		if(sWait) { sWait = wait; return true; } else { sWait = wait; return false; }
		// return true;
	}
	static void wait() __attribute__((always_inline)) { if(shouldWait()) { while(!(SPSR0 & (1<<SPIF))); } }
	static void waitFully() __attribute__((always_inline)) { wait(); }

	static void writeWord(uint16_t w) __attribute__((always_inline)) { writeByte(w>>8); writeByte(w&0xFF); }

	static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); SPDR0=b;  shouldWait(true); }
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { SPDR0=b; shouldWait(true); wait(); }
	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { SPDR0=b; shouldWait(true); }

	template <uint8_t BIT> inline static void writeBit(uint8_t b) {
		SPCR0 &= ~(1 << SPE);
		if(b & (1 << BIT)) {
			FastPin<_DATA_PIN>::hi();
		} else {
			FastPin<_DATA_PIN>::lo();
		}

		FastPin<_CLOCK_PIN>::hi();
		FastPin<_CLOCK_PIN>::lo();
		SPCR0 |= 1 << SPE;
		shouldWait(false);
	}

	void enable_pins() {
		SPCR0 |= ((1<<SPE) | (1<<MSTR) ); 		// enable SPI as master
	}

	void disable_pins() {
		SPCR0 &= ~(((1<<SPE) | (1<<MSTR) )); // disable SPI
	}

	void select() {
		if(m_pSelect != NULL) { m_pSelect->select(); }
		enable_pins();
		setSPIRate();
	}

	void release() {
		if(m_pSelect != NULL) { m_pSelect->release(); }
		disable_pins();
	}

	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) { writeByte(value); }
	}

	void writeBytesValue(uint8_t value, int len) {
		//setSPIRate();
		select();
		while(len--) {
			writeByte(value);
		}
		release();
	}

	// Write a block of n uint8_ts out
	template <class D> void writeBytes(register uint8_t *data, int len) {
		//setSPIRate();
		uint8_t *end = data + len;
		select();
		while(data != end) {
			// a slight touch of delay here helps optimize the timing of the status register check loop (not used on ARM)
			writeByte(D::adjust(*data++)); delaycycles<3>();
		}
		release();
	}

	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) {
		//setSPIRate();
		int len = pixels.mLen;

		select();
		while(pixels.has(1)) {
			if(FLAGS & FLAG_START_BIT) {
				writeBit<0>(1);
				writeBytePostWait(D::adjust(pixels.loadAndScale0()));
				writeBytePostWait(D::adjust(pixels.loadAndScale1()));
				writeBytePostWait(D::adjust(pixels.loadAndScale2()));
			} else {
				writeByte(D::adjust(pixels.loadAndScale0()));
				writeByte(D::adjust(pixels.loadAndScale1()));
				writeByte(D::adjust(pixels.loadAndScale2()));
			}

			pixels.advanceData();
			pixels.stepDithering();
		}
		D::postBlock(len);
		waitFully();
		release();
	}
};
#endif

#else
// #define FASTLED_FORCE_SOFTWARE_SPI
#endif

FASTLED_NAMESPACE_END;


#endif
