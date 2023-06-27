#include "wl_134.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wl_134 {

static const char *const TAG = "wl_134.sensor";
static const uint8_t ASCII_CR = 0x0D;
static const uint8_t ASCII_NBSP = 0xFF;
static const int MAX_DATA_LENGTH_BYTES = 6;

void Wl134Component::setup() { this->publish_state(""); }

void Wl134Component::loop() {
  while (this->available() >= RFID134_PACKET_SIZE) {
    Wl134Component::Rfid134Error error = this->read_packet_();
    if (error != RFID134_ERROR_NONE) {
      ESP_LOGW(TAG, "Error: %d", error);
    }
  }
}

Wl134Component::Rfid134Error Wl134Component::read_packet_() {
  uint8_t packet[RFID134_PACKET_SIZE];
  packet[RFID134_PACKET_START_CODE] = this->read();

  // check for the first byte being the packet start code
  if (packet[RFID134_PACKET_START_CODE] != 0x02) {
    // just out of sync, ignore until we are synced up
    return RFID134_ERROR_NONE;
  }

  if (!this->read_array(&(packet[RFID134_PACKET_ID]), RFID134_PACKET_SIZE - 1)) {
    return RFID134_ERROR_PACKET_SIZE;
  }

  if (packet[RFID134_PACKET_END_CODE] != 0x03) {
    return RFID134_ERROR_PACKET_END_CODE_MISSMATCH;
  }

  // calculate checksum
  uint8_t checksum = 0;
  for (uint8_t i = RFID134_PACKET_ID; i < RFID134_PACKET_CHECKSUM; i++) {
    checksum = checksum ^ packet[i];
  }

  // test checksum
  if (checksum != packet[RFID134_PACKET_CHECKSUM]) {
    return RFID134_ERROR_PACKET_CHECKSUM;
  }

  if (static_cast<uint8_t>(~checksum) != static_cast<uint8_t>(packet[RFID134_PACKET_CHECKSUM_INVERT])) {
    return RFID134_ERROR_PACKET_CHECKSUM_INVERT;
  }

  Rfid134Reading reading;

  // convert packet into the reading struct
  reading.id = this->hex_lsb_ascii_to_uint64_(&(packet[RFID134_PACKET_ID]), RFID134_PACKET_COUNTRY - RFID134_PACKET_ID);
  reading.country = this->hex_lsb_ascii_to_uint64_(&(packet[RFID134_PACKET_COUNTRY]),
                                                   RFID134_PACKET_DATA_FLAG - RFID134_PACKET_COUNTRY);
  reading.isData = packet[RFID134_PACKET_DATA_FLAG] == '1';
  reading.isAnimal = packet[RFID134_PACKET_ANIMAL_FLAG] == '1';
  reading.reserved0 = this->hex_lsb_ascii_to_uint64_(&(packet[RFID134_PACKET_RESERVED0]),
                                                     RFID134_PACKET_RESERVED1 - RFID134_PACKET_RESERVED0);
  reading.reserved1 = this->hex_lsb_ascii_to_uint64_(&(packet[RFID134_PACKET_RESERVED1]),
                                                     RFID134_PACKET_CHECKSUM - RFID134_PACKET_RESERVED1);

  ESP_LOGV(TAG, "Tag id:    %012lld", reading.id);
  ESP_LOGV(TAG, "Country:   %03d", reading.country);
  ESP_LOGV(TAG, "isData:    %s", reading.isData ? "true" : "false");
  ESP_LOGV(TAG, "isAnimal:  %s", reading.isAnimal ? "true" : "false");
  ESP_LOGV(TAG, "Reserved0: %d", reading.reserved0);
  ESP_LOGV(TAG, "Reserved1: %d", reading.reserved1);

  char buf[20];
  sprintf(buf, "%03d%012lld", reading.country, reading.id);
  this->publish_state(buf);
  if (this->do_reset_) {
    this->set_timeout(1000, [this]() { this->publish_state(""); });
  }

  return RFID134_ERROR_NONE;
}

uint64_t Wl134Component::hex_lsb_ascii_to_uint64_(const uint8_t *text, uint8_t text_size) {
  uint64_t value = 0;
  uint8_t i = text_size;
  do {
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
}  // namespace wl_134
}  // namespace esphome
