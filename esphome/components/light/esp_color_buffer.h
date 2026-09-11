#pragma once

#include "channel_colors.h"
#include "esp_color_correction.h"
#include "esp_color_view.h"
#include "esp_range_view.h"
#include "esphome/core/helpers.h"

namespace esphome::light {

class ESPColorBuffer {
 public:
  ESPColorBuffer(size_t num_leds) : num_leds_(num_leds) {}

  /// Get the number of LEDs in the buffer.
  size_t size() const { return this->num_leds_; }

  /// Get a color view of the LED at an index between -(size - 1) and (size - 1) inclusively.
  /// Use a negative index to access LEDs from the end of the buffer.
  ESPColorView operator[](int32_t index) {
    return this->get_color_view(size_t(interpret_index(index, this->num_leds_)));
  }

  /// Get a color view of the LED at an index between -(size - 1) and (size - 1) inclusively.
  /// Use a negative index to access LEDs from the end of the buffer.
  ESPColorView get(int32_t index) { return (*this)[index]; }

  /// Get a range view over the LEDs at indices between -(size - 1) and (size - 1) inclusively.
  /// Use a negative index to access LEDs from the end of the buffer.
  ESPRangeView range(int32_t from, int32_t to) {
    from = interpret_index(from, this->num_leds_);
    to = interpret_index(to, this->num_leds_);
    return ESPRangeView(this, from, to);
  }

  /// Get a range view over all LEDs in the buffer.
  ESPRangeView all() { return ESPRangeView(this, 0, this->num_leds_); }

  ESPRangeIterator begin() { return this->all().begin(); }

  ESPRangeIterator end() { return this->all().end(); }

  /// Shift all LED color to the left by the specified number of positions.
  void shift_left(int32_t amount);

  /// Shift all LED colors to the right by the specified number of positions.
  void shift_right(int32_t amount) { this->shift_left(-amount); }

  /// Returns true if all LEDs have a zero raw color value.
  virtual bool is_all_black() const = 0;

  /// Set the light effect data associated with all LEDs in the buffer to zero.
  virtual void clear_effect_data() = 0;

 protected:
  virtual ESPColorView get_color_view(size_t index) = 0;

  inline static bool is_all_black_internal(const uint8_t *led_data, size_t num_leds,
                                           const ChannelColors &channel_colors, size_t bytes_per_led) {
    for (size_t i = 0; i < num_leds; i++, led_data += bytes_per_led) {
      if (led_data[channel_colors.r] != 0 || led_data[channel_colors.g] != 0 || led_data[channel_colors.b] != 0 ||
          (channel_colors.has_white() && led_data[channel_colors.w] != 0)) {
        return false;
      }
    }
    return true;
  }

  inline static void clear_effect_data_internal(uint8_t *effect_data, size_t num_leds) {
    memset(effect_data, 0, num_leds);
  }

  inline static ESPColorView get_color_view_internal(size_t index, uint8_t *led_data, uint8_t *effect_data,
                                                     const ChannelColors &channel_colors, size_t bytes_per_led,
                                                     const ESPColorCorrection *color_correction) {
    led_data += index * bytes_per_led;
    return ESPColorView{&led_data[channel_colors.r], &led_data[channel_colors.g],
                        &led_data[channel_colors.b], channel_colors.has_white() ? &led_data[channel_colors.w] : nullptr,
                        &effect_data[index],         color_correction};
  }

  const size_t num_leds_;
};

struct InterleavedColorBufferLayout {
  ChannelColors channel_colors{};
  uint8_t bytes_per_led{uint8_t(channel_colors.has_white() ? 4 : 3)};
  uint8_t leading_bytes{0};
  uint8_t trailing_bytes{0};
};

class InterleavedColorBuffer final : public ESPColorBuffer {
 public:
  InterleavedColorBuffer(size_t num_leds, InterleavedColorBufferLayout layout,
                         const ESPColorCorrection *color_correction)
      : ESPColorBuffer(num_leds), layout_(layout), color_correction_(color_correction) {}

  const InterleavedColorBufferLayout &layout() const { return this->layout_; }

  bool allocate_and_setup(RAMAllocator<uint8_t> *allocator);
  void setup(uint8_t *led_data, uint8_t *effect_data);

  bool is_all_black() const override;

  void clear_effect_data() override;

  uint8_t *get_led_data() { return this->led_data_after_leading_bytes_ - this->layout_.leading_bytes; }
  const uint8_t *get_led_data() const { return this->led_data_after_leading_bytes_ - this->layout_.leading_bytes; }

  size_t get_led_data_bytes() const {
    return this->layout_.leading_bytes + this->num_leds_ * this->layout_.bytes_per_led + this->layout_.trailing_bytes;
  }

 protected:
  ESPColorView get_color_view(size_t index) override;

  const InterleavedColorBufferLayout layout_;
  const ESPColorCorrection *const color_correction_;
  uint8_t *led_data_after_leading_bytes_{nullptr};
  uint8_t *effect_data_{nullptr};
};

}  // namespace esphome::light
