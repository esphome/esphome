#ifndef __INC_CLOCKLESS_ARM_KL26
#define __INC_CLOCKLESS_ARM_KL26

#include "../common/m0clockless.h"
FASTLED_NAMESPACE_BEGIN
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

  virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
    mWait.wait();
    cli();
    uint32_t clocks = showRGBInternal(pixels);
    if(!clocks) {
      sei(); delayMicroseconds(WAIT_TIME); cli();
      clocks = showRGBInternal(pixels);
    }
    long microsTaken = CLKS_TO_MICROS(clocks * ((T1 + T2 + T3) * 24));
    MS_COUNTER += (microsTaken / 1000);
    sei();
    mWait.mark();
  }

  // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
  // gcc will use register Y for the this pointer.
  static uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
    struct M0ClocklessData data;
    data.d[0] = pixels.d[0];
    data.d[1] = pixels.d[1];
    data.d[2] = pixels.d[2];
    data.s[0] = pixels.mScale[0];
    data.s[1] = pixels.mScale[1];
    data.s[2] = pixels.mScale[2];
    data.e[0] = pixels.e[0];
    data.e[1] = pixels.e[1];
    data.e[2] = pixels.e[2];
    data.adj = pixels.mAdvance;

    typename FastPin<DATA_PIN>::port_ptr_t portBase = FastPin<DATA_PIN>::port();
    return showLedData<4,8,T1,T2,T3,RGB_ORDER, WAIT_TIME>(portBase, FastPin<DATA_PIN>::mask(), pixels.mData, pixels.mLen, &data);
    // return 0; // 0x00FFFFFF - _VAL;
  }


};

FASTLED_NAMESPACE_END


#endif // __INC_CLOCKLESS_ARM_KL26
