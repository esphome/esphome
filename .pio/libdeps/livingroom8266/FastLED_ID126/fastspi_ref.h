#ifndef __INC_FASTSPI_ARM_SAM_H
#define __INC_FASTSPI_ARM_SAM_H

#if 0 // guard against the arduino ide idiotically including every header file
#include "FastLED.h"

FASTLED_NAMESPACE_BEGIN

// A skeletal implementation of hardware SPI support.  Fill in the necessary code for init, waiting, and writing.  The rest of
// the method implementations should provide a starting point, even if not hte most efficient to start with
template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class REFHardwareSPIOutput {
	Selectable *m_pSelect;
public:
	SAMHardwareSPIOutput() { m_pSelect = NULL; }
	SAMHArdwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }

	// set the object representing the selectable
	void setSelect(Selectable *pSelect) { /* TODO */ }

	// initialize the SPI subssytem
	void init() { /* TODO */ }

	// latch the CS select
	void inline select() __attribute__((always_inline)) { if(m_pSelect != NULL) { m_pSelect->select(); } }

	// release the CS select
	void inline release() __attribute__((always_inline)) { if(m_pSelect != NULL) { m_pSelect->release(); } }

	// wait until all queued up data has been written
	static void waitFully() { /* TODO */ }

	// write a byte out via SPI (returns immediately on writing register)
	static void writeByte(uint8_t b) { /* TODO */ }

	// write a word out via SPI (returns immediately on writing register)
	static void writeWord(uint16_t w) { /* TODO */ }

	// A raw set of writing byte values, assumes setup/init/waiting done elsewhere
	static void writeBytesValueRaw(uint8_t value, int len) {
		while(len--) { writeByte(value); }
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytesValue(uint8_t value, int len) {
		select(); writeBytesValueRaw(value, len); release();
	}

	// A full cycle of writing a value for len bytes, including select, release, and waiting
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

	// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }

	// write a single bit out, which bit from the passed in byte is determined by template parameter
	template <uint8_t BIT> inline static void writeBit(uint8_t b) { /* TODO */ }

	// write a block of uint8_ts out in groups of three.  len is the total number of uint8_ts to write out.  The template
	// parameters indicate how many uint8_ts to skip at the beginning and/or end of each grouping
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) {
		select();
		while(data != end) {
			if(FLAGS & FLAG_START_BIT) {
				writeBit<0>(1);
			}
			writeByte(D::adjust(pixels.loadAndScale0()));
			writeByte(D::adjust(pixels.loadAndScale1()));
			writeByte(D::adjust(pixels.loadAndScale2()));

			pixels.advanceData();
			pixels.stepDithering();
			data += (3+skip);
		}
		D::postBlock(len);
		release();
	}

};

FASTLED_NAMESPACE_END

#endif

#endif

