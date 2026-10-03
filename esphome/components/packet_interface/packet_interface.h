#pragma once

#include <utility>

#include "esphome/core/string_ref.h"
#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "packet_buffer.h"

namespace esphome::packet_interface {

static const char *const TAG = "packet_interface";

/**
 * A struct to hold metadata about a packet.
 * @param info Additional information about the packet
 * @param rssi The RSSI of the packet
 * @param snr The SNR of the packet
 * @param mac_address The MAC address of the sender/receiver
 */
class PacketMetaData {
 public:
  StringRef info{};
  float rssi{NAN};
  float snr{NAN};
  uint8_t mac_address[MAC_ADDRESS_SIZE]{};
};
/**
 * A class that will transport data to and from an abstract channel.
 *
 * The operations defined are:
 *
 * Transmit: the function transmit() takes a PacketBuffer of bytes, and returns when all bytes are queued for
 * transmission at least.
 *
 * Receive: The callbacks for an instance of this class are called with currently received data.
 */
class PacketInterface : public Component {
 public:
  /**
   *
   * Method to be implemented by child classes to send data to the interface.
   *
   * @param data The data to be sent
   * @param meta_data Metadata about the packet to be sent
   * @return True if the data was queued for transmission, false if it could not be sent
   */
  virtual bool send_to_interface(const PacketBuffer &data, PacketMetaData meta_data) = 0;

  virtual size_t get_max_packet_size() { return this->max_packet_size_; }
  void set_max_packet_size(size_t size) { this->max_packet_size_ = size; }

  /**
   * Add a listener that will be called when data is received.
   *
   * @param callback The callback to be called when data is received.
   */
  void add_packet_interface_listener(std::function<void(const PacketBuffer &, PacketMetaData meta_data)> callback) {
    this->callback_.add(std::move(callback));
  }

  void dump_config() override;

 protected:
  size_t max_packet_size_{512};
  /**
   * This function should be called by implementing classes with received data to be passed to any listeners.
   *
   * @param data The received data
   * @param meta_data Metadata about the received packet
   */
  void on_receive_from_interface_(const PacketBuffer &data, PacketMetaData meta_data);
  virtual ~PacketInterface() = default;

  LazyCallbackManager<void(const PacketBuffer &, PacketMetaData meta_data)> callback_;
};
}  // namespace esphome::packet_interface
