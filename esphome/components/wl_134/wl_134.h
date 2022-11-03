#pragma once

#include <string>

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace wl_134 {

class Wl134Component : public text_sensor::TextSensor, public Component, public uart::UARTDevice {
 public:
  enum Rfid134Error {
    RFID134_ERROR_NONE,

    // from library
    RFID134_ERROR_PACKET_SIZE = 0x81,
    RFID134_ERROR_PACKET_END_CODE_MISSMATCH,
    RFID134_ERROR_PACKET_CHECKSUM,
    RFID134_ERROR_PACKET_CHECKSUM_INVERT
  };

  struct Rfid134Reading {
    uint16_t country;
    uint64_t id;
    bool isData;
    bool isAnimal;
    uint16_t reserved0;
    uint32_t reserved1;
  };
  // Nothing really public.

  // ========== INTERNAL METHODS ==========
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_do_reset(bool do_reset) { this->do_reset_ = do_reset; }

 private:
  enum DfMp3Packet {
    RFID134_PACKET_START_CODE,
    RFID134_PACKET_ID = 1,
    RFID134_PACKET_COUNTRY = 11,
    RFID134_PACKET_DATA_FLAG = 15,
    RFID134_PACKET_ANIMAL_FLAG = 16,
    RFID134_PACKET_RESERVED0 = 17,
    RFID134_PACKET_RESERVED1 = 21,
    RFID134_PACKET_CHECKSUM = 27,
    RFID134_PACKET_CHECKSUM_INVERT = 28,
    RFID134_PACKET_END_CODE = 29,
    RFID134_PACKET_SIZE
  };

  bool do_reset_;

  Rfid134Error read_packet_();
  uint64_t hex_lsb_ascii_to_uint64_(const uint8_t *text, uint8_t text_size);
};

}  // namespace wl_134
}  // namespace esphome
