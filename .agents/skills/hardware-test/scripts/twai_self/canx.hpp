#pragma once
// Extra checks for esphome#21950: classic frame with DLC 9..15 on the wire, and the send_data paths.
#include "canself.hpp"

namespace canx {
using esphome::esp32_can::ESP32Can;

// Classic frame with a non compliant DLC (12): 8 data bytes on the wire, DLC field 12
inline void send_dlc12(ESP32Can *can) {
  twai_message_t m{};
  m.identifier = 0x0ABCDE;
  m.data_length_code = 12;
  m.flags = TWAI_MSG_FLAG_SELF | TWAI_MSG_FLAG_EXTD | TWAI_MSG_FLAG_DLC_NON_COMP;
  for (int i = 0; i < 8; i++)
    m.data[i] = 0xA0 + i;
  esp_err_t e = twai_transmit_v2(can->*get(canself::Handle{}), &m, pdMS_TO_TICKS(50));
  ESP_LOGI("canx", "dlc12 transmit %s", esp_err_to_name(e));
}

inline void tx_status(ESP32Can *can, const char *when) {
  twai_status_info_t s{};
  twai_get_status_info_v2(can->*get(canself::Handle{}), &s);
  ESP_LOGI("canx", "STATUS %s state=%d to_tx=%u tx_err=%u tx_failed=%u rx_missed=%u bus_err=%u", when, (int) s.state,
           (unsigned) s.msgs_to_tx, (unsigned) s.tx_error_counter, (unsigned) s.tx_failed_count,
           (unsigned) s.rx_missed_count, (unsigned) s.bus_error_count);
}

// Classic send_data overloads present on both dev and the PR
inline void tx_classic(ESP32Can *can) {
  static const uint8_t d8[] = {1, 2, 3, 4, 5, 6, 7, 8};
  static const uint8_t d10[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  ESP_LOGI("canx", "TX std8 -> %d", (int) can->send_data(0x201, false, false, d8, 8));
  ESP_LOGI("canx", "TX ext8 -> %d", (int) can->send_data(0x1234567, true, false, d8, 8));
  ESP_LOGI("canx", "TX rtr -> %d", (int) can->send_data(0x202, false, true, nullptr, 0));
  ESP_LOGI("canx", "TX empty -> %d", (int) can->send_data(0x203, false, false, nullptr, 0));
  ESP_LOGI("canx", "TX std10 (clamped) -> %d", (int) can->send_data(0x204, false, false, d10, 10));
  ESP_LOGI("canx", "TX vec3 -> %d", (int) can->send_data(0x205, false, false, std::vector<uint8_t>{9, 8, 7}));
}
}  // namespace canx
