#ifndef __INC_DMX_H
#define __INC_DMX_H

#include "FastLED.h"

#ifdef DmxSimple_h
#include <DmxSimple.h>
#define HAS_DMX_SIMPLE

///@ingroup chipsets
///@{
FASTLED_NAMESPACE_BEGIN

// note - dmx simple must be included before FastSPI for this code to be enabled
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB> class DMXSimpleController : public CPixelLEDController<RGB_ORDER> {
public:
	// initialize the LED controller
	virtual void init() { DmxSimple.usePin(DATA_PIN); }

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		int iChannel = 1;
		while(pixels.has(1)) {
			DmxSimple.write(iChannel++, pixels.loadAndScale0());
			DmxSimple.write(iChannel++, pixels.loadAndScale1());
			DmxSimple.write(iChannel++, pixels.loadAndScale2());
			pixels.advanceData();
			pixels.stepDithering();
		}
	}
};

FASTLED_NAMESPACE_END

#endif

#ifdef DmxSerial_h
#include <DMXSerial.h>

FASTLED_NAMESPACE_BEGIN

template <EOrder RGB_ORDER = RGB> class DMXSerialController : public CPixelLEDController<RGB_ORDER> {
public:
	// initialize the LED controller
	virtual void init() { DMXSerial.init(DMXController); }

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		int iChannel = 1;
		while(pixels.has(1)) {
			DMXSerial.write(iChannel++, pixels.loadAndScale0());
			DMXSerial.write(iChannel++, pixels.loadAndScale1());
			DMXSerial.write(iChannel++, pixels.loadAndScale2());
			pixels.advanceData();
			pixels.stepDithering();
		}
	}
};

FASTLED_NAMESPACE_END
///@}

#define HAS_DMX_SERIAL
#endif

#endif
