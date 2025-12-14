#include "hlk_fm22x.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <array>
#include <cinttypes>

namespace esphome::hlk_fm22x {

static const char *const TAG = "hlk_fm22x";

void HlkFm22xComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HLK-FM22X...");
  this->set_enrolling_(false);
  while (this->available()) {
    this->read();
  }
  this->defer([this]() { this->send_command_(HlkFm22xCommand::GET_STATUS); });
}

void HlkFm22xComponent::update() {
  if (this->active_command_ != HlkFm22xCommand::NONE) {
    if (this->wait_cycles_ > 600) {
      ESP_LOGE(TAG, "Command 0x%.2X timed out", this->active_command_);
      if (HlkFm22xCommand::RESET == this->active_command_) {
        this->mark_failed();
      } else {
        this->reset();
      }
    }
  }
  this->recv_command_();
}

void HlkFm22xComponent::enroll_face(const std::string &name, HlkFm22xFaceDirection direction) {
  if (name.length() > 31) {
    ESP_LOGE(TAG, "enroll_face(): name too long '%s'", name.c_str());
    return;
  }
  ESP_LOGI(TAG, "Starting enrollment for %s", name.c_str());
  std::array<uint8_t, 35> data{};
  data[0] = 0;  // admin
  std::copy(name.begin(), name.end(), data.begin() + 1);
  // Remaining bytes are already zero-initialized
  data[33] = (uint8_t) direction;
  data[34] = 10;  // timeout
  this->send_command_(HlkFm22xCommand::ENROLL, data.data(), data.size());
  this->set_enrolling_(true);
}

void HlkFm22xComponent::scan_face() {
  ESP_LOGI(TAG, "Verify face");
  static const uint8_t DATA[] = {0, 0};
  this->send_command_(HlkFm22xCommand::VERIFY, DATA, sizeof(DATA));
}

void HlkFm22xComponent::delete_face(int16_t face_id) {
  ESP_LOGI(TAG, "Deleting face in slot %d", face_id);
  const uint8_t data[] = {(uint8_t) (face_id >> 8), (uint8_t) (face_id & 0xFF)};
  this->send_command_(HlkFm22xCommand::DELETE_FACE, data, sizeof(data));
}

void HlkFm22xComponent::delete_all_faces() {
  ESP_LOGI(TAG, "Deleting all stored faces");
  this->send_command_(HlkFm22xCommand::DELETE_ALL_FACES);
}

void HlkFm22xComponent::get_face_count_() {
  ESP_LOGD(TAG, "Getting face count");
  this->send_command_(HlkFm22xCommand::GET_ALL_FACE_IDS);
}

void HlkFm22xComponent::reset() {
  ESP_LOGI(TAG, "Resetting module");
  this->active_command_ = HlkFm22xCommand::NONE;
  this->wait_cycles_ = 0;
  this->set_enrolling_(false);
  this->send_command_(HlkFm22xCommand::RESET);
}

void HlkFm22xComponent::send_command_(HlkFm22xCommand command, const uint8_t *data, size_t size) {
  ESP_LOGV(TAG, "Send command: 0x%.2X", command);
  if (this->active_command_ != HlkFm22xCommand::NONE) {
    ESP_LOGW(TAG, "Command 0x%.2X already active", this->active_command_);
    return;
  }
  this->wait_cycles_ = 0;
  this->active_command_ = command;
  while (this->available())
    this->read();
  this->write((uint8_t) (START_CODE >> 8));
  this->write((uint8_t) (START_CODE & 0xFF));
  this->write((uint8_t) command);
  uint16_t data_size = size;
  this->write((uint8_t) (data_size >> 8));
  this->write((uint8_t) (data_size & 0xFF));

  uint8_t checksum = 0;
  checksum ^= (uint8_t) command;
  checksum ^= (data_size >> 8);
  checksum ^= (data_size & 0xFF);
  for (size_t i = 0; i < size; i++) {
    this->write(data[i]);
    checksum ^= data[i];
  }

  this->write(checksum);
  this->active_command_ = command;
  this->wait_cycles_ = 0;
}

void HlkFm22xComponent::recv_command_() {
  uint8_t byte, checksum = 0;
  uint16_t length = 0;

  if (this->available() < 7) {
    ++this->wait_cycles_;
    return;
  }
  this->wait_cycles_ = 0;

  if ((this->read() != (uint8_t) (START_CODE >> 8)) || (this->read() != (uint8_t) (START_CODE & 0xFF))) {
    ESP_LOGE(TAG, "Invalid start code");
    return;
  }

  byte = this->read();
  checksum ^= byte;
  HlkFm22xResponseType response_type = (HlkFm22xResponseType) byte;

  byte = this->read();
  checksum ^= byte;
  length = byte << 8;
  byte = this->read();
  checksum ^= byte;
  length |= byte;

  std::vector<uint8_t> data;
  data.reserve(length);
  for (uint16_t idx = 0; idx < length; ++idx) {
    byte = this->read();
    checksum ^= byte;
    data.push_back(byte);
  }

  ESP_LOGV(TAG, "Recv type: 0x%.2X, data: %s", response_type, format_hex_pretty(data).c_str());

  byte = this->read();
  if (byte != checksum) {
    ESP_LOGE(TAG, "Invalid checksum for data. Calculated: 0x%.2X, Received: 0x%.2X", checksum, byte);
    return;
  }
  switch (response_type) {
    case HlkFm22xResponseType::NOTE:
      this->handle_note_(data);
      break;
    case HlkFm22xResponseType::REPLY:
      this->handle_reply_(data);
      break;
    default:
      ESP_LOGW(TAG, "Unexpected response type: 0x%.2X", response_type);
      break;
  }
}

void HlkFm22xComponent::handle_note_(const std::vector<uint8_t> &data) {
  switch (data[0]) {
    case HlkFm22xNoteType::FACE_STATE:
      if (data.size() < 17) {
        ESP_LOGE(TAG, "Invalid face note data size: %u", data.size());
        break;
      }
      {
        int16_t info[8];
        uint8_t offset = 1;
        for (int16_t &i : info) {
          i = ((int16_t) data[offset + 1] << 8) | data[offset];
          offset += 2;
        }
        ESP_LOGV(TAG, "Face state: status: %d, left: %d, top: %d, right: %d, bottom: %d, yaw: %d, pitch: %d, roll: %d",
                 info[0], info[1], info[2], info[3], info[4], info[5], info[6], info[7]);
        this->face_info_callback_.call(info[0], info[1], info[2], info[3], info[4], info[5], info[6], info[7]);
      }
      break;
    case HlkFm22xNoteType::READY:
      ESP_LOGE(TAG, "Command 0x%.2X timed out", this->active_command_);
      switch (this->active_command_) {
        case HlkFm22xCommand::ENROLL:
          this->set_enrolling_(false);
          this->enrollment_failed_callback_.call(HlkFm22xResult::FAILED4_TIMEOUT);
          break;
        case HlkFm22xCommand::VERIFY:
          this->face_scan_invalid_callback_.call(HlkFm22xResult::FAILED4_TIMEOUT);
          break;
        default:
          break;
      }
      this->active_command_ = HlkFm22xCommand::NONE;
      this->wait_cycles_ = 0;
      break;
    default:
      ESP_LOGW(TAG, "Unhandled note: 0x%.2X", data[0]);
      break;
  }
}

void HlkFm22xComponent::handle_reply_(const std::vector<uint8_t> &data) {
  auto expected = this->active_command_;
  this->active_command_ = HlkFm22xCommand::NONE;
  if (data[0] != (uint8_t) expected) {
    ESP_LOGE(TAG, "Unexpected response command. Expected: 0x%.2X, Received: 0x%.2X", expected, data[0]);
    return;
  }

  if (data[1] != HlkFm22xResult::SUCCESS) {
    ESP_LOGE(TAG, "Command <0x%.2X> failed. Error: 0x%.2X", data[0], data[1]);
    switch (expected) {
      case HlkFm22xCommand::ENROLL:
        this->set_enrolling_(false);
        this->enrollment_failed_callback_.call(data[1]);
        break;
      case HlkFm22xCommand::VERIFY:
        if (data[1] == HlkFm22xResult::REJECTED) {
          this->face_scan_unmatched_callback_.call();
        } else {
          this->face_scan_invalid_callback_.call(data[1]);
        }
        break;
      default:
        break;
    }
    return;
  }
  switch (expected) {
    case HlkFm22xCommand::VERIFY: {
      int16_t face_id = ((int16_t) data[2] << 8) | data[3];
      std::string name(data.begin() + 4, data.begin() + 36);
      ESP_LOGD(TAG, "Face verified. ID: %d, name: %s", face_id, name.c_str());
      if (this->last_face_id_sensor_ != nullptr) {
        this->last_face_id_sensor_->publish_state(face_id);
      }
      if (this->last_face_name_text_sensor_ != nullptr) {
        this->last_face_name_text_sensor_->publish_state(name);
      }
      this->face_scan_matched_callback_.call(face_id, name);
      break;
    }
    case HlkFm22xCommand::ENROLL: {
      int16_t face_id = ((int16_t) data[2] << 8) | data[3];
      HlkFm22xFaceDirection direction = (HlkFm22xFaceDirection) data[4];
      ESP_LOGI(TAG, "Face enrolled. ID: %d, Direction: 0x%.2X", face_id, direction);
      this->enrollment_done_callback_.call(face_id, (uint8_t) direction);
      this->set_enrolling_(false);
      this->defer([this]() { this->get_face_count_(); });
      break;
    }
    case HlkFm22xCommand::GET_STATUS:
      if (this->status_sensor_ != nullptr) {
        this->status_sensor_->publish_state(data[2]);
      }
      this->defer([this]() { this->send_command_(HlkFm22xCommand::GET_VERSION); });
      break;
    case HlkFm22xCommand::GET_VERSION:
      if (this->version_text_sensor_ != nullptr) {
        std::string version(data.begin() + 2, data.end());
        this->version_text_sensor_->publish_state(version);
      }
      this->defer([this]() { this->get_face_count_(); });
      break;
    case HlkFm22xCommand::GET_ALL_FACE_IDS:
      if (this->face_count_sensor_ != nullptr) {
        this->face_count_sensor_->publish_state(data[2]);
      }
      break;
    case HlkFm22xCommand::DELETE_FACE:
      ESP_LOGI(TAG, "Deleted face");
      break;
    case HlkFm22xCommand::DELETE_ALL_FACES:
      ESP_LOGI(TAG, "Deleted all faces");
      break;
    case HlkFm22xCommand::RESET:
      ESP_LOGI(TAG, "Module reset");
      this->defer([this]() { this->send_command_(HlkFm22xCommand::GET_STATUS); });
      break;
    default:
      ESP_LOGW(TAG, "Unhandled command: 0x%.2X", this->active_command_);
      break;
  }
}

void HlkFm22xComponent::set_enrolling_(bool enrolling) {
  if (this->enrolling_binary_sensor_ != nullptr) {
    this->enrolling_binary_sensor_->publish_state(enrolling);
  }
}

void HlkFm22xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HLK_FM22X:");
  LOG_UPDATE_INTERVAL(this);
  if (this->version_text_sensor_) {
    LOG_TEXT_SENSOR("  ", "Version", this->version_text_sensor_);
    ESP_LOGCONFIG(TAG, "    Current Value: %s", this->version_text_sensor_->get_state().c_str());
  }
  if (this->enrolling_binary_sensor_) {
    LOG_BINARY_SENSOR("  ", "Enrolling", this->enrolling_binary_sensor_);
    ESP_LOGCONFIG(TAG, "    Current Value: %s", this->enrolling_binary_sensor_->state ? "ON" : "OFF");
  }
  if (this->face_count_sensor_) {
    LOG_SENSOR("  ", "Face Count", this->face_count_sensor_);
    ESP_LOGCONFIG(TAG, "    Current Value: %u", (uint16_t) this->face_count_sensor_->get_state());
  }
  if (this->status_sensor_) {
    LOG_SENSOR("  ", "Status", this->status_sensor_);
    ESP_LOGCONFIG(TAG, "    Current Value: %u", (uint8_t) this->status_sensor_->get_state());
  }
  if (this->last_face_id_sensor_) {
    LOG_SENSOR("  ", "Last Face ID", this->last_face_id_sensor_);
    ESP_LOGCONFIG(TAG, "    Current Value: %u", (int16_t) this->last_face_id_sensor_->get_state());
  }
  if (this->last_face_name_text_sensor_) {
    LOG_TEXT_SENSOR("  ", "Last Face Name", this->last_face_name_text_sensor_);
    ESP_LOGCONFIG(TAG, "    Current Value: %s", this->last_face_name_text_sensor_->get_state().c_str());
  }
}

}  // namespace esphome::hlk_fm22x
