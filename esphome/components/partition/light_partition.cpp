#include "light_partition.h"
#include "esphome/core/log.h"

namespace esphome::partition {

ESPHOME_LOG_TAG(TAG, "partition.light");

light::ESPColorView PartitionLightOutput::get_color_view_(int32_t index) const {
  uint32_t lo = 0;
  uint32_t hi = this->segments_.size() - 1;
  while (lo < hi) {
    uint32_t mid = (lo + hi) / 2;
    int32_t begin = this->segments_[mid].get_dst_offset();
    int32_t end = begin + this->segments_[mid].get_size();
    if (index < begin) {
      hi = mid - 1;
    } else if (index >= end) {
      lo = mid + 1;
    } else {
      lo = hi = mid;
    }
  }
  auto &seg = this->segments_[lo];
  // offset within the segment
  int32_t seg_off = index - seg.get_dst_offset();
  // offset within the src
  int32_t src_off;
  if (seg.is_reversed()) {
    src_off = seg.get_src_offset() + seg.get_size() - seg_off - 1;
  } else {
    src_off = seg.get_src_offset() + seg_off;
  }

  const auto color_view = (*seg.get_src())[src_off];
#if defined(USE_LIGHT_PARTITION) && defined(USE_LIGHT_POWER_ESTIMATION)
  // A partition has no power limit of its own so it adopts the power brightness of the underlying segment.
  // PartitionColorAccessor calls this method to access one LED, applies color correction if needed, then
  // discards the view.  This method won't be called again until the next access thus the power brightness
  // that is set here is guaranteed to be valid for the entire lifetime of the view that is returned.
  const light::ESPColorCorrection *color_view_correction = color_view.get_color_correction_internal();  // NOLINT
  const_cast<light::ESPColorCorrection &>(this->correction_)
      .set_power_brightness(color_view_correction ? color_view_correction->get_power_brightness() : 255);
#endif
  return color_view;
}

}  // namespace esphome::partition
