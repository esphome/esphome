#pragma once

#include <utility>

#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace packet_interface {

static const char *TAG = "packet_interface";

/**
 * A class that will transport data to and from an abstract channel.
 *
 * The operations defined are:
 *
 * Transmit: the function transmit() takes a vector of bytes, and returns when all bytes are queued for
 * transmission at least.
 *
 * Receive: The callbacks for an instance of this class are called with currently received data.
 */
class PacketInterface : public Component {
 public:
  /**
   *
   * @param data The data to be sent
   * @return True if the data was queued for transmission, false if it could not be sent
   */
  bool transmit(const std::vector<uint8_t> &data);

  /**
   * Add a listener that will be called when data is received.
   *
   * @param callback The callback to be called when data is received.
   */
  void add_packet_interface_listener(std::function<void(const std::vector<uint8_t> &)> callback) {
    this->callback_.add(std::move(callback));
  }

 protected:
  /**
   * This function should be called by implementing classes with received data to be passed to any listeners.
   *
   * @param data The received data
   */
  void on_receive_data_(const std::vector<uint8_t> &data);
  virtual ~PacketInterface() = default;

  /**
   *
   * @param data The data to be sent
   * @return True if the data was queued for transmission, false if it could not be sent
   */
  virtual bool send_data_(const std::vector<uint8_t> &data) = 0;
  LazyCallbackManager<void(const std::vector<uint8_t> &)> callback_;
};
}  // namespace packet_interface
}  // namespace esphome
