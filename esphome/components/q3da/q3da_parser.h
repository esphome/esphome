#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "constants.h"

namespace esphome {
namespace q3da {

using bytes = std::vector<uint8_t>;

class Q3DANode {
 public:
  uint8_t type;
  bytes value_bytes;
  std::vector<Q3DANode> nodes;
};

class ObisInfo {
  public:
  ObisInfo(bytes server_id, char* token);
  bytes server_id;
  uint8_t medium;   // Value Group A: Electricity = 1, Gas = 7, ...
  uint8_t channels; // Value Group B: internal or external
  uint8_t unit;     // Value Group C: Active, Reactive, ...
  uint8_t type;     // Value Group D: Kind of measurement: Max, current, ...
  uint8_t tariff;   // Value Group E: Kind of tariff: Total, Tarif1, Tarif2, ...
  uint8_t scaler;   // Value Group F: 00..99, 255 = ignored?
  float value;      // Actual Value for example 1234.5
  char* symbol;     // Either represents a unit (e.g. W or kWh) or textual data.
  std::string code_repr() const;
};

class Q3DATelegram {
 public:
  Q3DATelegram(const bytes buffer);
  std::vector<ObisInfo> get_obis_info();
 protected:
  std::vector<ObisInfo> messages;
  const bytes buffer_;
};

std::string bytes_repr(const bytes &buffer);

}  // namespace sml
}  // namespace esphome
