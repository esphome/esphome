#pragma once

#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/components/packet_transport/packet_transport.h"

namespace esphome {
namespace packet_interface {

/**
 * A transport protocol adapter that bridges PacketInterface to PacketTransport.
 * This allows packet_transport functionality to be used with any PacketInterface implementation.
 */
class PacketInterfaceTransport : public packet_transport::PacketTransport {
 public:
  void set_packet_interface(PacketInterface *interface) { this->interface_ = interface; }

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  /**
   * Callback for receiving packets from the PacketInterface.
   * @param data The received packet data
   * @param meta_data Metadata about the received packet
   */
  void on_packet_(const std::vector<uint8_t> &data, PacketMetaData meta_data) { this->process_(data); }

 protected:
  void send_packet(const std::vector<uint8_t> &buf) const override {
    if (this->interface_ != nullptr) {
      this->interface_->transmit(buf);
    }
  }

  size_t get_max_packet_size() override { return 508; }  // Default max packet size

  PacketInterface *interface_{nullptr};
};

}  // namespace packet_interface
}  // namespace esphome
