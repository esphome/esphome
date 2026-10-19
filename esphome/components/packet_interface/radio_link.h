#pragma once

#include <vector>

#include "esphome/components/packet_interface/listen_before_talk.h"
#include "esphome/components/packet_interface/packet_interface.h"

namespace esphome::packet_interface {

/// The parts of RadioLink that do not depend on the radio.
class RadioLinkBase : public PacketInterface {
 public:
  void set_max_busy(uint32_t max_busy_ms) { this->listen_before_talk_.set_max_busy(max_busy_ms); }

 protected:
  explicit RadioLinkBase(size_t max_packet_size) : PacketInterface(max_packet_size) {}
  /// False if a packet of this size is too large.
  bool check_size_(size_t size) const;
  /// False if sending must wait, as the radio is busy; starts loop() to watch the radio.
  bool may_send_(bool busy);
  /// Ends the busy period when loop() finds the radio free.
  void radio_free_();
  void receive_(const std::vector<uint8_t> &packet, float rssi, float snr);

  ListenBeforeTalk listen_before_talk_;
};

/**
 * A packet interface link over a packet radio: each packet is one radio packet, with the RSSI and SNR of received
 * packets in the metadata. Sending waits for the radio to finish transmitting. A radio cannot hear while it sends, so
 * sending is refused while the radio is receiving a packet, and the layers above send again shortly. While a busy
 * period lasts, loop() watches the radio and ends the period as soon as the radio is free, so a later reception
 * starts a new one.
 *
 * Radio provides is_receiving(), register_listener(Listener *) and transmit_packet(const uint8_t *, size_t), which
 * returns Error::NONE on success. Listener declares on_packet(packet, rssi, snr). MaxPacketSize is the radio's limit.
 */
template<typename Radio, typename Listener, typename Error, size_t MaxPacketSize>
class RadioLink : public RadioLinkBase, public Listener {
 public:
  RadioLink(Radio *radio, size_t max_packet_size) : RadioLinkBase(max_packet_size), radio_(radio) {}

  void setup() override {
    this->radio_->register_listener(this);
    this->disable_loop();
  }
  // Runs only during a busy period; a busy flag that never clears keeps it running
  void loop() override {
    if (!this->radio_->is_receiving())
      this->radio_free_();
  }

  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) override {
    // The radio is set up after the layers above, which may send from their own setup
    if (!this->radio_->is_ready())
      return false;
    size_t size = data.size();
    if (size == 0)
      return true;
    if (!this->check_size_(size) || !this->may_send_(this->radio_->is_receiving()))
      return false;
    if (data.is_contiguous()) {
      auto span = data.as_span();
      return this->radio_->transmit_packet(span.data(), span.size()) == Error::NONE;
    }
    uint8_t buf[MaxPacketSize];
    data.copy_to(buf, size);
    return this->radio_->transmit_packet(buf, size) == Error::NONE;
  }

  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override {
    this->receive_(packet, rssi, snr);
  }

 protected:
  Radio *radio_;
};

}  // namespace esphome::packet_interface
