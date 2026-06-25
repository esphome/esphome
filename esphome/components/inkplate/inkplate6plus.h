#pragma once

#include "inkplate.h"
#include "inkplate_common.h"

#ifdef USE_ESP32

namespace esphome::inkplate {

static const uint8_t INKPLATE6PLUS_WAVEFORM3BIT[8][9] = {
    /* C0 gray0 */ {0, 0, 0, 0, 0, 2, 1, 1, 0},
    /* C1 gray1 */ {0, 0, 2, 1, 1, 1, 2, 1, 0},
    /* C2 gray2 */ {0, 2, 2, 2, 1, 1, 2, 1, 0},
    /* C3 gray3 */ {0, 0, 2, 2, 2, 1, 2, 1, 0},
    /* C4 gray4 */ {0, 0, 0, 0, 2, 2, 2, 1, 0},
    /* C5 gray5 */ {0, 0, 2, 1, 2, 1, 1, 2, 0},
    /* C6 gray6 */ {0, 0, 2, 2, 2, 1, 1, 2, 0},
    /* C7 gray7 */ {0, 0, 0, 0, 2, 2, 2, 2, 0},
};

class Inkplate6PLUS : public InkplateParallelBase {
 public:
  Inkplate6PLUS(int width, int height, int dark_phases, int partial_phases, int grayscale_phases)
      : InkplateParallelBase(width, height, dark_phases, partial_phases, grayscale_phases) {
    this->clean_seq_ = CLEAN_SEQ;
    this->clean_seq_len_ = CLEAN_SEQ_LEN;
  }

  void setup() override;
  void dump_config() override;

 protected:
  // Source: Inkplate6PLUSDriver.cpp EPDDriver::display1b() / display3b()
  static const CleanStep CLEAN_SEQ[6];
  static constexpr size_t CLEAN_SEQ_LEN = 6;
};

}  // namespace esphome::inkplate

#endif  // USE_ESP32
