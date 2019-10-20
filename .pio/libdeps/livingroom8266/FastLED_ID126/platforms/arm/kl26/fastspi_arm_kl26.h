#ifndef __INC_FASTSPI_ARM_KL26_H
#define __INC_FASTSPI_ARM_KL26_h

FASTLED_NAMESPACE_BEGIN

template <int VAL> void getScalars(uint8_t & sppr, uint8_t & spr) {
  if(VAL > 4096) { sppr=7; spr=8; }
  else if(VAL > 3584) { sppr=6; spr=8; }
  else if(VAL > 3072) { sppr=5; spr=8; }
  else if(VAL > 2560) { sppr=4; spr=8; }
  else if(VAL > 2048) { sppr=7; spr=7; }
  else if(VAL > 2048) { sppr=3; spr=8; }
  else if(VAL > 1792) { sppr=6; spr=7; }
  else if(VAL > 1536) { sppr=5; spr=7; }
  else if(VAL > 1536) { sppr=2; spr=8; }
  else if(VAL > 1280) { sppr=4; spr=7; }
  else if(VAL > 1024) { sppr=7; spr=6; }
  else if(VAL > 1024) { sppr=3; spr=7; }
  else if(VAL > 1024) { sppr=1; spr=8; }
  else if(VAL > 896) { sppr=6; spr=6; }
  else if(VAL > 768) { sppr=5; spr=6; }
  else if(VAL > 768) { sppr=2; spr=7; }
  else if(VAL > 640) { sppr=4; spr=6; }
  else if(VAL > 512) { sppr=7; spr=5; }
  else if(VAL > 512) { sppr=3; spr=6; }
  else if(VAL > 512) { sppr=1; spr=7; }
  else if(VAL > 512) { sppr=0; spr=8; }
  else if(VAL > 448) { sppr=6; spr=5; }
  else if(VAL > 384) { sppr=5; spr=5; }
  else if(VAL > 384) { sppr=2; spr=6; }
  else if(VAL > 320) { sppr=4; spr=5; }
  else if(VAL > 256) { sppr=7; spr=4; }
  else if(VAL > 256) { sppr=3; spr=5; }
  else if(VAL > 256) { sppr=1; spr=6; }
  else if(VAL > 256) { sppr=0; spr=7; }
  else if(VAL > 224) { sppr=6; spr=4; }
  else if(VAL > 192) { sppr=5; spr=4; }
  else if(VAL > 192) { sppr=2; spr=5; }
  else if(VAL > 160) { sppr=4; spr=4; }
  else if(VAL > 128) { sppr=7; spr=3; }
  else if(VAL > 128) { sppr=3; spr=4; }
  else if(VAL > 128) { sppr=1; spr=5; }
  else if(VAL > 128) { sppr=0; spr=6; }
  else if(VAL > 112) { sppr=6; spr=3; }
  else if(VAL > 96) { sppr=5; spr=3; }
  else if(VAL > 96) { sppr=2; spr=4; }
  else if(VAL > 80) { sppr=4; spr=3; }
  else if(VAL > 64) { sppr=7; spr=2; }
  else if(VAL > 64) { sppr=3; spr=3; }
  else if(VAL > 64) { sppr=1; spr=4; }
  else if(VAL > 64) { sppr=0; spr=5; }
  else if(VAL > 56) { sppr=6; spr=2; }
  else if(VAL > 48) { sppr=5; spr=2; }
  else if(VAL > 48) { sppr=2; spr=3; }
  else if(VAL > 40) { sppr=4; spr=2; }
  else if(VAL > 32) { sppr=7; spr=1; }
  else if(VAL > 32) { sppr=3; spr=2; }
  else if(VAL > 32) { sppr=1; spr=3; }
  else if(VAL > 32) { sppr=0; spr=4; }
  else if(VAL > 28) { sppr=6; spr=1; }
  else if(VAL > 24) { sppr=5; spr=1; }
  else if(VAL > 24) { sppr=2; spr=2; }
  else if(VAL > 20) { sppr=4; spr=1; }
  else if(VAL > 16) { sppr=7; spr=0; }
  else if(VAL > 16) { sppr=3; spr=1; }
  else if(VAL > 16) { sppr=1; spr=2; }
  else if(VAL > 16) { sppr=0; spr=3; }
  else if(VAL > 14) { sppr=6; spr=0; }
  else if(VAL > 12) { sppr=5; spr=0; }
  else if(VAL > 12) { sppr=2; spr=1; }
  else if(VAL > 10) { sppr=4; spr=0; }
  else if(VAL > 8) { sppr=3; spr=0; }
  else if(VAL > 8) { sppr=1; spr=1; }
  else if(VAL > 8) { sppr=0; spr=2; }
  else if(VAL > 6) { sppr=2; spr=0; }
  else if(VAL > 4) { sppr=1; spr=0; }
  else if(VAL > 4) { sppr=0; spr=1; }
  else /* if(VAL > 2) */ { sppr=0; spr=0; }
}


#define SPIX (*(KINETISL_SPI_t*)pSPIX)
#define ARM_HARDWARE_SPI

template <uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER, uint32_t pSPIX>
class ARMHardwareSPIOutput {
  Selectable *m_pSelect;

  static inline void enable_pins(void) __attribute__((always_inline)) {
    switch(_DATA_PIN) {
      case 0: CORE_PIN0_CONFIG =  PORT_PCR_MUX(2); break;
      case 1: CORE_PIN1_CONFIG =  PORT_PCR_MUX(5); break;
      case 7: CORE_PIN7_CONFIG =  PORT_PCR_MUX(2); break;
      case 8: CORE_PIN8_CONFIG =  PORT_PCR_MUX(5); break;
      case 11: CORE_PIN11_CONFIG =  PORT_PCR_MUX(2); break;
      case 12: CORE_PIN12_CONFIG =  PORT_PCR_MUX(5); break;
      case 21: CORE_PIN21_CONFIG =  PORT_PCR_MUX(2); break;
    }

    switch(_CLOCK_PIN) {
      case 13: CORE_PIN13_CONFIG =  PORT_PCR_MUX(2); break;
      case 14: CORE_PIN14_CONFIG =  PORT_PCR_MUX(2); break;
      case 20: CORE_PIN20_CONFIG =  PORT_PCR_MUX(2); break;
    }
  }

  static inline void disable_pins(void) __attribute((always_inline)) {
    switch(_DATA_PIN) {
      case 0: CORE_PIN0_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 1: CORE_PIN1_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 7: CORE_PIN7_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 8: CORE_PIN8_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 11: CORE_PIN11_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 12: CORE_PIN12_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 21: CORE_PIN21_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
    }

    switch(_CLOCK_PIN) {
      case 13: CORE_PIN13_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 14: CORE_PIN14_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
      case 20: CORE_PIN20_CONFIG = PORT_PCR_SRE | PORT_PCR_MUX(1); break;
    }
  }

  void setSPIRate() {
    uint8_t sppr, spr;
    getScalars<_SPI_CLOCK_DIVIDER>(sppr, spr);

    // Set the speed
    SPIX.BR = SPI_BR_SPPR(sppr) | SPI_BR_SPR(spr);

    // Also, force 8 bit transfers (don't want to juggle 8/16 since that flushes the world)
    SPIX.C2 = 0;
    SPIX.C1 |= SPI_C1_SPE;
  }

public:
  ARMHardwareSPIOutput() { m_pSelect = NULL; }
  ARMHardwareSPIOutput(Selectable *pSelect) { m_pSelect = pSelect; }

  // set the object representing the selectable
  void setSelect(Selectable *pSelect) { m_pSelect = pSelect; }

  // initialize the SPI subssytem
  void init() {
    FastPin<_DATA_PIN>::setOutput();
    FastPin<_CLOCK_PIN>::setOutput();

    // Enable the SPI clocks
    uint32_t sim4 = SIM_SCGC4;
    if ((pSPIX == 0x40076000) && !(sim4 & SIM_SCGC4_SPI0)) {
      SIM_SCGC4 = sim4 | SIM_SCGC4_SPI0;
    }

    if ( (pSPIX == 0x40077000) && !(sim4 & SIM_SCGC4_SPI1)) {
      SIM_SCGC4 = sim4 | SIM_SCGC4_SPI1;
    }

    SPIX.C1 = SPI_C1_MSTR | SPI_C1_SPE;
    SPIX.C2 = 0;
    SPIX.BR = SPI_BR_SPPR(1) | SPI_BR_SPR(0);
  }

  // latch the CS select
  void inline select() __attribute__((always_inline)) {
    if(m_pSelect != NULL) { m_pSelect->select(); }
    setSPIRate();
    enable_pins();
  }


  // release the CS select
  void inline release() __attribute__((always_inline)) {
    disable_pins();
    if(m_pSelect != NULL) { m_pSelect->release(); }
  }

  // Wait for the world to be clear
  static void wait() __attribute__((always_inline)) { while(!(SPIX.S & SPI_S_SPTEF));  }

  // wait until all queued up data has been written
  void waitFully() { wait(); }

  // not the most efficient mechanism in the world - but should be enough for sm16716 and friends
  template <uint8_t BIT> inline static void writeBit(uint8_t b) { /* TODO */ }

  // write a byte out via SPI (returns immediately on writing register)
  static void writeByte(uint8_t b) __attribute__((always_inline)) { wait(); SPIX.DL = b; }
  // write a word out via SPI (returns immediately on writing register)
  static void writeWord(uint16_t w) __attribute__((always_inline)) { writeByte(w>>8); writeByte(w & 0xFF); }

  // A raw set of writing byte values, assumes setup/init/waiting done elsewhere (static for use by adjustment classes)
  static void writeBytesValueRaw(uint8_t value, int len) {
    while(len--) { writeByte(value); }
  }

  // A full cycle of writing a value for len bytes, including select, release, and waiting
  void writeBytesValue(uint8_t value, int len) {
    setSPIRate();
    select();
    while(len--) {
      writeByte(value);
    }
    waitFully();
    release();
  }

  // A full cycle of writing a raw block of data out, including select, release, and waiting
  template <class D> void writeBytes(register uint8_t *data, int len) {
    setSPIRate();
    uint8_t *end = data + len;
    select();
    // could be optimized to write 16bit words out instead of 8bit bytes
    while(data != end) {
      writeByte(D::adjust(*data++));
    }
    D::postBlock(len);
    waitFully();
    release();
  }

  void writeBytes(register uint8_t *data, int len) { writeBytes<DATA_NOP>(data, len); }


  template <uint8_t FLAGS, class D, EOrder RGB_ORDER> void writePixels(PixelController<RGB_ORDER> pixels) {
    int len = pixels.mLen;

    select();
    while(pixels.has(1)) {
      if(FLAGS & FLAG_START_BIT) {
        writeBit<0>(1);
        writeByte(D::adjust(pixels.loadAndScale0()));
        writeByte(D::adjust(pixels.loadAndScale1()));
        writeByte(D::adjust(pixels.loadAndScale2()));
      } else {
        writeByte(D::adjust(pixels.loadAndScale0()));
        writeByte(D::adjust(pixels.loadAndScale1()));
        writeByte(D::adjust(pixels.loadAndScale2()));
      }

      pixels.advanceData();
      pixels.stepDithering();
    }
    D::postBlock(len);
    release();
  }

};

FASTLED_NAMESPACE_END

#endif
