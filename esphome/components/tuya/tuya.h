#pragma once

#include <cinttypes>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

namespace esphome::tuya {

// Max bytes to log for datapoint values (larger values are truncated)
static constexpr size_t MAX_DATAPOINT_LOG_BYTES = 16;

enum class TuyaDatapointType : uint8_t {
  RAW = 0x00,      // variable length
  BOOLEAN = 0x01,  // 1 byte (0/1)
  INTEGER = 0x02,  // 4 byte
  STRING = 0x03,   // variable length
  ENUM = 0x04,     // 1 byte
  BITMASK = 0x05,  // 1/2/4 bytes
};

struct TuyaDatapoint {
  uint8_t id;
  TuyaDatapointType type;
  size_t len;
  union {
    bool value_bool;
    int value_int;
    uint32_t value_uint;
    uint8_t value_enum;
    uint32_t value_bitmask;
  };
  std::string value_string;
  std::vector<uint8_t> value_raw;

  /// Log a warning when this datapoint is not of type `expected`, and return it.
  const TuyaDatapoint &expect_type(TuyaDatapointType expected) const;
};

struct TuyaDatapointListener {
  uint8_t datapoint_id;
  Callback<void(const TuyaDatapoint &)> on_datapoint;
};

/** Transport-agnostic base class for Tuya MCU controllers.
 *
 * Holds the datapoint store, listener registration and the datapoint setters.
 * A subclass provides the transport by implementing send_datapoint_command()
 * and feeding received datapoints to handle_datapoint_().
 */
class Tuya : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void dump_config() override;
  template<typename F> void register_listener(uint8_t datapoint_id, F &&func) {
    this->register_listener_(datapoint_id, Callback<void(const TuyaDatapoint &)>::create(std::forward<F>(func)));
  }
  void set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
  void set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
  void set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length);
  void force_set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void force_set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
  void force_set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void force_set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void force_set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
  void force_set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length);
  void add_ignore_mcu_update_on_datapoints(uint8_t ignore_mcu_update_on_datapoints) {
    this->ignore_mcu_update_on_datapoints_.push_back(ignore_mcu_update_on_datapoints);
  }
  template<typename F> void add_on_initialized_callback(F &&callback) {
    this->initialized_callback_.add(std::forward<F>(callback));
  }

 protected:
  void register_listener_(uint8_t datapoint_id, Callback<void(const TuyaDatapoint &)> func);
  /// Store a datapoint received from the MCU and notify listeners.
  void handle_datapoint_(const TuyaDatapoint &datapoint);
  optional<TuyaDatapoint> get_datapoint_(uint8_t datapoint_id);
  void dump_datapoints_();
  void set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, uint32_t value,
                                    uint8_t length, bool forced);
  void set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced);
  void set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced);
  /// Deliver a datapoint value to the MCU over the subclass's transport.
  virtual void send_datapoint_command(uint8_t datapoint_id, TuyaDatapointType datapoint_type,
                                      std::vector<uint8_t> data) = 0;

  std::vector<TuyaDatapointListener> listeners_;
  std::vector<TuyaDatapoint> datapoints_;
  std::vector<uint8_t> ignore_mcu_update_on_datapoints_{};
  CallbackManager<void()> initialized_callback_{};
};

}  // namespace esphome::tuya
