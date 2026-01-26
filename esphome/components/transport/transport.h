#pragma once

#include <utility>

#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome {
namespace transport {

static const char *TAG = "transport";

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
class Transport : public Component {
 public:
  bool transmit(const std::vector<uint8_t> &data) {
    auto result = this->send_data_(data);
    ESP_LOGV(TAG, "send_data returns %s for data  %s", TRUEFALSE(result), format_hex_pretty(data).c_str());
    return result;
  }

  void add(std::function<void(const std::vector<uint8_t> &)> callback) { this->callback_.add(std::move(callback)); }

 protected:
  /**
   * This function should be called by implementing classes when data is received and will be passed to any listeners.
   *
   * @param data The received data
   */
  void on_receive_data_(const std::vector<uint8_t> &data) {
    ESP_LOGV(TAG, "Received data %s", format_hex_pretty(data.data(), data.size()).c_str());
    this->callback_.call(data);
  }
  virtual ~Transport() = default;

  /**
   *
   * @param data The data to be sent
   * @return True if the data was queued for transmission, false if it could not be sent
   */
  virtual bool send_data_(const std::vector<uint8_t> &data) = 0;
  CallbackManager<void(const std::vector<uint8_t> &)> callback_;
};
}  // namespace transport
}  // namespace esphome
