#pragma once

#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/light/addressable_light.h"

namespace esphome::partition {

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

class PartitionLightOutput final : public light::AddressableLight {
 public:
  explicit PartitionLightOutput(std::vector<AddressableSegment> segments)
      : segments_(std::move(segments)), internal_accessor_(this) {
    int32_t off = 0;
    for (auto &seg : this->segments_) {
      seg.set_dst_offset(off);
      off += seg.get_size();
    }
    this->accessor_ = &this->internal_accessor_;
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
  class PartitionColorAccessor final : public light::ESPColorAccessor {
   public:
    PartitionColorAccessor(const PartitionLightOutput *parent) : parent_(parent) {}

    Color get(int32_t index) const override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      return this->parent_->get_color_correction_(color_view)->color_uncorrect(color_view.get_raw());
    }
    uint8_t get_channel(int32_t index, uint8_t channel) const override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      return this->parent_->get_color_correction_(color_view)
          ->color_uncorrect_channel(channel, color_view.get_channel_raw(channel));
    }
    Color get_raw(int32_t index) const override { return this->parent_->get_color_view_(index).get_raw(); }
    uint8_t get_channel_raw(int32_t index, uint8_t channel) const override {
      return this->parent_->get_color_view_(index).get_channel_raw(channel);
    }
    uint8_t get_effect_data(int32_t index) const override {
      return this->parent_->get_color_view_(index).get_effect_data();
    }

    void set(int32_t index, Color color) override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      color_view.set_raw(this->parent_->get_color_correction_(color_view)->color_correct(color));
    }
    void set_channel(int32_t index, uint8_t channel, uint8_t value) override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      color_view.set_channel_raw(
          channel, this->parent_->get_color_correction_(color_view)->color_correct_channel(channel, value));
    }
    void set_raw(int32_t index, Color raw_color) override { this->parent_->get_color_view_(index).set_raw(raw_color); }
    void set_channel_raw(int32_t index, uint8_t channel, uint8_t raw_value) override {
      this->parent_->get_color_view_(index).set_channel_raw(channel, raw_value);
    }
    void set_effect_data(int32_t index, uint8_t value) override {
      this->parent_->get_color_view_(index).set_effect_data(value);
    }

#ifdef USE_LIGHT_POWER_ESTIMATION
    const light::ESPColorCorrection *get_color_correction_internal(int32_t index) const override {  // NOLINT
      return this->parent_->get_color_correction_(this->parent_->get_color_view_(index));
    }
#endif

   protected:
    const PartitionLightOutput *const parent_;
  };

  light::ESPColorView get_color_view_(int32_t index) const;

  // Gets the color correction associated with the specified color view.
  // The correction will only be valid until the next call to this method.
  const light::ESPColorCorrection *get_color_correction_(const light::ESPColorView &color_view) const {
#ifdef USE_LIGHT_POWER_ESTIMATION
    // A partition has no power limit of its own so it adopts the power brightness of the underlying segment.
    // PartitionColorAccessor calls this method to access one LED, applies color correction if needed, then
    // discards the view.  This method won't be called again until the next access thus the power brightness
    // that is set here is guaranteed to be valid for the entire lifetime of the view that is returned.
    const light::ESPColorCorrection *color_view_correction = color_view.get_color_correction_internal();  // NOLINT
    const_cast<light::ESPColorCorrection &>(this->correction_)
        .set_power_brightness(color_view_correction ? color_view_correction->get_power_brightness() : 255);
#endif
    return &this->correction_;
  }

  std::vector<AddressableSegment> segments_;
  PartitionColorAccessor internal_accessor_;
};

}  // namespace esphome::partition
