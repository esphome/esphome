#ifndef __INC_FASTSPI_LED2_H
#define __INC_FASTSPI_LED2_H

///@file FastLED.h
/// central include file for FastLED, defines the CFastLED class/object

#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
#define FASTLED_HAS_PRAGMA_MESSAGE
#endif

#define FASTLED_VERSION 3002009
#ifndef FASTLED_INTERNAL
#  ifdef FASTLED_HAS_PRAGMA_MESSAGE
#    pragma message "FastLED version 3.002.009"
#  else
#    warning FastLED version 3.002.009  (Not really a warning, just telling you here.)
#  endif
#endif

#ifndef __PROG_TYPES_COMPAT__
#define __PROG_TYPES_COMPAT__
#endif

#ifdef SmartMatrix_h
#include <SmartMatrix.h>
#endif

#ifdef DmxSimple_h
#include <DmxSimple.h>
#endif

#ifdef DmxSerial_h
#include <DMXSerial.h>
#endif

#include <stdint.h>

#include "cpp_compat.h"

#include "fastled_config.h"
#include "led_sysdefs.h"

// Utility functions
#include "fastled_delay.h"
#include "bitswap.h"

#include "controller.h"
#include "fastpin.h"
#include "fastspi_types.h"
#include "dmx.h"

#include "platforms.h"
#include "fastled_progmem.h"

#include "lib8tion.h"
#include "pixeltypes.h"
#include "hsv2rgb.h"
#include "colorutils.h"
#include "pixelset.h"
#include "colorpalettes.h"

#include "noise.h"
#include "power_mgt.h"

#include "fastspi.h"
#include "chipsets.h"

FASTLED_NAMESPACE_BEGIN

/// definitions for the spi chipset constants
enum ESPIChipsets {
    LPD6803,
	LPD8806,
	WS2801,
	WS2803,
	SM16716,
	P9813,
	APA102,
	SK9822,
	DOTSTAR
};

enum ESM { SMART_MATRIX };
enum OWS2811 { OCTOWS2811,OCTOWS2811_400, OCTOWS2813};
enum SWS2812 { WS2812SERIAL };

#ifdef HAS_PIXIE
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class PIXIE : public PixieController<DATA_PIN, RGB_ORDER> {};
#endif

#ifdef FASTLED_HAS_CLOCKLESS
template<uint8_t DATA_PIN> class NEOPIXEL : public WS2812Controller800Khz<DATA_PIN, GRB> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class SM16703 : public SM16703Controller<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class TM1829 : public TM1829Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class TM1812 : public TM1809Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class TM1809 : public TM1809Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class TM1804 : public TM1809Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class TM1803 : public TM1803Controller400Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class UCS1903 : public UCS1903Controller400Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class UCS1903B : public UCS1903BController800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class UCS1904 : public UCS1904Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class UCS2903 : public UCS2903Controller<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class WS2812 : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class WS2852 : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class WS2812B : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class GS1903 : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class SK6812 : public SK6812Controller<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class SK6822 : public SK6822Controller<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class APA106 : public SK6822Controller<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class PL9823 : public PL9823Controller<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class WS2811 : public WS2811Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class WS2813 : public WS2813Controller<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class APA104 : public WS2811Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class WS2811_400 : public WS2811Controller400Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class GE8822 : public GE8822Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class GW6205 : public GW6205Controller800Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class GW6205_400 : public GW6205Controller400Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class LPD1886 : public LPD1886Controller1250Khz<DATA_PIN, RGB_ORDER> {};
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class LPD1886_8BIT : public LPD1886Controller1250Khz_8bit<DATA_PIN, RGB_ORDER> {};
#ifdef DmxSimple_h
template<uint8_t DATA_PIN, EOrder RGB_ORDER> class DMXSIMPLE : public DMXSimpleController<DATA_PIN, RGB_ORDER> {};
#endif
#ifdef DmxSerial_h
template<EOrder RGB_ORDER> class DMXSERIAL : public DMXSerialController<RGB_ORDER> {};
#endif
#endif

enum EBlockChipsets {
#ifdef PORTA_FIRST_PIN
	WS2811_PORTA,
	WS2813_PORTA,
	WS2811_400_PORTA,
	TM1803_PORTA,
	UCS1903_PORTA,
#endif
#ifdef PORTB_FIRST_PIN
	WS2811_PORTB,
	WS2813_PORTB,
	WS2811_400_PORTB,
	TM1803_PORTB,
	UCS1903_PORTB,
#endif
#ifdef PORTC_FIRST_PIN
	WS2811_PORTC,
	WS2813_PORTC,
	WS2811_400_PORTC,
	TM1803_PORTC,
	UCS1903_PORTC,
#endif
#ifdef PORTD_FIRST_PIN
	WS2811_PORTD,
	WS2813_PORTD,
	WS2811_400_PORTD,
	TM1803_PORTD,
	UCS1903_PORTD,
#endif
#ifdef HAS_PORTDC
	WS2811_PORTDC,
	WS2813_PORTDC,
	WS2811_400_PORTDC,
	TM1803_PORTDC,
	UCS1903_PORTDC,
#endif
};

#if defined(LIB8_ATTINY)
#define NUM_CONTROLLERS 2
#else
#define NUM_CONTROLLERS 8
#endif

typedef uint8_t (*power_func)(uint8_t scale, uint32_t data);

/// High level controller interface for FastLED.  This class manages controllers, global settings and trackings
/// such as brightness, and refresh rates, and provides access functions for driving led data to controllers
/// via the show/showColor/clear methods.
/// @nosubgrouping
class CFastLED {
	// int m_nControllers;
	uint8_t  m_Scale; 				///< The current global brightness scale setting
	uint16_t m_nFPS;					///< Tracking for current FPS value
	uint32_t m_nMinMicros;		///< minimum µs between frames, used for capping frame rates.
	uint32_t m_nPowerData;		///< max power use parameter
	power_func m_pPowerFunc;	///< function for overriding brightness when using FastLED.show();

public:
	CFastLED();


	/// Add a CLEDController instance to the world.  Exposed to the public to allow people to implement their own
	/// CLEDController objects or instances.  There are two ways to call this method (as well as the other addLeds)
	/// variations.  The first is with 3 arguments, in which case the arguments are the controller, a pointer to
	/// led data, and the number of leds used by this controller.  The second is with 4 arguments, in which case
	/// the first two arguments are the same, the third argument is an offset into the CRGB data where this controller's
	/// CRGB data begins, and the fourth argument is the number of leds for this controller object.
	/// @param pLed - the led controller being added
	/// @param data - base point to an array of CRGB data structures
	/// @param nLedsOrOffset - number of leds (3 argument version) or offset into the data array
	/// @param nLedsIfOffset - number of leds (4 argument version)
	/// @returns a reference to the added controller
	static CLEDController &addLeds(CLEDController *pLed, struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0);

	/// @name Adding SPI based controllers
  //@{
	/// Add an SPI based  CLEDController instance to the world.
	/// There are two ways to call this method (as well as the other addLeds)
	/// variations.  The first is with 2 arguments, in which case the arguments are  a pointer to
	/// led data, and the number of leds used by this controller.  The second is with 3 arguments, in which case
	/// the first  argument is the same, the second argument is an offset into the CRGB data where this controller's
	/// CRGB data begins, and the third argument is the number of leds for this controller object.
	///
	/// This method also takes a 1 to 5 template parameters for identifying the specific chipset, data and clock pins,
	/// RGB ordering, and SPI data rate
	/// @param data - base point to an array of CRGB data structures
	/// @param nLedsOrOffset - number of leds (3 argument version) or offset into the data array
	/// @param nLedsIfOffset - number of leds (4 argument version)
	/// @tparam CHIPSET - the chipset type
	/// @tparam DATA_PIN - the optional data pin for the leds (if omitted, will default to the first hardware SPI MOSI pin)
	/// @tparam CLOCK_PIN - the optional clock pin for the leds (if omitted, will default to the first hardware SPI clock pin)
	/// @tparam RGB_ORDER - the rgb ordering for the leds (e.g. what order red, green, and blue data is written out in)
	/// @tparam SPI_DATA_RATE - the data rate to drive the SPI clock at, defined using DATA_RATE_MHZ or DATA_RATE_KHZ macros
	/// @returns a reference to the added controller
	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER, uint8_t SPI_DATA_RATE > CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) {
			case LPD6803: { static LPD6803Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case LPD8806: { static LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2801: { static WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2803: { static WS2803Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SM16716: { static SM16716Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case P9813: { static P9813Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case DOTSTAR:
			case APA102: { static APA102Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SK9822: { static SK9822Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}

	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN > static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) {
			case LPD6803: { static LPD6803Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case LPD8806: { static LPD8806Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2801: { static WS2801Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2803: { static WS2803Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SM16716: { static SM16716Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case P9813: { static P9813Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case DOTSTAR:
			case APA102: { static APA102Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SK9822: { static SK9822Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}

	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER > static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) {
			case LPD6803: { static LPD6803Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case LPD8806: { static LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2801: { static WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2803: { static WS2803Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SM16716: { static SM16716Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case P9813: { static P9813Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case DOTSTAR:
			case APA102: { static APA102Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SK9822: { static SK9822Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}

#ifdef SPI_DATA
	template<ESPIChipsets CHIPSET> static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB>(data, nLedsOrOffset, nLedsIfOffset);
	}

	template<ESPIChipsets CHIPSET, EOrder RGB_ORDER> static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB_ORDER>(data, nLedsOrOffset, nLedsIfOffset);
	}

	template<ESPIChipsets CHIPSET, EOrder RGB_ORDER, uint8_t SPI_DATA_RATE> static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB_ORDER, SPI_DATA_RATE>(data, nLedsOrOffset, nLedsIfOffset);
	}

#endif
	//@}

#ifdef FASTLED_HAS_CLOCKLESS
	/// @name Adding 3-wire led controllers
	//@{
	/// Add a clockless (aka 3wire, also DMX) based CLEDController instance to the world.
	/// There are two ways to call this method (as well as the other addLeds)
	/// variations.  The first is with 2 arguments, in which case the arguments are  a pointer to
	/// led data, and the number of leds used by this controller.  The second is with 3 arguments, in which case
	/// the first  argument is the same, the second argument is an offset into the CRGB data where this controller's
	/// CRGB data begins, and the third argument is the number of leds for this controller object.
	///
	/// This method also takes a 2 to 3 template parameters for identifying the specific chipset, data pin, and rgb ordering
	/// RGB ordering, and SPI data rate
	/// @param data - base point to an array of CRGB data structures
	/// @param nLedsOrOffset - number of leds (3 argument version) or offset into the data array
	/// @param nLedsIfOffset - number of leds (4 argument version)
	/// @tparam CHIPSET - the chipset type (required)
	/// @tparam DATA_PIN - the optional data pin for the leds (required)
	/// @tparam RGB_ORDER - the rgb ordering for the leds (e.g. what order red, green, and blue data is written out in)
	/// @returns a reference to the added controller
	template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN, RGB_ORDER> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN, RGB> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	template<template<uint8_t DATA_PIN> class CHIPSET, uint8_t DATA_PIN>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<DATA_PIN> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	#ifdef FASTSPI_USE_DMX_SIMPLE
	template<EClocklessChipsets CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER=RGB>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		switch(CHIPSET) {
			case DMX: { static DMXController<DATA_PIN> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}
	#endif
	//@}
#endif

	/// @name Adding 3rd party library controllers
	//@{
	/// Add a 3rd party library based CLEDController instance to the world.
	/// There are two ways to call this method (as well as the other addLeds)
	/// variations.  The first is with 2 arguments, in which case the arguments are  a pointer to
	/// led data, and the number of leds used by this controller.  The second is with 3 arguments, in which case
	/// the first  argument is the same, the second argument is an offset into the CRGB data where this controller's
	/// CRGB data begins, and the third argument is the number of leds for this controller object. This class includes the SmartMatrix
	/// and OctoWS2811 based controllers
	///
	/// This method also takes a 1 to 2 template parameters for identifying the specific chipset and rgb ordering
	/// RGB ordering, and SPI data rate
	/// @param data - base point to an array of CRGB data structures
	/// @param nLedsOrOffset - number of leds (3 argument version) or offset into the data array
	/// @param nLedsIfOffset - number of leds (4 argument version)
	/// @tparam CHIPSET - the chipset type (required)
	/// @tparam RGB_ORDER - the rgb ordering for the leds (e.g. what order red, green, and blue data is written out in)
	/// @returns a reference to the added controller
	template<template<EOrder RGB_ORDER> class CHIPSET, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<RGB_ORDER> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

	template<template<EOrder RGB_ORDER> class CHIPSET>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		static CHIPSET<RGB> c;
		return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset);
	}

#ifdef USE_OCTOWS2811
	template<OWS2811 CHIPSET, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		switch(CHIPSET) {
			case OCTOWS2811: { static COctoWS2811Controller<RGB_ORDER,WS2811_800kHz> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case OCTOWS2811_400: { static COctoWS2811Controller<RGB_ORDER,WS2811_400kHz> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
#ifdef WS2813_800kHz
      case OCTOWS2813: { static COctoWS2811Controller<RGB_ORDER,WS2813_800kHz> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
#endif
		}
	}

	template<OWS2811 CHIPSET>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		return addLeds<CHIPSET,GRB>(data,nLedsOrOffset,nLedsIfOffset);
	}

#endif

#ifdef USE_WS2812SERIAL
	template<SWS2812 CHIPSET, int DATA_PIN, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		static CWS2812SerialController<DATA_PIN,RGB_ORDER> controller;
		return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset);
	}
#endif

#ifdef SmartMatrix_h
	template<ESM CHIPSET>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0)
	{
		switch(CHIPSET) {
			case SMART_MATRIX: { static CSmartMatrixController controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}
#endif
	//@}


#ifdef FASTLED_HAS_BLOCKLESS

	/// @name adding parallel output controllers
  //@{
	/// Add a block based CLEDController instance to the world.
	/// There are two ways to call this method (as well as the other addLeds)
	/// variations.  The first is with 2 arguments, in which case the arguments are  a pointer to
	/// led data, and the number of leds used by this controller.  The second is with 3 arguments, in which case
	/// the first  argument is the same, the second argument is an offset into the CRGB data where this controller's
	/// CRGB data begins, and the third argument is the number of leds for this controller object.
	///
	/// This method also takes a 2 to 3 template parameters for identifying the specific chipset and rgb ordering
	/// RGB ordering, and SPI data rate
	/// @param data - base point to an array of CRGB data structures
	/// @param nLedsOrOffset - number of leds (3 argument version) or offset into the data array
	/// @param nLedsIfOffset - number of leds (4 argument version)
	/// @tparam CHIPSET - the chipset/port type (required)
	/// @tparam NUM_LANES - how many parallel lanes of output to write
	/// @tparam RGB_ORDER - the rgb ordering for the leds (e.g. what order red, green, and blue data is written out in)
	/// @returns a reference to the added controller
	template<EBlockChipsets CHIPSET, int NUM_LANES, EOrder RGB_ORDER>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) {
		#ifdef PORTA_FIRST_PIN
				case WS2811_PORTA: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTA: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(800), NS(800), NS(900), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTA: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER, 0, false, 300>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTA: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(700), NS(1100), NS(700), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTA: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(500), NS(1500), NS(500), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef PORTB_FIRST_PIN
				case WS2811_PORTB: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTB: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, NS(800), NS(800), NS(900), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTB: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER, 0, false, 300>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTB: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, NS(700), NS(1100), NS(700), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTB: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTB_FIRST_PIN, NS(500), NS(1500), NS(500), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef PORTC_FIRST_PIN
				case WS2811_PORTC: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTC: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, NS(800), NS(800), NS(900), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTC: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER, 0, false, 300>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTC: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, NS(700), NS(1100), NS(700), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTC: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTC_FIRST_PIN, NS(500), NS(1500), NS(500), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef PORTD_FIRST_PIN
				case WS2811_PORTD: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTD: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, NS(800), NS(800), NS(900), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTD: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, NS(320), NS(320), NS(640), RGB_ORDER, 0, false, 300>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTD: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, NS(700), NS(1100), NS(700), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTD: return addLeds(new InlineBlockClocklessController<NUM_LANES, PORTD_FIRST_PIN, NS(500), NS(1500), NS(500), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		#ifdef HAS_PORTDC
				case WS2811_PORTDC: return addLeds(new SixteenWayInlineBlockClocklessController<NUM_LANES,NS(320), NS(320), NS(640), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case WS2811_400_PORTDC: return addLeds(new SixteenWayInlineBlockClocklessController<NUM_LANES,NS(800), NS(800), NS(900), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
        case WS2813_PORTDC: return addLeds(new SixteenWayInlineBlockClocklessController<NUM_LANES, NS(320), NS(320), NS(640), RGB_ORDER, 0, false, 300>(), data, nLedsOrOffset, nLedsIfOffset);
				case TM1803_PORTDC: return addLeds(new SixteenWayInlineBlockClocklessController<NUM_LANES, NS(700), NS(1100), NS(700), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
				case UCS1903_PORTDC: return addLeds(new SixteenWayInlineBlockClocklessController<NUM_LANES, NS(500), NS(1500), NS(500), RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		#endif
		}
	}

	template<EBlockChipsets CHIPSET, int NUM_LANES>
	static CLEDController &addLeds(struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		return addLeds<CHIPSET,NUM_LANES,GRB>(data,nLedsOrOffset,nLedsIfOffset);
	}
	//@}
#endif

	/// Set the global brightness scaling
	/// @param scale a 0-255 value for how much to scale all leds before writing them out
	void setBrightness(uint8_t scale) { m_Scale = scale; }

	/// Get the current global brightness setting
	/// @returns the current global brightness value
	uint8_t getBrightness() { return m_Scale; }

	/// Set the maximum power to be used, given in volts and milliamps.
	/// @param volts - how many volts the leds are being driven at (usually 5)
	/// @param milliamps - the maximum milliamps of power draw you want
	inline void setMaxPowerInVoltsAndMilliamps(uint8_t volts, uint32_t milliamps) { setMaxPowerInMilliWatts(volts * milliamps); }

	/// Set the maximum power to be used, given in milliwatts
	/// @param milliwatts - the max power draw desired, in milliwatts
	inline void setMaxPowerInMilliWatts(uint32_t milliwatts) { m_pPowerFunc = &calculate_max_brightness_for_power_mW; m_nPowerData = milliwatts; }

	/// Update all our controllers with the current led colors, using the passed in brightness
	/// @param scale temporarily override the scale
	void show(uint8_t scale);

	/// Update all our controllers with the current led colors
	void show() { show(m_Scale); }

	/// clear the leds, wiping the local array of data, optionally black out the leds as well
	/// @param writeData whether or not to write out to the leds as well
	void clear(bool writeData = false);

	/// clear out the local data array
	void clearData();

	/// Set all leds on all controllers to the given color/scale
	/// @param color what color to set the leds to
	/// @param scale what brightness scale to show at
	void showColor(const struct CRGB & color, uint8_t scale);

	/// Set all leds on all controllers to the given color
	/// @param color what color to set the leds to
	void showColor(const struct CRGB & color) { showColor(color, m_Scale); }

	/// Delay for the given number of milliseconds.  Provided to allow the library to be used on platforms
	/// that don't have a delay function (to allow code to be more portable).  Note: this will call show
 	/// constantly to drive the dithering engine (and will call show at least once).
	/// @param ms the number of milliseconds to pause for
	void delay(unsigned long ms);

	/// Set a global color temperature.  Sets the color temperature for all added led strips, overriding whatever
	/// previous color temperature those controllers may have had
	/// @param temp A CRGB structure describing the color temperature
	void setTemperature(const struct CRGB & temp);

	/// Set a global color correction.  Sets the color correction for all added led strips,
	/// overriding whatever previous color correction those controllers may have had.
	/// @param correction A CRGB structure describin the color correction.
	void setCorrection(const struct CRGB & correction);

	/// Set the dithering mode.  Sets the dithering mode for all added led strips, overriding
	/// whatever previous dithering option those controllers may have had.
	/// @param ditherMode - what type of dithering to use, either BINARY_DITHER or DISABLE_DITHER
	void setDither(uint8_t ditherMode = BINARY_DITHER);

	/// Set the maximum refresh rate.  This is global for all leds.  Attempts to
	/// call show faster than this rate will simply wait.  Note that the refresh rate
	/// defaults to the slowest refresh rate of all the leds added through addLeds.  If
	/// you wish to set/override this rate, be sure to call setMaxRefreshRate _after_
	/// adding all of your leds.
	/// @param refresh - maximum refresh rate in hz
	/// @param constrain - constrain refresh rate to the slowest speed yet set
	void setMaxRefreshRate(uint16_t refresh, bool constrain=false);

	/// for debugging, will keep track of time between calls to countFPS, and every
	/// nFrames calls, it will update an internal counter for the current FPS.
	/// @todo make this a rolling counter
	/// @param nFrames - how many frames to time for determining FPS
	void countFPS(int nFrames=25);

	/// Get the number of frames/second being written out
	/// @returns the most recently computed FPS value
	uint16_t getFPS() { return m_nFPS; }

	/// Get how many controllers have been registered
  /// @returns the number of controllers (strips) that have been added with addLeds
	int count();

	/// Get a reference to a registered controller
  /// @returns a reference to the Nth controller
	CLEDController & operator[](int x);

	/// Get the number of leds in the first controller
  /// @returns the number of LEDs in the first controller
	int size() { return (*this)[0].size(); }

	/// Get a pointer to led data for the first controller
  /// @returns pointer to the CRGB buffer for the first controller
	CRGB *leds() { return (*this)[0].leds(); }
};

#define FastSPI_LED FastLED
#define FastSPI_LED2 FastLED
#ifndef LEDS
#define LEDS FastLED
#endif

extern CFastLED FastLED;

// Warnings for undefined things
#ifndef HAS_HARDWARE_PIN_SUPPORT
#warning "No pin/port mappings found, pin access will be slightly slower. See fastpin.h for info."
#define NO_HARDWARE_PIN_SUPPORT
#endif


FASTLED_NAMESPACE_END

#endif
