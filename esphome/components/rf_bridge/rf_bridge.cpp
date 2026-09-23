#include "rf_bridge.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>
#include <cstring>

namespace esphome::rf_bridge {

static const char *const TAG = "rf_bridge";

void RFBridgeComponent::ack_() {
  ESP_LOGV(TAG, "Sending ACK");
  this->write(RF_CODE_START);
  this->write(RF_CODE_ACK);
  this->write(RF_CODE_STOP);
  this->flush();
}

bool RFBridgeComponent::parse_bridge_byte_(uint8_t byte) {
  if (this->bucket_frame_candidate_ && byte == RF_CODE_START) {
    // A queued next frame proves the trailing 0x55 really was the bucket
    // frame's terminator: Portisch builds pulse entries from alternating
    // signal edges, so the two level bits inside one pulse byte are always
    // opposite — 0xAA (two high-level nibbles) cannot occur in pulse data.
    // Finalize before this byte starts the new frame, so back-to-back
    // deliveries are split even when loop() never observed a quiet gap
    // between them.
    this->finish_bucket_frame_();
  }
  size_t at = this->rx_buffer_.size();
  this->rx_buffer_.push_back(byte);
  const uint8_t *raw = &this->rx_buffer_[0];

  ESP_LOGVV(TAG, "Processing byte: 0x%02X", byte);

  // Byte 0: Start
  if (at == 0)
    return byte == RF_CODE_START;

  // Byte 1: Action
  if (at == 1)
    return byte >= RF_CODE_ACK && byte <= RF_CODE_RFIN_BUCKET;
  uint8_t action = raw[1];

  switch (action) {
    case RF_CODE_ACK:
      ESP_LOGD(TAG, "Action OK");
      break;
    case RF_CODE_LEARN_KO:
      ESP_LOGD(TAG, "Learning timeout");
      break;
    case RF_CODE_LEARN_OK:
    case RF_CODE_RFIN: {
      if (byte != RF_CODE_STOP || at < RF_MESSAGE_SIZE + 2)
        return true;

      RFBridgeData data;
      data.sync = (raw[2] << 8) | raw[3];
      data.low = (raw[4] << 8) | raw[5];
      data.high = (raw[6] << 8) | raw[7];
      data.code = (raw[8] << 16) | (raw[9] << 8) | raw[10];

      if (action == RF_CODE_LEARN_OK) {
        ESP_LOGD(TAG, "Learning success");
      }

      ESP_LOGI(TAG,
               "Received RFBridge Code: sync=0x%04" PRIX16 " low=0x%04" PRIX16 " high=0x%04" PRIX16
               " code=0x%06" PRIX32,
               data.sync, data.low, data.high, data.code);
      this->data_callback_.call(data);
      break;
    }
    case RF_CODE_LEARN_OK_NEW:
    case RF_CODE_ADVANCED_RFIN: {
      if (byte != RF_CODE_STOP) {
        return at < (raw[2] + 3);
      }

      RFBridgeAdvancedData data{};

      data.length = raw[2];
      data.protocol = raw[3];
      char next_byte[3];  // 2 hex chars + null
      for (uint8_t i = 0; i + 1 < data.length; i++) {
        buf_append_printf(next_byte, sizeof(next_byte), 0, "%02X", raw[4 + i]);
        data.code += next_byte;
      }

      ESP_LOGI(TAG, "Received RFBridge Advanced Code: length=0x%02X protocol=0x%02X code=0x%s", data.length,
               data.protocol, data.code.c_str());
      this->advanced_data_callback_.call(data);
      break;
    }
    case RF_CODE_RFIN_BUCKET: {
      if (at == 2) {
        // The count byte: Portisch sends at most 7 buckets + sync, so 0 or
        // >8 cannot be a genuine capture — reject before it can occupy the
        // buffer for a full frame timeout.
        return byte != 0 && byte <= B1_MAX_BUCKET_COUNT;
      }
      // 0x55 is legal DATA inside a B1 frame: bucket durations are sent
      // with only their HIGH byte masked to 7 bits, so a duration such as
      // 0x0155 puts a raw 0x55 low byte inside the table — the first 0x55
      // must therefore not end the capture. The header declares the table
      // length (raw[2] pairs), so a 0x55 there is always data; one at or
      // past the first pulse index is a terminator CANDIDATE, confirmed
      // once the UART goes quiet (finish_bucket_frame_ in loop()).
      this->bucket_frame_candidate_ = byte == RF_CODE_STOP && at >= 3 + static_cast<size_t>(raw[2]) * 2;
      return true;
    }
    default:
      ESP_LOGW(TAG, "Unknown action: 0x%02X", action);
      break;
  }

  ESP_LOGVV(TAG, "Parsed: 0x%02X", byte);

  if (byte == RF_CODE_STOP && action != RF_CODE_ACK)
    this->ack_();

  // return false to reset buffer
  return false;
}

void RFBridgeComponent::finish_bucket_frame_() {
  if (this->rx_buffer_.size() < 4) {
    // The candidate flag requires a header + non-empty bucket table, so
    // this cannot happen while flag and buffer stay consistent; guard the
    // raw[2] / size-1 reads against any future divergence anyway.
    this->rx_buffer_.clear();
    this->bucket_frame_candidate_ = false;
    return;
  }
  const uint8_t *raw = this->rx_buffer_.data();
  const size_t at = this->rx_buffer_.size() - 1;

  uint8_t buckets = raw[2] << 1;
  std::string str;
  char next_byte[3];  // 2 hex chars + null

  for (uint32_t i = 0; i <= at; i++) {
    buf_append_printf(next_byte, sizeof(next_byte), 0, "%02X", raw[i]);
    str += next_byte;
    if ((i > 3) && buckets) {
      buckets--;
    }
    if ((i < 3) || (buckets % 2) || (i == at - 1)) {
      str += " ";
    }
  }
  ESP_LOGI(TAG, "Received RFBridge Bucket: %s", str.c_str());

  // Deliberately NOT ACKed: Portisch's B1 command handler leaves its
  // last_sniffing_command at the previous mode (RF_CODE_RFIN), and its
  // host-ACK handler re-arms sniffing from that stale value — so ACKing a
  // bucket delivery silently reverts the radio to standard sniffing and
  // ends bucket capture. Its delivery path is fire-and-forget and never
  // waits for a host ACK. Stock Itead firmware never sends B1 frames, so
  // suppressing this ACK cannot change stock-firmware behavior.
  // https://github.com/esphome/esphome/issues/17682

  this->rx_buffer_.clear();
  this->bucket_frame_candidate_ = false;
}

void RFBridgeComponent::write_byte_str_(const std::string &codes) {
  uint8_t code;
  int size = codes.length();
  for (int i = 0; i < size; i += 2) {
    code = strtol(codes.substr(i, 2).c_str(), nullptr, 16);
    this->write(code);
  }
}

void RFBridgeComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  size_t avail = this->available();
  if (avail == 0 && this->bucket_frame_candidate_ && now - this->last_bridge_byte_ > BUCKET_CANDIDATE_QUIET_MS) {
    // The trailing 0x55 was followed by UART quiet, so it really was the
    // frame terminator and not an interior data byte.
    this->finish_bucket_frame_();
    this->last_bridge_byte_ = now;
  }
  const bool receiving_bucket = this->rx_buffer_.size() >= 2 && this->rx_buffer_[1] == RF_CODE_RFIN_BUCKET;
  if (receiving_bucket) {
    // Never declare an in-progress bucket frame dead while its continuation
    // bytes are already queued: a stalled loop() otherwise discards a live
    // frame that the UART buffer proves is still arriving.
    if (avail == 0 && now - this->last_bridge_byte_ > BUCKET_FRAME_TIMEOUT_MS) {
      ESP_LOGD(TAG, "Discarding incomplete RFBridge Bucket frame (%u bytes)",
               static_cast<unsigned>(this->rx_buffer_.size()));
      this->rx_buffer_.clear();
      this->bucket_frame_candidate_ = false;
      this->last_bridge_byte_ = now;
    }
  } else if (now - this->last_bridge_byte_ > 50) {
    this->rx_buffer_.clear();
    this->bucket_frame_candidate_ = false;
    this->last_bridge_byte_ = now;
  }

  while (avail > 0) {
    uint8_t buf[64];
    size_t to_read = std::min(avail, sizeof(buf));
    if (!this->read_array(buf, to_read)) {
      break;
    }
    avail -= to_read;
    for (size_t i = 0; i < to_read; i++) {
      if (this->rx_buffer_.size() > MAX_RX_BUFFER_SIZE) {
        this->rx_buffer_.clear();
        this->bucket_frame_candidate_ = false;
      }
      if (this->parse_bridge_byte_(buf[i])) {
        ESP_LOGVV(TAG, "Parsed: 0x%02X", buf[i]);
        this->last_bridge_byte_ = now;
      } else {
        this->rx_buffer_.clear();
        this->bucket_frame_candidate_ = false;
      }
    }
  }
}

void RFBridgeComponent::send_code(RFBridgeData data) {
  ESP_LOGD(TAG, "Sending code: sync=0x%04" PRIX16 " low=0x%04" PRIX16 " high=0x%04" PRIX16 " code=0x%06" PRIX32,
           data.sync, data.low, data.high, data.code);
  this->write(RF_CODE_START);
  this->write(RF_CODE_RFOUT);
  this->write((data.sync >> 8) & 0xFF);
  this->write(data.sync & 0xFF);
  this->write((data.low >> 8) & 0xFF);
  this->write(data.low & 0xFF);
  this->write((data.high >> 8) & 0xFF);
  this->write(data.high & 0xFF);
  this->write((data.code >> 16) & 0xFF);
  this->write((data.code >> 8) & 0xFF);
  this->write(data.code & 0xFF);
  this->write(RF_CODE_STOP);
  this->flush();
}

void RFBridgeComponent::send_advanced_code(const RFBridgeAdvancedData &data) {
  ESP_LOGD(TAG, "Sending advanced code: length=0x%02X protocol=0x%02X code=0x%s", data.length, data.protocol,
           data.code.c_str());
  this->write(RF_CODE_START);
  this->write(RF_CODE_RFOUT_NEW);
  this->write(data.length & 0xFF);
  this->write(data.protocol & 0xFF);
  this->write_byte_str_(data.code);
  this->write(RF_CODE_STOP);
  this->flush();
}

void RFBridgeComponent::learn() {
  ESP_LOGD(TAG, "Learning mode");
  this->write(RF_CODE_START);
  this->write(RF_CODE_LEARN);
  this->write(RF_CODE_STOP);
  this->flush();
}

void RFBridgeComponent::dump_config() { ESP_LOGCONFIG(TAG, "RF_Bridge:"); }

void RFBridgeComponent::start_advanced_sniffing() {
  ESP_LOGI(TAG, "Advanced Sniffing on");
  this->write(RF_CODE_START);
  this->write(RF_CODE_SNIFFING_ON);
  this->write(RF_CODE_STOP);
  this->flush();
}

void RFBridgeComponent::stop_advanced_sniffing() {
  ESP_LOGI(TAG, "Advanced Sniffing off");
  this->write(RF_CODE_START);
  this->write(RF_CODE_SNIFFING_OFF);
  this->write(RF_CODE_STOP);
  this->flush();
}

void RFBridgeComponent::start_bucket_sniffing() {
  ESP_LOGI(TAG, "Raw Bucket Sniffing on");
  this->write(RF_CODE_START);
  this->write(RF_CODE_RFIN_BUCKET);
  this->write(RF_CODE_STOP);
  this->flush();
}

void RFBridgeComponent::send_raw(const std::string &raw_code) {
  ESP_LOGD(TAG, "Sending Raw Code: %s", raw_code.c_str());

  this->write_byte_str_(raw_code);
  this->flush();
}

void RFBridgeComponent::beep(uint16_t ms) {
  ESP_LOGD(TAG, "Beeping for %hu ms", ms);

  this->write(RF_CODE_START);
  this->write(RF_CODE_BEEP);
  this->write((ms >> 8) & 0xFF);
  this->write(ms & 0xFF);
  this->write(RF_CODE_STOP);
  this->flush();
}

}  // namespace esphome::rf_bridge
