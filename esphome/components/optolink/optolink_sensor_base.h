#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class Optolink;

class OptolinkSensorBase {
 protected:
  Optolink *optolink_;
  bool writeable_;
  IDatapoint *datapoint_ = nullptr;
  uint32_t address_;
  int bytes_;
  int div_ratio_ = 1;

  void setup_datapoint_();
  void update_datapoint_(float value);

 public:
  OptolinkSensorBase(Optolink *optolink, bool writeable = false) {
    optolink_ = optolink;
    writeable_ = writeable;
  }

  void set_address(uint32_t address) { address_ = address; }
  void set_bytes(int bytes) { bytes_ = bytes; }
  void set_div_ratio(int div_ratio) { div_ratio_ = div_ratio; }

 protected:
  virtual const StringRef &get_sensor_name() = 0;
  virtual void value_changed(float state) = 0;
};

// NOLINTNEXTLINE
class conv2_100_F : public DPType {
 public:
  void encode(uint8_t *out, DPValue in);
  DPValue decode(const uint8_t *in);
  size_t get_length() const { return 2; }
};

}  // namespace optolink
}  // namespace esphome

#endif
