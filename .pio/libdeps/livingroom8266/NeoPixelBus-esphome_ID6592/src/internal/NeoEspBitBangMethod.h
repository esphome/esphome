/*-------------------------------------------------------------------------
NeoPixel library helper functions for Esp8266 and Esp32

Written by Michael C. Miller.

I invest time and resources providing this open source code,
please support me by dontating (see https://github.com/Makuna/NeoPixelBus)

-------------------------------------------------------------------------
This file is part of the Makuna/NeoPixelBus library.

NeoPixelBus is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

NeoPixelBus is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with NeoPixel.  If not, see
<http://www.gnu.org/licenses/>.
-------------------------------------------------------------------------*/

#pragma once

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)

#if defined(ARDUINO_ARCH_ESP8266)
#include <eagle_soc.h>
#endif

// ESP32 doesn't define ICACHE_RAM_ATTR
#ifndef ICACHE_RAM_ATTR
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif

static uint32_t _getCycleCount(void) __attribute__((always_inline));

static inline uint32_t _getCycleCount(void)
{
	uint32_t ccount;
	__asm__ __volatile__("rsr %0,ccount":"=a" (ccount));
	return ccount;
}

#define CYCLES_LOOPTEST   (4) // adjustment due to loop exit test instruction cycles

class NeoEspSpeedWs2811
{
public:
	const static uint32_t T0H = (F_CPU / 3333333 - CYCLES_LOOPTEST); // 0.3us
	const static uint32_t T1H = (F_CPU / 1052632 - CYCLES_LOOPTEST); // 0.95us
	const static uint32_t Period = (F_CPU / 800000 - CYCLES_LOOPTEST); // 1.25us per bit
};

class NeoEspSpeed800Mhz
{
public:
	const static uint32_t T0H = (F_CPU / 2500000 - CYCLES_LOOPTEST); // 0.4us
	const static uint32_t T1H = (F_CPU / 1250000 - CYCLES_LOOPTEST); // 0.8us
	const static uint32_t Period = (F_CPU / 800000 - CYCLES_LOOPTEST); // 1.25us per bit
};

class NeoEspSpeed400Mhz
{
public:
	const static uint32_t T0H = (F_CPU / 2000000 - CYCLES_LOOPTEST); 
	const static uint32_t T1H = (F_CPU /  833333 - CYCLES_LOOPTEST); 
	const static uint32_t Period = (F_CPU / 400000 - CYCLES_LOOPTEST);
};

class NeoEspPinset
{
public:
	const static uint8_t IdleLevel = LOW;

	inline const static void setPin(const uint32_t pinRegister)
	{
#if defined(ARDUINO_ARCH_ESP32)
		GPIO.out_w1ts = pinRegister;
#else
		GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pinRegister);
#endif
	}

	inline const static void resetPin(const uint32_t pinRegister)
	{
#if defined(ARDUINO_ARCH_ESP32)
		GPIO.out_w1tc = pinRegister;
#else
		GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pinRegister);
#endif
	}
};

class NeoEspPinsetInverted
{
public:
	const static uint8_t IdleLevel = HIGH;

	inline const static void setPin(const uint32_t pinRegister)
	{
#if defined(ARDUINO_ARCH_ESP32)
		GPIO.out_w1tc = pinRegister;
#else
		GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pinRegister);
#endif
	}

	inline const static void resetPin(const uint32_t pinRegister)
	{
#if defined(ARDUINO_ARCH_ESP32)
		GPIO.out_w1ts = pinRegister;
#else
		GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pinRegister);
#endif
	}
};

template<typename T_SPEED, typename T_PINSET> class NeoEspBitBangBase
{
public:
	static void ICACHE_RAM_ATTR send_pixels(uint8_t* pixels, uint8_t* end, uint8_t pin)
	{
		const uint32_t pinRegister = _BV(pin);
		uint8_t mask = 0x80;
		uint8_t subpix = *pixels++;
		uint32_t cyclesStart = 0; // trigger emediately
		uint32_t cyclesNext = 0;

		for (;;)
		{
			// do the checks here while we are waiting on time to pass
			uint32_t cyclesBit = T_SPEED::T0H;
			if (subpix & mask)
			{
				cyclesBit = T_SPEED::T1H;
			}

			// after we have done as much work as needed for this next bit
			// now wait for the HIGH
			while (((cyclesStart = _getCycleCount()) - cyclesNext) < T_SPEED::Period);

			// set pin state
			T_PINSET::setPin(pinRegister);

			// wait for the LOW
			while ((_getCycleCount() - cyclesStart) < cyclesBit);

			// reset pin start
			T_PINSET::resetPin(pinRegister);

			cyclesNext = cyclesStart;

			// next bit
			mask >>= 1;
			if (mask == 0)
			{
				// no more bits to send in this byte
				// check for another byte
				if (pixels >= end)
				{
					// no more bytes to send so stop
					break;
				}
				// reset mask to first bit and get the next byte
				mask = 0x80;
				subpix = *pixels++;
			}
		}
	}

};

class NeoEspBitBangSpeedWs2811 : public NeoEspBitBangBase<NeoEspSpeedWs2811, NeoEspPinset>
{
public:
	static const uint32_t ResetTimeUs = 300;
};

class NeoEspBitBangSpeedWs2812x : public NeoEspBitBangBase<NeoEspSpeed800Mhz, NeoEspPinset>
{
public:
    static const uint32_t ResetTimeUs = 300;
};

class NeoEspBitBangSpeedSk6812 : public NeoEspBitBangBase<NeoEspSpeed800Mhz, NeoEspPinset>
{
public:
    static const uint32_t ResetTimeUs = 80;
};

class NeoEspBitBangSpeed800Kbps : public NeoEspBitBangBase<NeoEspSpeed800Mhz, NeoEspPinset>
{
public:
    static const uint32_t ResetTimeUs = 50; 
};

class NeoEspBitBangSpeed400Kbps : public NeoEspBitBangBase<NeoEspSpeed400Mhz, NeoEspPinset>
{
public:
    static const uint32_t ResetTimeUs = 50;
};


class NeoEspBitBangInvertedSpeedWs2811 : public NeoEspBitBangBase<NeoEspSpeedWs2811, NeoEspPinsetInverted>
{
public:
	static const uint32_t ResetTimeUs = 300;
};

class NeoEspBitBangInvertedSpeedWs2812x : public NeoEspBitBangBase<NeoEspSpeed800Mhz, NeoEspPinsetInverted>
{
public:
	static const uint32_t ResetTimeUs = 300;
};

class NeoEspBitBangInvertedSpeedSk6812 : public NeoEspBitBangBase<NeoEspSpeed800Mhz, NeoEspPinsetInverted>
{
public:
	static const uint32_t ResetTimeUs = 80;
};

class NeoEspBitBangInvertedSpeed800Kbps : public NeoEspBitBangBase<NeoEspSpeed800Mhz, NeoEspPinsetInverted>
{
public:
	static const uint32_t ResetTimeUs = 50;
};

class NeoEspBitBangInvertedSpeed400Kbps : public NeoEspBitBangBase<NeoEspSpeed400Mhz, NeoEspPinsetInverted>
{
public:
	static const uint32_t ResetTimeUs = 50;
};

template<typename T_SPEED, typename T_PINSET> class NeoEspBitBangMethodBase
{
public:
    NeoEspBitBangMethodBase(uint8_t pin, uint16_t pixelCount, size_t elementSize) :
        _pin(pin)
    {
        pinMode(pin, OUTPUT);

        _sizePixels = pixelCount * elementSize;
        _pixels = (uint8_t*)malloc(_sizePixels);
        memset(_pixels, 0, _sizePixels);
    }

    ~NeoEspBitBangMethodBase()
    {
        pinMode(_pin, INPUT);

        free(_pixels);
    }

    bool IsReadyToUpdate() const
    {
        uint32_t delta = micros() - _endTime;

        return (delta >= T_SPEED::ResetTimeUs);
    }

    void Initialize()
    {
        digitalWrite(_pin, T_PINSET::IdleLevel);

        _endTime = micros();
    }

    void Update(bool)
    {
        // Data latch = 50+ microsecond pause in the output stream.  Rather than
        // put a delay at the end of the function, the ending time is noted and
        // the function will simply hold off (if needed) on issuing the
        // subsequent round of data until the latch time has elapsed.  This
        // allows the mainline code to start generating the next frame of data
        // rather than stalling for the latch.
        while (!IsReadyToUpdate())
        {
            yield(); // allows for system yield if needed
        }

		// Need 100% focus on instruction timing
#if defined(ARDUINO_ARCH_ESP32)
		delay(1); // required
		portMUX_TYPE updateMux = portMUX_INITIALIZER_UNLOCKED;

        portENTER_CRITICAL(&updateMux);
#else
        noInterrupts(); 
#endif

        T_SPEED::send_pixels(_pixels, _pixels + _sizePixels, _pin);
		
#if defined(ARDUINO_ARCH_ESP32)
        portEXIT_CRITICAL(&updateMux);
#else
        interrupts();
#endif

        // save EOD time for latch on next call
        _endTime = micros();
    }

    uint8_t* getPixels() const
    {
        return _pixels;
    };

    size_t getPixelsSize() const
    {
        return _sizePixels;
    };

private:
    uint32_t _endTime;       // Latch timing reference
    size_t    _sizePixels;   // Size of '_pixels' buffer below
    uint8_t* _pixels;        // Holds LED color values
    uint8_t _pin;            // output pin number
};


#if defined(ARDUINO_ARCH_ESP32)

typedef NeoEspBitBangMethodBase<NeoEspBitBangSpeedWs2811, NeoEspPinset> NeoEsp32BitBangWs2811Method;
typedef NeoEspBitBangMethodBase<NeoEspBitBangSpeedWs2812x, NeoEspPinset> NeoEsp32BitBangWs2812xMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangSpeedSk6812, NeoEspPinset> NeoEsp32BitBangSk6812Method;
typedef NeoEspBitBangMethodBase<NeoEspBitBangSpeed800Kbps, NeoEspPinset> NeoEsp32BitBang800KbpsMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangSpeed400Kbps, NeoEspPinset> NeoEsp32BitBang400KbpsMethod;

typedef NeoEsp32BitBangWs2812xMethod NeoEsp32BitBangWs2813Method;
typedef NeoEsp32BitBang800KbpsMethod NeoEsp32BitBangWs2812Method;
typedef NeoEsp32BitBangSk6812Method NeoEsp32BitBangLc8812Method;
typedef NeoEsp32BitBang400KbpsMethod NeoEsp32BitBangApa106Method;

typedef NeoEspBitBangMethodBase<NeoEspBitBangInvertedSpeedWs2811, NeoEspPinsetInverted> NeoEsp32BitBangWs2811InvertedMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangInvertedSpeedWs2812x, NeoEspPinsetInverted> NeoEsp32BitBangWs2812xInvertedMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangInvertedSpeedSk6812, NeoEspPinsetInverted> NeoEsp32BitBangSk6812InvertedMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangInvertedSpeed800Kbps, NeoEspPinsetInverted> NeoEsp32BitBang800KbpsInvertedMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangInvertedSpeed400Kbps, NeoEspPinsetInverted> NeoEsp32BitBang400KbpsInvertedMethod;

typedef NeoEsp32BitBangWs2812xInvertedMethod NeoEsp32BitBangWs2813InvertedMethod;
typedef NeoEsp32BitBang800KbpsInvertedMethod NeoEsp32BitBangWs2812InvertedMethod;
typedef NeoEsp32BitBangSk6812InvertedMethod NeoEsp32BitBangLc8812InvertedMethod;
typedef NeoEsp32BitBang400KbpsInvertedMethod NeoEsp32BitBangApa106InvertedMethod;

#else

typedef NeoEspBitBangMethodBase<NeoEspBitBangSpeedWs2811, NeoEspPinset> NeoEsp8266BitBangWs2811Method;
typedef NeoEspBitBangMethodBase<NeoEspBitBangSpeedWs2812x, NeoEspPinset> NeoEsp8266BitBangWs2812xMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangSpeedSk6812, NeoEspPinset> NeoEsp8266BitBangSk6812Method;
typedef NeoEspBitBangMethodBase<NeoEspBitBangSpeed800Kbps, NeoEspPinset> NeoEsp8266BitBang800KbpsMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangSpeed400Kbps, NeoEspPinset> NeoEsp8266BitBang400KbpsMethod;

typedef NeoEsp8266BitBangWs2812xMethod NeoEsp8266BitBangWs2813Method;
typedef NeoEsp8266BitBang800KbpsMethod NeoEsp8266BitBangWs2812Method;
typedef NeoEsp8266BitBangSk6812Method NeoEsp8266BitBangLc8812Method;
typedef NeoEsp8266BitBang400KbpsMethod NeoEsp8266BitBangApa106Method;

typedef NeoEspBitBangMethodBase<NeoEspBitBangInvertedSpeedWs2811, NeoEspPinsetInverted> NeoEsp8266BitBangWs2811InvertedMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangInvertedSpeedWs2812x, NeoEspPinsetInverted> NeoEsp8266BitBangWs2812xInvertedMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangInvertedSpeedSk6812, NeoEspPinsetInverted> NeoEsp8266BitBangSk6812InvertedMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangInvertedSpeed800Kbps, NeoEspPinsetInverted> NeoEsp8266BitBang800KbpsInvertedMethod;
typedef NeoEspBitBangMethodBase<NeoEspBitBangInvertedSpeed400Kbps, NeoEspPinsetInverted> NeoEsp8266BitBang400KbpsInvertedMethod;

typedef NeoEsp8266BitBangWs2812xInvertedMethod NeoEsp8266BitBangWs2813InvertedMethod;
typedef NeoEsp8266BitBang800KbpsInvertedMethod NeoEsp8266BitBangWs2812InvertedMethod;
typedef NeoEsp8266BitBangSk6812InvertedMethod NeoEsp8266BitBangLc8812InvertedMethod;
typedef NeoEsp8266BitBang400KbpsInvertedMethod NeoEsp8266BitBangApa106InvertedMethod;
#endif

// ESP bitbang doesn't have defaults and should avoided except for testing
#endif