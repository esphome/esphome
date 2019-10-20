#ifndef __INC_WS2812SERIAL_CONTROLLER_H
#define __INC_WS2812SERIAL_CONTROLLER_H

#ifdef USE_WS2812SERIAL

FASTLED_NAMESPACE_BEGIN

template<int DATA_PIN, EOrder RGB_ORDER>
class CWS2812SerialController : public CPixelLEDController<RGB_ORDER, 8, 0xFF> {
  WS2812Serial *pserial;
  uint8_t *drawbuffer,*framebuffer;

  void _init(int nLeds) {
    if (pserial == NULL) {
      drawbuffer = (uint8_t*)malloc(nLeds * 3);
      framebuffer = (uint8_t*)malloc(nLeds * 12);
      pserial = new WS2812Serial(nLeds, framebuffer, drawbuffer, DATA_PIN, WS2812_RGB);
      pserial->begin();
    }
  }
public:
  CWS2812SerialController() { pserial = NULL; }

  virtual void init() { /* do nothing yet */ }

  virtual void showPixels(PixelController<RGB_ORDER, 8, 0xFF> & pixels) {
    _init(pixels.size());

    uint8_t *p = drawbuffer;

    while(pixels.has(1)) {
      *p++ = pixels.loadAndScale0();
      *p++ = pixels.loadAndScale1();
      *p++ = pixels.loadAndScale2();
      pixels.stepDithering();
      pixels.advanceData();
    }
    pserial->show();
  }

};

FASTLED_NAMESPACE_END

#endif // USE_WS2812SERIAL
#endif // __INC_WS2812SERIAL_CONTROLLER_H
