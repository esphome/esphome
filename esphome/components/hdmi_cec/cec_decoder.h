#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <array>

#include "hdmi_cec.h"

namespace esphome::hdmi_cec {

/**
 * This LookupTable is const-initialised and only provides a simple 'find()'.
 * This creates a compact (smaller code) alternative to an std::map;
 */
template<typename KeyT, typename ValueT, unsigned int SIZE> class LookupTable {
 public:
  struct KeyValue {
    KeyT key_;
    ValueT value_;
  };

  const ValueT *find(KeyT key) const {
    int lo_inx = 0;
    int hi_inx = SIZE - 1;
    while (lo_inx <= hi_inx) {
      // Search for 'key' in the array range lo_inx..hi_inx, bounds included.
      // Rely on the array being sorted on increasing opcode!
      int mid = (lo_inx + hi_inx) / 2;
      if (key == table_[mid].key_) {
        return &table_[mid].value_;
      }
      if (key < table_[mid].key_) {
        hi_inx = mid - 1;
      } else {
        lo_inx = mid + 1;
      }
    }
    return nullptr;  // requested 'key' not found in table
  }

  const KeyValue table_[SIZE];
};

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
  Decoder(const Frame &frame, char *line, unsigned int size)
      : frame_(frame), line_(line), size_(size), length_(0), offset_(2) {}

  /**
   * Create a one-line textual representation of the Frame content for logging and debugging
   * Store the text in the earlier provided 'line' buffer
   * @return the length of the written text, excluding the terminating null character
   */
  int decode();

 protected:
  const char *find_opcode_name_(uint32_t opcode) const;
  void address_decode_();

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
    const char *name_;                // name of the operation (of the op_code)
    const OperandDecode_f decode_f_;  // a pointer to the corresponding 'do_operand_()' method
  };

  using CecOpcodeTable = LookupTable<uint8_t, FrameType, 71>;
  using VendorIdTable = LookupTable<uint32_t, const char *, 28>;

  const Frame &frame_;
  char *const line_;         // buffer to hold the text of the decoded frame
  const unsigned int size_;  // size of the 'line_' buffer
  unsigned int length_;      // currently accumulated length of output text in 'line'
  unsigned int offset_;      // current offset in frame to process next operand byte(s) (frame[0] and [1] are skipped)

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
   * Helper function to implement the 'do_operand' methods:
   * Append one word
   * @return true if a further operand shall be decoded, false to terminate operand decoding
   */
  bool append_operand_(const char *word, uint8_t offset_incr = 1);

  /**
   * Append one word, determined by the operand value as index to an array of names
   * @return true if a further operand shall be decoded, false to terminate operand decoding
   */
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
  const static CecOpcodeTable CEC_OPCODE_TABLE;
  const static VendorIdTable VENDOR_IDS;
};  // class Decoder
}  // namespace esphome::hdmi_cec
