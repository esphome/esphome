#pragma once

#include <vector>
#include <algorithm>
#include <array>
#include <memory>

namespace esphome::light {
class ChannelMap {
 public:
  enum class ChannelName : uint8_t { R = 0, G, B, W, CW, WW, SIZE };

 private:
  // Create all channels
  std::array<int8_t, static_cast<size_t>(ChannelName::SIZE)> channels_ = {};

  // Store the number of existing channels for faster processing. This is not this->channels_.size() but the number of
  // existent channels within this->channels_.
  uint8_t channel_count_ = 0;

  // Save as string for logging purposes. Don't use *_ptr<char[]> because it is incompatible with the embedded gcc
  // toolchain.
  std::vector<char> channel_map_str_ = {};

  // Save color mode pre-computed by python to free up runtime resources
  ColorMode color_mode_ = ColorMode::UNKNOWN;

 public:
  // Create a ChannelMap from a list of channel names ordered by their index.
  ChannelMap(const std::vector<ChannelName> &ordered_channel_names, const char *channel_map_str, ColorMode color_mode) {
    // Initialize all channels with sentinel value
    std::fill(this->channels_.begin(), this->channels_.end(), -1);

    // Assign indices to channels
    for (int8_t channel_index = 0; channel_index < static_cast<int8_t>(ordered_channel_names.size()); ++channel_index) {
      this->channels_.at(static_cast<size_t>(ordered_channel_names[channel_index])) = channel_index;
    }
    this->channel_count_ = ordered_channel_names.size();

    // Copy channel map string if provided, otherwise set as "undefined"
    if (channel_map_str) {
      this->channel_map_str_.resize(strlen(channel_map_str) + 1);
      memcpy(this->channel_map_str_.data(), channel_map_str, strlen(channel_map_str) + 1);
    } else {
      constexpr const char *undefined_str = "undefined";
      this->channel_map_str_.assign(undefined_str, undefined_str + strlen(undefined_str) + 1);
    }

    this->color_mode_ = color_mode;
  }

  uint8_t get_channel_count() const { return this->channel_count_; }
  const char *get_str() const { return this->channel_map_str_.data(); }
  ColorMode get_color_mode() const { return this->color_mode_; }

  uint8_t *get_address_by_channel_name(const uint8_t *base_ptr, const ChannelName channel_name) const {
    int8_t index = this->channels_.at(static_cast<size_t>(channel_name));
    if (index != -1) {
      return const_cast<uint8_t *>(base_ptr) + index;
    }
    return nullptr;  // Channel does not exist
  }
};
}  // namespace esphome::light
