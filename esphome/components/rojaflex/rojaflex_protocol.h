#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace esphome::rojaflex {

static constexpr uint32_t AUTO_LEARN_FRAMES_REQUIRED = 3;

enum class Command : uint8_t {
  STOP = 0x0,
  UP = 0x1,
  DOWN = 0x8,
  SAVE_FAV = 0x9,
  GOTO_FAV = 0xD,
  REQUEST = 0xE,
};

inline bool is_valid_housecode(const std::string &housecode) {
  if (housecode.length() != 7) {
    return false;
  }
  for (char c : housecode) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  return true;
}

inline bool is_housecode_configured(const std::string &housecode) {
  return !housecode.empty() && housecode != "0000000";
}

inline bool apply_manual_housecode(const std::string &new_housecode, std::string &housecode,
                                   std::string &auto_learn_housecode, uint32_t &auto_learn_count) {
  if (!is_valid_housecode(new_housecode)) {
    return false;
  }
  housecode = new_housecode;
  auto_learn_housecode.clear();
  auto_learn_count = 0;
  return true;
}

inline bool is_valid_p109_payload(const std::vector<uint8_t> &payload) { return payload.size() == 9 && payload[0] == 0x08; }

inline std::string extract_housecode_from_payload(const std::vector<uint8_t> &payload) {
  if (!is_valid_p109_payload(payload)) {
    return "";
  }
  char rx_hc_buf[8];
  snprintf(rx_hc_buf, sizeof(rx_hc_buf), "%02X%02X%02X%1X", payload[1], payload[2], payload[3],
           (payload[4] >> 4) & 0x0F);
  return std::string(rx_hc_buf);
}

struct AutoLearnResult {
  bool learned_now{false};
  std::string configured_housecode;
  std::string candidate_housecode;
  uint32_t candidate_count{0};
};

inline AutoLearnResult auto_learn_housecode_step(const std::vector<uint8_t> &payload, const std::string &configured_housecode,
                                                 const std::string &candidate_housecode, uint32_t candidate_count) {
  AutoLearnResult result;
  result.configured_housecode = configured_housecode;
  result.candidate_housecode = candidate_housecode;
  result.candidate_count = candidate_count;

  if (!is_valid_p109_payload(payload) || is_housecode_configured(configured_housecode)) {
    return result;
  }

  const std::string rx_housecode = extract_housecode_from_payload(payload);
  if (rx_housecode.empty()) {
    return result;
  }

  if (rx_housecode == candidate_housecode) {
    result.candidate_count = candidate_count + 1;
    if (result.candidate_count >= AUTO_LEARN_FRAMES_REQUIRED) {
      result.learned_now = true;
      result.configured_housecode = rx_housecode;
      result.candidate_housecode = "";
      result.candidate_count = 0;
    }
    return result;
  }

  result.candidate_housecode = rx_housecode;
  result.candidate_count = 1;
  return result;
}

enum class PositionSource : uint8_t {
  NONE,
  MOTOR_FEEDBACK,
  REMOTE_INFERRED,
};

struct P109Frame {
  bool valid{false};
  bool housecode_match{false};
  std::string rx_housecode;
  uint8_t channel{0};
  bool applies_to_all_channels{false};
  uint8_t device_type{0};
  uint8_t cmd{0};
  uint8_t cmd_value{0};
  PositionSource position_source{PositionSource::NONE};
  int pct{0};
  std::string raw;
  std::string info;
};

inline P109Frame decode_p109_frame(const std::vector<uint8_t> &payload, const std::string &configured_housecode) {
  P109Frame f;
  if (!is_valid_p109_payload(payload)) {
    return f;
  }
  f.valid = true;

  f.rx_housecode = extract_housecode_from_payload(payload);
  f.channel = payload[4] & 0x0F;
  const uint8_t cmd_dev = payload[5];
  f.device_type = cmd_dev & 0x0F;
  f.cmd = (cmd_dev >> 4) & 0x0F;
  f.cmd_value = payload[6];
  f.applies_to_all_channels = (f.device_type == 0xA && f.channel == 0);

  char raw_buf[32];
  snprintf(raw_buf, sizeof(raw_buf), "%02X%02X%02X%02X%02X%02X%02X%02X%02X", payload[0], payload[1], payload[2], payload[3],
           payload[4], payload[5], payload[6], payload[7], payload[8]);
  f.raw = std::string(raw_buf);

  char info_buf[96];
  snprintf(info_buf, sizeof(info_buf), "HC=%s CH=%u DEV=%X CMD=%X VAL=%u", f.rx_housecode.c_str(), f.channel, f.device_type, f.cmd,
           f.cmd_value);
  f.info = std::string(info_buf);

  if (!is_housecode_configured(configured_housecode) || f.rx_housecode != configured_housecode) {
    return f;
  }
  f.housecode_match = true;

  if (f.device_type == 0x5) {
    f.position_source = PositionSource::MOTOR_FEEDBACK;
    f.pct = f.cmd_value > 100 ? 100 : f.cmd_value;
  } else if (f.device_type == 0xA) {
    if (f.cmd == 0x1) {
      f.position_source = PositionSource::REMOTE_INFERRED;
      f.pct = 0;
    } else if (f.cmd == 0x8) {
      f.position_source = PositionSource::REMOTE_INFERRED;
      f.pct = 100;
    }
  }

  return f;
}

struct ShutterMotionPlan {
  enum class Action : uint8_t {
    NONE,
    STOP,
    UP_TO_END,
    DOWN_TO_END,
    UP_THEN_STOP,
    DOWN_THEN_STOP,
  };
  Action action{Action::NONE};
  uint32_t duration_ms{0};
  int target_pct{0};
  const char *info{""};
};

inline ShutterMotionPlan compute_shutter_motion_plan(int current_pct, int target_pct, int time_to_open_s, int time_to_close_s) {
  ShutterMotionPlan plan;

  if (target_pct < 0)
    target_pct = 0;
  if (target_pct > 100)
    target_pct = 100;
  plan.target_pct = target_pct;

  if (target_pct == 0) {
    plan.action = ShutterMotionPlan::Action::UP_TO_END;
    return plan;
  }
  if (target_pct == 100) {
    plan.action = ShutterMotionPlan::Action::DOWN_TO_END;
    return plan;
  }

  if (current_pct < 0) {
    plan.info = "current position unknown - move to an end stop first";
    return plan;
  }

  if (current_pct == target_pct) {
    plan.action = ShutterMotionPlan::Action::STOP;
    return plan;
  }

  if (target_pct > current_pct) {
    if (time_to_close_s < 0) {
      plan.info = "close time not calibrated - drive fully closed once";
      return plan;
    }
    const uint32_t delta = static_cast<uint32_t>(target_pct - current_pct);
    plan.duration_ms = (delta * static_cast<uint32_t>(time_to_close_s) * 1000u) / 100u;
    plan.action = ShutterMotionPlan::Action::DOWN_THEN_STOP;
  } else {
    if (time_to_open_s < 0) {
      plan.info = "open time not calibrated - drive fully open once";
      return plan;
    }
    const uint32_t delta = static_cast<uint32_t>(current_pct - target_pct);
    plan.duration_ms = (delta * static_cast<uint32_t>(time_to_open_s) * 1000u) / 100u;
    plan.action = ShutterMotionPlan::Action::UP_THEN_STOP;
  }
  return plan;
}

inline bool build_tx_packet(const std::string &housecode, uint8_t channel, uint8_t cmd_code, std::vector<uint8_t> &tx_packet,
                            std::string &final_msg) {
  if (!is_housecode_configured(housecode)) {
    return false;
  }

  const uint8_t channel_nib = channel & 0x0F;
  const uint8_t cmd_nib = cmd_code & 0x0F;
  const uint8_t device_nib = 0xA;

  char msg[32];
  snprintf(msg, sizeof(msg), "P109#08%s%X%X%s%X%XA", housecode.c_str(), channel_nib, cmd_nib, "A01", cmd_nib, device_nib);

  uint8_t sum = 0;
  for (int i = 7; i < 20; i += 2) {
    char hex_byte[3] = {msg[i], msg[i + 1], '\0'};
    uint8_t byte_val = strtol(hex_byte, nullptr, 16);
    sum += byte_val;
  }
  sum &= 0xFF;

  char final_buf[40];
  snprintf(final_buf, sizeof(final_buf), "%s%02X", msg, sum);
  final_msg = std::string(final_buf);

  tx_packet.clear();
  tx_packet.reserve(9);
  const char *hex_payload = final_msg.c_str() + 5;
  for (int i = 0; i < 18; i += 2) {
    char hex_byte[3] = {hex_payload[i], hex_payload[i + 1], '\0'};
    tx_packet.push_back(static_cast<uint8_t>(strtol(hex_byte, nullptr, 16)));
  }
  return true;
}

}  // namespace esphome::rojaflex
