#ifndef __INC_SMARTMATRIX_T3_H
#define __INC_SMARTMATRIX_T3_H

#ifdef SmartMatrix_h
#include <SmartMatrix.h>

FASTLED_NAMESPACE_BEGIN

extern SmartMatrix *pSmartMatrix;

// note - dmx simple must be included before FastSPI for this code to be enabled
class CSmartMatrixController : public CPixelLEDController<RGB_ORDER> {
  SmartMatrix matrix;

public:
  // initialize the LED controller
  virtual void init() {
      // Initialize 32x32 LED Matrix
    matrix.begin();
    matrix.setBrightness(255);
    matrix.setColorCorrection(ccNone);

    // Clear screen
    clearLeds(0);
    matrix.swapBuffers();
    pSmartMatrix = &matrix;
  }

  virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
    if(SMART_MATRIX_CAN_TRIPLE_BUFFER) {
      rgb24 *md = matrix.getRealBackBuffer();
    } else {
      rgb24 *md = matrix.backBuffer();
    }
    while(pixels.has(1)) {
      md->red = pixels.loadAndScale0();
      md->green = pixels.loadAndScale1();
      md->blue = pixels.loadAndScale2();
      md++;
      pixels.advanceData();
      pixels.stepDithering();
    }
    matrix.swapBuffers();
    if(SMART_MATRIX_CAN_TRIPLE_BUFFER && pixels.advanceBy() > 0) {
      matrix.setBackBuffer(pixels.mData);
    }
  }

};

FASTLED_NAMESPACE_END

#endif

#endif
