#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "esphome/components/dsmr/dsmr.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::dsmr::testing {

namespace {

// The decryption key of tests/components/dsmr/common.yaml
constexpr const char *DECRYPTION_KEY = "00112233445566778899AABBCCDDEEFF";

// A 76 byte telegram, encrypted with DECRYPTION_KEY: 18 byte header, 76 encrypted bytes, 12 byte GCM tag.
constexpr std::array<uint8_t, 18 + 76 + 12> ENCRYPTED_FRAME = {
    // header: DB 08, system title "DSMRTEST", 82, length 93 (1 + 4 + 76 + 12), 30, invocation counter 1
    0xDB, 0x08, 0x44, 0x53, 0x4D, 0x52, 0x54, 0x45, 0x53, 0x54, 0x82, 0x00, 0x5D, 0x30, 0x00, 0x00, 0x00, 0x01,
    // the encrypted telegram
    0x9C, 0xB9, 0xB6, 0xCB, 0x29, 0x12, 0x42, 0xA5, 0x7E, 0xE1, 0x34, 0x64, 0xAE, 0x8F, 0xC4, 0x2D, 0x84, 0x4F, 0x60,
    0x95, 0xC1, 0x20, 0x62, 0x99, 0x37, 0xA3, 0xBE, 0x4A, 0x6F, 0xD7, 0xB7, 0xB6, 0x17, 0xFD, 0x74, 0xCC, 0x83, 0xF2,
    0x25, 0x3E, 0x6A, 0x45, 0xB4, 0xAD, 0x01, 0x8D, 0x20, 0x34, 0x73, 0x5A, 0x2B, 0xB0, 0x32, 0xE7, 0xB6, 0x28, 0xB2,
    0xBF, 0x35, 0xCE, 0x47, 0x5B, 0xD4, 0xB5, 0xBD, 0x39, 0x74, 0x67, 0x6B, 0x82, 0x50, 0xA0, 0x3B, 0x1A, 0xFB, 0x12,
    // GCM tag
    0xAF, 0x52, 0xA4, 0x33, 0x4B, 0x3A, 0x09, 0x89, 0xBC, 0x7B, 0x40, 0x90};

// The text sensor gets the decrypted telegram up to and with the CRC, without the line end after it.
constexpr const char *EXPECTED_TELEGRAM = "/KFM5KAIFA-METER\r\n"
                                          "\r\n"
                                          "1-0:1.8.1(000671.578*kWh)\r\n"
                                          "1-0:1.7.0(00.318*kW)\r\n"
                                          "!1E1D";

// An encrypted frame ends when no byte arrives for the receive timeout.
constexpr uint32_t RECEIVE_TIMEOUT_MS = 10;

class FakeUART : public uart::UARTComponent {
 public:
  std::vector<uint8_t> rx;

  void write_array(const uint8_t *data, size_t len) override {}
  bool peek_byte(uint8_t *data) override { return false; }
  bool read_array(uint8_t *data, size_t len) override {
    if (this->rx.size() < len)
      return false;
    std::copy(this->rx.begin(), this->rx.begin() + len, data);
    this->rx.erase(this->rx.begin(), this->rx.begin() + len);
    return true;
  }
  size_t available() override { return this->rx.size(); }
  uart::UARTFlushResult flush() override { return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS; }
  void check_logger_conflict() override {}
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif
};

}  // namespace

// The platform's AES-GCM backend decrypts the frame, and the telegram text sensor publishes the telegram.
TEST(DsmrDecryptionTest, PublishesTheDecryptedTelegram) {
  FakeUART uart;
  text_sensor::TextSensor telegram_sensor;
  std::string published;
  telegram_sensor.add_on_state_callback([&published](const std::string &state) { published = state; });

  // CRC check on, telegrams up to 1500 bytes, no request interval, no request pin
  Dsmr dsmr(&uart, true, 1500, 0, RECEIVE_TIMEOUT_MS, nullptr, DECRYPTION_KEY);
  dsmr.set_telegram(&telegram_sensor);
  dsmr.setup();
  dsmr.loop();  // the request starts

  uart.rx.assign(ENCRYPTED_FRAME.begin(), ENCRYPTED_FRAME.end());
  dsmr.loop();  // reads the frame
  std::this_thread::sleep_for(std::chrono::milliseconds(2 * RECEIVE_TIMEOUT_MS));
  dsmr.loop();  // the pause ends the frame: decrypt and publish

  EXPECT_EQ(published, EXPECTED_TELEGRAM);
}

}  // namespace esphome::dsmr::testing
