#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class Optolink;

class OptolinkSensorBase {
 public:
  OptolinkSensorBase(Optolink *optolink, bool writeable = false) {
    optolink_ = optolink;
    writeable_ = writeable;
  }

  void set_address(uint32_t address) { address_ = address; }
  void set_bytes(int bytes) { bytes_ = bytes; }
  void set_div_ratio(int div_ratio) { div_ratio_ = div_ratio; }

 protected:
  Optolink *optolink_;
  bool writeable_;
  IDatapoint *datapoint_ = nullptr;
  uint32_t address_;
  size_t bytes_;
  size_t div_ratio_ = 0;

  virtual const StringRef &get_component_name() = 0;
  void setup_datapoint();
  virtual void value_changed(float value);
  virtual void value_changed(uint8_t value);
  virtual void value_changed(uint16_t value);
  virtual void value_changed(uint32_t value);
  virtual void value_changed(std::string value);
  virtual void value_changed(uint8_t *value, size_t length);
  void update_datapoint(float value);
  void update_datapoint(uint8_t value);
  void update_datapoint(uint16_t value);
  void update_datapoint(uint32_t value);
  void update_datapoint(uint8_t *value, size_t length);

  void unfitting_value_type();

 private:
  void update_datapoint(DPValue dp_value);
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
