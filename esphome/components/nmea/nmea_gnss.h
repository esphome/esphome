#pragma once

#include "esphome/components/gnss/gnss.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/line_reader.h"
#include "nmea_parser.h"

namespace esphome::nmea {

/// A receiver that sends NMEA 0183 sentences.
class NMEAGNSS : public gnss::GNSSComponent, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void dump_config() override;
  void loop() override;

 protected:
  void handle_line_(char *line, size_t len);

  LineReader<NMEA_MAX_LINE + 1> line_reader_;
};

}  // namespace esphome::nmea
