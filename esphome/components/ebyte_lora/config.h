#pragma once
#include <utility>
#include <vector>
#include "esphome/core/log.h"
namespace esphome {
namespace ebyte_lora {

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
  std::string air_data_rate_description_() {
    switch (this->air_data_rate) {
      case _2_4kb:
        return "air_data_rate: 2.4kb";
      case _4_8kb:
        return "air_data_rate: 4.8kb";
      case _9_6kb:
        return "air_data_rate: 9.6kb";
      case _19_2kb:
        return "air_data_rate: 19.2kb";
      case _38_4kb:
        return "air_data_rate: 38.4kb";
      case _62_5kb:
        return "air_data_rate: 62.5kb";
      default:
        return "";
    }
  }
  uint8_t parity : 2;
  std::string parity_description_() {
    switch (this->parity) {
      case _8N1:
        return "uart_parity: 8N1";
      case _8O1:
        return "uart_parity: 8O1";
      case _8E1:
        return "uart_parity: 8E1";
      default:
        return "";
    }
  }
  uint8_t uart_baud : 3;
  std::string uart_baud_description_() {
    switch (this->uart_baud) {
      case _1200:
        return "uart_baud: 1200";
      case _2400:
        return "uart_baud: 2400";
      case _4800:
        return "uart_baud: 4800";
      case _9600:
        return "uart_baud: 9600";
      case _19200:
        return "uart_baud: 19200";
      case _38400:
        return "uart_baud: 38400";
      case _57600:
        return "uart_baud: 57600";
      case _115200:
        return "uart_baud: 115200";
      default:
        return "";
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
  std::string transmission_power_description_() {
    switch (this->transmission_power) {
      case _DEFAULT_MAX:
        return "transmission_power: default or max";
      case _LOWER:
        return "transmission_power: lower";
      case _EVEN_LOWER:
        return "transmission_power: even lower";
      case _LOWEST:
        return "transmission_power: Lowest";
      default:
        return "";
    }
  }
  uint8_t reserve : 3;
  uint8_t rssi_noise : 1;
  std::string rssi_noise_description_() {
    switch (this->rssi_noise) {
      case ENABLED:
        return "rssi_noise: ENABLED";
      case DISABLED:
        return "rssi_noise: DISABLED";
      default:
        return "";
    }
  }
  uint8_t sub_packet : 2;
  std::string sub_packet_description_() {
    switch (this->sub_packet) {
      case _200b:
        return "sub_packet: 200 bytes";

      case _128b:
        return "sub_packet: 128 bytes";

      case _64b:
        return "sub_packet: 64 bytes";

      case _32b:
        return "sub_packet: 32 bytes";
      default:
        return "";
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
  std::string wor_period_description_() {
    switch (this->wor_period) {
      case _500:
        return "wor_period: 500";

      case _1000:
        return "wor_period: 1000";

      case _1500:
        return "wor_period: 1500";

      case _2000:
        return "wor_period: 2000";

      case _2500:
        return "wor_period: 2500";

      case _3000:
        return "wor_period: 3000";

      case _3500:
        return "wor_period: 3500";

      case _4000:
        return "wor_period: 4000";
      default:
        return "";
    }
  }
  uint8_t reserve1 : 1;
  uint8_t enable_lbt : 1;
  std::string enable_lbt_description_() {
    switch (this->enable_lbt) {
      case ENABLED:
        return "enable_lbt: ENABLED";

      case DISABLED:
        return "enable_lbt: DISABLED";

      default:
    }
  }
  uint8_t reserve2 : 1;
  uint8_t transmission_mode : 1;
  std::string transmission_type_description_() {
    switch (this->transmission_mode) {
      case TRANSPARENT:
        return "transmission_type: TRANSPARENT";

      case FIXED:
        return "transmission_type: FIXED";
      default:
        return "";
    }
  }
  uint8_t enable_rssi : 1;
  std::string enable_rssi_description_() {
    switch (this->enable_rssi) {
      case ENABLED:
        return "enable_rssi: ENABLED";

      case DISABLED:
        return "enable_rssi: DISABLED";
      default:
        return "";
    }
  }
};
static const uint8_t OPERATING_FREQUENCY = 700;
struct RegisterConfig {
  uint8_t command = 0;
  uint8_t starting_address = 0;
  uint8_t length = 0;
  uint8_t addh = 0;
  std::string addh_description_() { return "addh:" + this->addh; }
  uint8_t addl = 0;
  std::string addl_description_() { return "addl:" + this->addh; }
  struct REG0 reg_0;
  struct REG1 reg_1;
  // reg2
  uint8_t channel;
  std::string channel_description_() { return "channel:" + this->channel; }
  struct REG3 reg_3;
  uint8_t crypt_h;
  uint8_t crypt_l;
};
}  // namespace ebyte_lora
}  // namespace esphome
