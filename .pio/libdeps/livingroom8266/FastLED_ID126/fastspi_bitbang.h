#ifndef __INC_FASTSPI_BITBANG_H
#define __INC_FASTSPI_BITBANG_H

#include "FastLED.h"

#include "fastled_delay.h"

FASTLED_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Software SPI (aka bit-banging) support - with aggressive optimizations for when the clock and data pin are on the same port
//
// TODO: Replace the select pin definition with a set of pins, to allow using mux hardware for routing in the future
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint8_t SPI_SPEED>
class AVRSoftwareSPIOutput {
	// The data types for pointers to the pin port - typedef'd here from the Pin definition because on avr these
	// are pointers to 8 bit values, while on arm they are 32 bit
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<CLOCK_PIN>::port_ptr_t clock_ptr_t;

	// The data type for what's at a pin's port - typedef'd here from the Pin definition because on avr the ports
	// are 8 bits wide while on arm they are 32.
	typedef typename FastPin<DATA_PIN>::port_t data_t;
	typedef typename FastPin<CLOCK_PIN>::port_t clock_t;
	Selectable 	*m_pSelect;

public:
	AVRSoftwareSPIOutput() { m_pSelect = NULL; }
	AVRSoftwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

	void init() {
		// set the pins to output and make sure the select is released (which apparently means hi?  This is a bit
		// confusing to me)
		FastPin<DATA_PIN>::setOutput();
		FastPin<CLOCK_PIN>::setOutput();
		release();
	}

	// stop the SPI output.  Pretty much a NOP with software, as there's no registers to kick
	static void stop() { }

	// wait until the SPI subsystem is ready for more data to write.  A NOP when bitbanging
	static void wait() __attribute__((always_inline)) { }
	static void waitFully() __attribute__((always_inline)) { wait(); }

	static void writeByteNoWait(uint8_t b) __attribute__((always_inline)) { writeByte(b); }
	static void writeBytePostWait(uint8_t b) __attribute__((always_inline)) { writeByte(b); wait(); }

	static void writeWord(uint16_t w) __attribute__((always_inline)) { writeByte(w>>8); writeByte(w&0xFF); }

	// naive writeByte implelentation, simply calls writeBit on the 8 bits in the byte.
	static void writeByte(uint8_t b) {
		writeBit<7>(b);
		writeBit<6>(b);
		writeBit<5>(b);
		writeBit<4>(b);
		writeBit<3>(b);
		writeBit<2>(b);
		writeBit<1>(b);
		writeBit<0>(b);
	}

private:
	// writeByte implementation with data/clock registers passed in.
	static void writeByte(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin)  {
		writeBit<7>(b, clockpin, datapin);
		writeBit<6>(b, clockpin, datapin);
		writeBit<5>(b, clockpin, datapin);
		writeBit<4>(b, clockpin, datapin);
		writeBit<3>(b, clockpin, datapin);
		writeBit<2>(b, clockpin, datapin);
		writeBit<1>(b, clockpin, datapin);
		writeBit<0>(b, clockpin, datapin);
	}

	// writeByte implementation with the data register passed in and prebaked values for data hi w/clock hi and
	// low and data lo w/clock hi and lo.  This is to be used when clock and data are on the same GPIO register,
	// can get close to getting a bit out the door in 2 clock cycles!
	static void writeByte(uint8_t b, data_ptr_t datapin,
						  data_t hival, data_t loval,
						  clock_t hiclock, clock_t loclock) {
		writeBit<7>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<6>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<5>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<4>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<3>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<2>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<1>(b, datapin, hival, loval, hiclock, loclock);
		writeBit<0>(b, datapin, hival, loval, hiclock, loclock);
	}

	// writeByte implementation with not just registers passed in, but pre-baked values for said registers for
	// data hi/lo and clock hi/lo values.  Note: weird things will happen if this method is called in cases where
	// the data and clock pins are on the same port!  Don't do that!
	static void writeByte(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin,
						  data_t hival, data_t loval,
						  clock_t hiclock, clock_t loclock) {
		writeBit<7>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<6>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<5>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<4>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<3>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<2>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<1>(b, clockpin, datapin, hival, loval, hiclock, loclock);
		writeBit<0>(b, clockpin, datapin, hival, loval, hiclock, loclock);
	}

public:

	// We want to make sure that the clock pulse is held high for a nininum of 35ns.
	#define MIN_DELAY (NS(35) - 3)

  #define CLOCK_HI_DELAY delaycycles<MIN_DELAY>(); delaycycles<(((SPI_SPEED-6) / 2) - MIN_DELAY)>();
	#define CLOCK_LO_DELAY delaycycles<(((SPI_SPEED-6) / 4))>();

	// write the BIT'th bit out via spi, setting the data pin then strobing the clcok
	template <uint8_t BIT> __attribute__((always_inline, hot)) inline static void writeBit(uint8_t b) {
		//cli();
		if(b & (1 << BIT)) {
			FastPin<DATA_PIN>::hi();
#ifdef ESP32
			// try to ensure we never have adjacent write opcodes to the same register
			FastPin<CLOCK_PIN>::lo();
			FastPin<CLOCK_PIN>::hi(); CLOCK_HI_DELAY; 
			FastPin<CLOCK_PIN>::toggle(); CLOCK_LO_DELAY; 
#else
			FastPin<CLOCK_PIN>::hi(); CLOCK_HI_DELAY;
			FastPin<CLOCK_PIN>::lo(); CLOCK_LO_DELAY;
#endif
		} else {
			FastPin<DATA_PIN>::lo();
			FastPin<CLOCK_PIN>::hi(); CLOCK_HI_DELAY;
#ifdef ESP32
			// try to ensure we never have adjacent write opcodes to the same register
			FastPin<CLOCK_PIN>::toggle(); CLOCK_HI_DELAY; 
#else
			FastPin<CLOCK_PIN>::lo(); CLOCK_LO_DELAY;
#endif
		}
		//sei();
	}

private:
	// write the BIT'th bit out via spi, setting the data pin then strobing the clock, using the passed in pin registers to accelerate access if needed
	template <uint8_t BIT> __attribute__((always_inline)) inline static void writeBit(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin) {
		if(b & (1 << BIT)) {
			FastPin<DATA_PIN>::hi(datapin);
			FastPin<CLOCK_PIN>::hi(clockpin); CLOCK_HI_DELAY;
			FastPin<CLOCK_PIN>::lo(clockpin); CLOCK_LO_DELAY;
		} else {
			FastPin<DATA_PIN>::lo(datapin);
			FastPin<CLOCK_PIN>::hi(clockpin); CLOCK_HI_DELAY;
			FastPin<CLOCK_PIN>::lo(clockpin); CLOCK_LO_DELAY;
		}

	}

	// the version of write to use when clock and data are on separate pins with precomputed values for setting
	// the clock and data pins
	template <uint8_t BIT> __attribute__((always_inline)) inline static void writeBit(uint8_t b, clock_ptr_t clockpin, data_ptr_t datapin,
													data_t hival, data_t loval, clock_t hiclock, clock_t loclock) {
		// // only need to explicitly set clock hi if clock and data are on different ports
		if(b & (1 << BIT)) {
			FastPin<DATA_PIN>::fastset(datapin, hival);
			FastPin<CLOCK_PIN>::fastset(clockpin, hiclock); CLOCK_HI_DELAY;
			FastPin<CLOCK_PIN>::fastset(clockpin, loclock); CLOCK_LO_DELAY;
		} else {
			// FL_NOP;
			FastPin<DATA_PIN>::fastset(datapin, loval);
			FastPin<CLOCK_PIN>::fastset(clockpin, hiclock); CLOCK_HI_DELAY;
			FastPin<CLOCK_PIN>::fastset(clockpin, loclock); CLOCK_LO_DELAY;
		}
	}

	// the version of write to use when clock and data are on the same port with precomputed values for the various
	// combinations
	template <uint8_t BIT> __attribute__((always_inline)) inline static void writeBit(uint8_t b, data_ptr_t clockdatapin,
													data_t datahiclockhi, data_t dataloclockhi,
													data_t datahiclocklo, data_t dataloclocklo) {
#if 0
		writeBit<BIT>(b);
#else
		if(b & (1 << BIT)) {
			FastPin<DATA_PIN>::fastset(clockdatapin, datahiclocklo);
			FastPin<DATA_PIN>::fastset(clockdatapin, datahiclockhi); CLOCK_HI_DELAY;
			FastPin<DATA_PIN>::fastset(clockdatapin, datahiclocklo); CLOCK_LO_DELAY;
		} else {
			// FL_NOP;
			FastPin<DATA_PIN>::fastset(clockdatapin, dataloclocklo);
			FastPin<DATA_PIN>::fastset(clockdatapin, dataloclockhi); CLOCK_HI_DELAY;
			FastPin<DATA_PIN>::fastset(clockdatapin, dataloclocklo); CLOCK_LO_DELAY;
		}
#endif
	}
public:

	// select the SPI output (TODO: research whether this really means hi or lo.  Alt TODO: move select responsibility out of the SPI classes
	// entirely, make it up to the caller to remember to lock/select the line?)
	void select() { if(m_pSelect != NULL) { m_pSelect->select(); } } // FastPin<SELECT_PIN>::hi(); }

	// release the SPI line
	void release() { if(m_pSelect != NULL) { m_pSelect->release(); } } // FastPin<SELECT_PIN>::lo(); }

	// Write out len bytes of the given value out over SPI.  Useful for quickly flushing, say, a line of 0's down the line.
	void writeBytesValue(uint8_t value, int len) {
		select();
		writeBytesValueRaw(value, len);
		release();
	}

	static void writeBytesValueRaw(uint8_t value, int len) {
#ifdef FAST_SPI_INTERRUPTS_WRITE_PINS
		// TODO: Weird things may happen if software bitbanging SPI output and other pins on the output reigsters are being twiddled.  Need
		// to allow specifying whether or not exclusive i/o access is allowed during this process, and if i/o access is not allowed fall
		// back to the degenerative code below
		while(len--) {
			writeByte(value);
		}
#else
		register data_ptr_t datapin = FastPin<DATA_PIN>::port();

		if(FastPin<DATA_PIN>::port() != FastPin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register clock_ptr_t clockpin = FastPin<CLOCK_PIN>::port();
			register data_t datahi = FastPin<DATA_PIN>::hival();
			register data_t datalo = FastPin<DATA_PIN>::loval();
			register clock_t clockhi = FastPin<CLOCK_PIN>::hival();
			register clock_t clocklo = FastPin<CLOCK_PIN>::loval();
			while(len--) {
				writeByte(value, clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins
			register data_t datahi_clockhi = FastPin<DATA_PIN>::hival() | FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clockhi = FastPin<DATA_PIN>::loval() | FastPin<CLOCK_PIN>::mask();
			register data_t datahi_clocklo = FastPin<DATA_PIN>::hival() & ~FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clocklo = FastPin<DATA_PIN>::loval() & ~FastPin<CLOCK_PIN>::mask();

			while(len--) {
				writeByte(value, datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
		}
#endif
	}

	// write a block of len uint8_ts out.  Need to type this better so that explicit casts into the call aren't required.
	// note that this template version takes a class parameter for a per-byte modifier to the data.
	template <class D> void writeBytes(register uint8_t *data, int len) {
		select();
#ifdef FAST_SPI_INTERRUPTS_WRITE_PINS
		uint8_t *end = data + len;
		while(data != end) {
			writeByte(D::adjust(*data++));
		}
#else
		register clock_ptr_t clockpin = FastPin<CLOCK_PIN>::port();
		register data_ptr_t datapin = FastPin<DATA_PIN>::port();

		if(FastPin<DATA_PIN>::port() != FastPin<CLOCK_PIN>::port()) {
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register data_t datahi = FastPin<DATA_PIN>::hival();
			register data_t datalo = FastPin<DATA_PIN>::loval();
			register clock_t clockhi = FastPin<CLOCK_PIN>::hival();
			register clock_t clocklo = FastPin<CLOCK_PIN>::loval();
			uint8_t *end = data + len;

			while(data != end) {
				writeByte(D::adjust(*data++), clockpin, datapin, datahi, datalo, clockhi, clocklo);
			}

		} else {
			// FastPin<CLOCK_PIN>::hi();
			// If data and clock are on the same port then we can combine setting the data and clock pins
			register data_t datahi_clockhi = FastPin<DATA_PIN>::hival() | FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clockhi = FastPin<DATA_PIN>::loval() | FastPin<CLOCK_PIN>::mask();
			register data_t datahi_clocklo = FastPin<DATA_PIN>::hival() & ~FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clocklo = FastPin<DATA_PIN>::loval() & ~FastPin<CLOCK_PIN>::mask();

			uint8_t *end = data + len;

			while(data != end) {
				writeByte(D::adjust(*data++), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
			}
			// FastPin<CLOCK_PIN>::lo();
		}
#endif
		D::postBlock(len);
		release();
	}

	// default version of writing a block of data out to the SPI port, with no data modifications being made
	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }


	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning of each grouping, as well as a class specifying a per
	// byte of data modification to be made.  (See DATA_NOP above)
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER>  __attribute__((noinline)) void writePixels(PixelController<RGB_ORDER> pixels) {
		select();
		int len = pixels.mLen;

#ifdef FAST_SPI_INTERRUPTS_WRITE_PINS
		// If interrupts or other things may be generating output while we're working on things, then we need
		// to use this block
		while(pixels.has(1)) {
			if(FLAGS & FLAG_START_BIT) {
				writeBit<0>(1);
			}
			writeByte(D::adjust(pixels.loadAndScale0()));
			writeByte(D::adjust(pixels.loadAndScale1()));
			writeByte(D::adjust(pixels.loadAndScale2()));
			pixels.advanceData();
			pixels.stepDithering();
		}
#else
		// If we can guaruntee that no one else will be writing data while we are running (namely, changing the values of the PORT/PDOR pins)
		// then we can use a bunch of optimizations in here
		register data_ptr_t datapin = FastPin<DATA_PIN>::port();

		if(FastPin<DATA_PIN>::port() != FastPin<CLOCK_PIN>::port()) {
			register clock_ptr_t clockpin = FastPin<CLOCK_PIN>::port();
			// If data and clock are on different ports, then writing a bit will consist of writing the value foor
			// the bit (hi or low) to the data pin port, and then two writes to the clock port to strobe the clock line
			register data_t datahi = FastPin<DATA_PIN>::hival();
			register data_t datalo = FastPin<DATA_PIN>::loval();
			register clock_t clockhi = FastPin<CLOCK_PIN>::hival();
			register clock_t clocklo = FastPin<CLOCK_PIN>::loval();

			while(pixels.has(1)) {
				if(FLAGS & FLAG_START_BIT) {
					writeBit<0>(1, clockpin, datapin, datahi, datalo, clockhi, clocklo);
				}
				writeByte(D::adjust(pixels.loadAndScale0()), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				writeByte(D::adjust(pixels.loadAndScale1()), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				writeByte(D::adjust(pixels.loadAndScale2()), clockpin, datapin, datahi, datalo, clockhi, clocklo);
				pixels.advanceData();
				pixels.stepDithering();
			}

		} else {
			// If data and clock are on the same port then we can combine setting the data and clock pins
			register data_t datahi_clockhi = FastPin<DATA_PIN>::hival() | FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clockhi = FastPin<DATA_PIN>::loval() | FastPin<CLOCK_PIN>::mask();
			register data_t datahi_clocklo = FastPin<DATA_PIN>::hival() & ~FastPin<CLOCK_PIN>::mask();
			register data_t datalo_clocklo = FastPin<DATA_PIN>::loval() & ~FastPin<CLOCK_PIN>::mask();

			while(pixels.has(1)) {
				if(FLAGS & FLAG_START_BIT) {
					writeBit<0>(1, datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				}
				writeByte(D::adjust(pixels.loadAndScale0()), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				writeByte(D::adjust(pixels.loadAndScale1()), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				writeByte(D::adjust(pixels.loadAndScale2()), datapin, datahi_clockhi, datalo_clockhi, datahi_clocklo, datalo_clocklo);
				pixels.advanceData();
				pixels.stepDithering();
			}
		}
#endif
		D::postBlock(len);
		release();
	}
};

FASTLED_NAMESPACE_END

#endif
