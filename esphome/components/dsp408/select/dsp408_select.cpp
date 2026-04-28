#include "dsp408_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dsp408 {

static const char *const TAG = "dsp408.select";

static const char *kind_name(SelectKind k) {
  switch (k) {
    case SelectKind::CHANNEL_HPF_FILTER:
      return "HPF filter";
    case SelectKind::CHANNEL_HPF_SLOPE:
      return "HPF slope";
    case SelectKind::CHANNEL_LPF_FILTER:
      return "LPF filter";
    case SelectKind::CHANNEL_LPF_SLOPE:
      return "LPF slope";
  }
  return "?";
}

void DSP408Select::dump_config() {
  ESP_LOGCONFIG(TAG, "DSP-408 Select (%s ch=%u): '%s'", kind_name(this->kind_), this->channel_,
                this->get_name().c_str());
}

// Filter type indices: 0=Butterworth, 1=Bessel, 2=LR, 3=LR (alias).
static int filter_index_for(const std::string &v) {
  if (v == "Butterworth")
    return 0;
  if (v == "Bessel")
    return 1;
  if (v == "Linkwitz-Riley")
    return 2;
  return -1;
}

// Slope indices: 0..7 = 6/12/18/24/30/36/42/48 dB/oct, 8 = Off.
static int slope_index_for(const std::string &v) {
  static const char *NAMES[9] = {"6 dB/oct",  "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct",
                                 "36 dB/oct", "42 dB/oct", "48 dB/oct", "Off"};
  for (int i = 0; i < 9; i++)
    if (v == NAMES[i])
      return i;
  return -1;
}

void DSP408Select::control(const std::string &value) {
  if (this->parent_ == nullptr)
    return;
  this->publish_state(value);  // optimistic — device ack will reaffirm
  switch (this->kind_) {
    case SelectKind::CHANNEL_HPF_FILTER: {
      int idx = filter_index_for(value);
      if (idx < 0)
        return;
      this->parent_->request_channel_hpf_filter(this->channel_, static_cast<uint8_t>(idx));
      break;
    }
    case SelectKind::CHANNEL_HPF_SLOPE: {
      int idx = slope_index_for(value);
      if (idx < 0)
        return;
      this->parent_->request_channel_hpf_slope(this->channel_, static_cast<uint8_t>(idx));
      break;
    }
    case SelectKind::CHANNEL_LPF_FILTER: {
      int idx = filter_index_for(value);
      if (idx < 0)
        return;
      this->parent_->request_channel_lpf_filter(this->channel_, static_cast<uint8_t>(idx));
      break;
    }
    case SelectKind::CHANNEL_LPF_SLOPE: {
      int idx = slope_index_for(value);
      if (idx < 0)
        return;
      this->parent_->request_channel_lpf_slope(this->channel_, static_cast<uint8_t>(idx));
      break;
    }
  }
}

}  // namespace dsp408
}  // namespace esphome
