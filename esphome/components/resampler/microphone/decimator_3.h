#pragma once

#include "esphome/components/audio/decimator_3.h"

namespace esphome::resampler {
#if defined(USE_DSP_DECIMATOR) && defined(USE_ESP_IDF)
using DspDecimator3 = audio::DspDecimator3;
#endif
}  // namespace esphome::resampler
