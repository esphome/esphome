#pragma once

#include <utility>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome::rf_bridge {

static const uint8_t RF_MESSAGE_SIZE = 9;
static const uint8_t RF_CODE_START = 0xAA;
static const uint8_t RF_CODE_ACK = 0xA0;
static const uint8_t RF_CODE_LEARN = 0xA1;
static const uint8_t RF_CODE_LEARN_KO = 0xA2;
static const uint8_t RF_CODE_LEARN_OK = 0xA3;
static const uint8_t RF_CODE_RFIN = 0xA4;
static const uint8_t RF_CODE_RFOUT = 0xA5;
static const uint8_t RF_CODE_ADVANCED_RFIN = 0xA6;
static const uint8_t RF_CODE_SNIFFING_ON = 0xA6;
static const uint8_t RF_CODE_SNIFFING_OFF = 0xA7;
static const uint8_t RF_CODE_RFOUT_NEW = 0xA8;
static const uint8_t RF_CODE_LEARN_NEW = 0xA9;
static const uint8_t RF_CODE_LEARN_KO_NEW = 0xAA;
static const uint8_t RF_CODE_LEARN_OK_NEW = 0xAB;
static const uint8_t RF_CODE_RFOUT_BUCKET = 0xB0;
static const uint8_t RF_CODE_RFIN_BUCKET = 0xB1;
static const uint8_t RF_CODE_BEEP = 0xC0;
static const uint8_t RF_CODE_STOP = 0x55;
static const uint8_t RF_DEBOUNCE = 200;
static const size_t MAX_RX_BUFFER_SIZE = 512;
// ~10 byte times at 19200 baud: long enough to prove the UART went quiet
// after a possible bucket-frame terminator, short enough to finish well
// before the next radio capture can be delivered.
static const uint32_t BUCKET_CANDIDATE_QUIET_MS = 5;
// Portisch drains a B1 frame's header, bucket table, and pulse data as
// separate UART writes, so an in-progress bucket frame tolerates a longer
// inter-region gap than the generic 50 ms inter-byte timeout.
static const uint32_t BUCKET_FRAME_TIMEOUT_MS = 250;
// Portisch's uart_put_RF_buckets sends at most 7 buckets plus the sync
// bucket, so a B1 count byte above 8 (or 0) is malformed for any protocol.
static const uint8_t B1_MAX_BUCKET_COUNT = 8;

struct RFBridgeData {
  uint16_t sync;
  uint16_t low;
  uint16_t high;
  uint32_t code;
};

struct RFBridgeAdvancedData {
  uint8_t length;
  uint8_t protocol;
  std::string code;
};

class RFBridgeComponent final : public uart::UARTDevice, public Component {
 public:
  void loop() override;
  void dump_config() override;
  template<typename F> void add_on_code_received_callback(F &&callback) {
    this->data_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_advanced_code_received_callback(F &&callback) {
    this->advanced_data_callback_.add(std::forward<F>(callback));
  }
  void send_code(RFBridgeData data);
  void send_advanced_code(const RFBridgeAdvancedData &data);
  void learn();
  void start_advanced_sniffing();
  void stop_advanced_sniffing();
  void start_bucket_sniffing();
  void send_raw(const std::string &code);
  void beep(uint16_t ms);

 protected:
  void ack_();
  void decode_();
  bool parse_bridge_byte_(uint8_t byte);
  void finish_bucket_frame_();
  void write_byte_str_(const std::string &codes);

  std::vector<uint8_t> rx_buffer_;
  uint32_t last_bridge_byte_{0};
  bool bucket_frame_candidate_{false};

  CallbackManager<void(RFBridgeData)> data_callback_;
  CallbackManager<void(RFBridgeAdvancedData)> advanced_data_callback_;
};

}  // namespace esphome::rf_bridge
