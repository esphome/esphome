#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/line_reader.h"
#include "nmea.h"

namespace esphome::gps {

class GPS;

class GPSListener {
 public:
  /// Called after each GGA or RMC sentence; data.updated has the fields that sentence set.
  virtual void on_update(const GPSData &data) = 0;
  const GPSData &get_data() const;
#ifdef USE_GPS_FIELD_AGE
  uint32_t get_age(GPSField field) const;
#endif

 protected:
  friend GPS;

  GPS *parent_;
};

class GPS final : public PollingComponent, public uart::UARTDevice {
  SUB_SENSOR(latitude)
  SUB_SENSOR(longitude)
  SUB_SENSOR(speed)
  SUB_SENSOR(course)
  SUB_SENSOR(altitude)
  SUB_SENSOR(satellites)
  SUB_SENSOR(hdop)

 public:
  SLOT_LIST_REQUIRES(GPS_LISTENER_COUNT)
  void register_listener(GPSListener *listener) {
    listener->parent_ = this;
    this->listeners_.push_back(listener);
  }
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void dump_config() override;
  void loop() override;
  void update() override;

  const GPSData &get_data() const { return this->data_; }
#ifdef USE_GPS_FIELD_AGE
  /// Milliseconds since a sentence last set the field (one bit), or UINT32_MAX if none has.
  uint32_t get_age(GPSField field) const;
#endif

 protected:
#ifdef USE_GPS_FIELD_AGE
  static constexpr uint32_t AGE_LIMIT_MS = 0x80000000u;
#endif
  void handle_line_(char *line, size_t len);

  GPSData data_;
#ifdef USE_GPS_FIELD_AGE
  uint32_t updated_ms_[GPS_FIELD_COUNT]{};  ///< millis() when a sentence last set each field
#endif
  LineReader<NMEA_MAX_LINE + 1> line_reader_;
  [[no_unique_address]] SlotList<GPSListener *, GPS_LISTENER_COUNT> listeners_;
};

}  // namespace esphome::gps
