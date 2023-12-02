#pragma once

#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/light/addressable_light.h"

namespace esphome {
namespace partition {

class AddressableSegment {
 public:
  AddressableSegment(light::LightState *src, int32_t src_offset, int32_t size, bool reversed)
      : src_(static_cast<light::AddressableLight *>(src->get_output())),
        src_offset_(src_offset),
        size_(size),
        reversed_(reversed) {}

  light::AddressableLight *get_src() const { return this->src_; }
  int32_t get_src_offset() const { return this->src_offset_; }
  int32_t get_size() const { return this->size_; }
  int32_t get_dst_offset() const { return this->dst_offset_; }
  void set_dst_offset(int32_t dst_offset) { this->dst_offset_ = dst_offset; }
  bool is_reversed() const { return this->reversed_; }

 protected:
  light::AddressableLight *src_;
  int32_t src_offset_;
  int32_t size_;
  int32_t dst_offset_;
  bool reversed_;
};

class PartitionLightOutput : public light::AddressableLight {
 public:
  explicit PartitionLightOutput(std::vector<AddressableSegment> segments) : segments_(std::move(segments)) {
    int32_t off = 0;
    for (auto &seg : this->segments_) {
      seg.set_dst_offset(off);
      off += seg.get_size();
    }
  }
  int32_t size() const override {
    auto &last_seg = this->segments_[this->segments_.size() - 1];
    return last_seg.get_dst_offset() + last_seg.get_size();
  }
  void clear_effect_data() override {
    for (auto &seg : this->segments_) {
      seg.get_src()->clear_effect_data();
    }
  }
  light::LightTraits get_traits() override { return this->segments_[0].get_src()->get_traits(); }
  void write_state(light::LightState *state) override {
    for (auto seg : this->segments_) {
      seg.get_src()->schedule_show();
    }
    this->mark_shown_();
  }

 protected:
  light::ESPColorView get_view_internal(int32_t index) const override {
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

    auto view = (*seg.get_src())[src_off];
    view.raw_set_color_correction(&this->correction_);
    return view;
  }

  std::vector<AddressableSegment> segments_;
};

}  // namespace partition
}  // namespace esphome
