#ifndef __INC_CLOCKLESS_ARM_K20_H
#define __INC_CLOCKLESS_ARM_K20_H

FASTLED_NAMESPACE_BEGIN

// Definition for a single channel clockless controller for the k20 family of chips, like that used in the teensy 3.0/3.1
// See clockless.h for detailed info on how the template parameters are used.
#if defined(FASTLED_TEENSY3)

#define FASTLED_HAS_CLOCKLESS 1

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() {
		FastPin<DATA_PIN>::setOutput();
		mPinMask = FastPin<DATA_PIN>::mask();
		mPort = FastPin<DATA_PIN>::port();
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

	template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register data_ptr_t port, register data_t hi, register data_t lo, register uint8_t & b)  {
		for(register uint32_t i = BITS-1; i > 0; i--) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
			FastPin<DATA_PIN>::fastset(port, hi);
			if(b&0x80) {
				while((next_mark - ARM_DWT_CYCCNT) > (T3+(2*(F_CPU/24000000))));
				FastPin<DATA_PIN>::fastset(port, lo);
			} else {
				while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+(2*(F_CPU/24000000))));
				FastPin<DATA_PIN>::fastset(port, lo);
			}
			b <<= 1;
		}

		while(ARM_DWT_CYCCNT < next_mark);
		next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
		FastPin<DATA_PIN>::fastset(port, hi);

		if(b&0x80) {
			while((next_mark - ARM_DWT_CYCCNT) > (T3+(2*(F_CPU/24000000))));
			FastPin<DATA_PIN>::fastset(port, lo);
		} else {
			while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+(2*(F_CPU/24000000))));
			FastPin<DATA_PIN>::fastset(port, lo);
		}
	}

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
	    // Get access to the clock
		ARM_DEMCR    |= ARM_DEMCR_TRCENA;
		ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
		ARM_DWT_CYCCNT = 0;

		register data_ptr_t port = FastPin<DATA_PIN>::port();
		register data_t hi = *port | FastPin<DATA_PIN>::mask();;
		register data_t lo = *port & ~FastPin<DATA_PIN>::mask();;
		*port = lo;

		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		register uint8_t b = pixels.loadAndScale0();

		cli();
		uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

		while(pixels.has(1)) {
			pixels.stepDithering();
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			cli();
			// if interrupts took longer than 45µs, punt on the current frame
			if(ARM_DWT_CYCCNT > next_mark) {
				if((ARM_DWT_CYCCNT-next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); return 0; }
			}

			hi = *port | FastPin<DATA_PIN>::mask();
			lo = *port & ~FastPin<DATA_PIN>::mask();
			#endif
			// Write first byte, read next byte
			writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
			b = pixels.loadAndScale1();

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
			b = pixels.loadAndScale2();

			// Write third byte, read 1st byte of next pixel
			writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
			b = pixels.advanceAndLoadAndScale0();
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			sei();
			#endif
		};

		sei();
		return ARM_DWT_CYCCNT;
	}
};
#endif

FASTLED_NAMESPACE_END

#endif
