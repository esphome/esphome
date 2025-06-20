#include "iwr6843aop.h"
#include "esp_timer.h"
#include "esphome/core/log.h"
#include "iwr6843aop_cfg.h"
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

namespace esphome {
namespace iwr6843aop {

static const char *const TAG = "iwr6843aop";

// Helper to convert little-endian bytes to float
static float le_bytes_to_float(const uint8_t *data) {
  float value;
  std::memcpy(&value, data, sizeof(float));
  return value;
}

void IWR6843AOPComponent::setup() {
  ESP_LOGD(TAG, "Setting up IWR6843AOPComponent");
  this->last_update_ = esp_timer_get_time() / 1000;
  this->cfg_iwr6843aop();
}

void IWR6843AOPComponent::loop() {
  static uint32_t last_uart2_read = 0;
  uint32_t now = millis();
  if (now - last_uart2_read >= 50) {  // 20 Hz = every 50 ms
    last_uart2_read = now;
    this->read_uart2();
  }
}

void IWR6843AOPComponent::set_float_input(const std::string &key, esphome::number::Number *number) {
  if (key == "corner_1_x")
    corner_1_x_ = number;
  else if (key == "corner_1_y")
    corner_1_y_ = number;
  else if (key == "corner_2_x")
    corner_2_x_ = number;
  else if (key == "corner_2_y")
    corner_2_y_ = number;
}

void IWR6843AOPComponent::cfg_iwr6843aop() {
  uart::UARTDevice uart1_dev(uart1_dev_);

  for (size_t i = 0; i < IWR6843AOP_CFG_LEN; ++i) {
    std::string line_to_send = std::string(IWR6843AOP_CFG[i]) + "\n";
    uart1_dev.write_str(line_to_send.c_str());
    ESP_LOGI(TAG, "UART1 WRITE: %s", IWR6843AOP_CFG[i]);

    vTaskDelay(50 / portTICK_PERIOD_MS);  // Give the device time to respond

    std::string response;
    while (uart1_dev.available()) {
      int c = uart1_dev.read();
      if (c >= 0)
        response += static_cast<char>(c);
    }
    if (!response.empty()) {
      ESP_LOGI(TAG, "UART1 READ: %s", response.c_str());
    } else {
      ESP_LOGI(TAG, "UART1 READ: <no response>");
    }
  }
}

void IWR6843AOPComponent::read_uart2() {
  uart::UARTDevice uart2_dev(uart2_dev_);
  static std::vector<uint8_t> buffer;
  // Read all available bytes into buffer
  int available = uart2_dev.available();

  if (available) {
    ESP_LOGI(TAG, "UART2 available: %d", available);
    for (int i = 0; i < available; ++i) {
      int c = uart2_dev.read();
      if (c >= 0) {
        buffer.push_back(static_cast<uint8_t>(c));
      }
    }
    ESP_LOGI(TAG, "Buffer size after reading: %d", static_cast<int>(buffer.size()));
    // Log buffer contents in hex format
    std::string hex_str;
    char bytebuf[4];
    for (auto b : buffer) {
      snprintf(bytebuf, sizeof(bytebuf), "%02X ", b);
      hex_str += bytebuf;
    }
    ESP_LOGI(TAG, "Buffer contents (hex): %s", hex_str.c_str());
  }

  // Example: parse frames from buffer
  // Replace these constants with your actual protocol values
  const uint8_t MAGIC_WORD[8] = {0x02, 0x01, 0x04, 0x03, 0x06, 0x05, 0x08, 0x07};
  const size_t HEADER_LEN = 40;     // mmWave SDK header is typically 40 bytes
  const size_t TLV_HEADER_LEN = 8;  // TLV header: 4 bytes type, 4 bytes length

  while (true) {
    // Find magic word
    if (buffer.size() >= 8 && std::equal(buffer.begin(), buffer.begin() + 8, std::begin(MAGIC_WORD))) {
      ESP_LOGI(TAG, "Magic word found at buffer start!");
      // ...parse frame...
    } else {
      ESP_LOGW(TAG, "Magic word NOT found at buffer start!");
      buffer.clear();
      break;
    }

    size_t start = 8;  // Start parsing after the magic word
    if (buffer.size() < start + HEADER_LEN)
      break;  // Not enough data for header

    // Get total packet length from header (bytes 12-15, little-endian)
    uint32_t packet_len;
    std::memcpy(&packet_len, &buffer[start + 12], 4);
    if (buffer.size() < start + packet_len)
      break;  // Wait for more data

    // Get number of TLVs (bytes 36-39, little-endian)
    uint32_t num_tlvs;
    std::memcpy(&num_tlvs, &buffer[start + 36], 4);

    // Parse TLVs
    size_t tlv_offset = start + HEADER_LEN;
    for (uint32_t i = 0; i < num_tlvs; ++i) {
      if (tlv_offset + TLV_HEADER_LEN > buffer.size())
        break;
      uint32_t tlv_type, tlv_length;
      std::memcpy(&tlv_type, &buffer[tlv_offset], 4);
      std::memcpy(&tlv_length, &buffer[tlv_offset + 4], 4);

      if (tlv_offset + TLV_HEADER_LEN + tlv_length > buffer.size())
        break;

      // TLV type 1010 is Target List
      if (tlv_type == 1010) {
        std::vector<uint8_t> tlv_payload(buffer.begin() + tlv_offset + TLV_HEADER_LEN,
                                         buffer.begin() + tlv_offset + TLV_HEADER_LEN + tlv_length);

        // Log number of targets detected
        size_t target_struct_size = 4 + 9 * 4 + 16 * 4 + 2 * 4;
        size_t num_targets = tlv_payload.size() / target_struct_size;
        ESP_LOGI("iwr6843aop", "Detected %u targets in TLV 1010", (unsigned int) num_targets);

        this->parse_target_list_tlv(tlv_payload);
      }

      tlv_offset += TLV_HEADER_LEN + tlv_length;
    }

    // Remove parsed frame from buffer
    buffer.erase(buffer.begin(), buffer.begin() + packet_len);
  }
}

void IWR6843AOPComponent::parse_target_list_tlv(const std::vector<uint8_t> &tlv_payload) {
  // Structure: <I f f f f f f f f f f*16 f f>
  // id, posX, posY, posZ, velX, velY, velZ, accX, accY, accZ, ec[16], g, confidence
  const size_t target_struct_size = 4 + 9 * 4 + 16 * 4 + 2 * 4;  // 4 bytes for id, 9 floats, 16 floats, 2 floats
  size_t num_targets = tlv_payload.size() / target_struct_size;

  ESP_LOGI(TAG, "tlv payload length: %d structure size: %d Number of targets: %d", (int) tlv_payload.size(),
           (int) target_struct_size, (int) num_targets);

  for (size_t i = 0; i < num_targets; ++i) {
    size_t offset = i * target_struct_size;
    const uint8_t *ptr = tlv_payload.data() + offset;

    uint32_t id;
    std::memcpy(&id, ptr, 4);
    ptr += 4;
    float posX = le_bytes_to_float(ptr);
    ptr += 4;
    float posY = le_bytes_to_float(ptr);
    ptr += 4;
    float posZ = le_bytes_to_float(ptr);
    ptr += 4;
    float velX = le_bytes_to_float(ptr);
    ptr += 4;
    float velY = le_bytes_to_float(ptr);
    ptr += 4;
    float velZ = le_bytes_to_float(ptr);
    ptr += 4;
    float accX = le_bytes_to_float(ptr);
    ptr += 4;
    float accY = le_bytes_to_float(ptr);
    ptr += 4;
    float accZ = le_bytes_to_float(ptr);
    ptr += 4;

    // Covariance matrix (ec), gating gain (g), confidence
    float ec[16];
    for (int j = 0; j < 16; ++j) {
      ec[j] = le_bytes_to_float(ptr);
      ptr += 4;
    }
    float g = le_bytes_to_float(ptr);
    ptr += 4;
    float confidence = le_bytes_to_float(ptr);
    ptr += 4;

    ESP_LOGI(TAG, "Target %u: x=%.2f y=%.2f z=%.2f", id, posX, posY, posZ);
    // Optionally log velocity, acceleration, etc.
    // ESP_LOGD("iwr6843aop", "vel=(%.2f,%.2f,%.2f) acc=(%.2f,%.2f,%.2f) conf=%.2f", velX, velY, velZ, accX, accY, accZ,
    // confidence);
  }
}

}  // namespace iwr6843aop
}  // namespace esphome