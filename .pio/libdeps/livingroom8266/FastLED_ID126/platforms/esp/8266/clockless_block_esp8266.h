#ifndef __INC_CLOCKLESS_BLOCK_ESP8266_H
#define __INC_CLOCKLESS_BLOCK_ESP8266_H

#define FASTLED_HAS_BLOCKLESS 1

#define FIX_BITS(bits) (((bits & 0x0fL) << 12) | (bits & 0x30))

#define MIN(X,Y) (((X)<(Y)) ? (X):(Y))
#define USED_LANES (MIN(LANES, 6))
#define PORT_MASK (((1 << USED_LANES)-1) & 0x0000FFFFL)
#define PIN_MASK FIX_BITS(PORT_MASK)

FASTLED_NAMESPACE_BEGIN

#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
extern uint32_t _frame_cnt;
extern uint32_t _retry_cnt;
#endif

template <uint8_t LANES, int FIRST_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class InlineBlockClocklessController : public CPixelLEDController<RGB_ORDER, LANES, PORT_MASK> {
	typedef typename FastPin<FIRST_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<FIRST_PIN>::port_t data_t;

	CMinWait<WAIT_TIME> mWait;
public:
	virtual int size() { return CLEDController::size() * LANES; }

	virtual void showPixels(PixelController<RGB_ORDER, LANES, PORT_MASK> & pixels) {
		// mWait.wait();
		/*uint32_t clocks = */
		int cnt=FASTLED_INTERRUPT_RETRY_COUNT;
		while(!showRGBInternal(pixels) && cnt--) {
      os_intr_unlock();
			#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
			_retry_cnt++;
			#endif
      delayMicroseconds(WAIT_TIME * 10);
      os_intr_lock();
    }
		// #if FASTLED_ALLOW_INTTERUPTS == 0
		// Adjust the timer
		// long microsTaken = CLKS_TO_MICROS(clocks);
		// MS_COUNTER += (1 + (microsTaken / 1000));
		// #endif

		// mWait.mark();
	}

  template<int PIN> static void initPin() {
			_ESPPIN<PIN, 1<<(PIN & 0xFF)>::setOutput();
  }

  virtual void init() {
		void (* funcs[])() ={initPin<12>, initPin<13>, initPin<14>, initPin<15>, initPin<4>, initPin<5>};

		for (uint8_t i = 0; i < USED_LANES; ++i) {
			funcs[i]();
		}
  }

  virtual uint16_t getMaxRefreshRate() const { return 400; }

	typedef union {
		uint8_t bytes[8];
		uint16_t shorts[4];
		uint32_t raw[2];
	} Lines;

#define ESP_ADJUST 0 // (2*(F_CPU/24000000))
#define ESP_ADJUST2 0
  template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & last_mark, register Lines & b, PixelController<RGB_ORDER, LANES, PORT_MASK> &pixels) { // , register uint32_t & b2)  {
	  Lines b2 = b;
		transpose8x1_noinline(b.bytes,b2.bytes);

		register uint8_t d = pixels.template getd<PX>(pixels);
		register uint8_t scale = pixels.template getscale<PX>(pixels);

		for(register uint32_t i = 0; i < USED_LANES; i++) {
			while((__clock_cycles() - last_mark) < (T1+T2+T3));
			last_mark = __clock_cycles();
			*FastPin<FIRST_PIN>::sport() = PIN_MASK;

			uint32_t nword = (uint32_t)(~b2.bytes[7-i]);
			while((__clock_cycles() - last_mark) < (T1-6));
			*FastPin<FIRST_PIN>::cport() = FIX_BITS(nword);

			while((__clock_cycles() - last_mark) < (T1+T2));
			*FastPin<FIRST_PIN>::cport() = PIN_MASK;

			b.bytes[i] = pixels.template loadAndScale<PX>(pixels,i,d,scale);
		}

		for(register uint32_t i = USED_LANES; i < 8; i++) {
			while((__clock_cycles() - last_mark) < (T1+T2+T3));
			last_mark = __clock_cycles();
			*FastPin<FIRST_PIN>::sport() = PIN_MASK;

			uint32_t nword = (uint32_t)(~b2.bytes[7-i]);
			while((__clock_cycles() - last_mark) < (T1-6));
			*FastPin<FIRST_PIN>::cport() = FIX_BITS(nword);

			while((__clock_cycles() - last_mark) < (T1+T2));
			*FastPin<FIRST_PIN>::cport() = PIN_MASK;
		}
	}

  // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
		static uint32_t ICACHE_RAM_ATTR showRGBInternal(PixelController<RGB_ORDER, LANES, PORT_MASK> &allpixels) {

		// Setup the pixel controller and load/scale the first byte
		Lines b0;

		for(int i = 0; i < USED_LANES; i++) {
			b0.bytes[i] = allpixels.loadAndScale0(i);
		}
		allpixels.preStepFirstByteDithering();

		os_intr_lock();
		uint32_t _start = __clock_cycles();
		uint32_t last_mark = _start;

		while(allpixels.has(1)) {
			// Write first byte, read next byte
			writeBits<8+XTRA0,1>(last_mark, b0, allpixels);

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0,2>(last_mark, b0, allpixels);
			allpixels.advanceData();

			// Write third byte
			writeBits<8+XTRA0,0>(last_mark, b0, allpixels);

      #if (FASTLED_ALLOW_INTERRUPTS == 1)
			os_intr_unlock();
			#endif

			allpixels.stepDithering();

			#if (FASTLED_ALLOW_INTERRUPTS == 1)
      os_intr_lock();
			// if interrupts took longer than 45µs, punt on the current frame
			if((int32_t)(__clock_cycles()-last_mark) > 0) {
				if((int32_t)(__clock_cycles()-last_mark) > (T1+T2+T3+((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US))) { os_intr_unlock(); return 0; }
			}
			#endif
		};

    os_intr_unlock();
		#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
		_frame_cnt++;
		#endif
    return __clock_cycles() - _start;
	}
};

FASTLED_NAMESPACE_END
#endif
