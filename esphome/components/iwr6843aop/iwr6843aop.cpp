#include "iwr6843aop.h"
#include "iwr6843aop_cfg.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace iwr6843aop {

static const char *const TAG = "iwr6843aop";

static float le_bytes_to_float(const uint8_t *data) {
  float value;
  std::memcpy(&value, data, sizeof(float));
  return value;
}

void IWR6843AOPComponent::setup() {
  // Only proceed if UART devices are available
  if (uart1_dev_ == nullptr || uart2_dev_ == nullptr) {
    ESP_LOGW(TAG, "IWR6843AOP component disabled - UART devices not configured");
    return;
  }
  
  ESP_LOGD(TAG, "Setting up IWR6843AOPComponent - initialization deferred to loop");
  this->last_update_ = millis();
  this->init_start_time_ = millis();
  this->init_delay_ = 10000; // 10 second delay after boot
}

void IWR6843AOPComponent::loop() {
  // Only proceed if UART devices are available
  if (uart1_dev_ == nullptr || uart2_dev_ == nullptr) {
    return;
  }
  
  uint32_t now = millis();
  
  // Handle delayed initialization
  if (!configured_ && !config_in_progress_ && (now - init_start_time_) >= init_delay_) {
    ESP_LOGI(TAG, "Starting delayed IWR6843AOP initialization...");
    this->cfg_iwr6843aop();
    config_in_progress_ = true;
  }
  
  // Handle step-by-step configuration
  if (config_in_progress_) {
    this->process_config_step();
  }
  
  // Only read data if configured
  if (configured_) {
    static uint32_t last_uart2_read = 0;
    if (now - last_uart2_read >= 50) {  // 20 Hz = every 50 ms
      last_uart2_read = now;
      this->read_iwr6843aop_data();
    }
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
  // This function now just starts the configuration process
  ESP_LOGI(TAG, "Starting IWR6843AOP configuration...");
  this->config_step_ = 0;
  this->config_start_time_ = millis();
  this->config_timeout_ = 1000; // 1 second timeout per command
  this->config_response_.clear();
}

void IWR6843AOPComponent::process_config_step() {
  uart::UARTDevice uart1_dev(uart1_dev_);
  uint32_t now = millis();
  
  // Check if we've completed all configuration steps
  if (config_step_ >= IWR6843AOP_CFG_LEN) {
    ESP_LOGI(TAG, "IWR6843AOP configuration completed");
    config_in_progress_ = false;
    configured_ = true;
    return;
  }
  
  // Check for timeout on current command
  if ((now - config_start_time_) > config_timeout_) {
    ESP_LOGW(TAG, "UART1 READ: <timeout after %dms> for command: %s", config_timeout_, IWR6843AOP_CFG[config_step_]);
    // Move to next command
    config_step_++;
    config_start_time_ = now;
    config_response_.clear();
    return;
  }
  
  // If this is the first time processing this step, send the command
  if (config_response_.empty() && (now - config_start_time_) < 100) {
    std::string line_to_send = std::string(IWR6843AOP_CFG[config_step_]) + "\n";
    uart1_dev.write_str(line_to_send.c_str());
    ESP_LOGI(TAG, "UART1 WRITE: %s", IWR6843AOP_CFG[config_step_]);
  }
  
  // Read response
  while (uart1_dev.available()) {
    int c = uart1_dev.read();
    if (c >= 0) {
      config_response_ += static_cast<char>(c);
      // Check for end of response (newline or carriage return)
      if (c == '\n' || c == '\r') {
        if (!config_response_.empty()) {
          ESP_LOGI(TAG, "UART1 READ: %s", config_response_.c_str());
        }
        // Move to next command
        config_step_++;
        config_start_time_ = now;
        config_response_.clear();
        return;
      }
    }
  }
}

void IWR6843AOPComponent::read_iwr6843aop_data() {
  // Check if UART2 is available and working
  if (uart2_dev_ == nullptr) {
    return;
  }
  
  // Skip data reading entirely since UART2 has rx flow thresh error
  // This prevents hanging and watchdog timeouts
  ESP_LOGD(TAG, "UART2 data reading skipped due to UART2 configuration error");
}

void IWR6843AOPComponent::parse_target_list_tlv(const std::vector<uint8_t> &tlv_payload) {
  // Simple placeholder - just log that we received data
  ESP_LOGD(TAG, "parse_target_list_tlv called with %d bytes", (int)tlv_payload.size());
}

}  // namespace iwr6843aop
}  // namespace esphome