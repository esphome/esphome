#ifndef __INC_FASTSPI_NOP_H
#define __INC_FASTSPI_NOP_H

#if 0 // Guard against the arduino ide idiotically including every header file

#include "FastLED.h"

FASTLED_NAMESPACE_BEGIN

/// A nop/stub class, mostly to show the SPI methods that are needed/used by the various SPI chipset implementations.  Should
/// be used as a definition for the set of methods that the spi implementation classes should use (since C++ doesn't support the
/// idea of interfaces - it's possible this could be done with virtual classes, need to decide if i want that overhead)
template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class NOPSPIOutput {
	Selectable *m_pSelect;

public:
	NOPSPIOutput() { m_pSelect = NULL; }
	NOPSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }

	/// set the object representing the selectable
	void setSelect(Selectable *pSelect) { m_pSelect = pSelect;  }

	/// initialize the SPI subssytem
	void init() { /* TODO */ }

	/// latch the CS select
	void select() { /* TODO */ }

	/// release the CS select
	void release() { /* TODO */ }

	/// wait until all queued up data has been written
	void waitFully();

	/// not the most efficient mechanism in the world - but should be enough for sm16716 and friends
	template <uint8_t BIT> inline static void writeBit(uint8_t b) { /* TODO */ }

	/// write a byte out via SPI (returns immediately on writing register)
	void writeByte(uint8_t b) { /* TODO */ }
	/// write a word out via SPI (returns immediately on writing register)
	void writeWord(uint16_t w) { /* TODO */ }

	/// A raw set of writing byte values, assumes setup/init/waiting done elsewhere (static for use by adjustment classes)
	static void writeBytesValueRaw(uint8_t value, int len) { /* TODO */ }

	/// A full cycle of writing a value for len bytes, including select, release, and waiting
	void writeBytesValue(uint8_t value, int len) { /* TODO */ }

	/// A full cycle of writing a raw block of data out, including select, release, and waiting
	void writeBytes(uint8_t *data, int len) { /* TODO */ }

	/// write a single bit out, which bit from the passed in byte is determined by template parameter
	template <uint8_t BIT> inline static void writeBit(uint8_t b) { /* TODO */ }

	/// write out pixel data from the given PixelController object
	template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) { /* TODO */ }

};

FASTLED_NAMESPACE_END

#endif
#endif
