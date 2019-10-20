/*-------------------------------------------------------------------------
NeoPixel library helper functions for Esp32.

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

#ifdef ARDUINO_ARCH_ESP32

/*  General Reference documentation for the APIs used in this implementation
LOW LEVEL:  (what is actually used)
DOCS: https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/rmt.html
EXAMPLE: https://github.com/espressif/esp-idf/blob/826ff7186ae07dc81e960a8ea09ebfc5304bfb3b/examples/peripherals/rmt_tx/main/rmt_tx_main.c

HIGHER LEVEL:
NO TRANSLATE SUPPORT so this was not used
NOTE: https://github.com/espressif/arduino-esp32/commit/50d142950d229b8fabca9b749dc4a5f2533bc426
Esp32-hal-rmt.h
Esp32-hal-rmt.c
*/

extern "C"
{
#include <Arduino.h>
#include <driver/rmt.h>
}

class NeoEsp32RmtSpeed
{
public:
    // ClkDiv of 2 provides for good resolution and plenty of reset resolution; but
    // a ClkDiv of 1 will provide enough space for the longest reset and does show
    // little better pulse accuracy
    const static uint8_t RmtClockDivider = 2; 

    inline constexpr static uint32_t FromNs(uint32_t ns)
    {
        return ns / NsPerRmtTick;
    }


protected:
    const static uint32_t RmtCpu = 80000000L; // 80 mhz RMT clock
    const static uint32_t NsPerSecond = 1000000000L;
    const static uint32_t RmtTicksPerSecond = (RmtCpu / RmtClockDivider);
    const static uint32_t NsPerRmtTick = (NsPerSecond / RmtTicksPerSecond); // about 25 
};

class NeoEsp32RmtSpeedBase : public NeoEsp32RmtSpeed
{
public:
	// this is used rather than the rmt_item32_t as you can't correctly initialize
    // it as a static constexpr within the template
	inline constexpr static uint32_t Item32Val(uint16_t nsHigh, uint16_t nsLow)
	{
		return (FromNs(nsLow) << 16) | (1 << 15) | (FromNs(nsHigh));
	}

	const static rmt_idle_level_t IdleLevel = RMT_IDLE_LEVEL_LOW;
};

class NeoEsp32RmtInvertedSpeedBase : public NeoEsp32RmtSpeed
{
public:
	// this is used rather than the rmt_item32_t as you can't correctly initialize
	// it as a static constexpr within the template
	inline constexpr static uint32_t Item32Val(uint16_t nsHigh, uint16_t nsLow)
	{
		return (FromNs(nsLow) << 16) | (1 << 31) | (FromNs(nsHigh));
	}

	const static rmt_idle_level_t IdleLevel = RMT_IDLE_LEVEL_HIGH;
};

class NeoEsp32RmtSpeedWs2811 : public NeoEsp32RmtSpeedBase
{
public:
    const static uint32_t RmtBit0 = Item32Val(300, 950); 
    const static uint32_t RmtBit1 = Item32Val(900, 350); 
    const static uint16_t RmtDurationReset = FromNs(300000); // 300us
};

class NeoEsp32RmtSpeedWs2812x : public NeoEsp32RmtSpeedBase
{
public:
	const static uint32_t RmtBit0 = Item32Val(400, 850);
	const static uint32_t RmtBit1 = Item32Val(800, 450);
	const static uint16_t RmtDurationReset = FromNs(300000); // 300us
};

class NeoEsp32RmtSpeedSk6812 : public NeoEsp32RmtSpeedBase
{
public:
    const static uint32_t RmtBit0 = Item32Val(400, 850); 
    const static uint32_t RmtBit1 = Item32Val(800, 450); 
    const static uint16_t RmtDurationReset = FromNs(80000); // 80us
};

class NeoEsp32RmtSpeed800Kbps : public NeoEsp32RmtSpeedBase
{
public:
    const static uint32_t RmtBit0 = Item32Val(400, 850); 
    const static uint32_t RmtBit1 = Item32Val(800, 450); 
    const static uint16_t RmtDurationReset = FromNs(50000); // 50us
};

class NeoEsp32RmtSpeed400Kbps : public NeoEsp32RmtSpeedBase
{
public:
    const static uint32_t RmtBit0 = Item32Val(800, 1700); 
    const static uint32_t RmtBit1 = Item32Val(1600, 900); 
    const static uint16_t RmtDurationReset = FromNs(50000); // 50us
};

class NeoEsp32RmtSpeedApa106 : public NeoEsp32RmtSpeedBase
{
public:
	const static uint32_t RmtBit0 = Item32Val(400, 1250);
	const static uint32_t RmtBit1 = Item32Val(1250, 400);
	const static uint16_t RmtDurationReset = FromNs(50000); // 50us
};

class NeoEsp32RmtInvertedSpeedWs2811 : public NeoEsp32RmtInvertedSpeedBase
{
public:
	const static uint32_t RmtBit0 = Item32Val(300, 950);
	const static uint32_t RmtBit1 = Item32Val(900, 350);
	const static uint16_t RmtDurationReset = FromNs(300000); // 300us
};

class NeoEsp32RmtInvertedSpeedWs2812x : public NeoEsp32RmtInvertedSpeedBase
{
public:
	const static uint32_t RmtBit0 = Item32Val(400, 850);
	const static uint32_t RmtBit1 = Item32Val(800, 450);
	const static uint16_t RmtDurationReset = FromNs(300000); // 300us
};

class NeoEsp32RmtInvertedSpeedSk6812 : public NeoEsp32RmtInvertedSpeedBase
{
public:
	const static uint32_t RmtBit0 = Item32Val(400, 850);
	const static uint32_t RmtBit1 = Item32Val(800, 450);
	const static uint16_t RmtDurationReset = FromNs(80000); // 80us
};

class NeoEsp32RmtInvertedSpeed800Kbps : public NeoEsp32RmtInvertedSpeedBase
{
public:
	const static uint32_t RmtBit0 = Item32Val(400, 850);
	const static uint32_t RmtBit1 = Item32Val(800, 450);
	const static uint16_t RmtDurationReset = FromNs(50000); // 50us
};

class NeoEsp32RmtInvertedSpeed400Kbps : public NeoEsp32RmtInvertedSpeedBase
{
public:
	const static uint32_t RmtBit0 = Item32Val(800, 1700);
	const static uint32_t RmtBit1 = Item32Val(1600, 900);
	const static uint16_t RmtDurationReset = FromNs(50000); // 50us
};

class NeoEsp32RmtInvertedSpeedApa106 : public NeoEsp32RmtInvertedSpeedBase
{
public:
	const static uint32_t RmtBit0 = Item32Val(400, 1250);
	const static uint32_t RmtBit1 = Item32Val(1250, 400);
	const static uint16_t RmtDurationReset = FromNs(50000); // 50us
};

class NeoEsp32RmtChannel0
{
public:
    const static rmt_channel_t RmtChannelNumber = RMT_CHANNEL_0;
};

class NeoEsp32RmtChannel1
{
public:
    const static rmt_channel_t RmtChannelNumber = RMT_CHANNEL_1;
};

class NeoEsp32RmtChannel2
{
public:
    const static rmt_channel_t RmtChannelNumber = RMT_CHANNEL_2;
};

class NeoEsp32RmtChannel3
{
public:
    const static rmt_channel_t RmtChannelNumber = RMT_CHANNEL_3;
};

class NeoEsp32RmtChannel4
{
public:
    const static rmt_channel_t RmtChannelNumber = RMT_CHANNEL_4;
};

class NeoEsp32RmtChannel5
{
public:
    const static rmt_channel_t RmtChannelNumber = RMT_CHANNEL_5;
};

class NeoEsp32RmtChannel6
{
public:
    const static rmt_channel_t RmtChannelNumber = RMT_CHANNEL_6;
};

class NeoEsp32RmtChannel7
{
public:
    const static rmt_channel_t RmtChannelNumber = RMT_CHANNEL_7;
};

template<typename T_SPEED, typename T_CHANNEL> class NeoEsp32RmtMethodBase
{
public:
    NeoEsp32RmtMethodBase(uint8_t pin, uint16_t pixelCount, size_t elementSize)  :
        _pin(pin)
    {
        _pixelsSize = pixelCount * elementSize;

        _pixelsEditing = static_cast<uint8_t*>(malloc(_pixelsSize));
        memset(_pixelsEditing, 0x00, _pixelsSize);

        _pixelsSending = static_cast<uint8_t*>(malloc(_pixelsSize));
        // no need to initialize it, it gets overwritten on every send
    }

    ~NeoEsp32RmtMethodBase()
    {
        // wait until the last send finishes before destructing everything
        // arbitrary time out of 10 seconds
        ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_wait_tx_done(T_CHANNEL::RmtChannelNumber, 10000 / portTICK_PERIOD_MS));

        ESP_ERROR_CHECK(rmt_driver_uninstall(T_CHANNEL::RmtChannelNumber));

        free(_pixelsEditing);
        free(_pixelsSending);
    }


    bool IsReadyToUpdate() const
    {
        return (ESP_OK == rmt_wait_tx_done(T_CHANNEL::RmtChannelNumber, 0));
    }

    void Initialize()
    {
        rmt_config_t config;

        config.rmt_mode = RMT_MODE_TX;
        config.channel = T_CHANNEL::RmtChannelNumber;
        config.gpio_num = static_cast<gpio_num_t>(_pin);
        config.mem_block_num = 1;
        config.tx_config.loop_en = false;
        
        config.tx_config.idle_output_en = true;
        config.tx_config.idle_level = T_SPEED::IdleLevel;

        config.tx_config.carrier_en = false;
        config.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;

        config.clk_div = T_SPEED::RmtClockDivider;

        ESP_ERROR_CHECK(rmt_config(&config));
        ESP_ERROR_CHECK(rmt_driver_install(T_CHANNEL::RmtChannelNumber, 0, 0));
        ESP_ERROR_CHECK(rmt_translator_init(T_CHANNEL::RmtChannelNumber, _translate));
    }

    void Update(bool maintainBufferConsistency)
    {
        // wait for not actively sending data
        // this will time out at 10 seconds, an arbitrarily long period of time
        // and do nothing if this happens
        if (ESP_OK == ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_wait_tx_done(T_CHANNEL::RmtChannelNumber, 10000 / portTICK_PERIOD_MS)))
        {
            // now start the RMT transmit with the editing buffer before we swap
            ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_write_sample(T_CHANNEL::RmtChannelNumber, _pixelsEditing, _pixelsSize, false));

            if (maintainBufferConsistency)
            {
                // copy editing to sending,
                // this maintains the contract that "colors present before will
                // be the same after", otherwise GetPixelColor will be inconsistent
                memcpy(_pixelsSending, _pixelsEditing, _pixelsSize);
            }

            // swap so the user can modify without affecting the async operation
            std::swap(_pixelsSending, _pixelsEditing);
        }
    }

    uint8_t* getPixels() const
    {
        return _pixelsEditing;
    };

    size_t getPixelsSize() const
    {
        return _pixelsSize;
    }

private:
    const uint8_t _pin;            // output pin number

    size_t    _pixelsSize;      // Size of '_pixels' buffer 
    uint8_t*  _pixelsEditing;   // Holds LED color values exposed for get and set
    uint8_t*  _pixelsSending;   // Holds LED color values used to async send using RMT


    // stranslate NeoPixelBuffer into RMT buffer
    // this is done on the fly so we don't require a send buffer in raw RMT format
    // which would be 32x larger than the primary buffer
    static void IRAM_ATTR _translate(const void* src,
        rmt_item32_t* dest,
        size_t src_size,
        size_t wanted_num,
        size_t* translated_size,
        size_t* item_num)
    {
        if (src == NULL || dest == NULL) 
        {
            *translated_size = 0;
            *item_num = 0;
            return;
        }

        size_t size = 0;
        size_t num = 0;
        const uint8_t* psrc = static_cast<const uint8_t*>(src);
        rmt_item32_t* pdest = dest;

        for (;;)
        {
            uint8_t data = *psrc;

            for (uint8_t bit = 0; bit < 8; bit++)
            {
                pdest->val = (data & 0x80) ? T_SPEED::RmtBit1 : T_SPEED::RmtBit0;
                pdest++;
                data <<= 1;
            }
            num += 8;
            size++;

            // if this is the last byte we need to adjust the length of the last pulse
            if (size >= src_size)
            {
                // extend the last bits LOW value to include the full reset signal length
                pdest--;
                pdest->duration1 = T_SPEED::RmtDurationReset;
                // and stop updating data to send
                break; 
            }

            if (num >= wanted_num)
            {
                // stop updating data to send
                break;
            }

            psrc++;
        }

        *translated_size = size;
        *item_num = num;
    }
};

// normal
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel0> NeoEsp32Rmt0Ws2811Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel0> NeoEsp32Rmt0Ws2812xMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel0> NeoEsp32Rmt0Sk6812Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel0> NeoEsp32Rmt0Apa106Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel0> NeoEsp32Rmt0800KbpsMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel0> NeoEsp32Rmt0400KbpsMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel1> NeoEsp32Rmt1Ws2811Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel1> NeoEsp32Rmt1Ws2812xMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel1> NeoEsp32Rmt1Sk6812Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel1> NeoEsp32Rmt1Apa106Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel1> NeoEsp32Rmt1800KbpsMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel1> NeoEsp32Rmt1400KbpsMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel2> NeoEsp32Rmt2Ws2811Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel2> NeoEsp32Rmt2Ws2812xMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel2>  NeoEsp32Rmt2Sk6812Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel2> NeoEsp32Rmt2Apa106Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel2> NeoEsp32Rmt2800KbpsMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel2> NeoEsp32Rmt2400KbpsMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel3> NeoEsp32Rmt3Ws2811Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel3> NeoEsp32Rmt3Ws2812xMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel3>  NeoEsp32Rmt3Sk6812Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel3> NeoEsp32Rmt3Apa106Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel3> NeoEsp32Rmt3800KbpsMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel3> NeoEsp32Rmt3400KbpsMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel4> NeoEsp32Rmt4Ws2811Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel4> NeoEsp32Rmt4Ws2812xMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel4>  NeoEsp32Rmt4Sk6812Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel4> NeoEsp32Rmt4Apa106Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel4> NeoEsp32Rmt4800KbpsMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel4> NeoEsp32Rmt4400KbpsMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel5> NeoEsp32Rmt5Ws2811Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel5> NeoEsp32Rmt5Ws2812xMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel5>  NeoEsp32Rmt5Sk6812Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel5> NeoEsp32Rmt5Apa106Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel5> NeoEsp32Rmt5800KbpsMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel5> NeoEsp32Rmt5400KbpsMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel6> NeoEsp32Rmt6Ws2811Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel6> NeoEsp32Rmt6Ws2812xMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel6>  NeoEsp32Rmt6Sk6812Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel6> NeoEsp32Rmt6Apa106Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel6> NeoEsp32Rmt6800KbpsMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel6> NeoEsp32Rmt6400KbpsMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2811, NeoEsp32RmtChannel7> NeoEsp32Rmt7Ws2811Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedWs2812x, NeoEsp32RmtChannel7> NeoEsp32Rmt7Ws2812xMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedSk6812, NeoEsp32RmtChannel7>  NeoEsp32Rmt7Sk6812Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedApa106, NeoEsp32RmtChannel7> NeoEsp32Rmt7Apa106Method;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed800Kbps, NeoEsp32RmtChannel7> NeoEsp32Rmt7800KbpsMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeed400Kbps, NeoEsp32RmtChannel7> NeoEsp32Rmt7400KbpsMethod;

// inverted
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel0> NeoEsp32Rmt0Ws2811InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel0> NeoEsp32Rmt0Ws2812xInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel0> NeoEsp32Rmt0Sk6812InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel0> NeoEsp32Rmt0Apa106InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel0> NeoEsp32Rmt0800KbpsInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel0> NeoEsp32Rmt0400KbpsInvertedMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel1> NeoEsp32Rmt1Ws2811InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel1> NeoEsp32Rmt1Ws2812xInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel1> NeoEsp32Rmt1Sk6812InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel1> NeoEsp32Rmt1Apa106InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel1> NeoEsp32Rmt1800KbpsInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel1> NeoEsp32Rmt1400KbpsInvertedMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel2> NeoEsp32Rmt2Ws2811InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel2> NeoEsp32Rmt2Ws2812xInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel2>  NeoEsp32Rmt2Sk6812InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel2> NeoEsp32Rmt2Apa106InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel2> NeoEsp32Rmt2800KbpsInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel2> NeoEsp32Rmt2400KbpsInvertedMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel3> NeoEsp32Rmt3Ws2811InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel3> NeoEsp32Rmt3Ws2812xInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel3>  NeoEsp32Rmt3Sk6812InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel3> NeoEsp32Rmt3Apa106InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel3> NeoEsp32Rmt3800KbpsInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel3> NeoEsp32Rmt3400KbpsInvertedMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel4> NeoEsp32Rmt4Ws2811InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel4> NeoEsp32Rmt4Ws2812xInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel4>  NeoEsp32Rmt4Sk6812InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel4> NeoEsp32Rmt4Apa106InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel4> NeoEsp32Rmt4800KbpsInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel4> NeoEsp32Rmt4400KbpsInvertedMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel5> NeoEsp32Rmt5Ws2811InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel5> NeoEsp32Rmt5Ws2812xInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel5>  NeoEsp32Rmt5Sk6812InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel5> NeoEsp32Rmt5Apa106InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel5> NeoEsp32Rmt5800KbpsInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel5> NeoEsp32Rmt5400KbpsInvertedMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel6> NeoEsp32Rmt6Ws2811InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel6> NeoEsp32Rmt6Ws2812xInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel6>  NeoEsp32Rmt6Sk6812InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel6> NeoEsp32Rmt6Apa106InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel6> NeoEsp32Rmt6800KbpsInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel6> NeoEsp32Rmt6400KbpsInvertedMethod;

typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2811, NeoEsp32RmtChannel7> NeoEsp32Rmt7Ws2811InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedWs2812x, NeoEsp32RmtChannel7> NeoEsp32Rmt7Ws2812xInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedSk6812, NeoEsp32RmtChannel7>  NeoEsp32Rmt7Sk6812InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeedApa106, NeoEsp32RmtChannel7> NeoEsp32Rmt7Apa106InvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed800Kbps, NeoEsp32RmtChannel7> NeoEsp32Rmt7800KbpsInvertedMethod;
typedef NeoEsp32RmtMethodBase<NeoEsp32RmtInvertedSpeed400Kbps, NeoEsp32RmtChannel7> NeoEsp32Rmt7400KbpsInvertedMethod;

// due to a core issue where requests to send aren't consistent with I2s, RMT ch6 is temporarily the default
// RMT channel 6 method is the default method for Esp32
typedef NeoEsp32Rmt6Ws2812xMethod NeoWs2813Method;
typedef NeoEsp32Rmt6Ws2812xMethod NeoWs2812xMethod;
typedef NeoEsp32Rmt6800KbpsMethod NeoWs2812Method;
typedef NeoEsp32Rmt6Ws2812xMethod NeoWs2811Method;
typedef NeoEsp32Rmt6Sk6812Method NeoSk6812Method;
typedef NeoEsp32Rmt6Sk6812Method NeoLc8812Method;
typedef NeoEsp32Rmt6Apa106Method NeoApa106Method;

typedef NeoEsp32Rmt6Ws2812xMethod Neo800KbpsMethod;
typedef NeoEsp32Rmt6400KbpsMethod Neo400KbpsMethod;

typedef NeoEsp32Rmt6Ws2812xInvertedMethod NeoWs2813InvertedMethod;
typedef NeoEsp32Rmt6Ws2812xInvertedMethod NeoWs2812xInvertedMethod;
typedef NeoEsp32Rmt6Ws2812xInvertedMethod NeoWs2811InvertedMethod;
typedef NeoEsp32Rmt6800KbpsInvertedMethod NeoWs2812InvertedMethod;
typedef NeoEsp32Rmt6Sk6812InvertedMethod NeoSk6812InvertedMethod;
typedef NeoEsp32Rmt6Sk6812InvertedMethod NeoLc8812InvertedMethod;
typedef NeoEsp32Rmt6Apa106InvertedMethod NeoApa106InvertedMethod;

typedef NeoEsp32Rmt6Ws2812xInvertedMethod Neo800KbpsInvertedMethod;
typedef NeoEsp32Rmt6400KbpsInvertedMethod Neo400KbpsInvertedMethod;


#endif
