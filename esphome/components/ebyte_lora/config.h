#pragma once
#include <utility>
#include <vector>
#include "esphome/core/log.h"
namespace esphome {
namespace ebyte_lora {

// check your data sheet to see what the values are, since each module does it diffrent

enum ENABLE_BYTE { EBYTE_ENABLED = 0b1, EBYTE_DISABLED = 0b0 };

enum AIR_DATA_RATE {
  AIR_2_4kb = 0b000,
  AIR_4_8kb = 0b011,
  AIR_9_6kb = 0b100,
  AIR_19_2kb = 0b101,
  AIR_38_4kb = 0b110,
  AIR_62_5kb = 0b111
};
enum UART_BPS_SPEED {
  UART_1200 = 0b000,
  UART_2400 = 0b001,
  UART_4800 = 0b010,
  UART_9600 = 0b011,
  UART_19200 = 0b100,
  UART_38400 = 0b101,
  UART_57600 = 0b110,
  UART_115200 = 0b111
};
enum UART_PARITY_SETTING { EBYTE_UART_8N1 = 0b00, EBYTE_UART_8O1 = 0b01, EBYTE_UART_8E1 = 0b10 };
struct REG0 {
  uint8_t air_data_rate : 3;
  std::string air_data_rate_description_() {
    switch (this->air_data_rate) {
      case AIR_2_4kb:
        return "air_data_rate: 2.4kb";
      case AIR_4_8kb:
        return "air_data_rate: 4.8kb";
      case AIR_9_6kb:
        return "air_data_rate: 9.6kb";
      case AIR_19_2kb:
        return "air_data_rate: 19.2kb";
      case AIR_38_4kb:
        return "air_data_rate: 38.4kb";
      case AIR_62_5kb:
        return "air_data_rate: 62.5kb";
      default:
        return "";
    }
  }
  uint8_t parity : 2;
  std::string parity_description_() {
    switch (this->parity) {
      case EBYTE_UART_8N1:
        return "uart_parity: 8N1";
      case EBYTE_UART_8O1:
        return "uart_parity: 8O1";
      case EBYTE_UART_8E1:
        return "uart_parity: 8E1";
      default:
        return "";
    }
  }
  uint8_t uart_baud : 3;
  std::string uart_baud_description_() {
    switch (this->uart_baud) {
      case UART_1200:
        return "uart_baud: 1200";
      case UART_2400:
        return "uart_baud: 2400";
      case UART_4800:
        return "uart_baud: 4800";
      case UART_9600:
        return "uart_baud: 9600";
      case UART_19200:
        return "uart_baud: 19200";
      case UART_38400:
        return "uart_baud: 38400";
      case UART_57600:
        return "uart_baud: 57600";
      case UART_115200:
        return "uart_baud: 115200";
      default:
        return "";
    }
  }
};
enum TRANSMISSION_POWER {
  TX_DEFAULT_MAX = 0b00,
  TX_LOWER = 0b01,
  TX_EVEN_LOWER = 0b10,
  TX_LOWEST = 0b11

};
enum SUB_PACKET_SETTING { SUB_200b = 0b00, SUB_128b = 0b01, SUB_64b = 0b10, SUB_32b = 0b11 };
// again in reverse order on the data sheet
struct REG1 {
  uint8_t transmission_power : 2;
  std::string transmission_power_description_() {
    switch (this->transmission_power) {
      case TX_DEFAULT_MAX:
        return "transmission_power: default or max";
      case TX_LOWER:
        return "transmission_power: lower";
      case TX_EVEN_LOWER:
        return "transmission_power: even lower";
      case TX_LOWEST:
        return "transmission_power: Lowest";
      default:
        return "";
    }
  }
  uint8_t reserve : 3;
  uint8_t rssi_noise : 1;
  std::string rssi_noise_description_() {
    switch (this->rssi_noise) {
      case EBYTE_ENABLED:
        return "rssi_noise: ENABLED";
      case EBYTE_DISABLED:
        return "rssi_noise: DISABLED";
      default:
        return "";
    }
  }
  uint8_t sub_packet : 2;
  std::string sub_packet_description_() {
    switch (this->sub_packet) {
      case SUB_200b:
        return "sub_packet: 200 bytes";

      case SUB_128b:
        return "sub_packet: 128 bytes";

      case SUB_64b:
        return "sub_packet: 64 bytes";

      case SUB_32b:
        return "sub_packet: 32 bytes";
      default:
        return "";
    }
  }
};
enum TRANSMISSION_MODE { TRANSPARENT = 0b0, FIXED = 0b1 };
enum WOR_PERIOD {
  WOR_500 = 0b000,
  WOR_1000 = 0b001,
  WOR_1500 = 0b010,
  WOR_2000 = 0b011,
  WOR_2500 = 0b100,
  WOR_3000 = 0b101,
  WOR_3500 = 0b110,
  WOR_4000 = 0b111

};
// reverse order on the data sheet
struct REG3 {
  uint8_t wor_period : 3;
  std::string wor_period_description_() {
    switch (this->wor_period) {
      case WOR_500:
        return "wor_period: 500";
      case WOR_1000:
        return "wor_period: 1000";
      case WOR_1500:
        return "wor_period: 1500";
      case WOR_2000:
        return "wor_period: 2000";
      case WOR_2500:
        return "wor_period: 2500";
      case WOR_3000:
        return "wor_period: 3000";
      case WOR_3500:
        return "wor_period: 3500";
      case WOR_4000:
        return "wor_period: 4000";
      default:
        return "";
    }
  }
  uint8_t reserve1 : 1;
  uint8_t enable_lbt : 1;
  std::string enable_lbt_description_() {
    switch (this->enable_lbt) {
      case EBYTE_ENABLED:
        return "enable_lbt: ENABLED";
      case EBYTE_DISABLED:
        return "enable_lbt: DISABLED";
      default:
        return "";
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
      case EBYTE_ENABLED:
        return "enable_rssi: ENABLED";
      case EBYTE_DISABLED:
        return "enable_rssi: DISABLED";
      default:
        return "";
    }
  }
};
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
