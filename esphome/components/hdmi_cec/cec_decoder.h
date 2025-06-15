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
  const char *find_opcode_name(uint32_t opcode) const;
  std::string address_decode() const;

  /**
   * Generic operand decode method, later specialised with operand-type-specific methods
   * @return true if further conversions can continue, false when to stop.
   */
  template<uint32_t OPERANDS> bool do_operand();

  /**
   * The CecOpcodeTable is extracted from the HDMI CEC standard (1.4):
   * It lists all Frame opcodes with their <name> and their expected [operand argument type(s)]
   */
  using OperandDecode_f = bool (Decoder::*)();
  struct FrameType {
    const char *name;                // name of the operation (of the op_code)
    const OperandDecode_f decode_f;  // a pointer to the corresponding 'do_operand()' method
  };
  using CecOpcodeTable = const std::map<uint8_t, FrameType>;
  const static CecOpcodeTable cec_opcode_table;

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
    None,
    AbortReason,
    AnalogBroadcastType,
    AnalogFrequency,
    AsciiDigit,
    Ascii,
    AudioFormat,
    AudioRate,
    AudioStatus,
    Boolean,
    BroadcastSystem,
    CecVersion,
    ChannelIdentifier,
    // DayOfMonth, integrated in StartDateTime
    DeckControlMode,
    DeckInfo,
    DeviceType,
    DigitalServiceIdentification,
    DisplayControl,
    Duration,
    ExternalPhysicalAddress,
    ExternalPlug,
    ExternalSourceSpecifier,
    Hour,
    FeatureOpcode,
    Language,
    MenuRequestType,
    MenuState,
    Minute,
    // MonthOfYear, integrated in StartDateTime
    NewAddress,
    OriginalAddress,
    OsdName,
    OsdString = OsdName,
    PhysicalAddress,
    PlayMode,
    PowerStatus,
    ProgramTitleString,
    RecordSource,
    RecordStatusInfo,
    RecordingSequence,
    ShortAudioDescriptor,
    StatusRequest,
    StartDateTime,
    SystemAudioStatus,
    Time,
    TimerClearedStatusData,
    TimerStatusData,
    TunerDeviceInfo,
    UIBroadcastType,
    UICommand,
    UIFunctionMedia,
    UIFunctionSelectAVInput,
    UIFunctionSelectAudioInput,
    UISoundPresentationControl,
    VendorId,
    VendorSpecificData,
    VendorSpecificRCCode,
  };
  /**
   * The plain 'operand types' are uint8.
   * Further uint32 'operand type' values are used to encode a sequence of upto 4 (potentially different) operands
   * in the MSB bytes of an uint32 value.
   */
  constexpr static uint32_t Two(uint32_t first, uint32_t secnd) { return first | (secnd << 8); }
  constexpr static uint32_t Three(uint32_t first, uint32_t secnd, uint32_t third) {
    return first | (secnd << 8) | (third << 16);
  }

  /**
   * Helper function to implement the 'do_operand' methods
   */
  bool append_operand(const char *word, uint8_t offset_incr = 1);

  template<uint32_t N_STRINGS> bool append_operand(const std::array<const char *, N_STRINGS> &strings) {
    uint32_t operand_value = frame_[offset_];
    const char *s = (operand_value < N_STRINGS) ? strings[operand_value] : "?";
    return append_operand(s);
  }

  /**
   * String tables used in the subsequent 'do_operand' decode functions
   */
  const static std::array<const char *, 0x77> UI_Commands;
  const static std::array<const char *, 0x11> audio_formats;
  const static std::array<const char *, 8> audio_samplerates;
  const static std::map<uint32_t, const char *> vendor_ids;
};  // class Decoder
}  // namespace hdmi_cec
}  // namespace esphome
