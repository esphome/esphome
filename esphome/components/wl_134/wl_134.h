#pragma once

#include <string>

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
  namespace wl_134 {

    class Wl134Component : public text_sensor::TextSensor, public Component, public uart::UARTDevice {
      public:
        enum Rfid134_Error {
          Rfid134_Error_None,

          // from library
          Rfid134_Error_PacketSize = 0x81,
          Rfid134_Error_PacketEndCodeMissmatch,
          Rfid134_Error_PacketChecksum,
          Rfid134_Error_PacketChecksumInvert
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
        enum DfMp3_Packet {
          Rfid134_Packet_StartCode,
          Rfid134_Packet_Id = 1,
          Rfid134_Packet_Country = 11,
          Rfid134_Packet_DataFlag = 15,
          Rfid134_Packet_AnimalFlag = 16,
          Rfid134_Packet_Reserved0 = 17,
          Rfid134_Packet_Reserved1 = 21,
          Rfid134_Packet_CheckSum = 27,
          Rfid134_Packet_CheckSumInvert = 28,
          Rfid134_Packet_EndCode = 29,
          Rfid134_Packet_SIZE
        };

        bool do_reset_;

        Rfid134_Error read_packet_();
        uint64_t hex_lsb_ascii_to_uint64_(uint8_t* text, uint8_t textSize);
    };

  } // namespace hrxl_maxsonar_wr
} // namespace esphome
