#include "mitp_bridge.h"

namespace esphome {
namespace mitsubishi_itp {

MITPBridge::MITPBridge(uart::UARTComponent *uart_component, PacketProcessor *packet_processor)
    : uart_comp_{*uart_component}, pkt_processor_{*packet_processor} {}

// The heatpump loop expects responses for most sent packets, so it tracks the last send packet and wait for a response
void HeatpumpBridge::loop() {
  // Try to get a packet
  if (optional<RawPacket> pkt = receive_raw_packet_(SourceBridge::HEATPUMP,
                                                    packet_awaiting_response_.has_value()
                                                        ? packet_awaiting_response_.value().get_controller_association()
                                                        : ControllerAssociation::MITP)) {
    ESP_LOGV(BRIDGE_TAG, "Parsing %x heatpump packet", pkt.value().get_packet_type());
    // Check the packet's checksum and either process it, or log an error
    if (pkt.value().is_checksum_valid()) {
      // If we're waiting for a response, associate the incomming packet with the request packet
      classify_and_process_raw_packet_(pkt.value());
    } else {
      ESP_LOGW(BRIDGE_TAG, "Invalid packet checksum!\n%s",
               format_hex_pretty(&pkt.value().get_bytes()[0], pkt.value().get_length()).c_str());
    }

    // If there was a packet waiting for a response, remove it.
    // TODO: This incoming packet wasn't *nessesarily* a response, but for now
    // it's probably not worth checking to make sure it matches.
    if (packet_awaiting_response_.has_value()) {
      packet_awaiting_response_.reset();
    }
  } else if (!packet_awaiting_response_.has_value() && !pkt_queue_.empty()) {
    // If we're not waiting for a response and there's a packet in the queue...

    // If the packet expects a response, add it to the awaitingResponse variable
    if (pkt_queue_.front().is_response_expected()) {
      packet_awaiting_response_ = pkt_queue_.front();
    }

    ESP_LOGV(BRIDGE_TAG, "Sending to heatpump %s", pkt_queue_.front().to_string().c_str());
    write_raw_packet_(pkt_queue_.front().raw_packet());
    packet_sent_millis_ = millis();

    // Remove packet from queue
    pkt_queue_.pop();
  } else if (packet_awaiting_response_.has_value() && (millis() - packet_sent_millis_ > RESPONSE_TIMEOUT_MS)) {
    // We've been waiting too long for a response, give up
    // TODO: We could potentially retry here, but that seems unnecessary
    packet_awaiting_response_.reset();
    ESP_LOGW(BRIDGE_TAG, "Timeout waiting for response to %x packet.",
             packet_awaiting_response_.value().get_packet_type());
  }
}

// The thermostat bridge loop doesn't expect any responses, so packets in queue are just sent without checking if they
// expect a response
void ThermostatBridge::loop() {
  // Try to get a packet
  if (optional<RawPacket> pkt = receive_raw_packet_(SourceBridge::THERMOSTAT, ControllerAssociation::THERMOSTAT)) {
    ESP_LOGV(BRIDGE_TAG, "Parsing %x thermostat packet", pkt.value().get_packet_type());
    // Check the packet's checksum and either process it, or log an error
    if (pkt.value().is_checksum_valid()) {
      classify_and_process_raw_packet_(pkt.value());
    } else {
      ESP_LOGW(BRIDGE_TAG, "Invalid packet checksum!\n%s",
               format_hex_pretty(&pkt.value().get_bytes()[0], pkt.value().get_length()).c_str());
    }
  } else if (!pkt_queue_.empty()) {
    // If there's a packet in the queue...

    ESP_LOGV(BRIDGE_TAG, "Sending to thermostat %s", pkt_queue_.front().to_string().c_str());
    write_raw_packet_(pkt_queue_.front().raw_packet());
    packet_sent_millis_ = millis();

    // Remove packet from queue
    pkt_queue_.pop();
  }
}

/* Queues a packet to be sent by the bridge.  If the queue is full, the packet will not be
enqueued.*/
void MITPBridge::send_packet(const Packet &packet_to_send) {
  if (pkt_queue_.size() <= MAX_QUEUE_SIZE) {
    pkt_queue_.push(packet_to_send);
  } else {
    ESP_LOGW(BRIDGE_TAG, "Packet queue full!  %x packet not sent.", packet_to_send.get_packet_type());
  }
}

void MITPBridge::write_raw_packet_(const RawPacket &packet_to_send) const {
  uart_comp_.write_array(packet_to_send.get_bytes(), packet_to_send.get_length());
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
optional<RawPacket> MITPBridge::receive_raw_packet_(const SourceBridge source_bridge,
                                                    const ControllerAssociation controller_association) const {
  // TODO: Can we make the source_bridge and controller_association inherent to the class instead of passed as
  // arguments?
  uint8_t packet_bytes[PACKET_MAX_SIZE];
  packet_bytes[0] = 0;  // Reset control byte before starting

  // Drain UART until we see a control byte (times out after 100ms in UARTComponent)
  while (uart_comp_.available() >= PACKET_HEADER_SIZE && uart_comp_.read_byte(&packet_bytes[0])) {
    if (packet_bytes[0] == BYTE_CONTROL)
      break;
    // TODO: If the serial is all garbage, this may never stop-- we should have our own timeout
  }

  // If we never found a control byte, we didn't receive a packet
  if (packet_bytes[0] != BYTE_CONTROL) {
    return nullopt;
  }

  // Read the header
  uart_comp_.read_array(&packet_bytes[1], PACKET_HEADER_SIZE - 1);

  // Read payload + checksum
  uint8_t payload_size = packet_bytes[PACKET_HEADER_INDEX_PAYLOAD_LENGTH];
  uart_comp_.read_array(&packet_bytes[PACKET_HEADER_SIZE], payload_size + 1);

  return RawPacket(packet_bytes, PACKET_HEADER_SIZE + payload_size + 1, source_bridge, controller_association);
}

template<class P> void MITPBridge::process_raw_packet_(RawPacket &pkt, bool expect_response) const {
  P packet = P(std::move(pkt));
  packet.set_response_expected(expect_response);
  pkt_processor_.process_packet(packet);
}

void MITPBridge::classify_and_process_raw_packet_(RawPacket &pkt) const {
  // Figure out how to do this without a static_cast?
  switch (static_cast<PacketType>(pkt.get_packet_type())) {
    case PacketType::CONNECT_REQUEST:
      process_raw_packet_<ConnectRequestPacket>(pkt, true);
      break;
    case PacketType::CONNECT_RESPONSE:
      process_raw_packet_<ConnectResponsePacket>(pkt, false);
      break;

    case PacketType::IDENTIFY_REQUEST:
      process_raw_packet_<CapabilitiesRequestPacket>(pkt, true);
      break;
    case PacketType::IDENTIFY_RESPONSE:
      process_raw_packet_<CapabilitiesResponsePacket>(pkt, false);
      break;

    case PacketType::GET_REQUEST:
      process_raw_packet_<GetRequestPacket>(pkt, true);
      break;
    case PacketType::GET_RESPONSE:
      switch (static_cast<GetCommand>(pkt.get_command())) {
        case GetCommand::SETTINGS:
          process_raw_packet_<SettingsGetResponsePacket>(pkt, false);
          break;
        case GetCommand::CURRENT_TEMP:
          process_raw_packet_<CurrentTempGetResponsePacket>(pkt, false);
          break;
        case GetCommand::ERROR_INFO:
          process_raw_packet_<ErrorStateGetResponsePacket>(pkt, false);
          break;
        case GetCommand::RUN_STATE:
          process_raw_packet_<RunStateGetResponsePacket>(pkt, false);
          break;
        case GetCommand::STATUS:
          process_raw_packet_<StatusGetResponsePacket>(pkt, false);
          break;
        case GetCommand::THERMOSTAT_STATE_DOWNLOAD:
          process_raw_packet_<ThermostatStateDownloadResponsePacket>(pkt, false);
          break;
        default:
          process_raw_packet_<Packet>(pkt, false);
      }
      break;
    case PacketType::SET_REQUEST:
      switch (static_cast<SetCommand>(pkt.get_command())) {
        case SetCommand::REMOTE_TEMPERATURE:
          process_raw_packet_<RemoteTemperatureSetRequestPacket>(pkt, true);
          break;
        case SetCommand::SETTINGS:
          process_raw_packet_<SettingsSetRequestPacket>(pkt, true);
          break;
        case SetCommand::THERMOSTAT_SENSOR_STATUS:
          process_raw_packet_<ThermostatSensorStatusPacket>(pkt, true);
          break;
        case SetCommand::THERMOSTAT_HELLO:
          process_raw_packet_<ThermostatHelloPacket>(pkt, false);
          break;
        case SetCommand::THERMOSTAT_STATE_UPLOAD:
          process_raw_packet_<ThermostatStateUploadPacket>(pkt, true);
          break;
        case SetCommand::THERMOSTAT_SET_AA:
          process_raw_packet_<ThermostatAASetRequestPacket>(pkt, true);
          break;
        default:
          process_raw_packet_<Packet>(pkt, true);
      }
      break;
    case PacketType::SET_RESPONSE:
      process_raw_packet_<SetResponsePacket>(pkt, false);
      break;

    default:
      process_raw_packet_<Packet>(pkt, true);  // If we get an unknown packet from the thermostat, expect a response
  }
}

}  // namespace mitsubishi_itp
}  // namespace esphome
