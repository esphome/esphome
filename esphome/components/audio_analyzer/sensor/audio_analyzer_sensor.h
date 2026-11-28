#pragma once

#include "esphome/components/audio_analyzer/audio_analyzer.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <array>
#include <cmath>

namespace esphome::audio_analyzer {

/// Publishes the highest RMS and band values (dBFS) of the frames since the previous update, so short sounds show at
/// a low rate. Publishes unknown once when the analyzer goes idle.
class AudioAnalyzerSensor final : public PollingComponent {
  SUB_SENSOR(rms)

 public:
  explicit AudioAnalyzerSensor(AudioAnalyzer *parent) : parent_(parent) { this->band_db_hold_.fill(NAN); }

  void dump_config() override;
  void setup() override;
  void update() override;

  void set_band_sensor(uint8_t band, sensor::Sensor *sensor) { this->band_sensors_[band] = sensor; }

 protected:
  void publish_(float rms_db, const float *band_db);

  AudioAnalyzer *parent_;
  std::array<sensor::Sensor *, NUM_BANDS> band_sensors_{};
  // Highest values since the last update; NAN until a frame is analyzed
  std::array<float, NUM_BANDS> band_db_hold_{};
  float rms_db_hold_{NAN};
  bool published_unknown_{false};
};

}  // namespace esphome::audio_analyzer
