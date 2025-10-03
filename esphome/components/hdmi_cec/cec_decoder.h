#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <array>

#include "hdmi_cec.h"

namespace esphome {
namespace hdmi_cec {

/**
 * This Decoder class interprets a binary CEC Frame to create a textual representation.
 * The information to create this decoder is mostly extracted from the HDMI 1.3a standard document,
 * from its section "Supplement 1 Consumer Electronics Control (CEC)".
 * Some further details were found in the Linux kernel source code of the "v4l-utils" repository,
 * such as the "ARC" related functionality of HDMI-CEC 1.4, and the HDMI vendor ID names.
 * Details on the digital audio format decoding came from the wikipedia page on
 * "Extended Display Identification Data (EDID)"
 */
class Decoder {
 public:
  Decoder(const Frame &frame) : frame_(frame), length_(0), offset_(2) {}
  std::string decode();

 protected:
  const char *find_opcode_name_(uint32_t opcode) const;
  std::string address_decode_() const;

  /**
   * Generic operand decode method, later specialised with operand-type-specific methods
   * @return true if further conversions can continue, false when to stop.
   */
  template<uint32_t OPERANDS> bool do_operand_();

  /**
   * The CecOpcodeTable is extracted from the HDMI CEC standard (1.4):
   * It lists all Frame opcodes with their <name> and their expected [operand argument type(s)]
   */
  using OperandDecode_f = bool (Decoder::*)();
  struct FrameType {
    const char *name;                // name of the operation (of the op_code)
    const OperandDecode_f decode_f;  // a pointer to the corresponding 'do_operand_()' method
  };
  using CecOpcodeTable = const std::map<uint8_t, FrameType>;
  const static CecOpcodeTable CEC_OPCODE_TABLE;

  const Frame &frame_;
  std::array<char, 256> line_;  // to hold the text of the decoded frame
  unsigned int length_;         // currently accumulated length of output text in 'line'
  unsigned int offset_;  // current offset in frame to process next operand byte(s) (frame[0] and [1] are skipped)

  /**
   * The HDMI CEC standard specifies a set of distinct operand (parameter) types,
   * used across the frame opcodes, denoted with "[operand type name]".
   * These specified operand types are enumerated here for later type-specific decoding to text
   */
  enum Operand : uint8_t {
    NONE,
    ABORT_REASON,
    ANALOG_BROADCAST_TYPE,
    ANALOG_FREQUENCY,
    ASCII_DIGIT,
    ASCII,
    AUDIO_FORMAT,
    AUDIO_RATE,
    AUDIO_STATUS,
    BOOLEAN,
    BROADCAST_SYSTEM,
    CEC_VERSION,
    CHANNEL_IDENTIFIER,
    // DayOfMonth, integrated in StartDateTime
    DECK_CONTROL_MODE,
    DECK_INFO,
    DEVICE_TYPE,
    DIGITAL_SERVICE_IDENTIFICATION,
    DISPLAY_CONTROL,
    DURATION,
    EXTERNAL_PHYSICAL_ADDRESS,
    EXTERNAL_PLUG,
    EXTERNAL_SOURCE_SPECIFIER,
    HOUR,
    FEATURE_OPCODE,
    LANGUAGE,
    MENU_REQUEST_TYPE,
    MENU_STATE,
    MINUTE,
    // MonthOfYear, integrated in StartDateTime
    NEW_ADDRESS,
    ORIGINAL_ADDRESS,
    OSD_NAME,
    OSD_STRING = OSD_NAME,
    PHYSICAL_ADDRESS,
    PLAY_MODE,
    POWER_STATUS,
    PROGRAM_TITLE_STRING,
    RECORD_SOURCE,
    RECORD_STATUS_INFO,
    RECORDING_SEQUENCE,
    SHORT_AUDIO_DESCRIPTOR,
    STATUS_REQUEST,
    START_DATE_TIME,
    SYSTEM_AUDIO_STATUS,
    TIME,
    TIMER_CLEARED_STATUS_DATA,
    TIMER_STATUS_DATA,
    TUNER_DEVICE_INFO,
    UI_BROADCAST_TYPE,
    UI_COMMAND,
    UI_FUNCTION_MEDIA,
    UI_FUNCTION_SELECT_AV_INPUT,
    UI_FUNCTION_SELECT_AUDIO_INPUT,
    UI_SOUND_PRESENTATION_CONTROL,
    VENDOR_ID,
    VENDOR_SPECIFIC_DATA,
    VENDOR_SPECIFIC_RC_CODE,
  };
  /**
   * The plain 'operand types' are uint8.
   * Further uint32 'operand type' values are used to encode a sequence of upto 4 (potentially different) operands
   * in the MSB bytes of an uint32 value.
   */
  constexpr static uint32_t two(uint32_t first, uint32_t secnd) { return first | (secnd << 8); }
  constexpr static uint32_t three(uint32_t first, uint32_t secnd, uint32_t third) {
    return first | (secnd << 8) | (third << 16);
  }

  /**
   * Helper function to implement the 'do_operand' methods
   */
  bool append_operand_(const char *word, uint8_t offset_incr = 1);

  template<uint32_t N_STRINGS> bool append_operand_(const std::array<const char *, N_STRINGS> &strings) {
    uint32_t operand_value = frame_[offset_];
    const char *s = (operand_value < N_STRINGS) ? strings[operand_value] : "?";
    return append_operand_(s);
  }

  /**
   * String tables used in the subsequent 'do_operand' decode functions
   */
  const static std::array<const char *, 0x77> UI_COMMANDS;
  const static std::array<const char *, 0x11> AUDIO_FORMATS;
  const static std::array<const char *, 8> AUDIO_SAMPLERATES;
  const static std::map<uint32_t, const char *> VENDOR_IDS;
};  // class Decoder
}  // namespace hdmi_cec
}  // namespace esphome
