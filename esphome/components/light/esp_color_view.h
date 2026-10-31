#pragma once

#include <cstring>
#include <utility>

#include "esphome/core/color.h"
#include "esphome/core/helpers.h"
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
/// Raw colors are mainly used internally for power supply control, power estimation, and unit tests.
class ESPColorAccessor {
 public:
  virtual Color get(int32_t index) const = 0;
  virtual Color get_raw(int32_t index) const = 0;
  virtual uint8_t get_channel(int32_t index, uint8_t channel) const = 0;
  virtual uint8_t get_channel_raw(int32_t index, uint8_t channel) const = 0;
  virtual uint8_t get_effect_data(int32_t index) const = 0;

  virtual void set(int32_t index, Color color) = 0;
  virtual void set_raw(int32_t index, Color raw_color) = 0;
  virtual void set_channel(int32_t index, uint8_t channel, uint8_t value) = 0;
  virtual void set_channel_raw(int32_t index, uint8_t channel, uint8_t raw_value) = 0;
  virtual void set_effect_data(int32_t index, uint8_t value) = 0;

#if defined(USE_LIGHT_PARTITION) && defined(USE_LIGHT_POWER_ESTIMATION)
  virtual const ESPColorCorrection *get_color_correction_internal(int32_t index) const { return nullptr; }
#endif
};

/// Color accessor that is completely at peace with the void of nothingness.
class NullColorAccessor final : public ESPColorAccessor {
 public:
  /// Singleton instance of this accessor.
  static NullColorAccessor instance;

  Color get(int32_t index) const override { return {}; }
  Color get_raw(int32_t index) const override { return {}; }
  uint8_t get_channel(int32_t index, uint8_t channel) const override { return 0; }
  uint8_t get_channel_raw(int32_t index, uint8_t channel) const override { return 0; }
  uint8_t get_effect_data(int32_t index) const override { return 0; }

  void set(int32_t index, Color color) override {}
  void set_raw(int32_t index, Color raw_color) override {}
  void set_channel(int32_t index, uint8_t channel, uint8_t value) override {}
  void set_channel_raw(int32_t index, uint8_t channel, uint8_t raw_value) override {}
  void set_effect_data(int32_t index, uint8_t value) override {}
};

/// Base class for color accessors backed by a buffer whose layout is described by ChannelColors.
class BufferColorAccessor : public ESPColorAccessor {
 public:
  BufferColorAccessor() = default;
  BufferColorAccessor(uint8_t *color_data, uint8_t *effect_data, ChannelColors channel_colors)
      : color_data(color_data), effect_data(effect_data), channel_colors(channel_colors) {}

  Color get_raw(int32_t index) const override { return this->channel_colors.get(this->color_data, index); }
  uint8_t get_channel_raw(int32_t index, uint8_t channel) const override {
    return this->channel_colors.get_channel(this->color_data, index, channel);
  }
  uint8_t get_effect_data(int32_t index) const override { return this->effect_data[index]; }

  void set_raw(int32_t index, Color raw_color) override {
    this->channel_colors.set(this->color_data, index, raw_color);
  }
  void set_channel_raw(int32_t index, uint8_t channel, uint8_t raw_value) override {
    this->channel_colors.set_channel(this->color_data, index, channel, raw_value);
  }
  void set_effect_data(int32_t index, uint8_t value) override { this->effect_data[index] = value; }

  /// Allocate the color and effect data for `num_leds` LEDs as one zeroed block, followed by `extra_bytes` more
  /// for the caller, which start at `effect_data + num_leds`. Returns false if it failed.
  bool allocate(RAMAllocator<uint8_t> &allocator, size_t num_leds, size_t extra_bytes = 0) {
    const size_t color_bytes = num_leds * this->channel_colors.bytes_per_led();
    const size_t total_bytes = color_bytes + num_leds + extra_bytes;
    uint8_t *buf = allocator.allocate(total_bytes);
    if (buf == nullptr)
      return false;
    memset(buf, 0, total_bytes);
    this->color_data = buf;
    this->effect_data = buf + color_bytes;
    return true;
  }

  /// Zero the effect data of the first `num_leds` LEDs; does nothing before the effect data is allocated.
  void clear_effect_data(size_t num_leds) {
    if (this->effect_data != nullptr)
      memset(this->effect_data, 0, num_leds);
  }

  // These fields must all be initialized to valid (non-null) values prior to using the accessor.
  uint8_t *color_data{nullptr};
  uint8_t *effect_data{nullptr};
  ChannelColors channel_colors{};
};

/// Color accessor backed by a buffer whose contents have no color correction applied.
class RawBufferColorAccessor final : public BufferColorAccessor {
 public:
  RawBufferColorAccessor() = default;
  RawBufferColorAccessor(uint8_t *color_data, uint8_t *effect_data, ChannelColors channel_colors)
      : BufferColorAccessor(color_data, effect_data, channel_colors) {}

  Color get(int32_t index) const override { return this->get_raw(index); }
  uint8_t get_channel(int32_t index, uint8_t channel) const override { return this->get_channel_raw(index, channel); }

  void set(int32_t index, Color color) override { this->set_raw(index, color); }
  void set_channel(int32_t index, uint8_t channel, uint8_t raw_value) override {
    this->set_channel_raw(index, channel, raw_value);
  }
};

/// Color accessor backed by a buffer whose contents have color correction applied.
class CorrectedBufferColorAccessor final : public BufferColorAccessor {
 public:
  CorrectedBufferColorAccessor() = default;
  CorrectedBufferColorAccessor(uint8_t *color_data, uint8_t *effect_data, ChannelColors channel_colors,
                               const ESPColorCorrection *color_correction)
      : BufferColorAccessor(color_data, effect_data, channel_colors), color_correction(color_correction) {}

  Color get(int32_t index) const override {
    const Color raw_color = this->get_raw(index);
    const ESPColorCorrection *correction = this->color_correction;
    // Skip white when the strip has none; uncorrecting its zero is wasted work on every read
    return Color(correction->color_uncorrect_red(raw_color.red), correction->color_uncorrect_green(raw_color.green),
                 correction->color_uncorrect_blue(raw_color.blue),
                 this->channel_colors.has_white() ? correction->color_uncorrect_white(raw_color.white) : 0);
  }
  uint8_t get_channel(int32_t index, uint8_t channel) const override {
    return this->color_correction->color_uncorrect_channel(channel, this->get_channel_raw(index, channel));
  }

  void set(int32_t index, Color color) override { this->set_raw(index, this->color_correction->color_correct(color)); }
  void set_channel(int32_t index, uint8_t channel, uint8_t value) override {
    this->set_channel_raw(index, channel, this->color_correction->color_correct_channel(channel, value));
  }

#if defined(USE_LIGHT_PARTITION) && defined(USE_LIGHT_POWER_ESTIMATION)
  const ESPColorCorrection *get_color_correction_internal(int32_t index) const override {
    return this->color_correction;
  }
#endif

  // Must be initialized to a valid non-null value prior to using the accessor.
  const ESPColorCorrection *color_correction{nullptr};
};

/// Abstract interface that supports setting colors and effect data.
/// May set zero, one, or more pixels according to the implementation.
class ESPColorSettable {
 public:
  virtual void set(Color color) = 0;
  virtual void set_channel(uint8_t channel, uint8_t value) = 0;
  virtual void set_effect_data(uint8_t value) = 0;

  virtual void fade_to_white(uint8_t amnt) = 0;
  virtual void fade_to_black(uint8_t amnt) = 0;
  virtual void lighten(uint8_t delta) = 0;
  virtual void darken(uint8_t delta) = 0;

  inline void set_red(uint8_t red) ESPHOME_ALWAYS_INLINE { this->set_channel(COLOR_CHANNEL_R, red); }
  inline void set_green(uint8_t green) ESPHOME_ALWAYS_INLINE { this->set_channel(COLOR_CHANNEL_G, green); }
  inline void set_blue(uint8_t blue) ESPHOME_ALWAYS_INLINE { this->set_channel(COLOR_CHANNEL_B, blue); }
  inline void set_white(uint8_t white) ESPHOME_ALWAYS_INLINE { this->set_channel(COLOR_CHANNEL_W, white); }

  void set_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    // This method should perhaps be deprecated because it's slower than `set(Color)`
    // in the common case where the intent is to set all color channels of the pixel at once
    // and there just happens to not be any white.
    this->set_red(red);
    this->set_green(green);
    this->set_blue(blue);
  }
  void set_rgbw(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) { this->set(Color(red, green, blue, white)); }

  ESPDEPRECATED("Use set_hsv(hsv_color) instead. Removed in 2027.5.0.", "2026.11.0")
  void set(ESPHSVColor hsv_color) { this->set(hsv_color.to_rgb()); }
  void set_hsv(ESPHSVColor hsv_color) { this->set(hsv_color.to_rgb()); }
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
  ESPColorView &operator=(const ESPColorView &other) {  // NOLINT
    const Color color = other.get();
    this->set(color);
    return *this;
  }

  void set(Color color) override { this->accessor_->set(this->index_, color); }
  void set_channel(uint8_t channel, uint8_t value) override {
    this->accessor_->set_channel(this->index_, channel, value);
  }

  void set_raw(Color raw_color) { this->accessor_->set_raw(this->index_, raw_color); }
  void set_channel_raw(uint8_t channel, uint8_t raw_value) {
    this->accessor_->set_channel_raw(this->index_, channel, raw_value);
  }

  inline void set_red_raw(uint8_t raw_red) ESPHOME_ALWAYS_INLINE { this->set_channel_raw(COLOR_CHANNEL_R, raw_red); }
  inline void set_green_raw(uint8_t raw_green) ESPHOME_ALWAYS_INLINE {
    this->set_channel_raw(COLOR_CHANNEL_G, raw_green);
  }
  inline void set_blue_raw(uint8_t raw_blue) ESPHOME_ALWAYS_INLINE { this->set_channel_raw(COLOR_CHANNEL_B, raw_blue); }
  inline void set_white_raw(uint8_t raw_white) ESPHOME_ALWAYS_INLINE {
    this->set_channel_raw(COLOR_CHANNEL_W, raw_white);
  }

  void set_effect_data(uint8_t value) override { this->accessor_->set_effect_data(this->index_, value); }

  void fade_to_white(uint8_t amnt) override { this->set(this->get().fade_to_white(amnt)); }
  void fade_to_black(uint8_t amnt) override { this->set(this->get().fade_to_black(amnt)); }
  void lighten(uint8_t delta) override { this->set(this->get().lighten(delta)); }
  void darken(uint8_t delta) override { this->set(this->get().darken(delta)); }

  Color get() const { return this->accessor_->get(this->index_); }
  uint8_t get_channel(uint8_t channel) const { return this->accessor_->get_channel(this->index_, channel); }

  inline uint8_t get_red() const ESPHOME_ALWAYS_INLINE { return this->get_channel(COLOR_CHANNEL_R); }
  inline uint8_t get_green() const ESPHOME_ALWAYS_INLINE { return this->get_channel(COLOR_CHANNEL_G); }
  inline uint8_t get_blue() const ESPHOME_ALWAYS_INLINE { return this->get_channel(COLOR_CHANNEL_B); }
  inline uint8_t get_white() const ESPHOME_ALWAYS_INLINE { return this->get_channel(COLOR_CHANNEL_W); }

  Color get_raw() const { return this->accessor_->get_raw(this->index_); }
  uint8_t get_channel_raw(uint8_t channel) const { return this->accessor_->get_channel_raw(this->index_, channel); }

  inline uint8_t get_red_raw() const ESPHOME_ALWAYS_INLINE { return this->get_channel_raw(COLOR_CHANNEL_R); }
  inline uint8_t get_green_raw() const ESPHOME_ALWAYS_INLINE { return this->get_channel_raw(COLOR_CHANNEL_G); }
  inline uint8_t get_blue_raw() const ESPHOME_ALWAYS_INLINE { return this->get_channel_raw(COLOR_CHANNEL_B); }
  inline uint8_t get_white_raw() const ESPHOME_ALWAYS_INLINE { return this->get_channel_raw(COLOR_CHANNEL_W); }

  uint8_t get_effect_data() const { return this->accessor_->get_effect_data(this->index_); }

#if defined(USE_LIGHT_PARTITION) && defined(USE_LIGHT_POWER_ESTIMATION)
  // This method is for internal use only.
  // Get the color correction applied by the underlying accessor or nullptr if there is none.
  const ESPColorCorrection *get_color_correction_internal() const {
    return this->accessor_->get_color_correction_internal(this->index_);
  }
#endif

 protected:
  ESPColorAccessor *const accessor_;
  const int32_t index_;
};

}  // namespace esphome::light
