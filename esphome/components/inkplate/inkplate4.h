#pragma once

#include "inkplate.h"
#include "inkplate_common.h"

#ifdef USE_ESP32

namespace esphome::inkplate {

static const uint8_t INKPLATE4_WAVEFORM3BIT[8][9] = {
    {0, 0, 0, 1, 1, 1, 1, 1, 0}, {0, 1, 1, 1, 2, 1, 1, 0, 0}, {0, 2, 1, 1, 0, 2, 1, 1, 0}, {0, 0, 0, 0, 1, 1, 1, 2, 0},
    {0, 2, 1, 1, 2, 1, 1, 2, 0}, {0, 1, 2, 1, 1, 2, 1, 2, 0}, {0, 1, 1, 1, 2, 1, 2, 2, 0}, {0, 0, 0, 0, 0, 0, 2, 2, 0},
};

class Inkplate4 : public InkplateParallelBase {
 public:
  Inkplate4(int width, int height, int dark_phases, int partial_phases, int grayscale_phases)
      : InkplateParallelBase(width, height, dark_phases, partial_phases, grayscale_phases) {
    this->clean_seq_ = CLEAN_SEQ;
    this->clean_seq_len_ = CLEAN_SEQ_LEN;
  }

  void setup() override;
  void dump_config() override;

 protected:
  static const CleanStep CLEAN_SEQ[5];
  static constexpr size_t CLEAN_SEQ_LEN = 5;

  bool do_board_transfer_step() override;
};

}  // namespace esphome::inkplate

#endif  // USE_ESP32
