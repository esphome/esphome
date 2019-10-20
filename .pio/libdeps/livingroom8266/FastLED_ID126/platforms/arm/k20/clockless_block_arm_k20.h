#ifndef __INC_BLOCK_CLOCKLESS_ARM_K20_H
#define __INC_BLOCK_CLOCKLESS_ARM_K20_H

// Definition for a single channel clockless controller for the k20 family of chips, like that used in the teensy 3.0/3.1
// See clockless.h for detailed info on how the template parameters are used.
#if defined(FASTLED_TEENSY3)
#define FASTLED_HAS_BLOCKLESS 1

#define PORTC_FIRST_PIN 15
#define PORTD_FIRST_PIN 2
#define HAS_PORTDC 1

#define PORT_MASK (((1<<LANES)-1) & ((FIRST_PIN==2) ? 0xFF : 0xFFF))

#define MIN(X,Y) (((X)<(Y)) ? (X):(Y))
#define USED_LANES ((FIRST_PIN==2) ? MIN(LANES,8) : MIN(LANES,12))

#include <kinetis.h>

FASTLED_NAMESPACE_BEGIN

template <uint8_t LANES, int FIRST_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 40>
class InlineBlockClocklessController : public CPixelLEDController<RGB_ORDER, LANES, PORT_MASK> {
	typedef typename FastPin<FIRST_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<FIRST_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual int size() { return CLEDController::size() * LANES; }

	virtual void showPixels(PixelController<RGB_ORDER, LANES, PORT_MASK> & pixels) { 
		mWait.wait();
		uint32_t clocks = showRGBInternal(pixels);
		#if FASTLED_ALLOW_INTERRUPTS == 0
		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(clocks);
		MS_COUNTER += (1 + (microsTaken / 1000));
		#endif

		mWait.mark();
	}

	virtual void init() {
		if(FIRST_PIN == PORTC_FIRST_PIN) { // PORTC
			switch(USED_LANES) {
				case 12: FastPin<30>::setOutput();
				case 11: FastPin<29>::setOutput();
				case 10: FastPin<27>::setOutput();
				case 9: FastPin<28>::setOutput();
				case 8: FastPin<12>::setOutput();
				case 7: FastPin<11>::setOutput();
				case 6: FastPin<13>::setOutput();
				case 5: FastPin<10>::setOutput();
				case 4: FastPin<9>::setOutput();
				case 3: FastPin<23>::setOutput();
				case 2: FastPin<22>::setOutput();
				case 1: FastPin<15>::setOutput();
			}
		} else if(FIRST_PIN == PORTD_FIRST_PIN) { // PORTD
			switch(USED_LANES) {
				case 8: FastPin<5>::setOutput();
				case 7: FastPin<21>::setOutput();
				case 6: FastPin<20>::setOutput();
				case 5: FastPin<6>::setOutput();
				case 4: FastPin<8>::setOutput();
				case 3: FastPin<7>::setOutput();
				case 2: FastPin<14>::setOutput();
				case 1: FastPin<2>::setOutput();
			}
		}
		mPinMask = FastPin<FIRST_PIN>::mask();
		mPort = FastPin<FIRST_PIN>::port();
	}

	virtual uint16_t getMaxRefreshRate() const { return 400; }

	typedef union {
		uint8_t bytes[12];
		uint16_t shorts[6];
		uint32_t raw[3];
	} Lines;

	template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register Lines & b, PixelController<RGB_ORDER, LANES, PORT_MASK> &pixels) { // , register uint32_t & b2)  {
		register Lines b2;
		if(USED_LANES>8) {
			transpose8<1,2>(b.bytes,b2.bytes);
			transpose8<1,2>(b.bytes+8,b2.bytes+1);
		} else {
			transpose8x1(b.bytes,b2.bytes);
		}
		register uint8_t d = pixels.template getd<PX>(pixels);
		register uint8_t scale = pixels.template getscale<PX>(pixels);

		for(register uint32_t i = 0; i < (USED_LANES/2); i++) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3)-3;
			*FastPin<FIRST_PIN>::sport() = PORT_MASK;

			while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+(2*(F_CPU/24000000))));
			if(USED_LANES>8) {
				*FastPin<FIRST_PIN>::cport() = ((~b2.shorts[i]) & PORT_MASK);
			} else {
				*FastPin<FIRST_PIN>::cport() = ((~b2.bytes[7-i]) & PORT_MASK);
			}

			while((next_mark - ARM_DWT_CYCCNT) > (T3));
			*FastPin<FIRST_PIN>::cport() = PORT_MASK;

			b.bytes[i] = pixels.template loadAndScale<PX>(pixels,i,d,scale);
			b.bytes[i+(USED_LANES/2)] = pixels.template loadAndScale<PX>(pixels,i+(USED_LANES/2),d,scale);
		}

		// if folks use an odd numnber of lanes, get the last byte's value here
		if(USED_LANES & 0x01) {
			b.bytes[USED_LANES-1] = pixels.template loadAndScale<PX>(pixels,USED_LANES-1,d,scale);
		}

		for(register uint32_t i = USED_LANES/2; i < 8; i++) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3)-3;
			*FastPin<FIRST_PIN>::sport() = PORT_MASK;
			while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+(2*(F_CPU/24000000))));
			if(USED_LANES>8) {
				*FastPin<FIRST_PIN>::cport() = ((~b2.shorts[i]) & PORT_MASK);
			} else {
				// b2.bytes[0] = 0;
				*FastPin<FIRST_PIN>::cport() = ((~b2.bytes[7-i]) & PORT_MASK);
			}

			while((next_mark - ARM_DWT_CYCCNT) > (T3));
			*FastPin<FIRST_PIN>::cport() = PORT_MASK;

		}
	}



	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
		static uint32_t showRGBInternal(PixelController<RGB_ORDER, LANES, PORT_MASK> &allpixels) {
		// Get access to the clock
		ARM_DEMCR    |= ARM_DEMCR_TRCENA;
		ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
		ARM_DWT_CYCCNT = 0;

		// Setup the pixel controller and load/scale the first byte
		allpixels.preStepFirstByteDithering();
		register Lines b0;

		allpixels.preStepFirstByteDithering();
		for(int i = 0; i < USED_LANES; i++) {
			b0.bytes[i] = allpixels.loadAndScale0(i);
		}

		cli();
		uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

		while(allpixels.has(1)) {
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			cli();
			// if interrupts took longer than 45µs, punt on the current frame
			if(ARM_DWT_CYCCNT > next_mark) {
				if((ARM_DWT_CYCCNT-next_mark) > ((WAIT_TIME-5)*CLKS_PER_US)) { sei(); return ARM_DWT_CYCCNT; }
			}
			#endif
			allpixels.stepDithering();

			// Write first byte, read next byte
			writeBits<8+XTRA0,1>(next_mark, b0, allpixels);

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0,2>(next_mark, b0, allpixels);
			allpixels.advanceData();

			// Write third byte
			writeBits<8+XTRA0,0>(next_mark, b0, allpixels);
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			sei();
			#endif
		};

		return ARM_DWT_CYCCNT;
	}
};

#define PMASK ((1<<(LANES))-1)
#define PMASK_HI (PMASK>>8 & 0xFF)
#define PMASK_LO (PMASK & 0xFF)

template <uint8_t LANES, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class SixteenWayInlineBlockClocklessController : public CPixelLEDController<RGB_ORDER, LANES, PMASK> {
	typedef typename FastPin<PORTC_FIRST_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<PORTC_FIRST_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() {
		static_assert(LANES <= 16, "Maximum of 16 lanes for Teensy parallel controllers!");
		// FastPin<30>::setOutput();
		// FastPin<29>::setOutput();
		// FastPin<27>::setOutput();
		// FastPin<28>::setOutput();
		switch(LANES) {
			case 16: FastPin<12>::setOutput();
			case 15: FastPin<11>::setOutput();
			case 14: FastPin<13>::setOutput();
			case 13: FastPin<10>::setOutput();
			case 12: FastPin<9>::setOutput();
			case 11: FastPin<23>::setOutput();
			case 10: FastPin<22>::setOutput();
			case 9:  FastPin<15>::setOutput();

			case 8:  FastPin<5>::setOutput();
			case 7:  FastPin<21>::setOutput();
			case 6:  FastPin<20>::setOutput();
			case 5:  FastPin<6>::setOutput();
			case 4:  FastPin<8>::setOutput();
			case 3:  FastPin<7>::setOutput();
			case 2:  FastPin<14>::setOutput();
			case 1:  FastPin<2>::setOutput();
		}
	}

	virtual void showPixels(PixelController<RGB_ORDER, LANES, PMASK> & pixels) { 
		mWait.wait();
		uint32_t clocks = showRGBInternal(pixels);
		#if FASTLED_ALLOW_INTERRUPTS == 0
		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(clocks);
		MS_COUNTER += (1 + (microsTaken / 1000));
		#endif

		mWait.mark();
	}

	typedef union {
		uint8_t bytes[16];
		uint16_t shorts[8];
		uint32_t raw[4];
	} Lines;

	template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register Lines & b, PixelController<RGB_ORDER,LANES, PMASK> &pixels) { // , register uint32_t & b2)  {
		register Lines b2;
		transpose8x1(b.bytes,b2.bytes);
		transpose8x1(b.bytes+8,b2.bytes+8);
		register uint8_t d = pixels.template getd<PX>(pixels);
		register uint8_t scale = pixels.template getscale<PX>(pixels);

		for(register uint32_t i = 0; (i < LANES) && (i < 8); i++) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3)-3;
			*FastPin<PORTD_FIRST_PIN>::sport() = PMASK_LO;
			*FastPin<PORTC_FIRST_PIN>::sport() = PMASK_HI;

			while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+6));
			*FastPin<PORTD_FIRST_PIN>::cport() = ((~b2.bytes[7-i]) & PMASK_LO);
			*FastPin<PORTC_FIRST_PIN>::cport() = ((~b2.bytes[15-i]) & PMASK_HI);

			while((next_mark - ARM_DWT_CYCCNT) > (T3));
			*FastPin<PORTD_FIRST_PIN>::cport() = PMASK_LO;
			*FastPin<PORTC_FIRST_PIN>::cport() = PMASK_HI;

			b.bytes[i] = pixels.template loadAndScale<PX>(pixels,i,d,scale);
			if(LANES==16 || (LANES>8 && ((i+8) < LANES))) {
				b.bytes[i+8] = pixels.template loadAndScale<PX>(pixels,i+8,d,scale);
			}
		}
	}



	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
		static uint32_t showRGBInternal(PixelController<RGB_ORDER,LANES, PMASK> &allpixels) {
		// Get access to the clock
		ARM_DEMCR    |= ARM_DEMCR_TRCENA;
		ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
		ARM_DWT_CYCCNT = 0;

		// Setup the pixel controller and load/scale the first byte
		allpixels.preStepFirstByteDithering();
		register Lines b0;

		allpixels.preStepFirstByteDithering();
		for(int i = 0; i < LANES; i++) {
			b0.bytes[i] = allpixels.loadAndScale0(i);
		}

		cli();
		uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

		while(allpixels.has(1)) {
			allpixels.stepDithering();
			#if 0 && (FASTLED_ALLOW_INTERRUPTS == 1)
			cli();
			// if interrupts took longer than 45µs, punt on the current frame
			if(ARM_DWT_CYCCNT > next_mark) {
				if((ARM_DWT_CYCCNT-next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); return ARM_DWT_CYCCNT; }
			}
			#endif

			// Write first byte, read next byte
			writeBits<8+XTRA0,1>(next_mark, b0, allpixels);

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0,2>(next_mark, b0, allpixels);
			allpixels.advanceData();

			// Write third byte
			writeBits<8+XTRA0,0>(next_mark, b0, allpixels);

			#if 0 && (FASTLED_ALLOW_INTERRUPTS == 1)
			sei();
			#endif
		};
		sei();

		return ARM_DWT_CYCCNT;
	}
};

FASTLED_NAMESPACE_END

#endif

#endif
