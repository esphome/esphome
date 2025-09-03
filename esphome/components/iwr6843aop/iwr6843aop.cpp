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
    this->read_iwr6843aop_data();
  }
}

void IWR6843AOPComponent::set_float_input(const std::string &key, esphome::number::Number *number) {
  if (key == "width")
    width_ = number;
  else if (key == "length")
    length_ = number;
}

void IWR6843AOPComponent::set_binary_sensor(const std::string &key, esphome::binary_sensor::BinarySensor *sensor) {
  if (key == "room_presence")
    room_presence_ = sensor;
  else if (key == "bed_presence")
    bed_presence_ = sensor;
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

void IWR6843AOPComponent::read_iwr6843aop_data() {
  uart::UARTDevice uart2_dev(uart2_dev_);
  static std::vector<uint8_t> buffer;
  // Read all available bytes into buffer
  int available = uart2_dev.available();

  while (available) {
    ESP_LOGD(TAG, "UART2 available: %d", available);
    for (int i = 0; i < available; ++i) {
      int c = uart2_dev.read();
      if (c >= 0) {
        buffer.push_back(static_cast<uint8_t>(c));
      }
    }
    ESP_LOGD(TAG, "Buffer size after reading: %d", static_cast<int>(buffer.size()));
    available = uart2_dev.available();
  }

  // Example: parse frames from buffer
  // Replace these constants with your actual protocol values
  const uint8_t MAGIC_WORD[8] = {0x02, 0x01, 0x04, 0x03, 0x06, 0x05, 0x08, 0x07};

 
  // Find magic word
  if (buffer.size() >= 8 && std::equal(buffer.begin(), buffer.begin() + 8, std::begin(MAGIC_WORD))) {
    ESP_LOGD(TAG, "Magic word found at buffer start!");

    if (buffer.size() < MAGIC_SIZE + HEADER_LEN){
      ESP_LOGD(TAG, "Not enough data to parse header!");
      buffer.clear();
      return;
    }else {
      ESP_LOGD(TAG, "Parsing header");

      // Get total packet length from header (bytes 12-15, little-endian)
      uint32_t packet_len;
      std::memcpy(&packet_len, &buffer[PACKET_LENGTH_OFFSET], PACKET_LENGTH_SIZE);

      if (buffer.size() < packet_len){
        ESP_LOGD(TAG, "Not enough data to parse packet! Expected %u bytes, but only %d available",
                 packet_len, static_cast<int>(buffer.size() - MAGIC_SIZE));
        buffer.clear();
        return;  // Not enough data to parse the full packet
      } else {
        ESP_LOGD(TAG, "Packet length: %u", packet_len);
        // Get number of TLVs (bytes 36-39, little-endian)
        uint32_t num_tlvs;
        std::memcpy(&num_tlvs, &buffer[NUM_TLVS_OFFSET], 4);

        ESP_LOGD(TAG, "Number of TLVs: %u", num_tlvs);

        size_t tlv_offset = HEADER_LEN;  // Start after magic word and header

        // Parse TLVs
        for (uint32_t i = 0; i < num_tlvs; ++i) {
          if (tlv_offset > buffer.size()){
            ESP_LOGD(TAG, "Not enough data to parse TLV header!");
            buffer.clear();
            return;  // Not enough data to parse the TLV header
          }else{
            ESP_LOGD(TAG, "Parsing TLV at offset %u", (unsigned int) tlv_offset);
            uint32_t tlv_type, tlv_length;

            std::memcpy(&tlv_type, &buffer[tlv_offset], 4);
            std::memcpy(&tlv_length, &buffer[tlv_offset + 4], 4);

            ESP_LOGD(TAG, "TLV type: %u, length: %u", tlv_type, tlv_length);

            if (tlv_offset  + tlv_length > buffer.size()){
              ESP_LOGD(TAG, "Not enough data to parse TLV payload! Expected %u bytes, but only %d available",
                       tlv_length, static_cast<int>(buffer.size() - tlv_offset));
            }else{
              // TLV type 1010 is Target List
              if (tlv_type == TLV_TYPE_TARGET_OBJECT_LIST) {
                std::vector<uint8_t> tlv_payload(buffer.begin() + tlv_offset ,
                                                  buffer.begin() + tlv_offset + tlv_length);
                this->parse_target_list_tlv(tlv_payload);
              }
            }
            tlv_offset += tlv_length + TLV_HEADER_SIZE;
          }
        }
      }
    }
    buffer.clear();
  } else {
   // ESP_LOGD(TAG, "Magic word NOT found at buffer start!");
    buffer.clear();
    return;
  }
}

void IWR6843AOPComponent::parse_target_list_tlv(const std::vector<uint8_t> &tlv_payload) {
  size_t num_targets = tlv_payload.size() / TARGET_STRUCT_SIZE;
  ESP_LOGD(TAG, "tlv payload length: %d  Number of targets: %d", (int) tlv_payload.size(), (int) num_targets);
  
  bool room_present = num_targets > 0;
  bool bed_present = false;

  for (size_t i = 0; i < num_targets; ++i) {
    size_t offset = i * TARGET_STRUCT_SIZE + TLV_HEADER_SIZE;
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

    ESP_LOGD(TAG, "Target %u: x=%.2f y=%.2f z=%.2f", id, posX, posY, posZ);
    // Optionally log velocity, acceleration, etc.
    // ESP_LOGD("iwr6843aop", "vel=(%.2f,%.2f,%.2f) acc=(%.2f,%.2f,%.2f) conf=%.2f", velX, velY, velZ, accX, accY, accZ,
    // confidence);

    // Bed presence logic: abs(x) < width and abs(z) < length
    if (width_ && length_) {
      float width_val = width_->state;
      float length_val = length_->state;
      if (std::abs(posX) < width_val && std::abs(posZ) < length_val) {
        bed_present = true;
      }
    }
  }

  if (room_presence_)
    room_presence_->publish_state(room_present);
  if (bed_presence_)
    bed_presence_->publish_state(bed_present);
}

}  // namespace iwr6843aop
}  // namespace esphome