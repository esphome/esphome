#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

class Optolink;

struct HassSubscription {
  std::string entity_id;
  std::string last_state;
  std::vector<std::function<void(std::string)>> callbacks;
};

class DatapointComponent {
 public:
  DatapointComponent(Optolink *optolink, bool writeable = false) : dp_value_outstanding_((uint8_t) 0) {
    optolink_ = optolink;
    writeable_ = writeable;
  }

  void set_address(uint32_t address) { address_ = address; }
  void set_bytes(size_t bytes) { bytes_ = bytes; }
  void set_writeable(bool writeable) { writeable_ = writeable; }
  void set_div_ratio(size_t div_ratio) { div_ratio_ = div_ratio; }

 protected:
  virtual const StringRef &get_component_name() = 0;

  uint32_t get_address_() { return address_; }

  void setup_datapoint_();

  void datapoint_read_request_();

  virtual void datapoint_value_changed(float value);
  virtual void datapoint_value_changed(uint8_t value);
  virtual void datapoint_value_changed(uint16_t value);
  virtual void datapoint_value_changed(uint32_t value);
  virtual void datapoint_value_changed(std::string value);
  virtual void datapoint_value_changed(uint8_t *value, size_t length);

  void write_datapoint_value_(float value);
  void write_datapoint_value_(uint8_t value);
  void write_datapoint_value_(uint16_t value);
  void write_datapoint_value_(uint32_t value);
  void write_datapoint_value_(uint8_t *value, size_t length);

  void unfitting_value_type_();
  void set_optolink_state_(const char *format, ...);
  std::string get_optolink_state_();

  void subscribe_hass_(const std::string entity_id, std::function<void(std::string)> f);

 private:
  const size_t max_retries_until_reset_ = 10;
  Optolink *optolink_;
  IDatapoint *datapoint_ = nullptr;
  size_t read_retries_ = 0;
  size_t div_ratio_ = 0;
  size_t bytes_;
  uint32_t address_;
  bool writeable_;
  bool is_dp_value_writing_outstanding_ = false;
  DPValue dp_value_outstanding_;

  void datapoint_write_request_(DPValue dp_value);
};

// NOLINTBEGIN
class conv2_100_F : public DPType {
 public:
  void encode(uint8_t *out, DPValue in);
  DPValue decode(const uint8_t *in);
  virtual const size_t get_length() const { return 2; }
};

class conv4_1000_F : public DPType {
 public:
  void encode(uint8_t *out, DPValue in);
  DPValue decode(const uint8_t *in);
  virtual const size_t getLength() const { return 4; }
};
// NOLINTEND

}  // namespace optolink
}  // namespace esphome

#endif
