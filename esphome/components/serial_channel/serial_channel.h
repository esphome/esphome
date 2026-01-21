#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "serial_channel_call.h"
#include "serial_channel_traits.h"

namespace esphome::serial_channel {

#define LOG_SERIAL_CHANNEL(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon_ref().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon_ref().c_str()); \
    } \
  }

/** Serial channel for UART communication.
 *
 * A serial channel can use publish_state to send out received data.
 */
class SerialChannel : public EntityBase, public uart::UARTDevice, public Component {
 public:
  SerialChannel(size_t buffer_size) { this->rx_buffer_.init(buffer_size); }

  SerialChannelTraits traits;

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  /// Publish received data (raw bytes).
  void publish_state(const uint8_t *data, size_t len);
  /// Publish received data (already base64 encoded).
  void publish_state(const std::string &base64_data);

  /// Get current state (base64 encoded).
  const std::string &get_state() const { return this->state_; }

  /// Instantiate a SerialChannelCall object to send data to this channel.
  SerialChannelCall make_call() { return SerialChannelCall(this); }

  /// Add a callback to be called when data is received.
  void add_on_state_callback(std::function<void(const std::string &)> &&callback);

 protected:
  friend class SerialChannelCall;

  /** Send data to the serial channel via UART.
   *
   * This method is called by the SerialChannelCall.
   *
   * @param data The data to send.
   * @param len The length of the data.
   */
  void control(const uint8_t *data, size_t len);

  std::string state_;  // Last received data (base64 encoded)
  LazyCallbackManager<void(const std::string &)> state_callback_;
  FixedVector<uint8_t> rx_buffer_;
};

}  // namespace esphome::serial_channel
