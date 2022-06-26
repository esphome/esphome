#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#define FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ESP32_RAW_PIN_ORDER
#define FASTLED_RMT_BUILTIN_DRIVER true

// Avoid annoying compiler messages
#define FASTLED_INTERNAL
#include "FastLED.h"

#include "fastled_bus.h"

namespace esphome {
namespace fastled_bus {

namespace CLEDControllerFactory {
template<ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_DATA_RATE>
static CLEDController *create() {
  switch (CHIPSET) {
    case LPD8806: {
      return new LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB, SPI_DATA_RATE>();
    }
    case WS2801: {
      return new WS2801Controller<DATA_PIN, CLOCK_PIN, RGB, SPI_DATA_RATE>();
    }
    case WS2803: {
      return new WS2803Controller<DATA_PIN, CLOCK_PIN, RGB, SPI_DATA_RATE>();
    }
    case SM16716: {
      return new SM16716Controller<DATA_PIN, CLOCK_PIN, RGB, SPI_DATA_RATE>();
    }
    case P9813: {
      return new P9813Controller<DATA_PIN, CLOCK_PIN, RGB, SPI_DATA_RATE>();
    }
    case DOTSTAR:
    case APA102: {
      return new APA102Controller<DATA_PIN, CLOCK_PIN, RGB, SPI_DATA_RATE>();
    }
    case SK9822: {
      return new SK9822Controller<DATA_PIN, CLOCK_PIN, RGB, SPI_DATA_RATE>();
    }
  }
}

template<ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN> static CLEDController *create() {
  switch (CHIPSET) {
    case LPD8806: {
      return new LPD8806Controller<DATA_PIN, CLOCK_PIN>();
    }
    case WS2801: {
      return new WS2801Controller<DATA_PIN, CLOCK_PIN>();
    }
    case WS2803: {
      return new WS2803Controller<DATA_PIN, CLOCK_PIN>();
    }
    case SM16716: {
      return new SM16716Controller<DATA_PIN, CLOCK_PIN>();
    }
    case P9813: {
      return new P9813Controller<DATA_PIN, CLOCK_PIN>();
    }
    case DOTSTAR:
    case APA102: {
      return new APA102Controller<DATA_PIN, CLOCK_PIN>();
    }
    case SK9822: {
      return new SK9822Controller<DATA_PIN, CLOCK_PIN>();
    }
  }
}

template<ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN> static CLEDController *create(int num_leds) {
  switch (CHIPSET) {
    case LPD8806: {
      return new LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB>();
    }
    case WS2801: {
      return new WS2801Controller<DATA_PIN, CLOCK_PIN, RGB>();
    }
    case WS2803: {
      return new WS2803Controller<DATA_PIN, CLOCK_PIN, RGB>();
    }
    case SM16716: {
      return new SM16716Controller<DATA_PIN, CLOCK_PIN, RGB>();
    }
    case P9813: {
      return new P9813Controller<DATA_PIN, CLOCK_PIN, RGB>();
    }
    case DOTSTAR:
    case APA102: {
      return new APA102Controller<DATA_PIN, CLOCK_PIN, RGB>();
    }
    case SK9822: {
      return new SK9822Controller<DATA_PIN, CLOCK_PIN, RGB>();
    }
  }
}

#ifdef FASTLED_HAS_CLOCKLESS
template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN> static CLEDController *create() {
  return new CHIPSET<DATA_PIN, RGB>();
}
#endif

#ifdef FASTLED_HAS_BLOCKLESS
template<EBlockChipsets CHIPSET, int NUM_LANES> static CLEDController *create() {
  switch (CHIPSET) {
#ifdef PORTA_FIRST_PIN
    case WS2811_PORTA:
      return new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(320), NS(320), NS(640)>();
    case WS2811_400_PORTA:
      return new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(800), NS(800), NS(900)>();
    case WS2813_PORTA:
      return new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(320), NS(320), NS(640), RGB, 0, false,
                                                300>();
    case TM1803_PORTA:
      return new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(700), NS(1100), NS(700), RGB>();
    case UCS1903_PORTA:
      return new InlineBlockClocklessController<NUM_LANES, PORTA_FIRST_PIN, NS(500), NS(1500), NS(500), RGB>();
#endif
  }
}
#endif
}  // namespace CLEDControllerFactory

}  // namespace fastled_bus
}  // namespace esphome

#endif
