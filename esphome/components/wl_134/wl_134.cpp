#include "wl_134.h"
#include "esphome/core/log.h"

namespace esphome {
  namespace wl_134 {

    static const char *const TAG = "wl.134.sensor";
    static const uint8_t ASCII_CR = 0x0D;
    static const uint8_t ASCII_NBSP = 0xFF;
    static const int MAX_DATA_LENGTH_BYTES = 6;

    void Wl134Component::setup() {
      this-> publish_state("");
    }

    void Wl134Component::loop() {
      uint8_t data;
      while (this->available() >= Rfid134_Packet_SIZE) {
        Wl134Component::Rfid134_Error error = this->read_packet_();
        if (error != Rfid134_Error_None) {
          ESP_LOGW(TAG, "Error: %d", error);
        }
      }
    }

    Wl134Component::Rfid134_Error Wl134Component::read_packet_() {
      uint8_t packet[Rfid134_Packet_SIZE];
      packet[Rfid134_Packet_StartCode] = this->read();

      // check for the first byte being the packet start code
      if (packet[Rfid134_Packet_StartCode] != 0x02) {
          // just out of sync, ignore until we are synced up
          return Rfid134_Error_None;
      }

      uint8_t read;

      if (! this->read_array(&(packet[Rfid134_Packet_Id]), Rfid134_Packet_SIZE - 1)) {
          return Rfid134_Error_PacketSize;
      }

      if (packet[Rfid134_Packet_EndCode] != 0x03) {
          return Rfid134_Error_PacketEndCodeMissmatch;
      }

      // calculate checksum
      uint8_t checksum = 0;
      for (uint8_t i = Rfid134_Packet_Id; i < Rfid134_Packet_CheckSum; i++) {
          checksum = checksum ^ packet[i];
      }

      // test checksum
      if (checksum != packet[Rfid134_Packet_CheckSum]) {
          return Rfid134_Error_PacketChecksum;
      }

      if (static_cast<uint8_t>(~checksum) != static_cast<uint8_t>(packet[Rfid134_Packet_CheckSumInvert])) {
          return Rfid134_Error_PacketChecksumInvert;
      }

      Rfid134Reading reading;

      // convert packet into the reading struct
      reading.id = this->hex_lsb_ascii_to_uint64_(&(packet[Rfid134_Packet_Id]), Rfid134_Packet_Country - Rfid134_Packet_Id);
      reading.country = this->hex_lsb_ascii_to_uint64_(&(packet[Rfid134_Packet_Country]), Rfid134_Packet_DataFlag - Rfid134_Packet_Country);
      reading.isData = packet[Rfid134_Packet_DataFlag] == '1';
      reading.isAnimal = packet[Rfid134_Packet_AnimalFlag] == '1';
      reading.reserved0 = this->hex_lsb_ascii_to_uint64_(&(packet[Rfid134_Packet_Reserved0]), Rfid134_Packet_Reserved1 - Rfid134_Packet_Reserved0);
      reading.reserved1 = this->hex_lsb_ascii_to_uint64_(&(packet[Rfid134_Packet_Reserved1]), Rfid134_Packet_CheckSum - Rfid134_Packet_Reserved1);

      ESP_LOGV(TAG, "Tag id:    %012lld", reading.id);
      ESP_LOGV(TAG, "Country:   %03d", reading.country);
      ESP_LOGV(TAG, "isData:    %s", reading.isData?"true":"false");
      ESP_LOGV(TAG, "isAnimal:  %s", reading.isAnimal?"true":"false");
      ESP_LOGV(TAG, "Reserved0: %d", reading.reserved0);
      ESP_LOGV(TAG, "Reserved1: %d", reading.reserved1);

      char buf[20];
      sprintf(buf, "%03d%012lld", reading.country, reading.id);
      this->publish_state(buf);
      if (this->do_reset_) { this-> publish_state(""); }

      return Rfid134_Error_None;
    }

    uint64_t Wl134Component::hex_lsb_ascii_to_uint64_(uint8_t* text, uint8_t textSize) {
        uint64_t value = 0;
        uint8_t i = textSize;
        do  {
            i--;

            uint8_t digit = text[i];
            if (digit >= 'A') {
                digit = digit - 'A' + 10;
            } else {
                digit = digit - '0';
            }
            value = (value << 4) + digit;
        } while (i != 0);

        return value;
    }

    void Wl134Component::dump_config() {
      ESP_LOGCONFIG(TAG, "WL-134 Sensor:");
      LOG_TEXT_SENSOR("", "Tag", this);
      // As specified in the sensor's data sheet
      this->check_uart_settings(9600, 1, esphome::uart::UART_CONFIG_PARITY_NONE, 8);
    }
  }
}
