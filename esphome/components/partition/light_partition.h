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
    this->accessor_ = &internal_accessor_;
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
      return this->parent_->get_color_correction_(color_view)->color_uncorrect(this->get_raw(index));
    }
    Color get_raw(int32_t index) const override { return this->parent_->get_color_view_(index).get_raw(); }
    uint8_t get_red(int32_t index) const override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      return this->parent_->get_color_correction_(color_view)->color_uncorrect_red(this->get_raw(index).red);
    }
    uint8_t get_green(int32_t index) const override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      return this->parent_->get_color_correction_(color_view)->color_uncorrect_green(this->get_raw(index).green);
    }
    uint8_t get_blue(int32_t index) const override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      return this->parent_->get_color_correction_(color_view)->color_uncorrect_blue(this->get_raw(index).blue);
    }
    uint8_t get_white(int32_t index) const override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      return this->parent_->get_color_correction_(color_view)->color_uncorrect_white(this->get_raw(index).white);
    }
    uint8_t get_effect_data(int32_t index) const override {
      return this->parent_->get_color_view_(index).get_effect_data();
    }

    void set(int32_t index, Color color) override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      color_view.set_raw(this->parent_->get_color_correction_(color_view)->color_correct(color));
    }
    void set_raw(int32_t index, Color raw_color) override { this->parent_->get_color_view_(index).set_raw(raw_color); }
    void set_red(int32_t index, uint8_t red) override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      Color raw_color = color_view.get_raw();
      raw_color.red = this->parent_->get_color_correction_(color_view)->color_correct_red(red);
      color_view.set_raw(raw_color);
    }
    void set_green(int32_t index, uint8_t green) override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      Color raw_color = color_view.get_raw();
      raw_color.green = this->parent_->get_color_correction_(color_view)->color_correct_green(green);
      color_view.set_raw(raw_color);
    }
    void set_blue(int32_t index, uint8_t blue) override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      Color raw_color = color_view.get_raw();
      raw_color.blue = this->parent_->get_color_correction_(color_view)->color_correct_blue(blue);
      color_view.set_raw(raw_color);
    }
    void set_white(int32_t index, uint8_t white) override {
      light::ESPColorView color_view = this->parent_->get_color_view_(index);
      Color raw_color = color_view.get_raw();
      raw_color.white = this->parent_->get_color_correction_(color_view)->color_correct_white(white);
      color_view.set_raw(raw_color);
    }
    void set_effect_data(int32_t index, uint8_t value) override {
      this->parent_->get_color_view_(index).set_effect_data(value);
    }

#if defined(USE_LIGHT_PARTITION) && defined(USE_LIGHT_POWER_ESTIMATION)
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
#if defined(USE_LIGHT_PARTITION) && defined(USE_LIGHT_POWER_ESTIMATION)
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
