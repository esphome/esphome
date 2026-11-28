#pragma once

#include "esphome/components/geolocation/geolocation.h"
#include <cstddef>
#include <cstdint>

namespace esphome::nmea {

/// Longest NMEA line kept, without the line end; the standard limit is 80, some receivers go over it.
static constexpr size_t NMEA_MAX_LINE = 96;

/// Parse one NMEA line of len bytes, which is changed in place; the sentence starts at the last '$'. On a GGA or RMC
/// sentence with a good checksum and no malformed field, store its values in data and return true; otherwise leave
/// data unchanged and return false.
bool parse_nmea_sentence(char *line, size_t len, geolocation::GeolocationData &data);

}  // namespace esphome::nmea
