#pragma once

#include "esphome/components/packet_interface/listen_before_talk.h"
#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/components/sx127x/sx127x.h"

namespace esphome::sx127x {

/// A packet interface link over an SX127x radio: each packet is one radio packet, with the RSSI and SNR of received
/// packets in the metadata. Sending waits for the radio to finish transmitting. A radio cannot hear while it
/// sends, so sending is refused while the radio is receiving a packet; the layers above send again shortly.
class SX127xPacketInterface : public packet_interface::PacketInterface, public SX127xListener {
 public:
  explicit SX127xPacketInterface(SX127x *radio) : radio_(radio) {}

  void setup() override { this->radio_->register_listener(this); }
  bool send_to_interface(const packet_interface::PacketBuffer &data,
                         const packet_interface::PacketMetaData &meta_data) override;
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;

 protected:
  SX127x *radio_;
  packet_interface::ListenBeforeTalk listen_before_talk_;
};

}  // namespace esphome::sx127x
