#pragma once

#include "esphome/components/button/button.h"
#include "../rs485_frame.h"

#include <vector>

namespace esphome::rs485_frame {

/// Press-only button bound to an RS485FrameHub. Three construction modes:
///
///   1. `value:` form — the hub uses its configured command_format (preamble /
///      value_element_bytes / endianness / postamble) to encode one or more values into
///      the on-wire payload, back-to-back. Restricted to hubs that have a command_format
///      set; the button platform's _final_validate rejects this form against a hub that
///      has none. An optional top-level `value_element_bytes:` on the button overrides
///      just the element byte width while inheriting the hub's preamble/endian/postamble.
///
///   2. Raw form — caller supplies the frame_type prefix and the full unframed payload.
///      The hub adds DLE-STX/ETX wrapping, byte-stuffing, and CRC, but the payload
///      bytes are otherwise emitted verbatim. This is the path that lets a hub with no
///      command_format drive button entities.
///
///   3. `value:` + `command_format:` form — like mode 1 but the button uses its own
///      full format parameters instead of the hub's. Allows a specific button to use a
///      different wire format than the hub default (e.g. a System Off button that must
///      always use the wireless preamble on a wired hub), and also allows `value:`
///      against a generic hub that has no hub-level command_format.
///
/// raw_mode_ is set from the constructor and is immutable for the lifetime of the button.
/// has_cmd_format_ is set by set_command_format() after construction (mode 3 only).
class RS485FrameButton : public button::Button {
 public:
  // Command-mode constructor (modes 1 and 3). press_action() calls
  // hub->queue_command_values(command_values_.data(), command_values_.size()) unless
  // set_command_format() was called, in which case it calls
  // hub->queue_command_with_format() with the button's own params.
  RS485FrameButton(RS485FrameHub *parent, const std::vector<uint32_t> &command_values)
      : parent_(parent), raw_mode_(false) {
    this->command_values_.assign(command_values.begin(), command_values.end());
  }

  // Raw-mode constructor. press_action() concatenates frame_type + payload and calls
  // hub->queue_raw_frame(). The arguments are copied into member vectors so they remain
  // valid for the lifetime of the button.
  RS485FrameButton(RS485FrameHub *parent, const std::vector<uint8_t> &frame_type, const std::vector<uint8_t> &payload)
      : parent_(parent), raw_mode_(true) {
    this->raw_frame_.reserve(frame_type.size() + payload.size());
    this->raw_frame_.insert(this->raw_frame_.end(), frame_type.begin(), frame_type.end());
    this->raw_frame_.insert(this->raw_frame_.end(), payload.begin(), payload.end());
  }

  // Called from to_code when command_format: is set on the button. Enables mode 3:
  // press_action() uses queue_command_with_format() with these parameters instead of
  // the hub's own command_format. Must only be called on value-mode buttons (not raw).
  void set_command_format(const std::vector<uint8_t> &preamble, uint8_t value_element_bytes, bool big_endian,
                          const std::vector<uint8_t> &postamble) {
    this->cmd_preamble_.assign(preamble.begin(), preamble.end());
    this->cmd_postamble_.assign(postamble.begin(), postamble.end());
    this->cmd_value_element_bytes_ = value_element_bytes;
    this->cmd_big_endian_ = big_endian;
    this->has_cmd_format_ = true;
  }

  // Called from to_code when value_element_bytes: is set on the button (lightweight
  // override). press_action() calls queue_command_values_with_element_bytes() — uses the
  // hub's preamble/endian/postamble but the provided element byte width.
  void set_value_element_bytes(uint8_t element_bytes) {
    this->value_element_bytes_override_ = element_bytes;
    this->has_value_element_bytes_override_ = true;
  }

 protected:
  void press_action() override;
  RS485FrameHub *parent_;
  StaticVector<uint32_t, MAX_COMMAND_VALUES> command_values_;
  // Pre-composed frame_type+payload buffer, only populated in raw mode. Held by value so
  // there is no allocation in press_action — only the queue copy inside queue_raw_frame.
  std::vector<uint8_t> raw_frame_;
  bool raw_mode_;
  // Per-button full command_format override (mode 3). Only active when has_cmd_format_ is true.
  bool has_cmd_format_{false};
  StaticVector<uint8_t, MAX_COMMAND_PREAMBLE_LEN> cmd_preamble_;
  StaticVector<uint8_t, MAX_COMMAND_POSTAMBLE_LEN> cmd_postamble_;
  uint8_t cmd_value_element_bytes_{4};
  bool cmd_big_endian_{true};
  // Lightweight element-width override. When set, press_action() calls
  // queue_command_values_with_element_bytes() using the hub's preamble/endian/postamble.
  bool has_value_element_bytes_override_{false};
  uint8_t value_element_bytes_override_{4};
};

}  // namespace esphome::rs485_frame
