#pragma once

#include "../waveshare_io_ch32v003.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace waveshare_io_ch32v003 {

class WaveshareIOCH32V003Output : public output::FloatOutput, public Parented<WaveshareIOCH32V003Component> {
 protected:
  void write_state(float state) override;
};

}  // namespace waveshare_io_ch32v003
}  // namespace esphome
