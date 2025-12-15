#pragma once

#include <map>
#include <string>
#include <vector>
#include <algorithm>

namespace esphome::light {
struct ChannelMap {
 private:
  struct Channel {
    bool exists_ = false;
    int8_t position_ = -1;
  };

  std::map<std::string, Channel> channels_{
      {"R", Channel()}, {"G", Channel()}, {"B", Channel()}, {"W", Channel()}, {"CW", Channel()}, {"WW", Channel()},
  };

  uint8_t channel_count_ = -1;

  bool set_channel_(const std::string &channel_name, int index) {
    for (auto &[name, channel] : this->channels_) {
      if (channel_name == name) {
        channel.exists_ = true;
        channel.position_ = index;
        return true;
      }
    }
    return false;  // Channel not found
  }

 public:
  bool is_rgb() const {
    return this->channels_.at("R").exists_ && this->channels_.at("G").exists_ && this->channels_.at("B").exists_ &&
           !this->channels_.at("W").exists_ && !this->channels_.at("CW").exists_ && !this->channels_.at("WW").exists_;
  }

  bool is_rgbw() const {
    return this->channels_.at("R").exists_ && this->channels_.at("G").exists_ && this->channels_.at("B").exists_ &&
           this->channels_.at("W").exists_ && !this->channels_.at("CW").exists_ && !this->channels_.at("WW").exists_;
  }

  bool is_rgbcct() const {
    return this->channels_.at("R").exists_ && this->channels_.at("G").exists_ && this->channels_.at("B").exists_ &&
           !this->channels_.at("W").exists_ && this->channels_.at("CW").exists_ && this->channels_.at("WW").exists_;
  }

  uint8_t get_channel_count() const { return this->channel_count_; }

  uint8_t *get_pointer_position(uint8_t *base_ptr, const std::string &channel_name) const {
    auto it = this->channels_.find(channel_name);
    if (it != this->channels_.end() && it->second.exists_) {
      return base_ptr + it->second.position_;
    }
    return nullptr;  // Channel does not exist
  }

  void from_string(const std::string &map) {
    std::string s = map;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });

    size_t start = 0, pos = 0;
    int index = 0;
    while ((pos = s.find(',', start)) != std::string::npos) {
      this->set_channel_(s.substr(start, pos - start), index);
      start = pos + 1;
      index++;
    }
    if (this->set_channel_(s.substr(start), index)) {  // Last channel
      index++;  // Only increment when a valid channel was found and a channel was actually set
    }

    this->channel_count_ = index;
  }

  std::string to_string() const {
    std::vector<std::string> tokens(this->channels_.size(), "");
    for (const auto &[name, channel] : this->channels_) {
      if (channel.exists_) {
        tokens[channel.position_] = name;
      }
    }
    std::string result;
    for (const auto &t : tokens) {
      if (t.empty()) {
        continue;
      }
      if (!result.empty()) {
        result += ",";
      }
      result += t;
    }
    return result;
  }
};
}  // namespace esphome::light
