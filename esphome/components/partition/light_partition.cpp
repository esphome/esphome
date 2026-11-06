#include "light_partition.h"
#include "esphome/core/log.h"

namespace esphome::partition {

ESPHOME_LOG_TAG(TAG, "partition.light");

const AddressableSegment &PartitionLightOutput::get_segment_(int32_t index) const {
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
  return this->segments_[lo];
}

light::ESPColorView PartitionLightOutput::get_color_view_(const AddressableSegment &seg, int32_t index) const {
  // offset within the segment
  int32_t seg_off = index - seg.get_dst_offset();
  // offset within the src
  int32_t src_off;
  if (seg.is_reversed()) {
    src_off = seg.get_src_offset() + seg.get_size() - seg_off - 1;
  } else {
    src_off = seg.get_src_offset() + seg_off;
  }
  return (*seg.get_src())[src_off];
}

}  // namespace esphome::partition
