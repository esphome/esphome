#pragma once
#include <utility>
#include <vector>
#include "esphome/core/log.h"
namespace esphome {
namespace ebyte_lora {

// check your data sheet to see what the values are, since each module does it diffrent

enum EnableByte : uint8_t { EBYTE_ENABLED = 0b1, EBYTE_DISABLED = 0b0 };

enum AirDataRate : uint8_t {
  AIR_2_4kb = 0b000,
  AIR_4_8kb = 0b011,
  AIR_9_6kb = 0b100,
  AIR_19_2kb = 0b101,
  AIR_38_4kb = 0b110,
  AIR_62_5kb = 0b111
};
enum UartBpsSpeed : uint8_t {
  UART_1200 = 0b000,
  UART_2400 = 0b001,
  UART_4800 = 0b010,
  UART_9600 = 0b011,
  UART_19200 = 0b100,
  UART_38400 = 0b101,
  UART_57600 = 0b110,
  UART_115200 = 0b111
};
enum UartParitySetting : uint8_t { EBYTE_UART_8N1 = 0b00, EBYTE_UART_8O1 = 0b01, EBYTE_UART_8E1 = 0b10 };
enum TransmissionPower : uint8_t {
  TX_DEFAULT_MAX = 0b00,
  TX_LOWER = 0b01,
  TX_EVEN_LOWER = 0b10,
  TX_LOWEST = 0b11

};
enum SubPacketSetting : uint8_t { SUB_200b = 0b00, SUB_128b = 0b01, SUB_64b = 0b10, SUB_32b = 0b11 };
// again in reverse order on the data sheet

enum TransmissionMode { TRANSPARENT = 0b0, FIXED = 0b1 };
enum WorPeriod : uint8_t {
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

struct RegisterConfig {
  struct {
    uint8_t command : 8;
    uint8_t starting_address : 8;
    uint8_t length : 8;
    uint8_t addh : 8;
    uint8_t addl : 8;
    struct {
      uint8_t air_data_rate : 3;
      uint8_t parity : 2;
      uint8_t uart_baud : 3;

    } reg_0;
    struct {
      uint8_t transmission_power : 2;
      uint8_t reserve : 3;
      uint8_t rssi_noise : 1;
      uint8_t sub_packet : 2;
    } reg_1;
    // reg2
    uint8_t channel : 8;
    struct {
      uint8_t wor_period : 3;
      uint8_t reserve1 : 1;
      uint8_t enable_lbt : 1;
      uint8_t reserve2 : 1;
      uint8_t transmission_mode : 1;
      uint8_t enable_rssi : 1;
    } reg_3;
    uint8_t crypt_h : 8;
    uint8_t crypt_l : 8;
  };
} __attribute__((packed));

}  // namespace ebyte_lora
}  // namespace esphome
