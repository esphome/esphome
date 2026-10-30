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
  using EPaperUltraChip::EPaperUltraChip;

  void dump_config() override;
#ifdef USE_SENSOR
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
#endif

 protected:
  bool needs_previous_frame() const override { return true; }
  bool reset() override;
  bool initialise(bool partial) override;
  bool transfer_data() override;
  void power_on() override;
  void refresh_screen(bool partial) override;
  void power_off() override;
  void deep_sleep() override { this->send_deep_sleep_(); }

  // The stream for row y, where mono is that row from mono_row()
  void stream_row_(size_t y, const uint8_t *mono, uint8_t *out);
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
  uint8_t full_waveform(int temperature) const override;
};

}  // namespace esphome::epaper_spi
