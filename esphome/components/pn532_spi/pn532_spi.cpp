#include "pn532_spi.h"
#include "esphome/core/log.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532_spi {

static const char *TAG = "pn532_spi";

void PN532Spi::setup() {
  ESP_LOGI(TAG, "PN532Spi setup started!");
  this->spi_setup();

  // Wake the chip up from power down
  // 1. Enable the SS line for at least 2ms
  // 2. Send a dummy command to get the protocol synced up
  //    (this may time out, but that's ok)
  // 3. Send SAM config command with normal mode without waiting for ready bit (IRQ not initialized yet)
  // 4. Probably optional, send SAM config again, this time checking ACK and return value
  this->cs_->digital_write(false);
  delay(10);
  ESP_LOGI(TAG, "SPI setup finished!");
  PN532::setup();
}

void PN532Spi::pn532_write_command(const std::vector<uint8_t> &data) {
  this->enable();
  delay(2);
  // First byte, communication mode: Write data
  this->write_byte(0x01);

  // Preamble
  this->write_byte(0x00);

  // Start code
  this->write_byte(0x00);
  this->write_byte(0xFF);

  // Length of message, TFI + data bytes
  const uint8_t real_length = data.size() + 1;
  // LEN
  this->write_byte(real_length);
  // LCS (Length checksum)
  this->write_byte(~real_length + 1);

  // TFI (Frame Identifier, 0xD4 means to PN532, 0xD5 means from PN532)
  this->write_byte(0xD4);
  // calculate checksum, TFI is part of checksum
  uint8_t checksum = 0xD4;

  // DATA
  for (uint8_t dat : data) {
    this->write_byte(dat);
    checksum += dat;
  }

  // DCS (Data checksum)
  this->write_byte(~checksum + 1);
  // Postamble
  this->write_byte(0x00);

  this->disable();
}

std::vector<uint8_t> PN532Spi::pn532_read_data() {
  this->enable();
  delay(2);
  // Read data (transmission from the PN532 to the host)
  this->write_byte(0x03);

  // sometimes preamble is not transmitted for whatever reason
  // mostly happens during startup.
  // just read the first two bytes and check if that is the case
  uint8_t header[6];
  this->read_array(header, 2);
  if (header[0] == 0x00 && header[1] == 0x00) {
    // normal packet, preamble included
    this->read_array(header + 2, 4);
  } else if (header[0] == 0x00 && header[1] == 0xFF) {
    // weird packet, preamble skipped; make it look like a normal packet
    header[0] = 0x00;
    header[1] = 0x00;
    header[2] = 0xFF;
    this->read_array(header + 3, 3);
  } else {
    // invalid packet
    this->disable();
    ESP_LOGV(TAG, "read data invalid preamble!");
    return {};
  }

  bool valid_header = (header[0] == 0x00 &&                                                      // preamble
                       header[1] == 0x00 &&                                                      // start code
                       header[2] == 0xFF && static_cast<uint8_t>(header[3] + header[4]) == 0 &&  // LCS, len + lcs = 0
                       header[5] == 0xD5  // TFI - frame from PN532 to system controller
  );
  if (!valid_header) {
    this->disable();
    ESP_LOGV(TAG, "read data invalid header!");
    return {};
  }

  std::vector<uint8_t> ret;
  // full length of message, including TFI
  const uint8_t full_len = header[3];
  // length of data, excluding TFI
  uint8_t len = full_len - 1;
  if (full_len == 0)
    len = 0;

  ret.resize(len);
  this->read_array(ret.data(), len);

  uint8_t checksum = 0xD5;
  for (uint8_t dat : ret)
    checksum += dat;
  checksum = ~checksum + 1;

  uint8_t dcs = this->read_byte();
  if (dcs != checksum) {
    this->disable();
    ESP_LOGV(TAG, "read data invalid checksum! %02X != %02X", dcs, checksum);
    return {};
  }

  if (this->read_byte() != 0x00) {
    this->disable();
    ESP_LOGV(TAG, "read data invalid postamble!");
    return {};
  }
  this->disable();

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGVV(TAG, "PN532 Data Frame: (%u)", ret.size());  // NOLINT
  for (uint8_t dat : ret) {
    ESP_LOGVV(TAG, "  0x%02X", dat);
  }
#endif

  return ret;
}
bool PN532Spi::is_ready() {
  this->enable();
  // First byte, communication mode: Read state
  this->write_byte(0x02);
  // PN532 returns a single data byte,
  // "After having sent a command, the host controller must wait for bit 0 of Status byte equals 1
  // before reading the data from the PN532."
  bool ret = this->read_byte() == 0x01;
  this->disable();

  if (ret) {
    ESP_LOGVV(TAG, "Chip is ready!");
  }
  return ret;
}
bool PN532Spi::read_ack() {
  ESP_LOGVV(TAG, "Reading ACK...");
  this->enable();
  delay(2);
  // "Read data (transmission from the PN532 to the host) "
  this->write_byte(0x03);

  uint8_t ack[6];
  memset(ack, 0, sizeof(ack));

  this->read_array(ack, 6);
  this->disable();

  bool matches = (ack[0] == 0x00 &&                    // preamble
                  ack[1] == 0x00 &&                    // start of packet
                  ack[2] == 0xFF && ack[3] == 0x00 &&  // ACK packet code
                  ack[4] == 0xFF && ack[5] == 0x00     // postamble
  );
  ESP_LOGVV(TAG, "ACK valid: %s", YESNO(matches));
  return matches;
}

void PN532Spi::dump_config() {
  PN532::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

}  // namespace pn532_spi
}  // namespace esphome
