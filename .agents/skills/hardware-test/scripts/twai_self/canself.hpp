#pragma once
// TWAI self test without a transceiver: the controller's RX input is routed from its own TX pad through the GPIO
// matrix, the bus runs in NO_ACK mode, and frames go out with the self reception flag, so every frame sent here is
// received by the same controller and runs through the canbus component's normal receive path.
#include <cstring>
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"
#include "esphome/components/esp32_can/esp32_can.h"
#include "esphome/core/log.h"

namespace canself {
template<typename Tag, typename Tag::type M> struct Rob {
  friend typename Tag::type get(Tag) { return M; }
};
struct Handle {
  using type = twai_handle_t esphome::esp32_can::ESP32Can::*;
  friend type get(Handle);
};
template struct Rob<Handle, &esphome::esp32_can::ESP32Can::twai_handle_>;

static constexpr gpio_num_t TX_PAD = GPIO_NUM_25;
static uint32_t sent = 0, failed = 0;

inline void loop_back() {
  gpio_input_enable(TX_PAD);
  esp_rom_gpio_connect_in_signal(TX_PAD, TWAI_RX_IDX, false);
  ESP_LOGI("canself", "TWAI RX now reads the TX pad");
}

inline void send(esphome::esp32_can::ESP32Can *can, uint32_t id, bool ext, bool rtr, const uint8_t *data, uint8_t len) {
  twai_message_t m{};
  m.identifier = id;
  m.data_length_code = len;
  m.flags = TWAI_MSG_FLAG_SELF | (ext ? TWAI_MSG_FLAG_EXTD : 0) | (rtr ? TWAI_MSG_FLAG_RTR : 0);
  if (!rtr && len)
    memcpy(m.data, data, len);
  esp_err_t e = twai_transmit_v2(can->*get(Handle{}), &m, pdMS_TO_TICKS(50));
  if (e == ESP_OK)
    sent++;
  else {
    failed++;
    ESP_LOGW("canself", "transmit 0x%X failed: %s", (unsigned) id, esp_err_to_name(e));
  }
}

// One round of test frames.
inline void round(esphome::esp32_can::ESP32Can *can) {
  static const uint8_t d1[] = {0x01, 0x02};
  static const uint8_t d3[] = {0x03};
  static const uint8_t d4[] = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};
  static const uint8_t d6[] = {0xFF};
  static const uint8_t d7[] = {0x0F};
  send(can, 0x101, false, false, d1, 2);       // F1 std 0x101 data
  send(can, 0x101, false, true, nullptr, 0);   // F2 std 0x101 remote
  send(can, 0x101, true, false, d3, 1);        // F3 ext 0x101
  send(can, 0x12345678, true, false, d4, 8);   // F4 ext 0x12345678
  send(can, 0x1AB, false, false, nullptr, 0);  // F5 std 0x1AB empty
  send(can, 0x7FF, false, false, d6, 1);       // F6 std 0x7FF
  send(can, 0x0FF, false, false, d7, 1);       // F7 std 0x0FF
}
}  // namespace canself
