 #ifndef __INC_BLOCK_CLOCKLESS_H
#define __INC_BLOCK_CLOCKLESS_H

FASTLED_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base template for clockless controllers.  These controllers have 3 control points in their cycle for each bit.  The first point
// is where the line is raised hi.  The second pointsnt is where the line is dropped low for a zero.  The third point is where the
// line is dropped low for a one.  T1, T2, and T3 correspond to the timings for those three in clock cycles.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__SAM3X8E__)
#define PORT_MASK (((1<<LANES)-1) & ((FIRST_PIN==2) ? 0xFF : 0xFF))

#define FASTLED_HAS_BLOCKLESS 1

#define PORTD_FIRST_PIN 25
#define PORTA_FIRST_PIN 69
#define PORTB_FIRST_PIN 90

typedef union {
  uint8_t bytes[8];
  uint32_t raw[2];
} Lines;

#define TADJUST 0
#define TOTAL ( (T1+TADJUST) + (T2+TADJUST) + (T3+TADJUST) )
#define T1_MARK (TOTAL - (T1+TADJUST))
#define T2_MARK (T1_MARK - (T2+TADJUST))
template <uint8_t LANES, int FIRST_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class InlineBlockClocklessController : public CPixelLEDController<RGB_ORDER, LANES, PORT_MASK> {
	typedef typename FastPin<FIRST_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<FIRST_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual int size() { return CLEDController::size() * LANES; }
	virtual void init() {
    static_assert(LANES <= 8, "Maximum of 8 lanes for Due parallel controllers!");
    if(FIRST_PIN == PORTA_FIRST_PIN) {
      switch(LANES) {
        case 8: FastPin<31>::setOutput();
        case 7: FastPin<58>::setOutput();
        case 6: FastPin<100>::setOutput();
        case 5: FastPin<59>::setOutput();
        case 4: FastPin<60>::setOutput();
        case 3: FastPin<61>::setOutput();
        case 2: FastPin<68>::setOutput();
        case 1: FastPin<69>::setOutput();
      }
    } else if(FIRST_PIN == PORTD_FIRST_PIN) {
      switch(LANES) {
        case 8: FastPin<11>::setOutput();
        case 7: FastPin<29>::setOutput();
        case 6: FastPin<15>::setOutput();
        case 5: FastPin<14>::setOutput();
        case 4: FastPin<28>::setOutput();
        case 3: FastPin<27>::setOutput();
        case 2: FastPin<26>::setOutput();
        case 1: FastPin<25>::setOutput();
      }
    } else if(FIRST_PIN == PORTB_FIRST_PIN) {
      switch(LANES) {
        case 8: FastPin<97>::setOutput();
        case 7: FastPin<96>::setOutput();
        case 6: FastPin<95>::setOutput();
        case 5: FastPin<94>::setOutput();
        case 4: FastPin<93>::setOutput();
        case 3: FastPin<92>::setOutput();
        case 2: FastPin<91>::setOutput();
        case 1: FastPin<90>::setOutput();
      }
    }
    mPinMask = FastPin<FIRST_PIN>::mask();
    mPort = FastPin<FIRST_PIN>::port();
	}

	virtual uint16_t getMaxRefreshRate() const { return 400; }

  virtual void showPixels(PixelController<RGB_ORDER, LANES, PORT_MASK> & pixels) {
    mWait.wait();
    showRGBInternal(pixels);
    sei();
    mWait.mark();
  }

	static uint32_t showRGBInternal(PixelController<RGB_ORDER, LANES, PORT_MASK> &allpixels) {
		// Serial.println("Entering show");

    int nLeds = allpixels.mLen;

    // Setup the pixel controller and load/scale the first byte
		Lines b0,b1,b2;

    allpixels.preStepFirstByteDithering();
		for(uint8_t i = 0; i < LANES; i++) {
			b0.bytes[i] = allpixels.loadAndScale0(i);
		}

		// Setup and start the clock
    TC_Configure(DUE_TIMER,DUE_TIMER_CHANNEL,TC_CMR_TCCLKS_TIMER_CLOCK1);
    pmc_enable_periph_clk(DUE_TIMER_ID);
    TC_Start(DUE_TIMER,DUE_TIMER_CHANNEL);

    #if (FASTLED_ALLOW_INTERRUPTS == 1)
    cli();
    #endif
		uint32_t next_mark = (DUE_TIMER_VAL + (TOTAL));
		while(nLeds--) {
      allpixels.stepDithering();
      #if (FASTLED_ALLOW_INTERRUPTS == 1)
      cli();
      if(DUE_TIMER_VAL > next_mark) {
        if((DUE_TIMER_VAL - next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) {
          sei(); TC_Stop(DUE_TIMER,DUE_TIMER_CHANNEL); return DUE_TIMER_VAL;
        }
      }
      #endif

			// Write first byte, read next byte
			writeBits<8+XTRA0,1>(next_mark, b0, b1, allpixels);

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0,2>(next_mark, b1, b2, allpixels);

      allpixels.advanceData();
			// Write third byte
			writeBits<8+XTRA0,0>(next_mark, b2, b0, allpixels);

      #if (FASTLED_ALLOW_INTERRUPTS == 1)
      sei();
      #endif
		}

		return DUE_TIMER_VAL;
	}

  template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register Lines & b, Lines & b3, PixelController<RGB_ORDER,LANES, PORT_MASK> &pixels) { // , register uint32_t & b2)  {
    Lines b2;
    transpose8x1(b.bytes,b2.bytes);

    register uint8_t d = pixels.template getd<PX>(pixels);
    register uint8_t scale = pixels.template getscale<PX>(pixels);

    for(uint32_t i = 0; (i < LANES) && (i<8); i++) {
      while(DUE_TIMER_VAL < next_mark);
      next_mark = (DUE_TIMER_VAL+TOTAL);

      *FastPin<FIRST_PIN>::sport() = PORT_MASK;

      while((next_mark - DUE_TIMER_VAL) > (T2+T3+6));
      *FastPin<FIRST_PIN>::cport() = (~b2.bytes[7-i]) & PORT_MASK;

      while((next_mark - (DUE_TIMER_VAL)) > T3);
      *FastPin<FIRST_PIN>::cport() = PORT_MASK;

      b3.bytes[i] = pixels.template loadAndScale<PX>(pixels,i,d,scale);
    }

    for(uint32_t i = LANES; i < 8; i++) {
      while(DUE_TIMER_VAL < next_mark);
      next_mark = (DUE_TIMER_VAL+TOTAL);
      *FastPin<FIRST_PIN>::sport() = PORT_MASK;

      while((next_mark - DUE_TIMER_VAL) > (T2+T3+6));
      *FastPin<FIRST_PIN>::cport() = (~b2.bytes[7-i]) & PORT_MASK;

      while((next_mark - DUE_TIMER_VAL) > T3);
      *FastPin<FIRST_PIN>::cport() = PORT_MASK;
    }
  }


};

#endif

FASTLED_NAMESPACE_END

#endif
