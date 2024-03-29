#pragma once
#include <utility>
#include <vector>
#include "esphome/core/log.h"
namespace esphome {
namespace ebyte_lora {
static const char *const TAG = "ebyte_lora";
// check your data sheet to see what the values are, since each module does it diffrent

enum ENABLE_BYTE { ENABLED = 0b1, DISABLED = 0b0 };

enum AIR_DATA_RATE {
  _2_4kb = 0b000,
  _4_8kb = 0b011,
  _9_6kb = 0b100,
  _19_2kb = 0b101,
  _38_4kb = 0b110,
  _62_5kb = 0b111
};
enum UART_BPS_SPEED {
  _1200 = 0b000,
  _2400 = 0b001,
  _4800 = 0b010,
  _9600 = 0b011,
  _19200 = 0b100,
  _38400 = 0b101,
  _57600 = 0b110,
  _115200 = 0b111
};
enum UART_PARITY { _8N1 = 0b00, _8O1 = 0b01, _8E1 = 0b10 };
struct REG0 {
  uint8_t air_data_rate : 3;
  void air_data_rate_description_() {
    switch (this->air_data_rate) {
      case _2_4kb:
        ESP_LOGD(TAG, "air_data_rate: 2.4kb");
        break;
      case _4_8kb:
        ESP_LOGD(TAG, "air_data_rate: 4.8kb");
        break;
      case _9_6kb:
        ESP_LOGD(TAG, "air_data_rate: 9.6kb");
        break;
      case _19_2kb:
        ESP_LOGD(TAG, "air_data_rate: 19.2kb");
        break;
      case _38_4kb:
        ESP_LOGD(TAG, "air_data_rate: 38.4kb");
        break;
      case _62_5kb:
        ESP_LOGD(TAG, "air_data_rate: 62.5kb");
        break;
      default:
        break;
    }
  }
  uint8_t parity : 2;
  void parity_description_() {
    switch (this->parity) {
      case _8N1:
        ESP_LOGD(TAG, "uart_parity: 8N1");
        break;
      case _8O1:
        ESP_LOGD(TAG, "uart_parity: 8O1");
        break;
      case _8E1:
        ESP_LOGD(TAG, "uart_parity: 8E1");
        break;
      default:
        break;
    }
  }
  uint8_t uart_baud : 3;
  void uart_baud_description_() {
    switch (this->uart_baud) {
      case _1200:
        ESP_LOGD(TAG, "uart_baud: 1200");
        break;
      case _2400:
        ESP_LOGD(TAG, "uart_baud: 2400");
        break;
      case _4800:
        ESP_LOGD(TAG, "uart_baud: 4800");
        break;
      case _9600:
        ESP_LOGD(TAG, "uart_baud: 9600");
        break;
      case _19200:
        ESP_LOGD(TAG, "uart_baud: 19200");
        break;
      case _38400:
        ESP_LOGD(TAG, "uart_baud: 38400");
        break;
      case _57600:
        ESP_LOGD(TAG, "uart_baud: 57600");
        break;
      case _115200:
        ESP_LOGD(TAG, "uart_baud: 115200");
        break;
      default:
        break;
    }
  }
};
enum TRANSMISSION_POWER {
  _DEFAULT_MAX = 0b00,
  _LOWER = 0b01,
  _EVEN_LOWER = 0b10,
  _LOWEST = 0b11

};
enum SUB_PACKET_SETTING { _200b = 0b00, _128b = 0b01, _64b = 0b10, _32b = 0b11 };
// again in reverse order on the data sheet
struct REG1 {
  uint8_t transmission_power : 2;
  void transmission_power_description_() {
    switch (this->transmission_power) {
      case _DEFAULT_MAX:
        ESP_LOGD(TAG, "transmission_power: default or max");
        break;
      case _LOWER:
        ESP_LOGD(TAG, "transmission_power: lower");
        break;
      case _EVEN_LOWER:
        ESP_LOGD(TAG, "transmission_power: even lower");
        break;
      case _LOWEST:
        ESP_LOGD(TAG, "transmission_power: Lowest");
        break;
      default:
        break;
    }
  }
  uint8_t reserve : 3;
  uint8_t rssi_noise : 1;
  void rssi_noise_description_() {
    switch (this->rssi_noise) {
      case ENABLED:
        ESP_LOGD(TAG, "rssi_noise: ENABLED");
        break;
      case DISABLED:
        ESP_LOGD(TAG, "rssi_noise: DISABLED");
        break;
      default:
        break;
    }
  }
  uint8_t sub_packet : 2;
  void sub_packet_description_() {
    switch (this->sub_packet) {
      case _200b:
        ESP_LOGD(TAG, "sub_packet: 200 bytes");
        break;
      case _128b:
        ESP_LOGD(TAG, "sub_packet: 128 bytes");
        break;
      case _64b:
        ESP_LOGD(TAG, "sub_packet: 64 bytes");
        break;
      case _32b:
        ESP_LOGD(TAG, "sub_packet: 32 bytes");
        break;
      default:
        break;
    }
  }
};
enum TRANSMISSION_MODE { TRANSPARENT = 0b0, FIXED = 0b1 };
enum WOR_PERIOD {
  _500 = 0b000,
  _1000 = 0b001,
  _1500 = 0b010,
  _2000 = 0b011,
  _2500 = 0b100,
  _3000 = 0b101,
  _3500 = 0b110,
  _4000 = 0b111

};
// reverse order on the data sheet
struct REG3 {
  uint8_t wor_period : 3;
  void wor_period_description_() {
    switch (this->wor_period) {
      case _500:
        ESP_LOGD(TAG, "wor_period: 500");
        break;
      case _1000:
        ESP_LOGD(TAG, "wor_period: 1000");
        break;
      case _1500:
        ESP_LOGD(TAG, "wor_period: 1500");
        break;
      case _2000:
        ESP_LOGD(TAG, "wor_period: 2000");
        break;
      case _2500:
        ESP_LOGD(TAG, "wor_period: 2500");
        break;
      case _3000:
        ESP_LOGD(TAG, "wor_period: 3000");
        break;
      case _3500:
        ESP_LOGD(TAG, "wor_period: 3500");
        break;
      case _4000:
        ESP_LOGD(TAG, "wor_period: 4000");
        break;
      default:
        break;
    }
  }
  uint8_t reserve1 : 1;
  uint8_t enable_lbt : 1;
  void enable_lbt_description_() {
    switch (this->enable_lbt) {
      case ENABLED:
        ESP_LOGD(TAG, "enable_lbt: ENABLED");
        break;
      case DISABLED:
        ESP_LOGD(TAG, "enable_lbt: DISABLED");
        break;
      default:
        break;
    }
  }
  uint8_t reserve2 : 1;
  uint8_t transmission_mode : 1;
  void transmission_type_description_() {
    switch (this->transmission_mode) {
      case TRANSPARENT:
        ESP_LOGD(TAG, "transmission_type: TRANSPARENT");
        break;
      case FIXED:
        ESP_LOGD(TAG, "transmission_type: FIXED");
        break;
      default:
        break;
    }
  }
  uint8_t enable_rssi : 1;
  void enable_rssi_description_() {
    switch (this->enable_rssi) {
      case ENABLED:
        ESP_LOGD(TAG, "enable_rssi: ENABLED");
        break;
      case DISABLED:
        ESP_LOGD(TAG, "enable_rssi: DISABLED");
        break;
      default:
        break;
    }
  }
};
static const uint8_t OPERATING_FREQUENCY = 700;
struct RegisterConfig {
  uint8_t command = 0;
  uint8_t starting_address = 0;
  uint8_t length = 0;
  uint8_t addh = 0;
  void addh_description_() { ESP_LOGD(TAG, "addh: %u", this->addh); }
  uint8_t addl = 0;
  void addl_description_() { ESP_LOGD(TAG, "addl: %u", this->addh); }
  struct REG0 reg_0;
  struct REG1 reg_1;
  // reg2
  uint8_t channel;
  void channel_description_() { ESP_LOGD(TAG, "channel: %u", this->channel); }
  struct REG3 reg_3;
  uint8_t crypt_h;
  uint8_t crypt_l;
};
}  // namespace ebyte_lora
}  // namespace esphome
