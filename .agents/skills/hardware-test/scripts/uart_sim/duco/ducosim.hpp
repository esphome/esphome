#pragma once
// A Duco ventilation box on a second UART, cross-wired to the duco hub's UART.
// Port of the box model in tests/integration/fixtures/uart_mock_duco.yaml (esphome#22560):
// every request gets an acknowledgement (function + 1, empty) and then the answer, byte stuffed, CRC16.
// Call ducosim::poll() from a short interval; set ducosim::bus in on_boot.
#include <string>
#include <vector>
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace ducosim {

inline esphome::uart::UARTComponent *bus = nullptr;
inline std::vector<uint8_t> rx;
inline uint8_t mode = 0x04;
inline uint8_t comfort = 210;
inline uint32_t requests = 0;
// payload size of the box serial answer: 34 as in the PR fixture, 18 as in the dev fixture
inline size_t box_serial_size = 34;

inline void reply(uint8_t fn, uint8_t message_id, const std::vector<uint8_t> &payload) {
  std::vector<uint8_t> body{static_cast<uint8_t>(payload.size() + 2), fn, message_id};
  body.insert(body.end(), payload.begin(), payload.end());
  const uint16_t crc = esphome::crc16(body.data(), body.size());
  body.push_back(crc & 0xFF);
  body.push_back(crc >> 8);
  std::vector<uint8_t> out{0xAA, 0x55};
  for (uint8_t b : body) {
    out.push_back(b);
    if (b == 0xAA)
      out.push_back(0x01);
  }
  char hex[100];
  esphome::format_hex_to(hex, sizeof(hex), out.data(), out.size() > 40 ? 40 : out.size());
  ESP_LOGV("box", "TX %s", hex);
  bus->write_array(out.data(), out.size());
}

inline void handle(std::vector<uint8_t> frame) {
  // frame: length, function, message id, payload, CRC16 (low byte first) over everything before it
  const uint16_t crc = esphome::crc16(frame.data(), frame.size() - 2);
  if ((crc & 0xFF) != frame[frame.size() - 2] || (crc >> 8) != frame[frame.size() - 1])
    ESP_LOGW("box", "RX request with a bad CRC (answered anyway)");
  if (frame.size() < 10)
    frame.resize(10, 0);  // the handlers read up to five payload bytes
  const uint8_t function = frame[1];
  const uint8_t message_id = frame[2];
  const uint8_t *d = frame.data() + 3;
  requests++;
  char hex[64];
  esphome::format_hex_to(hex, sizeof(hex), frame.data(), frame.size() > 20 ? 20 : frame.size());
  ESP_LOGD("box", "RX #%u fn %02X id %02X: %s", (unsigned) requests, function, message_id, hex);

  reply(function + 1, message_id, {});
  if (function == 0x0c && d[0] == 0x01) {
    static constexpr uint8_t TYPES[] = {0, 17, 10, 12};
    static constexpr uint8_t NETWORK_TYPES[] = {0, 0, 3, 2};
    const uint8_t type = d[1] < sizeof(TYPES) ? TYPES[d[1]] : 0;
    const uint8_t network_type = d[1] < sizeof(NETWORK_TYPES) ? NETWORK_TYPES[d[1]] : 0;
    reply(0x0e, message_id, {type, network_type, 0, 0, 0, 0, 0});
  } else if (function == 0x0c && d[0] == 0x02) {
    std::vector<uint8_t> status(20, 0);
    status[0] = mode;
    status[2] = 45;
    status[12] = 0xAA;  // 21930 s, sent as AA 01 55: an escaped header inside the data
    status[13] = 0x55;
    reply(0x0e, message_id, status);
  } else if (function == 0x0c && d[0] == 0x04) {
    mode = d[2];
    ESP_LOGI("box", "Mode set to %02X", mode);
    reply(0x0e, message_id, {0x01});
  } else if (function == 0x10 && d[3] == 0x49) {
    // temperature 21.5, CO2 682 (0x02AA is byte stuffed), humidity 54.32, uptime 2190 min
    reply(0x12, message_id, {1, 4, 215, 0, 0xAA, 0x02, 0x38, 0x15, 0x8E, 0x08});
  } else if (function == 0x10 && d[3] == 0x1a) {
    std::vector<uint8_t> serial{0, 0, 'S', 'N', '1', '2', '3', '4'};
    serial.resize(box_serial_size, 0);
    reply(0x12, message_id, serial);
  } else if (function == 0x10 && d[3] == 0x08) {
    if (d[1] == 3) {
      reply(0x12, message_id, {1, d[4], 0xBE, 0});  // not printable: no state
    } else {
      // node 2 gives SN2234 like the fixture; the node digit tells other nodes apart
      std::vector<uint8_t> serial{1, d[4], 'S', 'N', '2', static_cast<uint8_t>('0' + d[1]), '3', '4'};
      serial.resize(2 + 2 * d[4], 0);
      reply(0x12, message_id, serial);
    }
  } else if (function == 0x10 && d[3] == 0x00) {
    reply(0x12, message_id, {1, 4, 1, 0, 2, 4, 3, 0, 0, 0});
  } else if (function == 0x30) {
    if (d[0] == 3)
      reply(0x32, message_id, {2, 0, 31, 19, 1, 0, 0, 99, 99, 99, 0});
    else if (d[0] == 4)
      reply(0x32, message_id, {0});
    else if (d[0] == 5)
      reply(0x32, message_id, {2, 0, 31, 19, 1, 0});  // second error missing
    else if (d[0] == 6)
      reply(0x32, message_id, {4, 0, 3, 1, 4, 0, 0, 4, 1, 4, 0, 0, 6, 1, 4, 0, 0, 16, 1, 4, 0});
    else if (d[0] == 7)  // three long descriptions, 210 characters: they still fit
      reply(0x32, message_id, {3, 0, 16, 1, 4, 0, 0, 29, 1, 4, 0, 0, 30, 1, 4, 0});
    else if (d[0] == 8)  // four short descriptions fit too: the fallback goes by length, not count
      reply(0x32, message_id, {4, 0, 31, 19, 1, 0, 0, 31, 19, 1, 0, 0, 31, 19, 1, 0, 0, 31, 19, 1, 0});
    else
      reply(0x32, message_id, {});
  } else if (function == 0x24 && d[0] == 0x00) {
    int16_t value = 0;
    if (d[2] == 0x0a && d[1] == 0x12)
      value = comfort;
    else if (d[1] == 0x00)
      value = -35;
    else if (d[1] == 0x01)
      value = 195;
    else if (d[1] == 0x02)
      value = 220;
    else if (d[1] == 0x03)
      value = 80;
    else if (d[1] == 0x10)
      value = 50;
    else if (d[1] == 0x30)
      value = 120;
    reply(0x26, message_id, {0x01, d[1], d[2], uint8_t(value & 0xFF), uint8_t((value >> 8) & 0xFF), 0, 0});
  } else if (function == 0x24 && d[0] == 0x01) {
    comfort = d[3];
    ESP_LOGI("box", "Comfort set to %u", comfort);
    reply(0x26, message_id, {0x01, d[1], d[2], d[3], 0, 0, 0});
  } else if (function == 0x24 && d[0] == 0x05) {
    ESP_LOGI("box", "Time set to %u", (unsigned) esphome::encode_uint32(d[4], d[3], d[2], d[1]));
    reply(0x26, message_id, {0x05});
  } else {
    ESP_LOGW("box", "RX request fn %02X sub %02X has no model answer, sent only the ack", function, d[0]);
  }
}

inline void poll() {
  if (bus == nullptr)
    return;
  uint8_t byte;
  while (bus->available() && bus->read_byte(&byte))
    rx.push_back(byte);
  while (rx.size() >= 3) {
    if (rx[0] != 0xAA || rx[1] != 0x55) {
      rx.erase(rx.begin());
      continue;
    }
    const size_t total = 2 + rx[2] + 3;
    if (rx.size() < total)
      return;
    std::vector<uint8_t> frame(rx.begin() + 2, rx.begin() + total);
    rx.erase(rx.begin(), rx.begin() + total);
    // One request per call, like a box answering in turn: a burst of answers written at once blocks this
    // loop pass for ~100 ms and overflows the hub's 256 byte UART receive buffer before it can read
    handle(frame);
    return;
  }
}

// Log a published text with its newlines shown as \n, so one state is one line.
inline void show(const char *name, const std::string &text) {
  std::string flat;
  for (char c : text) {
    if (c == '\n')
      flat += "\\n";
    else
      flat += c;
  }
  ESP_LOGI("hwt", "STATE %s = '%s' (%u B)", name, flat.c_str(), (unsigned) text.size());
}

}  // namespace ducosim
