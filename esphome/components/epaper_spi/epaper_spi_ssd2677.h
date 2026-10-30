#pragma once

#include "epaper_spi_gray4.h"
#include "epaper_spi_ultrachip.h"

#include "esphome/core/defines.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome::epaper_spi {

/**
 * SSD2677 controller, used by the second version of the Seeed reTerminal Sticky.
 *
 * Despite its name it has an UltraChip style command set, not the SSD1677 one. Each refresh takes one
 * stream of 2 bits per pixel through DTM1 and always covers the whole panel. For a partial refresh each pair is
 * (old << 1 | new); for a full refresh it is the gray level, with 0 black and 3 white. Nothing has to survive in
 * controller RAM, so the panel is reset before and put in deep sleep after every update.
 *
 * The waveform is chosen by forcing a temperature value, which selects a table in the panel's OTP. The value comes
 * from an optional sensor; without a reading the driver assumes 25 C.
 */
class EPaperSSD2677 : public EPaperUltraChip {
 public:
  EPaperSSD2677(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                size_t init_sequence_length, DisplayType display_type = DISPLAY_TYPE_BINARY, uint8_t bits_per_pixel = 1)
      : EPaperUltraChip(name, width, height, init_sequence, init_sequence_length, display_type, bits_per_pixel) {}

  void dump_config() override;
#ifdef USE_SENSOR
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
#endif

 protected:
  bool needs_previous_frame() const override { return true; }
  size_t sent_frame_length() const override { return this->plane_row_length_() * this->height_; }
  bool reset() override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  void power_off() override;
  void deep_sleep() override { this->send_deep_sleep_(); }

  // A partial update needs a whole previous frame to compare against
  bool is_partial_() const { return this->partial_ && this->sent_valid_; }
  // The stream for row y, where plane is that row from plane_row()
  virtual void stream_row(size_t y, const uint8_t *plane, uint8_t *out);
  // The reading in whole degrees C, or the default for a missing reading (NAN)
  static int whole_degrees_(float reading);
  float temperature_reading_() const;
  // The forced temperature value that selects the full refresh waveform at this temperature
  virtual uint8_t full_waveform(int temperature) const;

#ifdef USE_SENSOR
  sensor::Sensor *temperature_sensor_{nullptr};
#endif
  bool init_sequence_sent_{false};
};

/**
 * Four gray levels. The buffer holds 2 bits per pixel, which a full refresh sends as it is. A partial
 * update is black and white, each pixel reduced to its high bit, and the panel loses its gray levels
 * until the next full update.
 */
class EPaperSSD2677Gray4 : public EPaperGray4<EPaperSSD2677> {
 public:
  using EPaperGray4::EPaperGray4;

 protected:
  void stream_row(size_t y, const uint8_t *plane, uint8_t *out) override;
  uint8_t full_waveform(int temperature) const override;
};

}  // namespace esphome::epaper_spi
