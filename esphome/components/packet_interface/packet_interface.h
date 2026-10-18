#pragma once

#include <cmath>
#include <cstdint>
#include <utility>

#include "esphome/core/string_ref.h"
#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "packet_buffer.h"

namespace esphome::packet_interface {

/// What a stack of layers provides, as bits of PacketInterface::get_capabilities().
enum Capability : uint8_t {
  CAPABILITY_ENCRYPTION = 1 << 0,
  CAPABILITY_RELIABILITY = 1 << 1,
  CAPABILITY_SESSION = 1 << 2,
  CAPABILITY_ADDRESSING = 1 << 3,
  CAPABILITY_FRAGMENTATION = 1 << 4,
};

/**
 * A struct to hold metadata about a packet.
 * @param info Additional information about the packet
 * @param source_address With an addressing layer, the sender of a received packet; 0 if not known
 * @param destination_address With an addressing layer, the destination of a packet; when sending, 0 uses the
 * configured destination
 * @param rssi The RSSI of the packet
 * @param snr The SNR of the packet
 * @param mac_address The MAC address of the sender/receiver
 */
class PacketMetaData {
 public:
  StringRef info{};
  uint16_t source_address{0};
  uint16_t destination_address{0};
  float rssi{NAN};
  float snr{NAN};
  uint8_t mac_address[MAC_ADDRESS_SIZE]{};
};
/**
 * A class that will transport data to and from an abstract channel.
 *
 * The operations defined are:
 *
 * Transmit: the function send_to_interface() takes a PacketBuffer of bytes, and returns when all data is
 * written or buffered for transmission.
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
  virtual bool send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) = 0;

  size_t get_max_packet_size() const { return this->max_packet_size_; }
  /// How many packets send_to_interface() would accept now, so a layer above can tell before sending a series.
  virtual size_t get_send_capacity() const { return SIZE_MAX; }
  void set_max_packet_size(size_t size) { this->max_packet_size_ = size; }
  /// The capabilities of this layer and every layer below it.
  uint8_t get_capabilities() const { return this->capabilities_; }
  bool has_capability(Capability capability) const { return (this->capabilities_ & capability) != 0; }
  /**
   * With CAPABILITY_SESSION, the callback is called with true when the peer can be reached, including after it
   * restarts, and with false when it is lost. The session layer is always the top layer, so register on top().
   * A reliability layer calls it with false when it resets the link and discards data, which the session layer above
   * reports as the session lost.
   */
  template<typename F> void add_on_session_state_callback(F &&callback) {
    this->session_callback_.add(std::forward<F>(callback));
  }

  /// The topmost layer stacked on this one, or this one if nothing is stacked on it.
  PacketInterface *top() {
    PacketInterface *layer = this;
    while (layer->upper_ != nullptr)
      layer = layer->upper_;
    return layer;
  }
  /// Name shown in log messages; a layer takes the name of the link it is stacked on.
  void set_name(const char *name) { this->name_ = name; }
  const char *get_name() const { return this->name_; }

  /**
   * Add a listener that will be called when data is received.
   *
   * @param callback The callback to be called when data is received.
   */
  template<typename F> void add_packet_interface_listener(F &&callback) {
    this->callback_.add(std::forward<F>(callback));
  }

  void dump_config() override;

 protected:
  virtual ~PacketInterface() = default;
  /**
   * This function should be called by implementing classes with received data to be passed to any listeners.
   *
   * @param data The received data
   * @param meta_data Metadata about the received packet
   */
  void on_receive_from_interface_(const PacketBuffer &data, const PacketMetaData &meta_data);
  /**
   * Called by a layer for each received packet it understood, or could not make sense of. A run of packets that
   * cannot be understood usually means the peer has different layers configured, which is logged once per run.
   */
  void note_received_(bool understood);
  /// Called by a layer's constructor to record itself as the layer above \p lower, adding its own capability.
  void stack_on_(PacketInterface *lower, Capability capability) {
    lower->upper_ = this;
    this->inherit_from_(lower, capability);
  }
  /// Takes the name and capabilities of `lower` without becoming its top, for one of several stacks on a link.
  void inherit_from_(PacketInterface *lower, Capability capability) {
    this->name_ = lower->name_;
    this->capabilities_ = lower->capabilities_ | capability;
    // A link that has a session of its own, such as TCP, reports it to whatever is stacked above, as does a
    // reliability layer that discards data; the session layer handles the latter itself
    if ((lower->has_capability(CAPABILITY_SESSION) || lower->has_capability(CAPABILITY_RELIABILITY)) &&
        capability != CAPABILITY_SESSION) {
      lower->add_on_session_state_callback([this](bool connected) { this->session_callback_.call(connected); });
    }
  }

  LazyCallbackManager<void(const PacketBuffer &, const PacketMetaData &)> callback_;
  LazyCallbackManager<void(bool)> session_callback_;
  PacketInterface *upper_{nullptr};
  const char *name_{""};
  size_t max_packet_size_{512};
  uint8_t capabilities_{0};
  uint8_t not_understood_count_{0};
};
}  // namespace esphome::packet_interface
