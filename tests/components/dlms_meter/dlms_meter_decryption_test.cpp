#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "esphome/components/dlms_meter/dlms_meter.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/core/application.h"

namespace esphome::dlms_meter::testing {

namespace {

// The keys of tests/components/dlms_meter/common.yaml
constexpr const char *DECRYPTION_KEY = "36C66639E48A8CA4D6BC8B282A793BBB";
constexpr const char *AUTHENTICATION_KEY = "11223344556677889900AABBCCDDEEFF";

// A data notification, encrypted and authenticated with these keys: 16 byte header, 27 encrypted bytes, 12 byte tag.
// Its one structure holds OBIS 1.0.1.8.0.255 with the value 12345, scaler 0 and unit Wh.
constexpr std::array<uint8_t, 16 + 27 + 12> ENCRYPTED_FRAME = {
    // header: DB 08, system title "DLMSTEST", length 44 (1 + 4 + 27 + 12), 30, frame counter 1
    0xDB, 0x08, 0x44, 0x4C, 0x4D, 0x53, 0x54, 0x45, 0x53, 0x54, 0x2C, 0x30, 0x00, 0x00, 0x00, 0x01,
    // the encrypted data notification
    0x1D, 0x3F, 0xB7, 0x84, 0xB0, 0xF5, 0xA7, 0xB0, 0x48, 0x12, 0x8E, 0xEA, 0x40, 0x87, 0xD8, 0x11, 0x2C, 0x85, 0x95,
    0x6F, 0xEF, 0x52, 0x96, 0x91, 0x6C, 0x43, 0x8C,
    // GCM tag
    0x59, 0x41, 0x76, 0xB4, 0xE5, 0x3B, 0xA3, 0xBC, 0x01, 0x51, 0x84, 0xC5};

// A frame ends when no byte arrives for the receive timeout.
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

/// Feeds one frame to a meter with the test keys; false if the energy sensor got no state.
bool publishes_energy(const std::vector<uint8_t> &frame, sensor::Sensor &energy) {
  FakeUART uart;
  // CRC check on, both keys, no custom patterns
  DlmsMeterComponent meter(RECEIVE_TIMEOUT_MS, false, DECRYPTION_KEY, AUTHENTICATION_KEY, {});
  meter.set_uart_parent(&uart);
  meter.register_sensor(dlms_parser::ObisId(1, 0, 1, 8, 0, 255), &energy);
  meter.setup();

  uart.rx = frame;
  meter.loop();  // reads the frame at loop time 0
  // The loop time moves past the receive timeout
  LoopBlockingGuard clock{nullptr, nullptr, RECEIVE_TIMEOUT_MS + 1};
  meter.loop();  // the pause ends the frame: decrypt and publish
  return energy.has_state();
}

}  // namespace

// The platform's AES-GCM backend checks the tag and decrypts the frame, and the sensor gets the energy.
TEST(DlmsMeterDecryptionTest, PublishesTheDecryptedEnergy) {
  sensor::Sensor energy;
  ASSERT_TRUE(publishes_energy({ENCRYPTED_FRAME.begin(), ENCRYPTED_FRAME.end()}, energy));
  EXPECT_FLOAT_EQ(energy.state, 12345.0f);
}

// A frame whose GCM tag does not match is dropped, so the backend really authenticates.
TEST(DlmsMeterDecryptionTest, DropsAFrameWithATamperedTag) {
  std::vector<uint8_t> frame(ENCRYPTED_FRAME.begin(), ENCRYPTED_FRAME.end());
  frame.back() ^= 0x01;
  sensor::Sensor energy;
  EXPECT_FALSE(publishes_energy(frame, energy));
}

}  // namespace esphome::dlms_meter::testing
