#include "canopen.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");
static constexpr uint32_t SDO_REQUEST_BASE = 0x600;
static constexpr uint32_t SDO_RESPONSE_BASE = 0x580;
static constexpr uint8_t SDO_UPLOAD_REQUEST = 0x40;
static constexpr uint8_t SDO_DOWNLOAD_RESPONSE = 0x60;
static constexpr uint8_t SDO_ABORT = 0x80;

void CANopenComponent::setup() {
  this->bus_->add_callback([this](uint32_t id, bool extended, bool rtr, const std::vector<uint8_t> &data) {
    this->on_frame_(id, extended, rtr, data);
  });
}

void CANopenComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CANopen SDO client:");
  ESP_LOGCONFIG(TAG, "  Response timeout: %" PRIu32 " ms", this->response_timeout_ms_);
}

bool CANopenComponent::enqueue_(const Request &request) {
  if (this->is_failed() || request.node == 0 || request.node > 127 || this->count_ == QUEUE_SIZE) {
    ESP_LOGW(TAG, "SDO request rejected (invalid node or queue full)");
    return false;
  }
  this->requests_[(this->head_ + this->count_) % QUEUE_SIZE] = request;
  this->count_++;
  return true;
}

bool CANopenComponent::read_sdo(uint8_t node, uint16_t index, uint8_t subindex, SDOCallback callback, void *context) {
  return this->enqueue_({node, index, subindex, 0, 0, callback, context});
}

bool CANopenComponent::write_sdo(uint8_t node, uint16_t index, uint8_t subindex, uint32_t value, uint8_t size) {
  if ((size != 1 && size != 2 && size != 4) || (size < 4 && value >= (uint32_t{1} << (size * 8)))) {
    ESP_LOGW(TAG, "SDO write rejected (invalid size or value exceeds width)");
    return false;
  }
  return this->enqueue_({node, index, subindex, value, size, nullptr, nullptr});
}

void CANopenComponent::loop() {
  if (this->count_ == 0)
    return;
  const uint32_t now = App.get_loop_component_start_time();
  if (this->active_) {
    if (now - this->sent_at_ms_ >= this->response_timeout_ms_)
      this->finish_(SDOResult::SDO_RESULT_TIMEOUT);
    return;
  }
  const auto &request = this->requests_[this->head_];
  std::array<uint8_t, 8> data{};
  data[0] = request.size == 0 ? SDO_UPLOAD_REQUEST : (0x23 | ((4 - request.size) << 2));
  data[1] = request.index & 0xFF;
  data[2] = request.index >> 8;
  data[3] = request.subindex;
  for (uint8_t i = 0; i < request.size; i++)
    data[4 + i] = request.value >> (8 * i);
  this->active_ = true;
  this->sent_at_ms_ = now;
  if (this->bus_->send_data(SDO_REQUEST_BASE + request.node, false, false, data.data(), data.size()) != canbus::ERROR_OK)
    this->finish_(SDOResult::SDO_RESULT_TRANSPORT_ERROR);
}

void CANopenComponent::on_frame_(uint32_t id, bool extended, bool rtr, const std::vector<uint8_t> &data) {
  if (!this->active_ || extended || rtr || data.size() != 8)
    return;
  const auto &request = this->requests_[this->head_];
  if (id != SDO_RESPONSE_BASE + request.node || data[1] != (request.index & 0xFF) ||
      data[2] != (request.index >> 8) || data[3] != request.subindex)
    return;
  const uint32_t value = encode_uint32(data[7], data[6], data[5], data[4]);
  if (data[0] == SDO_ABORT) {
    this->finish_(SDOResult::SDO_RESULT_ABORT, value);
    return;
  }
  if (request.size != 0) {
    if (data[0] == SDO_DOWNLOAD_RESPONSE)
      this->finish_(SDOResult::SDO_RESULT_SUCCESS);
    return;
  }
  // Only initiate-upload responses belong to this request. Require expedited,
  // size-indicated payloads so sensor decoding never guesses an object's width.
  if ((data[0] & 0xE0) != 0x40)
    return;
  if ((data[0] & 0x03) != 0x03 || (data[0] & 0x10) != 0) {
    this->finish_(SDOResult::SDO_RESULT_UNSUPPORTED);
    return;
  }
  const uint8_t size = 4 - ((data[0] >> 2) & 0x03);
  if (size == 3) {
    this->finish_(SDOResult::SDO_RESULT_UNSUPPORTED);
    return;
  }
  const uint32_t mask = size == 4 ? UINT32_MAX : ((uint32_t{1} << (size * 8)) - 1);
  this->finish_(SDOResult::SDO_RESULT_SUCCESS, value & mask, size);
}

void CANopenComponent::finish_(SDOResult result, uint32_t value, uint8_t size) {
  const auto request = this->requests_[this->head_];
  this->head_ = (this->head_ + 1) % QUEUE_SIZE;
  this->count_--;
  this->active_ = false;
  if (result == SDOResult::SDO_RESULT_SUCCESS) {
    this->status_clear_warning();
  } else {
    this->status_set_warning();
    ESP_LOGW(TAG, "Node %u SDO 0x%04X:%u failed: result=%u, code=0x%08" PRIX32, request.node, request.index,
             request.subindex, static_cast<unsigned>(result), value);
  }
  if (request.callback != nullptr)
    request.callback(request.context, result, value, size);
}

}  // namespace esphome::canopen
