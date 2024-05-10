#include "muart_bridge.h"

namespace esphome {
namespace mitsubishi_uart {

MUARTBridge::MUARTBridge(uart::UARTComponent *uart_component, PacketProcessor *packet_processor)
    : uart_comp{*uart_component}, pkt_processor{*packet_processor} {}

// The heatpump loop expects responses for most sent packets, so it tracks the last send packet and wait for a response
void HeatpumpBridge::loop() {
  // Try to get a packet
  if (optional<RawPacket> pkt =
          receiveRawPacket(SourceBridge::heatpump, packetAwaitingResponse.has_value()
                                                       ? packetAwaitingResponse.value().getControllerAssociation()
                                                       : ControllerAssociation::muart)) {
    ESP_LOGV(BRIDGE_TAG, "Parsing %x heatpump packet", pkt.value().getPacketType());
    // Check the packet's checksum and either process it, or log an error
    if (pkt.value().isChecksumValid()) {
      // If we're waiting for a response, associate the incomming packet with the request packet
      classifyAndProcessRawPacket(pkt.value());
    } else {
      ESP_LOGW(BRIDGE_TAG, "Invalid packet checksum!\n%s",
               format_hex_pretty(&pkt.value().getBytes()[0], pkt.value().getLength()).c_str());
    }

    // If there was a packet waiting for a response, remove it.
    // TODO: This incoming packet wasn't *nessesarily* a response, but for now
    // it's probably not worth checking to make sure it matches.
    if (packetAwaitingResponse.has_value()) {
      packetAwaitingResponse.reset();
    }
  } else if (!packetAwaitingResponse.has_value() && !pkt_queue.empty()) {
    // If we're not waiting for a response and there's a packet in the queue...

    // If the packet expects a response, add it to the awaitingResponse variable
    if (pkt_queue.front().isResponseExpected()) {
      packetAwaitingResponse = pkt_queue.front();
    }

    ESP_LOGV(BRIDGE_TAG, "Sending to heatpump %s", pkt_queue.front().to_string().c_str());
    writeRawPacket(pkt_queue.front().rawPacket());
    packet_sent_millis = millis();

    // Remove packet from queue
    pkt_queue.pop();
  } else if (packetAwaitingResponse.has_value() && (millis() - packet_sent_millis > RESPONSE_TIMEOUT_MS)) {
    // We've been waiting too long for a response, give up
    // TODO: We could potentially retry here, but that seems unnecessary
    packetAwaitingResponse.reset();
    ESP_LOGW(BRIDGE_TAG, "Timeout waiting for response to %x packet.", packetAwaitingResponse.value().getPacketType());
  }
}

// The thermostat bridge loop doesn't expect any responses, so packets in queue are just sent without checking if they
// expect a response
void ThermostatBridge::loop() {
  // Try to get a packet
  if (optional<RawPacket> pkt = receiveRawPacket(SourceBridge::thermostat, ControllerAssociation::thermostat)) {
    ESP_LOGV(BRIDGE_TAG, "Parsing %x thermostat packet", pkt.value().getPacketType());
    // Check the packet's checksum and either process it, or log an error
    if (pkt.value().isChecksumValid()) {
      classifyAndProcessRawPacket(pkt.value());
    } else {
      ESP_LOGW(BRIDGE_TAG, "Invalid packet checksum!\n%s",
               format_hex_pretty(&pkt.value().getBytes()[0], pkt.value().getLength()).c_str());
    }
  } else if (!pkt_queue.empty()) {
    // If there's a packet in the queue...

    ESP_LOGV(BRIDGE_TAG, "Sending to thermostat %s", pkt_queue.front().to_string().c_str());
    writeRawPacket(pkt_queue.front().rawPacket());
    packet_sent_millis = millis();

    // Remove packet from queue
    pkt_queue.pop();
  }
}

/* Queues a packet to be sent by the bridge.  If the queue is full, the packet will not be
enqueued.*/
void MUARTBridge::sendPacket(const Packet &packetToSend) {
  if (pkt_queue.size() <= MAX_QUEUE_SIZE) {
    pkt_queue.push(packetToSend);
  } else {
    ESP_LOGW(BRIDGE_TAG, "Packet queue full!  %x packet not sent.", packetToSend.getPacketType());
  }
}

void MUARTBridge::writeRawPacket(const RawPacket &packetToSend) const {
  uart_comp.write_array(packetToSend.getBytes(), packetToSend.getLength());
}

/* Reads and deserializes a packet from UART.
Communication with heatpump is *slow*, so we need to check and make sure there are
enough packets available before we start reading.  If there aren't enough packets,
no packet will be returned.

Even at 2400 baud, the 100ms readtimeout should be enough to read a whole payload
after the first byte has been received though, so currently we're assuming that once
the header is available, it's safe to call read_array without timing out and severing
the packet.
*/
const optional<RawPacket> MUARTBridge::receiveRawPacket(const SourceBridge source_bridge,
                                                        const ControllerAssociation controller_association) const {
  // TODO: Can we make the source_bridge and controller_association inherent to the class instead of passed as
  // arguments?
  uint8_t packetBytes[PACKET_MAX_SIZE];
  packetBytes[0] = 0;  // Reset control byte before starting

  // Drain UART until we see a control byte (times out after 100ms in UARTComponent)
  while (uart_comp.available() >= PACKET_HEADER_SIZE && uart_comp.read_byte(&packetBytes[0])) {
    if (packetBytes[0] == BYTE_CONTROL)
      break;
    // TODO: If the serial is all garbage, this may never stop-- we should have our own timeout
  }

  // If we never found a control byte, we didn't receive a packet
  if (packetBytes[0] != BYTE_CONTROL) {
    return nullopt;
  }

  // Read the header
  uart_comp.read_array(&packetBytes[1], PACKET_HEADER_SIZE - 1);

  // Read payload + checksum
  uint8_t payloadSize = packetBytes[PACKET_HEADER_INDEX_PAYLOAD_LENGTH];
  uart_comp.read_array(&packetBytes[PACKET_HEADER_SIZE], payloadSize + 1);

  return RawPacket(packetBytes, PACKET_HEADER_SIZE + payloadSize + 1, source_bridge, controller_association);
}

template<class P> void MUARTBridge::processRawPacket(RawPacket &pkt, bool expectResponse) const {
  P packet = P(std::move(pkt));
  packet.setResponseExpected(expectResponse);
  pkt_processor.processPacket(packet);
}

void MUARTBridge::classifyAndProcessRawPacket(RawPacket &pkt) const {
  // Figure out how to do this without a static_cast?
  switch (static_cast<PacketType>(pkt.getPacketType())) {
    case PacketType::connect_request:
      processRawPacket<ConnectRequestPacket>(pkt, true);
      break;
    case PacketType::connect_response:
      processRawPacket<ConnectResponsePacket>(pkt, false);
      break;

    case PacketType::extended_connect_request:
      processRawPacket<ExtendedConnectRequestPacket>(pkt, true);
      break;
    case PacketType::extended_connect_response:
      processRawPacket<ExtendedConnectResponsePacket>(pkt, false);
      break;

    case PacketType::get_request:
      processRawPacket<GetRequestPacket>(pkt, true);
      break;
    case PacketType::get_response:
      switch (static_cast<GetCommand>(pkt.getCommand())) {
        case GetCommand::settings:
          processRawPacket<SettingsGetResponsePacket>(pkt, false);
          break;
        case GetCommand::current_temp:
          processRawPacket<CurrentTempGetResponsePacket>(pkt, false);
          break;
        case GetCommand::error_info:
          processRawPacket<ErrorStateGetResponsePacket>(pkt, false);
          break;
        case GetCommand::standby:
          processRawPacket<StandbyGetResponsePacket>(pkt, false);
          break;
        case GetCommand::status:
          processRawPacket<StatusGetResponsePacket>(pkt, false);
          break;
        case GetCommand::a_9:
          processRawPacket<A9GetRequestPacket>(pkt, false);
          break;
        default:
          processRawPacket<Packet>(pkt, false);
      }
      break;
    case PacketType::set_request:
      switch (static_cast<SetCommand>(pkt.getCommand())) {
        case SetCommand::remote_temperature:
          processRawPacket<RemoteTemperatureSetRequestPacket>(pkt, true);
          break;
        case SetCommand::settings:
          processRawPacket<SettingsSetRequestPacket>(pkt, true);
          break;
        case SetCommand::thermostat_hello:
          processRawPacket<ThermostatHelloRequestPacket>(pkt, false);
          break;
        default:
          processRawPacket<Packet>(pkt, true);
      }
      break;
    case PacketType::set_response:
      processRawPacket<SetResponsePacket>(pkt, false);
      break;

    default:
      processRawPacket<Packet>(pkt, true);  // If we get an unknown packet from the thermostat, expect a response
  }
}

}  // namespace mitsubishi_uart
}  // namespace esphome
