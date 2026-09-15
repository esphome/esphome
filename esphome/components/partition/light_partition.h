#pragma once

#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/light/addressable_light.h"

namespace esphome::partition {

class AddressableSegment {
 public:
  AddressableSegment(light::LightState *src, size_t src_offset, size_t size, bool reversed)
      : src_(static_cast<light::AddressableLight *>(src->get_output())),
        src_offset_(src_offset),
        size_(size),
        reversed_(reversed) {}

  light::AddressableLight *get_src() const { return this->src_; }
  size_t get_src_offset() const { return this->src_offset_; }
  size_t get_size() const { return this->size_; }
  size_t get_dst_offset() const { return this->dst_offset_; }
  void set_dst_offset(size_t dst_offset) { this->dst_offset_ = dst_offset; }
  bool is_reversed() const { return this->reversed_; }

 protected:
  light::AddressableLight *src_;
  size_t src_offset_;
  size_t size_;
  size_t dst_offset_;
  bool reversed_;
};

class PartitionLightOutput final : public light::AddressableLight, protected light::ESPColorBuffer {
 public:
  explicit PartitionLightOutput(std::vector<AddressableSegment> segments)
      : ESPColorBuffer(set_segment_dst_offsets(&segments)), segments_(std::move(segments)) {}

  light::ESPColorBuffer &buffer() override { return *this; }
  light::LightTraits get_traits() override { return this->segments_[0].get_src()->get_traits(); }
  void write_state(light::LightState *state) override {
    for (auto seg : this->segments_) {
      seg.get_src()->schedule_show();
    }
    this->mark_shown_();
  }

 protected:
  bool is_all_black() const override {
    for (auto &seg : this->segments_) {
      if (!seg.get_src()->buffer().is_all_black()) {
        return false;
      }
    }
    return true;
  }

  void clear_effect_data() override {
    for (auto &seg : this->segments_) {
      seg.get_src()->buffer().clear_effect_data();
    }
  }

  light::ESPColorView get_color_view(size_t index) override {
    size_t lo = 0;
    size_t hi = this->segments_.size() - 1;
    while (lo < hi) {
      size_t mid = (lo + hi) / 2;
      size_t begin = this->segments_[mid].get_dst_offset();
      size_t end = begin + this->segments_[mid].get_size();
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
    size_t seg_off = index - seg.get_dst_offset();
    // offset within the src
    size_t src_off;
    if (seg.is_reversed()) {
      src_off = seg.get_src_offset() + seg.get_size() - seg_off - 1;
    } else {
      src_off = seg.get_src_offset() + seg_off;
    }

    auto view = seg.get_src()->buffer()[src_off];
    view.raw_set_color_correction(&this->correction_);
    return view;
  }

  std::vector<AddressableSegment> segments_;

 private:
  static int32_t set_segment_dst_offsets(std::vector<AddressableSegment> *segments) {
    int32_t off = 0;
    for (auto &seg : *segments) {
      seg.set_dst_offset(off);
      off += seg.get_size();
    }
    return off;
  }
};

}  // namespace esphome::partition
