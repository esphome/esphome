#include "pn532_i2c.h"
#include "esphome/core/log.h"

// Based on:
// - https://cdn-shop.adafruit.com/datasheets/PN532C106_Application+Note_v1.2.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/AN133910.pdf
// - https://www.nxp.com/docs/en/nxp/application-notes/153710.pdf

namespace esphome {
namespace pn532_i2c {

static const char *TAG = "pn532_i2c";

void PN532I2C::pn532_write_command(const std::vector<uint8_t> &data) {
  std::vector<uint8_t> write_data;
  // Preamble
  write_data.push_back(0x00);

  // Start code
  write_data.push_back(0x00);
  write_data.push_back(0xFF);

  // Length of message, TFI + data bytes
  const uint8_t real_length = data.size() + 1;
  // LEN
  write_data.push_back(real_length);
  // LCS (Length checksum)
  write_data.push_back(~real_length + 1);

  // TFI (Frame Identifier, 0xD4 means to PN532, 0xD5 means from PN532)
  write_data.push_back(0xD4);
  // calculate checksum, TFI is part of checksum
  uint8_t checksum = 0xD4;

  // DATA
  for (uint8_t dat : data) {
    write_data.push_back(dat);
    checksum += dat;
  }

  // DCS (Data checksum)
  write_data.push_back(~checksum + 1);
  // Postamble
  write_data.push_back(0x00);

  this->write_bytes_raw(write_data);
}

std::vector<uint8_t> PN532I2C::pn532_read_data() {
  // sometimes preamble is not transmitted for whatever reason
  // mostly happens during startup.
  // just read the first two bytes and check if that is the case
  uint8_t header[6];
  this->read_bytes_raw(header, 2);
  if (header[0] == 0x00 && header[1] == 0x00) {
    // normal packet, preamble included
    this->read_bytes_raw(header + 2, 4);
  } else if (header[0] == 0x00 && header[1] == 0xFF) {
    // weird packet, preamble skipped; make it look like a normal packet
    header[0] = 0x00;
    header[1] = 0x00;
    header[2] = 0xFF;
    this->read_bytes_raw(header + 3, 3);
  } else {
    // invalid packet
    ESP_LOGV(TAG, "read data invalid preamble!");
    return {};
  }

  bool valid_header = (header[0] == 0x00 &&                                                      // preamble
                       header[1] == 0x00 &&                                                      // start code
                       header[2] == 0xFF && static_cast<uint8_t>(header[3] + header[4]) == 0 &&  // LCS, len + lcs = 0
                       header[5] == 0xD5  // TFI - frame from PN532 to system controller
  );
  if (!valid_header) {
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
  this->read_bytes_raw(ret.data(), len);

  uint8_t checksum = 0xD5;
  for (uint8_t dat : ret)
    checksum += dat;
  checksum = ~checksum + 1;

  std::vector<uint8_t> dcs(1);
  this->read_bytes_raw(dcs.data(), 1);
  if (dcs[0] != checksum) {
    ESP_LOGV(TAG, "read data invalid checksum! %02X != %02X", dcs[0], checksum);
    return {};
  }

  std::vector<uint8_t> postamble(1);
  this->read_bytes_raw(postamble.data(), 1);
  if (postamble[0] != 0x00) {
    ESP_LOGV(TAG, "read data invalid postamble!");
    return {};
  }

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGVV(TAG, "PN532 Data Frame: (%u)", ret.size());  // NOLINT
  for (uint8_t dat : ret) {
    ESP_LOGVV(TAG, "  0x%02X", dat);
  }
#endif

  return ret;
}
bool PN532I2C::read_ack() {
  ESP_LOGVV(TAG, "Reading ACK...");

  uint8_t ack[6];
  memset(ack, 0, sizeof(ack));

  this->read_bytes_raw(ack, 6);

  bool matches = (ack[0] == 0x00 &&                    // preamble
                  ack[1] == 0x00 &&                    // start of packet
                  ack[2] == 0xFF && ack[3] == 0x00 &&  // ACK packet code
                  ack[4] == 0xFF && ack[5] == 0x00     // postamble
  );
  ESP_LOGVV(TAG, "ACK valid: %s", YESNO(matches));
  return matches;
}

void PN532I2C::dump_config() {
  PN532::dump_config();
  LOG_I2C_DEVICE(this);
}

}  // namespace pn532_i2c
}  // namespace esphome
