#pragma once

#include <string>
#include <vector>
#include <algorithm>

namespace esphome::light {
struct ChannelMap {
 public:
  enum class ChannelName : uint8_t { R, G, B, W, CW, WW };

 private:
  struct Channel {
   public:
    ChannelName name{};
    std::string friendly_name{};
    bool exists_{};
    uint8_t index_{};
  };

  // Create all channels
  Channel r_{ChannelName::R, "R", false, 0};
  Channel g_{ChannelName::G, "G", false, 0};
  Channel b_{ChannelName::B, "B", false, 0};
  Channel w_{ChannelName::W, "W", false, 0};
  Channel cw_{ChannelName::CW, "CW", false, 0};
  Channel ww_{ChannelName::WW, "WW", false, 0};

  // Allows iterating over all channels
  std::vector<Channel> channels_ = {this->r_, this->g_, this->b_, this->w_, this->cw_, this->ww_};

  // Store the number of exsiting channels for faster proccesing. This is not this->channels_.size() but the number of
  // existent channels within this->channels_.
  uint8_t channel_count_ = 0;

  bool set_channel_by_friendly_name_(const std::string &channel_friendly_name, uint8_t index) {
    for (Channel &channel : this->channels_) {
      if (channel.friendly_name == channel_friendly_name) {
        channel.exists_ = true;
        channel.index_ = index;
        return true;
      }
    }
    return false;  // Channel not found
  }

 public:
  bool is_rgb() const {
    return this->r_.exists_ && this->g_.exists_ && this->b_.exists_ && !this->w_.exists_ && !this->cw_.exists_ &&
           !this->ww_.exists_;
  }

  bool is_rgbw() const {
    return this->r_.exists_ && this->g_.exists_ && this->b_.exists_ && this->w_.exists_ && !this->cw_.exists_ &&
           !this->ww_.exists_;
  }

  bool is_rgbcct() const {
    return this->r_.exists_ && this->g_.exists_ && this->b_.exists_ && !this->w_.exists_ && this->cw_.exists_ &&
           this->ww_.exists_;
  }

  uint8_t get_channel_count() const { return this->channel_count_; }

  uint8_t *get_address_by_channel_name(const uint8_t *base_ptr, const ChannelName &channel_name) const {
    for (const Channel &channel : this->channels_) {
      if ((channel.name == channel_name) && channel.exists_) {
        return const_cast<uint8_t *>(base_ptr) + channel.index_;
      }
    }
    return nullptr;  // Channel does not exist
  }

  void from_string(const std::string &map) {
    std::string s = map;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });

    // Reset channels if from_string() gets called multiple times
    for (Channel &channel : this->channels_) {
      channel.exists_ = false;
    }

    size_t start = 0, pos = 0;
    uint8_t index = 0;
    while ((pos = s.find(',', start)) != std::string::npos) {
      this->set_channel_by_friendly_name_(s.substr(start, pos - start), index);
      start = pos + 1;
      index++;
    }
    if (this->set_channel_by_friendly_name_(s.substr(start), index)) {  // Last channel
      index++;  // Only increment when a valid channel was found and a channel was actually set
    }

    this->channel_count_ = index;
  }

  std::string to_string() const {
    std::vector<std::string> tokens(this->channels_.size(), "");
    for (const Channel &channel : this->channels_) {
      if (channel.exists_) {
        tokens[channel.index_] = channel.friendly_name;
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
