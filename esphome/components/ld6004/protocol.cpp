#include "protocol.h"
#include <cmath>
#include <cstring>

namespace esphome::ld6004 {
bool valid_bounds(float minimum, float maximum) {
  return std::isfinite(minimum) && std::isfinite(maximum) && minimum < maximum;
}
uint32_t read_u32(const uint8_t *data) {
  return uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
}
float read_float(const uint8_t *data) {
  const uint32_t bits = read_u32(data);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}
void write_u32(uint8_t *data, uint32_t value) {
  for (size_t i = 0; i < 4; ++i)
    data[i] = uint8_t(value >> (8 * i));
}
void write_float(uint8_t *data, float value) {
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  write_u32(data, bits);
}
size_t encode(uint16_t id, uint16_t type, const uint8_t *data, uint16_t length, uint8_t *output) {
  output[0] = 1;
  output[1] = id >> 8;
  output[2] = id;
  output[3] = length >> 8;
  output[4] = length;
  output[5] = type >> 8;
  output[6] = type;
  output[7] = 255;
  for (size_t i = 0; i < 7; ++i)
    output[7] ^= output[i];
  if (length == 0)
    return 8;
  uint8_t sum = 255;
  for (size_t i = 0; i < length; ++i) {
    output[8 + i] = data[i];
    sum ^= data[i];
  }
  output[8 + length] = sum;
  return length + 9;
}
bool Parser::feed(uint8_t byte, uint32_t now) {
  if (this->position_ != 0 && now - this->last_byte_ > FRAME_TIMEOUT_MS) {
    this->position_ = 0;
    ++this->errors_;
  }
  this->last_byte_ = now;
  if (this->position_ == 0 && byte != 1)
    return false;
  if (this->position_ < 8) {
    this->header_[this->position_++] = byte;
    if (this->position_ != 8)
      return false;
    uint8_t sum = 0;
    for (uint8_t value : this->header_)
      sum ^= value;
    const uint16_t length = (uint16_t(this->header_[3]) << 8) | this->header_[4];
    if (sum != 255) {
      this->position_ = 0;
      ++this->errors_;
      return false;
    }
    this->frame_.id = (uint16_t(this->header_[1]) << 8) | this->header_[2];
    this->frame_.type = (uint16_t(this->header_[5]) << 8) | this->header_[6];
    this->frame_.length = length;
    if (length > MAX_PAYLOAD)
      ++this->errors_;
    this->checksum_ = 255;
    if (length == 0) {
      this->position_ = 0;
      return true;
    }
    return false;
  }
  if (this->position_ < size_t(this->frame_.length) + 8) {
    if (this->frame_.length <= MAX_PAYLOAD)
      this->frame_.data[this->position_ - 8] = byte;
    ++this->position_;
    this->checksum_ ^= byte;
    return false;
  }
  this->position_ = 0;
  if (this->frame_.length > MAX_PAYLOAD)
    return false;
  if (byte == this->checksum_)
    return true;
  ++this->errors_;
  return false;
}
bool decode_targets(const uint8_t *data, size_t length, Target *targets, size_t capacity, size_t &count) {
  if (length < 4)
    return false;
  const uint32_t size = read_u32(data);
  if (size > capacity || length != 4 + size * 20)
    return false;
  for (size_t i = 0; i < size; ++i) {
    const uint8_t *point = data + 4 + 20 * i;
    Target target{read_float(point),
                  read_float(point + 4),
                  read_float(point + 8),
                  static_cast<int32_t>(read_u32(point + 12)),
                  static_cast<int32_t>(read_u32(point + 16)),
                  true};
    if (!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z))
      return false;
    targets[i] = target;
  }
  count = size;
  return true;
}
bool valid_report(const Frame &frame) {
  const size_t length = frame.length;
  const uint8_t *data = frame.data.data();
  if (length > MAX_PAYLOAD)
    return false;
  if (length == 0) {
    return frame.type == MSG_CONTROL || (frame.type >= MSG_SET_ZONE && frame.type <= MSG_SET_OUTPUT_INTERVAL) ||
           frame.type == MSG_SET_BAUD_RATE || frame.type == MSG_VERSION;
  }
  switch (frame.type) {
    case MSG_TARGETS:
      return length >= 4 && read_u32(data) <= MAX_TARGETS && length == 4 + 20 * read_u32(data);
    case MSG_AREA_PRESENCE:
      if (length != 16)
        return false;
      for (size_t i = 0; i < 4; ++i) {
        if (read_u32(data + 4 * i) > 1)
          return false;
      }
      return true;
    case MSG_INTERFERENCE_ZONES:
    case MSG_DETECTION_ZONES:
    case MSG_DWELL_ZONES:
      if (length != 96)
        return false;
      for (size_t i = 0; i < 24; ++i) {
        if (!std::isfinite(read_float(data + 4 * i)))
          return false;
      }
      return true;
    case MSG_HOLD_DELAY:
    case MSG_SLEEP_TIME:
    case MSG_DWELL_LIFETIME:
    case MSG_OUTPUT_INTERVAL:
      return length == 4;
    case MSG_Z_RANGE:
      return length == 8 && std::isfinite(read_float(data)) && std::isfinite(read_float(data + 4)) &&
             read_float(data) <= read_float(data + 4);
    case MSG_SENSITIVITY:
    case MSG_TRIGGER_SPEED:
      return length == 1 && data[0] <= 2;
    case MSG_INSTALLATION_MODE:
    case MSG_WORK_STATUS:
      return length == 1 && data[0] <= 1;
    case MSG_WORK_MODE:
      return (length == 1 && data[0] <= 4) || (length == 4 && read_u32(data) <= 4);
    case MSG_P20_MODE:
      return length == 1 && data[0] <= 5;
    case MSG_VERSION:
      return length == 4;
    default:
      return false;
  }
}
bool Tracker::update(const uint8_t *data, size_t length, uint32_t now) {
  std::array<Target, MAX_TARGETS> incoming{};
  size_t count;
  if (!decode_targets(data, length, incoming.data(), incoming.size(), count))
    return false;
  for (size_t i = 0; i < count; ++i) {
    for (size_t j = 0; j < i; ++j) {
      if (incoming[i].id == incoming[j].id)
        return false;
    }
  }
  std::array<bool, MAX_TARGETS> used{};
  // Retain identities before filling holes, including when input order changes.
  for (Target &slot : this->slots_) {
    bool found = false;
    if (slot.active) {
      for (size_t i = 0; i < count; ++i) {
        if (slot.id == incoming[i].id) {
          slot = incoming[i];
          used[i] = true;
          found = true;
          break;
        }
      }
    }
    slot.active = found;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!used[i]) {
      for (Target &slot : this->slots_) {
        if (!slot.active) {
          slot = incoming[i];
          break;
        }
      }
    }
  }
  this->count_ = count;
  this->valid_ = true;
  this->updated_ = now;
  return true;
}
bool Tracker::expire(uint32_t now, uint32_t timeout) {
  if (!this->valid_ || now - this->updated_ <= timeout)
    return false;
  this->valid_ = false;
  for (Target &slot : this->slots_)
    slot.active = false;
  return true;
}
bool make_number_command(uint8_t index, float value, Command &command) {
  if (index > 5 || index == 1 || index == 2 || !std::isfinite(value) || value < 0 || value > 16777215 ||
      std::floor(value) != value || (index == 5 && value < 1))
    return false;
  command = Command{};
  command.type =
      index == 0 ? MSG_SET_HOLD_DELAY
                 : (index == 3 ? MSG_SET_SLEEP_TIME : (index == 4 ? MSG_SET_DWELL_LIFETIME : MSG_SET_OUTPUT_INTERVAL));
  command.query = index == 0 ? CMD_QUERY_HOLD_DELAY
                             : (index == 3 ? CMD_QUERY_SLEEP_TIME
                                           : (index == 4 ? CMD_QUERY_DWELL_LIFETIME : CMD_QUERY_OUTPUT_INTERVAL));
  command.report = index == 0 ? MSG_HOLD_DELAY
                              : (index == 3 ? MSG_SLEEP_TIME : (index == 4 ? MSG_DWELL_LIFETIME : MSG_OUTPUT_INTERVAL));
  command.length = 4;
  write_u32(command.data.data(), uint32_t(value));
  return true;
}
bool make_select_command(uint8_t index, size_t value, Command &command) {
  static constexpr uint8_t CHOICES[][6]{{CMD_SENSITIVITY_LOW, CMD_SENSITIVITY_MEDIUM, CMD_SENSITIVITY_HIGH, 0, 0, 0},
                                        {CMD_TRIGGER_SLOW, CMD_TRIGGER_MEDIUM, CMD_TRIGGER_FAST, 0, 0, 0},
                                        {CMD_INSTALLATION_TOP, CMD_INSTALLATION_SIDE, 0, 0, 0, 0},
                                        {CMD_WORK_NORMAL, CMD_WORK_LOW_POWER, CMD_WORK_RADAR_OFF_HIGH,
                                         CMD_WORK_RADAR_OFF_LOW, CMD_WORK_HIGH_REFLECTIVITY, 0},
                                        {CMD_P20_PRESENCE_HIGH, CMD_P20_PRESENCE_LOW, CMD_P20_CONSTANT_LOW,
                                         CMD_P20_CONSTANT_HIGH, CMD_P20_PULSE_LOW, CMD_P20_PULSE_HIGH}};
  static constexpr uint8_t QUERIES[]{CMD_QUERY_SENSITIVITY, CMD_QUERY_TRIGGER_SPEED, CMD_QUERY_INSTALLATION_MODE,
                                     CMD_QUERY_WORK_MODE, CMD_QUERY_P20_MODE};
  static constexpr uint16_t REPORTS[]{MSG_SENSITIVITY, MSG_TRIGGER_SPEED, MSG_INSTALLATION_MODE, MSG_WORK_MODE,
                                      MSG_P20_MODE};
  if (index > 4 || value > 5 || CHOICES[index][value] == 0)
    return false;
  command = Command{};
  command.type = MSG_CONTROL;
  command.length = 4;
  write_u32(command.data.data(), CHOICES[index][value]);
  command.query = QUERIES[index];
  command.report = REPORTS[index];
  return true;
}
bool CommandQueue::push(const Command &command) {
  if (this->blocked_ || this->size_ == this->queue_.size() || command.length > command.data.size())
    return false;
  this->queue_[(this->head_ + this->size_) % this->queue_.size()] = command;
  ++this->size_;
  return true;
}
void CommandQueue::recover() {
  this->size_ = 0;
  this->head_ = 0;
  this->attempts_ = 0;
  this->blocked_ = false;
  this->uncertain_ = false;
  this->have_rx_id_ = false;
  this->output_ack_ = 0;
  this->z_min_ = NAN;
  this->z_max_ = NAN;
}
void CommandQueue::block_() {
  ++this->failures_;
  this->blocked_ = true;
  this->size_ = 0;
  this->attempts_ = 0;
  this->output_ack_ = 0;
}
bool CommandQueue::consume_output_ack(bool &value) {
  if (this->output_ack_ == 0)
    return false;
  value = this->output_ack_ == CMD_ENABLE_TARGETS;
  this->output_ack_ = 0;
  return true;
}
void CommandQueue::finish_() {
  if (this->uncertain_) {
    this->block_();
    return;
  }
  this->head_ = (this->head_ + 1) % this->queue_.size();
  --this->size_;
  this->attempts_ = 0;
}
const Command *CommandQueue::poll(uint32_t now) {
  if (this->size_ == 0)
    return nullptr;
  Command &command = this->queue_[this->head_];
  if (this->attempts_ != 0 && now - this->sent_at_ < COMMAND_TIMEOUT_MS)
    return nullptr;
  if (this->attempts_ >= (command.retry ? MAX_ATTEMPTS : 1)) {
    this->block_();
    return nullptr;
  }
  if (this->attempts_ == 0) {
    if (command.z_bound != 0) {
      const float minimum = command.z_bound == 1 ? command.z_value : this->z_min_;
      const float maximum = command.z_bound == 2 ? command.z_value : this->z_max_;
      if (!valid_bounds(minimum, maximum)) {
        ++this->failures_;
        this->finish_();
        return nullptr;
      }
      write_float(command.data.data(), minimum);
      write_float(command.data.data() + 4, maximum);
      command.z_bound = 0;
    }
    if (++this->next_id_ == 0)
      ++this->next_id_;
    command.id = this->next_id_;
  }
  if (this->attempts_ != 0)
    this->uncertain_ = true;
  ++this->attempts_;
  this->sent_at_ = now;
  return &command;
}
bool CommandQueue::reply(uint16_t id, uint16_t type, size_t length, uint32_t now) {
  // RX is processed before poll(), so enforce expiry here as well.
  if (this->size_ != 0 && this->attempts_ != 0 && now - this->sent_at_ >= COMMAND_TIMEOUT_MS) {
    const Command &command = this->queue_[this->head_];
    if (!command.retry || this->attempts_ >= MAX_ATTEMPTS) {
      this->block_();
    } else {
      this->uncertain_ = true;
    }
    return false;
  }
  // Captured firmware uses its own sequence counter, not the request ID.
  // After silence allow a restarted radar to begin its sequence again.
  if (this->have_rx_id_ && now - this->last_rx_time_ <= RX_SEQUENCE_TIMEOUT_MS) {
    const uint16_t delta = id - this->last_rx_id_;
    if (delta == 0 || delta >= 0x8000)
      return false;
  }
  this->have_rx_id_ = true;
  this->last_rx_id_ = id;
  this->last_rx_time_ = now;
  if (this->size_ == 0 || this->attempts_ == 0)
    return false;
  Command &command = this->queue_[this->head_];
  if (type == command.type && length == 0) {
    if (command.query != 0) {
      write_u32(command.data.data(), command.query);
      command.type = MSG_CONTROL;
      command.length = 4;
      command.query = 0;
      command.retry = true;
      this->attempts_ = 0;
    } else if (command.report == 0) {
      if (this->uncertain_) {
        this->block_();
        return false;
      }
      if (command.type == MSG_CONTROL && command.length == 4 && command.data[0] >= CMD_ENABLE_TARGETS &&
          command.data[0] <= CMD_DISABLE_TARGETS)
        this->output_ack_ = command.data[0];
      this->finish_();
    }
    return true;
  }
  if (command.report != 0 && type == command.report && length != 0 && command.query == 0) {
    this->finish_();
    return true;
  }
  return false;
}
}  // namespace esphome::ld6004
