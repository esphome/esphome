#ifndef __INC_OCTOWS2811_CONTROLLER_H
#define __INC_OCTOWS2811_CONTROLLER_H

#ifdef USE_OCTOWS2811

// #include "OctoWS2811.h"

FASTLED_NAMESPACE_BEGIN

template<EOrder RGB_ORDER = GRB, uint8_t CHIP = WS2811_800kHz>
class COctoWS2811Controller : public CPixelLEDController<RGB_ORDER, 8, 0xFF> {
  OctoWS2811  *pocto;
  uint8_t *drawbuffer,*framebuffer;

  void _init(int nLeds) {
    if(pocto == NULL) {
      drawbuffer = (uint8_t*)malloc(nLeds * 8 * 3);
      framebuffer = (uint8_t*)malloc(nLeds * 8 * 3);

      // byte ordering is handled in show by the pixel controller
      int config = WS2811_RGB;
      config |= CHIP;

      pocto = new OctoWS2811(nLeds, framebuffer, drawbuffer, config);

      pocto->begin();
    }
  }
public:
  COctoWS2811Controller() { pocto = NULL; }


  virtual void init() { /* do nothing yet */ }

  typedef union {
    uint8_t bytes[8];
    uint32_t raw[2];
  } Lines;

  virtual void showPixels(PixelController<RGB_ORDER, 8, 0xFF> & pixels) {
    _init(pixels.size());

    uint8_t *pData = drawbuffer;
    while(pixels.has(1)) {
      Lines b;

      for(int i = 0; i < 8; i++) { b.bytes[i] = pixels.loadAndScale0(i); }
      transpose8x1_MSB(b.bytes,pData); pData += 8;
      for(int i = 0; i < 8; i++) { b.bytes[i] = pixels.loadAndScale1(i); }
      transpose8x1_MSB(b.bytes,pData); pData += 8;
      for(int i = 0; i < 8; i++) { b.bytes[i] = pixels.loadAndScale2(i); }
      transpose8x1_MSB(b.bytes,pData); pData += 8;
      pixels.stepDithering();
      pixels.advanceData();
    }

    pocto->show();
  }

};

FASTLED_NAMESPACE_END

#endif

#endif
