#pragma once

#include "esphome/components/display/display.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"

namespace esphome::ht16k33_char {

class HT16K33CharDisplay;
using ht16k33_char_writer_t = display::DisplayWriter<HT16K33CharDisplay>;

/// One add_characters entry
struct Glyph {
  uint16_t codepoint;
  uint8_t segments;
};

class HT16K33CharDisplay final : public PollingComponent, public i2c::I2CDevice {
 public:
  /// message holds capacity bytes of UTF-8 text
  HT16K33CharDisplay(char *message, uint8_t capacity) : message_(message), capacity_(capacity) {}

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void set_writer(ht16k33_char_writer_t &&writer) { this->writer_ = std::move(writer); }
  /// 0 (dimmest) to 15, clamped; written at once after setup
  void set_intensity(uint8_t intensity);
#ifdef HT16K33_CHAR_SECONDARY_COUNT
  void add_secondary_display(uint8_t address) { this->secondary_addresses_.push_back(address); }
#endif
#ifdef USE_HT16K33_CHAR_GLYPH_OVERRIDES
  void set_glyph_overrides(const Glyph *glyphs, uint8_t count) {
    this->glyph_overrides_ = glyphs;
    this->glyph_override_count_ = count;
  }
#endif

  /// For printing from automations; the lambda starts from an empty message
  void clear() { this->length_ = 0; }
  /// Write str at byte offset pos, where a `.`, a `:` and each byte of a multi-byte character count as one.
  /// Returns the bytes written.
  uint8_t print(uint8_t pos, const char *str);
  uint8_t print(const char *str) { return this->print(0, str); }
  uint8_t printf(uint8_t pos, const char *format, ...) __attribute__((format(printf, 3, 4)));
  uint8_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));
  uint8_t strftime(uint8_t pos, const char *format, ESPTime time) __attribute__((format(strftime, 3, 0)));
  uint8_t strftime(const char *format, ESPTime time) __attribute__((format(strftime, 2, 0)));
  /// HH:MM, or H:MM without the leading zero; 12-hour when use_ampm
  uint8_t clock_display(uint8_t pos, bool show_leading_zero, bool use_ampm, ESPTime time);

  /// 0 steady, 1 at 2 Hz, 2 at 1 Hz, 3 at 0.5 Hz
  void set_blink(uint8_t blink);
  void display_off(bool turn_off);
  void display_standby(bool standby);
  /// Clear the display memory; the next update redraws the message
  void blank();

 protected:
  template<typename F> void for_each_chip_(F &&f) {
    f(this->address_);
#ifdef HT16K33_CHAR_SECONDARY_COUNT
    for (uint8_t address : this->secondary_addresses_)
      f(address);
#endif
  }
  bool write_all_(uint8_t command);
  uint8_t find_segments_(uint32_t codepoint) const;
  void draw_();

  char *const message_;
  const uint8_t capacity_;
  uint8_t length_{0};
  uint8_t intensity_{15};
  /// Display setup register: on bit and blink rate
  uint8_t display_setup_{0};
#ifdef USE_HT16K33_CHAR_GLYPH_OVERRIDES
  uint8_t glyph_override_count_{0};
#endif
  ht16k33_char_writer_t writer_{};
#ifdef HT16K33_CHAR_SECONDARY_COUNT
  StaticVector<uint8_t, HT16K33_CHAR_SECONDARY_COUNT> secondary_addresses_;
#endif
#ifdef USE_HT16K33_CHAR_GLYPH_OVERRIDES
  const Glyph *glyph_overrides_{nullptr};
#endif
};

}  // namespace esphome::ht16k33_char
