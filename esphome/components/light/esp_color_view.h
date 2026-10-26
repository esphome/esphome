#pragma once

#include <utility>

#include "esphome/core/color.h"
#include "esp_hsv_color.h"
#include "esp_color_correction.h"
#include "channel_colors.h"

namespace esphome::light {

/// Abstract interface for accessing color and effect data for an addressable light by index.
///
/// This interface does not guarantee bit-for-bit round-trip fidelity for setting and getting color and effect data
/// within the underlying buffer.  Implementations may transform color data on-the-fly such as to apply brightness
/// scaling, color correction, and quantization.  They may also discard unused color channels and effect data
/// altogether and the corresponding getters will return zero.
///
/// Implementations must ensure that color transformations are idempotent.  This means that setting a value at
/// a particular index then getting it back might return a different value from what was originally set (because
/// it was transformed), and setting the new value again at the same index then getting it back again will return
/// the same new value.
///
/// Raw colors are mainly used internally for power supply control, power estimation, and unit tests and the per-channel
/// accessors that do not require per-channel accessors so they are intentionally omitted to reduce the size of the
/// interface.
class ESPColorAccessor {
 public:
  virtual Color get(int32_t index) const = 0;
  virtual Color get_raw(int32_t index) const = 0;
  virtual uint8_t get_red(int32_t index) const = 0;
  virtual uint8_t get_green(int32_t index) const = 0;
  virtual uint8_t get_blue(int32_t index) const = 0;
  virtual uint8_t get_white(int32_t index) const = 0;
  virtual uint8_t get_effect_data(int32_t index) const = 0;

  virtual void set(int32_t index, Color color) = 0;
  virtual void set_raw(int32_t index, Color raw_color) = 0;
  virtual void set_red(int32_t index, uint8_t red) = 0;
  virtual void set_green(int32_t index, uint8_t green) = 0;
  virtual void set_blue(int32_t index, uint8_t blue) = 0;
  virtual void set_white(int32_t index, uint8_t white) = 0;
  virtual void set_effect_data(int32_t index, uint8_t value) = 0;

#if defined(USE_LIGHT_PARTITION) && defined(USE_LIGHT_POWER_ESTIMATION)
  virtual const ESPColorCorrection *get_color_correction_internal(int32_t index) const = 0;
#endif
};

/// Base class for color accessors backed by a buffer whose layout is described by ChannelColors.
class BufferColorAccessor : public ESPColorAccessor {
 public:
  BufferColorAccessor(uint8_t *color_data, uint8_t *effect_data, ChannelColors channel_colors)
      : color_data_(color_data), effect_data_(effect_data), channel_colors_(channel_colors) {}

  Color get_raw(int32_t index) const override { return this->channel_colors_.get(this->color_data_, index); }
  uint8_t get_effect_data(int32_t index) const override { return this->effect_data_[index]; }

  void set_raw(int32_t index, Color raw_color) override {
    this->channel_colors_.set(this->color_data_, index, raw_color);
  }
  void set_effect_data(int32_t index, uint8_t value) override { this->effect_data_[index] = value; }

  uint8_t *swap_color_data_buffers(uint8_t *color_data) {
    std::swap(this->color_data_, color_data);
    return color_data;
  }

 protected:
  inline uint8_t get_red_raw_(int32_t index) const { return this->channel_colors_.get_red(this->color_data_, index); }
  inline uint8_t get_green_raw_(int32_t index) const {
    return this->channel_colors_.get_green(this->color_data_, index);
  }
  inline uint8_t get_blue_raw_(int32_t index) const { return this->channel_colors_.get_blue(this->color_data_, index); }
  inline uint8_t get_white_raw_(int32_t index) const {
    return this->channel_colors_.get_white(this->color_data_, index);
  }

  inline void set_red_raw_(int32_t index, uint8_t red) { this->channel_colors_.set_red(this->color_data_, index, red); }
  inline void set_green_raw_(int32_t index, uint8_t green) {
    this->channel_colors_.set_green(this->color_data_, index, green);
  }
  inline void set_blue_raw_(int32_t index, uint8_t blue) {
    this->channel_colors_.set_blue(this->color_data_, index, blue);
  }
  inline void set_white_raw_(int32_t index, uint8_t white) {
    this->channel_colors_.set_white(this->color_data_, index, white);
  }

  uint8_t *color_data_;
  uint8_t *const effect_data_;
  const ChannelColors channel_colors_;
};

/// Color accessor backed by a buffer whose contents have no color correction applied.
class RawBufferColorAccessor final : public BufferColorAccessor {
 public:
  RawBufferColorAccessor(uint8_t *color_data, uint8_t *effect_data, ChannelColors channel_colors)
      : BufferColorAccessor(color_data, effect_data, channel_colors) {}

  Color get(int32_t index) const override { return this->get_raw(index); }
  uint8_t get_red(int32_t index) const override { return this->get_red_raw_(index); }
  uint8_t get_green(int32_t index) const override { return this->get_green_raw_(index); }
  uint8_t get_blue(int32_t index) const override { return this->get_blue_raw_(index); }
  uint8_t get_white(int32_t index) const override { return this->get_white_raw_(index); }

  void set(int32_t index, Color color) override { this->set_raw(index, color); }
  void set_red(int32_t index, uint8_t red) override { this->set_red_raw_(index, red); }
  void set_green(int32_t index, uint8_t green) override { this->set_green_raw_(index, green); }
  void set_blue(int32_t index, uint8_t blue) override { this->set_blue_raw_(index, blue); }
  void set_white(int32_t index, uint8_t white) override { this->set_white_raw_(index, white); }

#if defined(USE_LIGHT_PARTITION) && defined(USE_LIGHT_POWER_ESTIMATION)
  const ESPColorCorrection *get_color_correction_internal(int32_t index) const { return nullptr; }
#endif
};

/// Color accessor backed by a buffer whose contents have color correction applied.
class CorrectedBufferColorAccessor final : public BufferColorAccessor {
 public:
  CorrectedBufferColorAccessor(uint8_t *color_data, uint8_t *effect_data, ChannelColors channel_colors,
                               const ESPColorCorrection *color_correction)
      : BufferColorAccessor(color_data, effect_data, channel_colors), color_correction_(color_correction) {}

  Color get(int32_t index) const override { return this->color_correction_->color_uncorrect(this->get_raw(index)); }
  uint8_t get_red(int32_t index) const override {
    return this->color_correction_->color_uncorrect_red(this->get_red_raw_(index));
  }
  uint8_t get_green(int32_t index) const override {
    return this->color_correction_->color_uncorrect_green(this->get_green_raw_(index));
  }
  uint8_t get_blue(int32_t index) const override {
    return this->color_correction_->color_uncorrect_blue(this->get_blue_raw_(index));
  }
  uint8_t get_white(int32_t index) const override {
    return this->color_correction_->color_uncorrect_white(this->get_white_raw_(index));
  }

  void set(int32_t index, Color color) override { this->set_raw(index, this->color_correction_->color_correct(color)); }
  void set_red(int32_t index, uint8_t red) override {
    this->set_red_raw_(index, this->color_correction_->color_correct_red(red));
  }
  void set_green(int32_t index, uint8_t green) override {
    this->set_green_raw_(index, this->color_correction_->color_correct_green(green));
  }
  void set_blue(int32_t index, uint8_t blue) override {
    this->set_blue_raw_(index, this->color_correction_->color_correct_blue(blue));
  }
  void set_white(int32_t index, uint8_t white) override {
    this->set_white_raw_(index, this->color_correction_->color_correct_white(white));
  }

#if defined(USE_LIGHT_PARTITION) && defined(USE_LIGHT_POWER_ESTIMATION)
  const ESPColorCorrection *get_color_correction_internal(int32_t index) const { return color_correction_; }
#endif

 protected:
  const ESPColorCorrection *const color_correction_;
};

/// Color accessor that is completely at peace with the void of nothingness.
class NullColorAccessor final : public ESPColorAccessor {
 public:
  /// Singleton instance of this accessor.
  static NullColorAccessor instance;

  Color get(int32_t index) const override { return {}; }
  Color get_raw(int32_t index) const override { return {}; }
  uint8_t get_red(int32_t index) const override { return 0; }
  uint8_t get_green(int32_t index) const override { return 0; }
  uint8_t get_blue(int32_t index) const override { return 0; }
  uint8_t get_white(int32_t index) const override { return 0; }
  uint8_t get_effect_data(int32_t index) const override { return 0; }

  void set(int32_t index, Color color) override {}
  void set_raw(int32_t index, Color raw_color) override {}
  void set_red(int32_t index, uint8_t red) override {}
  void set_green(int32_t index, uint8_t green) override {}
  void set_blue(int32_t index, uint8_t blue) override {}
  void set_white(int32_t index, uint8_t white) override {}
  void set_effect_data(int32_t index, uint8_t value) override {}

#if defined(USE_LIGHT_PARTITION) && defined(USE_LIGHT_POWER_ESTIMATION)
  const ESPColorCorrection *get_color_correction_internal(int32_t index) const { return nullptr; }
#endif
};

/// Abstract interface that supports setting colors and effect data.
/// May set zero, one, or more pixels according to the implementation.
class ESPColorSettable {
 public:
  virtual void set(Color color) = 0;
  virtual void set_red(uint8_t red) = 0;
  virtual void set_green(uint8_t green) = 0;
  virtual void set_blue(uint8_t blue) = 0;
  virtual void set_white(uint8_t white) = 0;
  virtual void set_effect_data(uint8_t value) = 0;

  virtual void fade_to_white(uint8_t amnt) = 0;
  virtual void fade_to_black(uint8_t amnt) = 0;
  virtual void lighten(uint8_t delta) = 0;
  virtual void darken(uint8_t delta) = 0;

  void set(ESPHSVColor hsv_color) { this->set(hsv_color.to_rgb()); }
  void set_hsv(ESPHSVColor hsv_color) { this->set(hsv_color.to_rgb()); }
  void set_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    // This method should perhaps be deprecated because it's much slower than `set(Color)`
    // in the common case where the intent is to set all color channels of the pixel at once
    // and there just happens to not be any white.
    this->set_red(red);
    this->set_green(green);
    this->set_blue(blue);
  }
  void set_rgbw(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) { this->set(Color(red, green, blue, white)); }
};

/// Gets or sets the color or effect data at a specific index within an color accessor.
///
/// To improve performance, prefer calling `get()` and `set(Color)` when you intend to access all color
/// channel at once instead of calling the getters and setters for each color channel individually.
class ESPColorView final : public ESPColorSettable {
 public:
  ESPColorView(ESPColorAccessor *accessor, int32_t index) : accessor_(accessor), index_(index) {}
  ESPColorView(const ESPColorView &) = default;

  inline ESPColorView &operator=(Color color) {
    this->set(color);
    return *this;
  }
  inline ESPColorView &operator=(ESPHSVColor hsv_color) {
    this->set_hsv(hsv_color);
    return *this;
  }
  ESPColorView &operator=(const ESPColorView &other) {
    const Color color = other.get();
    this->set(color);
    return *this;
  }

  using ESPColorSettable::set;  // ESPHSVColor overload
  void set(Color color) override { this->accessor_->set(this->index_, color); }
  void set_raw(Color raw_color) { this->accessor_->set_raw(this->index_, raw_color); }
  void set_red(uint8_t red) override { this->accessor_->set_red(this->index_, red); }
  void set_green(uint8_t green) override { this->accessor_->set_green(this->index_, green); }
  void set_blue(uint8_t blue) override { this->accessor_->set_blue(this->index_, blue); }
  void set_white(uint8_t white) override { this->accessor_->set_white(this->index_, white); }
  void set_effect_data(uint8_t value) override { this->accessor_->set_effect_data(this->index_, value); }

  void fade_to_white(uint8_t amnt) override { this->set(this->get().fade_to_white(amnt)); }
  void fade_to_black(uint8_t amnt) override { this->set(this->get().fade_to_black(amnt)); }
  void lighten(uint8_t delta) override { this->set(this->get().lighten(delta)); }
  void darken(uint8_t delta) override { this->set(this->get().darken(delta)); }

  Color get() const { return this->accessor_->get(this->index_); }
  Color get_raw() const { return this->accessor_->get_raw(this->index_); }
  uint8_t get_red() const { return this->accessor_->get_red(this->index_); }
  uint8_t get_green() const { return this->accessor_->get_green(this->index_); }
  uint8_t get_blue() const { return this->accessor_->get_blue(this->index_); }
  uint8_t get_white() const { return this->accessor_->get_white(this->index_); }
  uint8_t get_effect_data() const { return this->accessor_->get_effect_data(this->index_); }

  ESPDEPRECATED("Use get_raw().red instead. Removed in 2027.5.0", "2025.11.0")
  uint8_t get_red_raw() const { return this->get_raw().red; }
  ESPDEPRECATED("Use get_raw().green instead. Removed in 2027.5.0", "2025.11.0")
  uint8_t get_green_raw() const { return this->get_raw().green; }
  ESPDEPRECATED("Use get_raw().blue instead. Removed in 2027.5.0", "2025.11.0")
  uint8_t get_blue_raw() const { return this->get_raw().blue; }
  ESPDEPRECATED("Use get_raw().white instead. Removed in 2027.5.0", "2025.11.0")
  uint8_t get_white_raw() const { return this->get_raw().white; }

#if defined(USE_LIGHT_PARTITION) && defined(USE_LIGHT_POWER_ESTIMATION)
  // This method is for internal use only.
  // Get the color correction applied by the underlying accessor or nullptr if there is none.
  const ESPColorCorrection *get_color_correction_internal() const {
    return accessor_->get_color_correction_internal(this->index_);
  }
#endif

 protected:
  ESPColorAccessor *const accessor_;
  const int32_t index_;
};

}  // namespace esphome::light
