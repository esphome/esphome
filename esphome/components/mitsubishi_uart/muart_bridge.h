#pragma once

#include "esphome/components/uart/uart.h"
#include "muart_packet.h"
#include "queue"

namespace esphome {
namespace mitsubishi_uart {

static const char *BRIDGE_TAG = "muart_bridge";
static const uint32_t RESPONSE_TIMEOUT_MS = 3000;  // Maximum amount of time to wait for an expected response packet
/* Maximum number of packets allowed to be queued for sending.  In some circumstances the equipment response
time can be very slow and packets would queue up faster than they were being received.  TODO: Not sure what size this
should be, 4ish should be enough for almost all situations, so 8 seems plenty.*/
static const size_t MAX_QUEUE_SIZE = 8;

// A UARTComponent wrapper to send and receieve packets
class MUARTBridge {
 public:
  MUARTBridge(uart::UARTComponent *uart_component, PacketProcessor *packet_processor);

  // Enqueues a packet to be sent
  void sendPacket(const Packet &packetToSend);

  // Checks for incoming packets, processes them, sends queued packets
  virtual void loop() = 0;

 protected:
  const optional<RawPacket> receiveRawPacket(const SourceBridge source_bridge,
                                             const ControllerAssociation controller_association) const;
  void writeRawPacket(const RawPacket &pkt) const;
  template<class P> void processRawPacket(RawPacket &pkt, bool expectResponse = true) const;
  void classifyAndProcessRawPacket(RawPacket &pkt) const;

  uart::UARTComponent &uart_comp;
  PacketProcessor &pkt_processor;
  std::queue<Packet> pkt_queue;
  optional<Packet> packetAwaitingResponse = nullopt;
  uint32_t packet_sent_millis;
};

class HeatpumpBridge : public MUARTBridge {
 public:
  using MUARTBridge::MUARTBridge;
  void loop() override;
};

class ThermostatBridge : public MUARTBridge {
 public:
  using MUARTBridge::MUARTBridge;
  // ThermostatBridge(uart::UARTComponent &uart_component, PacketProcessor &packet_processor) :
  // MUARTBridge(uart_component, packet_processor){};
  void loop() override;
};

}  // namespace mitsubishi_uart
}  // namespace esphome
