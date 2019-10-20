#ifndef __INC_CLOCKLESS_ARM_SAM_H
#define __INC_CLOCKLESS_ARM_SAM_H

FASTLED_NAMESPACE_BEGIN

// Definition for a single channel clockless controller for the sam family of arm chips, like that used in the due and rfduino
// See clockless.h for detailed info on how the template parameters are used.

#if defined(__SAM3X8E__)


#define TADJUST 0
#define TOTAL ( (T1+TADJUST) + (T2+TADJUST) + (T3+TADJUST) )

#define FASTLED_HAS_CLOCKLESS 1

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
	typedef typename FastPinBB<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPinBB<DATA_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() {
		FastPinBB<DATA_PIN>::setOutput();
		mPinMask = FastPinBB<DATA_PIN>::mask();
		mPort = FastPinBB<DATA_PIN>::port();
	}

	virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mWait.wait();
		if(!showRGBInternal(pixels)) {
      sei(); delayMicroseconds(WAIT_TIME); cli();
      showRGBInternal(pixels);
    }
		mWait.mark();
	}

	template<int BITS>  __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register data_ptr_t port, register uint8_t & b) {
		// Make sure we don't slot into a wrapping spot, this will delay up to 12.5µs for WS2812
		// bool bShift=0;
		// while(VAL < (TOTAL*10)) { bShift=true; }
		// if(bShift) { next_mark = (VAL-TOTAL); };

		for(register uint32_t i = BITS; i > 0; i--) {
			// wait to start the bit, then set the pin high
			while(DUE_TIMER_VAL < next_mark);
			next_mark = (DUE_TIMER_VAL+TOTAL);
			*port = 1;

			// how long we want to wait next depends on whether or not our bit is set to 1 or 0
			if(b&0x80) {
				// we're a 1, wait until there's less than T3 clocks left
				while((next_mark - DUE_TIMER_VAL) > (T3));
			} else {
				// we're a 0, wait until there's less than (T2+T3+slop) clocks left in this bit
				while((next_mark - DUE_TIMER_VAL) > (T2+T3+6+TADJUST+TADJUST));
			}
			*port=0;
			b <<= 1;
		}
	}

#define FORCE_REFERENCE(var)  asm volatile( "" : : "r" (var) )
	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
		// Setup and start the clock
		TC_Configure(DUE_TIMER,DUE_TIMER_CHANNEL,TC_CMR_TCCLKS_TIMER_CLOCK1);
		pmc_enable_periph_clk(DUE_TIMER_ID);
		TC_Start(DUE_TIMER,DUE_TIMER_CHANNEL);

		register data_ptr_t port asm("r7") = FastPinBB<DATA_PIN>::port(); FORCE_REFERENCE(port);
		*port = 0;

		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		uint8_t b = pixels.loadAndScale0();

		uint32_t next_mark = (DUE_TIMER_VAL + (TOTAL));
		while(pixels.has(1)) {
			pixels.stepDithering();

			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			cli();
			if(DUE_TIMER_VAL > next_mark) {
				if((DUE_TIMER_VAL - next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); TC_Stop(DUE_TIMER,DUE_TIMER_CHANNEL); return 0; }
			}
			#endif

			writeBits<8+XTRA0>(next_mark, port, b);

			b = pixels.loadAndScale1();
			writeBits<8+XTRA0>(next_mark, port,b);

			b = pixels.loadAndScale2();
			writeBits<8+XTRA0>(next_mark, port,b);

			b = pixels.advanceAndLoadAndScale0();
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			sei();
			#endif
		};

		TC_Stop(DUE_TIMER,DUE_TIMER_CHANNEL);
		return DUE_TIMER_VAL;
	}
};

#endif

FASTLED_NAMESPACE_END

#endif
