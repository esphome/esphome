#pragma once

#include <map>
#include <array>
#include <cstdio>

#include "hdmi_cec.h"

namespace esphome {
namespace hdmi_cec {
namespace decoder {

/**
 * The HDMI CEC standard specifies a set of distinct operand (parameter) types,
 * used across the frame opcodes, described with "[type name]".
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
  DayOfMonth,
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
  MonthOfYear,
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
  StartTime,
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
 * Type declaration for all operand type-specific decode functions
 */
struct OperandChain {
  std::string text;
  unsigned int offset;
};
using OperandDecode_f = OperandChain (*)(const OperandChain, const Message *);

/**
 * The plain 'operand types' are uint8.
 * Further uint32 'operand type' values are used to encode a sequence of upto 4 (potentially different) operands
 * in the MSB bytes of an uint32 value.
 */
constexpr static uint32_t Two(uint32_t first, uint32_t secnd) { return first | (secnd << 8); }
constexpr static uint32_t Three(uint32_t first, uint32_t secnd, uint32_t third) {
  return first | (secnd << 8) | (third << 16);
}
constexpr static uint32_t Four(uint32_t first, uint32_t secnd, uint32_t third, uint32_t fourth) {
  return first | (secnd << 8) | (third << 16) | (fourth << 24);
}

constexpr static uint32_t RecordTime = Four(DayOfMonth, MonthOfYear, StartTime, Duration);

/**
 * Generic default operand decode function, later specialised with several specific functions
 */
template<uint32_t OPERANDS> static OperandChain operands(const OperandChain chain, const Message *frame) {
  if (OPERANDS <= 0xFF) {
    // generic function called for single operand of unkown type and length
    return {chain.text + "[.]", frame->size()};
  } else {
    OperandChain first = operands<OPERANDS & 0xFF>(chain, frame);
    return operands<(OPERANDS >> 8u)>(first, frame);
  }
}

/**
 * Operand strings used in the subsequent decode functions
 */
const static std::array<const char *, 0x77> UI_Commands = {
    /* 0x00 = */ "Select",
    "Up",
    "Down",
    "Left",
    "Right",
    "Right-Up",
    "Right-Down",
    "Left-Up",
    /* 0x08 = */ "Left-Down",
    "Root Menu",
    "Setup Menu",
    "Contents Menu",
    "Favorite Menu",
    "Exit",
    "Reserved",
    "Reserved",
    /* 0x10 = */ "Media Top Menu",
    "Media Context-sensitive Menu",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    /* 0x18 = */ "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Number Entry Mode",
    "11",
    "12",
    /* 0x20 = */ "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    /* 0x28 = */ "8",
    "9",
    "Dot",
    "Enter",
    "Clear",
    "Reserved",
    "Reserved",
    "Next Favorite",
    /* 0x30 = */ "Channel Up",
    "Channel Down",
    "Previous Channel",
    "Sound Select",
    "Input Select",
    "isplay Information",
    "Help",
    "Page Up",
    /* 0x38 = */ "Page Down",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    /* 0x40 = */ "Power",
    "Volume Up",
    "Volume Down",
    "Mute",
    "Play",
    "Stop",
    "Pause",
    "Record",
    /* 0x48 = */ "Rewind",
    "Fast forward",
    "Eject",
    "Forward",
    "Backward",
    "Stop-Record",
    "Pause-Record",
    "Reserved",
    /* 0x50 = */ "Angle",
    "Sub picture",
    "Video on Demand",
    "Electronic Program Guide",
    "Timer Programming",
    "Initial Configuration",
    "Select Broadcast Type",
    "Select Sound Presentation",
    /* 0x58 = */ "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    /* 0x60 = */ "Play Function",
    "Pause-Play Function",
    "Record Function",
    "Pause-Record Function",
    "Stop Function",
    "Mute Function",
    "Restore Volume Function",
    "Tune Function",
    /* 0x68 = */ "Select Media Function",
    "Select A/V Input Function",
    "Select Audio Input Function",
    "Power Toggle Function",
    "Power Off Function",
    "Power On Function",
    "Reserved",
    "Reserved",
    /* 0x70 = */ "Reserved",
    "F1 (Blue)",
    "F2 (Red)",
    "F3 (Green)",
    "F4 (Yellow)",
    "F5",
    "Data"};

// See 'short audio descriptor' in https://en.wikipedia.org/wiki/Extended_Display_Identification_Data
const static std::array<const char *, 0x11> audio_formats = {
    "reserved", "LPCM", "AC3",    "MPEG-1",           "MP3",       "MPEG-2",  "AAC",        "DTS", "ATRAC",
    "DSD",      "DD+",  "DTS-HD", "MAT/Dolby TrueHD", "DST Audio", "WMA Pro", "Extension?", "?"};
const static std::array<const char *, 8> audio_samplerates = {"32", "44.1", "48", "88", "96", "176", "192", "Reserved"};

inline uint8_t operand_with_limit(uint8_t offset, const Message *frame, uint8_t size) {
  return (offset < frame->size()) ? std::min(frame->data()[offset], (uint8_t) (size - 1)) : (size - 1);
}
/**
 * List of specialised operand decode functions, one function per operand type.
 * (Not fully complete, might be extended later)
 */
template<> OperandChain operands<None>(const OperandChain chain, const Message *frame) {
  return {chain.text + "[]", frame->size()};
}

template<> OperandChain operands<AbortReason>(const OperandChain chain, const Message *frame) {
  const static std::array<const char *, 7> names = {"[Unrecognized opcode]",
                                                    "[Not in correct mode to respond]",
                                                    "[Cannot provide source]",
                                                    "[Invalid operand]",
                                                    "[Refused]",
                                                    "[Unable to determine]",
                                                    "[?]"};
  uint8_t reason = operand_with_limit(chain.offset, frame, names.size());
  return {chain.text + names[reason], chain.offset + 1};
}

template<> OperandChain operands<AudioFormat>(const OperandChain chain, const Message *frame) {
  uint8_t id = operand_with_limit(chain.offset, frame, audio_formats.size());
  // int id = (chain.offset < frame->size()) ? std::min(frame->data()[chain.offset], (uint8_t)(0x10)) : 0x10;
  const OperandChain result = {chain.text + "[" + audio_formats[id] + "]", chain.offset + 1};
  // there might be a sequence of these operands:
  if (result.offset < frame->size()) {
    return operands<AudioFormat>(result, frame);
  } else {
    return result;
  }
}

template<> OperandChain operands<AudioStatus>(const OperandChain chain, const Message *frame) {
  char line[20];
  if (chain.offset < frame->size()) {
    uint8_t field = frame->data()[chain.offset];
    std::sprintf(line, "[Mute=%d,Vol=%02X]", (field >> 7), (field & 0x7f));
  } else {
    std::sprintf(line, "[?]");
  }
  return {chain.text + line, chain.offset + 1};
}

template<> OperandChain operands<DeviceType>(const OperandChain chain, const Message *frame) {
  const static std::array<const char *, 9> names = {
      "[TV]",           "[Recording Device]", "[Reserved]",    "[Tuner]", "[Playback Device]",
      "[Audio System]", "[Pure CEC Switch]",    "[Video Processor]", "[?]"};
  uint8_t device = operand_with_limit(chain.offset, frame, names.size());
  return {chain.text + names[device], chain.offset + 1};
}

template<> OperandChain operands<DisplayControl>(const OperandChain chain, const Message *frame) {
  const static std::array<const char *, 9> names = {"[Default Time]", "[Until cleared]", "[Clear previous]",
                                                    "[Reserved]", "[?]"};
  uint8_t cntl = operand_with_limit(chain.offset, frame, names.size());
  return {chain.text + names[cntl], chain.offset + 1};
}

static std::string opcode_name(uint8_t opcode);
template<> OperandChain operands<FeatureOpcode>(const OperandChain chain, const Message *frame) {
  if (chain.offset >= frame->size()) {
    return {chain.text + "[?]", frame->size()};
  }
  uint8_t opcode = frame->data()[chain.offset];
  return {chain.text + "[" + opcode_name(opcode) + "]", chain.offset + 1};
}

template<> OperandChain operands<OsdString>(const OperandChain chain, const Message *frame) {
  std::string s = chain.text + "[\"";
  s.append(reinterpret_cast<const char *>(frame->data() + chain.offset), frame->size() - chain.offset);
  return {s + "\"]", frame->size()};
}

template<> OperandChain operands<PhysicalAddress>(const OperandChain chain, const Message *frame) {
  // Exception: if this is an operand of <System Audio Mode Request> 0x70, then this operand is
  // merely optional, and its absence means 'Off'
  if (frame->at(1) == 0x70 && chain.offset >= frame->size()) {
    return {chain.text + "[Off]", frame->size()};
  }
  if (chain.offset >= frame->size()) {
    return {chain.text + "[?]", frame->size()};
  }
  char line[12];
  std::sprintf(line, "[PA=%02x%02x]", frame->at(chain.offset), frame->at(chain.offset + 1));
  return {chain.text + line, chain.offset + 2};
}

template<> OperandChain operands<PowerStatus>(const OperandChain chain, const Message *frame) {
  const static std::array<const char *, 5> names = {"[On]", "[Standby]", "[Standby->On]", "[On->Standby]", "[?]"};
  uint8_t status = operand_with_limit(chain.offset, frame, names.size());
  return {chain.text + names[status], chain.offset + 1};
}

template<> OperandChain operands<ShortAudioDescriptor>(const OperandChain chain, const Message *frame) {
  if (chain.offset + 2 >= frame->size()) {
    return {chain.text + "[?]", frame->size()};
  }
  const uint8_t *descriptor = frame->data() + chain.offset;
  const uint8_t format = (descriptor[0] >> 3) & 0x0F;
  std::string txt = audio_formats[format];
  txt += ",num_channels=" + std::to_string(1u + (descriptor[0] & 0x07));
  uint8_t rates = descriptor[1];
  for (int bit = 0; rates; bit++, rates >>= 1) {
    if (rates & 0x1) {
      txt += std::string(",") + audio_samplerates[bit] + "kHz";
    }
  }
  if (format == 1) {
    // bits per sample for LPCM format
    uint8_t widths = descriptor[2] & 0x7;
    if (widths & 0x1) {
      txt += ",16bits";
    }
    if (widths & 0x2) {
      txt += ",20bits";
    }
    if (widths & 0x4) {
      txt += ",24bits";
    }
  }
  // TODO: Further descriptor 'extensions' not yet decoded
  const OperandChain result = {chain.text + "[" + txt + "]", chain.offset + 3};
  // there might be a sequence of these operands:
  if (result.offset < frame->size()) {
    return operands<ShortAudioDescriptor>(result, frame);
  } else {
    return result;
  }
}

template<> OperandChain operands<SystemAudioStatus>(const OperandChain chain, const Message *frame) {
  const static std::array<const char *, 3> names = {"[Off]", "[On]", "[?]"};
  uint8_t status = operand_with_limit(chain.offset, frame, names.size());
  return {chain.text + names[status], chain.offset + 1};
}

template<> OperandChain operands<UICommand>(const OperandChain chain, const Message *frame) {
  constexpr static uint8_t limit = UI_Commands.size();
  uint8_t value = (chain.offset < frame->size()) ? std::min(frame->data()[chain.offset], limit) : limit;
  const char *command = (value < limit) ? UI_Commands[value] : "Reserved";
  const OperandChain result = {chain.text + "[" + command + "]", chain.offset + 1};
  // out of the 100+ UI commands, a few exceptional UI commands have appended an extra parameter:
  switch (value) {
    case 0x56:
      return operands<UIBroadcastType>(result, frame);
    case 0x57:
      return operands<UISoundPresentationControl>(result, frame);
    case 0x60:
      return operands<PlayMode>(result, frame);
    case 0x67:
      return operands<ChannelIdentifier>(result, frame);
    case 0x68:
      return operands<UIFunctionMedia>(result, frame);
    case 0x69:
      return operands<UIFunctionSelectAVInput>(result, frame);
    case 0x6A:
      return operands<UIFunctionSelectAudioInput>(result, frame);
    default:
      return result;
  }
}

/**
 * Extracted from the HDMI CEC standard (1.4):
 * All Frame opcodes with their <name> and their expected [operand argument type(s)]
 */
struct FrameType {
  const char *name;
  const OperandDecode_f decode_f;
};
using CecOpcodeTable = const std::map<uint8_t, FrameType>;
static CecOpcodeTable cec_opcode_table{
    // opcode,   name,     operands
    {0x82, {"Active Source", operands<PhysicalAddress>}},
    {0x04, {"Image View On", operands<None>}},
    {0x0D, {"Text View On", operands<None>}},
    {0x9D, {"Inactive Source", operands<PhysicalAddress>}},
    {0x85, {"Request Active Source", operands<None>}},
    {0x80, {"Routing Change", operands<Two(PhysicalAddress, PhysicalAddress)>}},
    {0x81, {"Routing Information", operands<PhysicalAddress>}},
    {0x86, {"Set Stream Path", operands<PhysicalAddress>}},
    {0x36, {"Standby", operands<None>}},
    {0x0B, {"Record Off", operands<None>}},
    {0x09, {"Record On", operands<RecordSource>}},
    {0x0A, {"Record Status", operands<RecordStatusInfo>}},
    {0x0F, {"Record TV Screen", operands<None>}},
    {0x33, {"Clear Analogue Timer", operands<RecordTime>}},
    {0x99, {"Clear Digital Timer", operands<RecordTime>}},
    {0xA1, {"Clear External Timer", operands<RecordTime>}},
    {0x34, {"Set Analogue Timer", operands<RecordTime>}},
    {0x97, {"Set Digital Timer", operands<RecordTime>}},
    {0xA2, {"Set External Timer", operands<RecordTime>}},
    {0x67, {"Set Timer Program Title", operands<ProgramTitleString>}},
    {0x43, {"Timer Cleared Status", operands<TimerClearedStatusData>}},
    {0x35, {"Timer Status", operands<TimerStatusData>}},
    {0x9E, {"CEC Version", operands<CecVersion>}},
    {0x9F, {"Get CEC Version", operands<None>}},
    {0x83, {"Give Physical Address", operands<None>}},
    {0x91, {"Get Menu Language", operands<None>}},
    {0x84, {"Report Physical Address", operands<Two(PhysicalAddress, DeviceType)>}},
    {0x32, {"Set Menu Language", operands<Language>}},
    {0x42, {"Deck Control", operands<DeckControlMode>}},
    {0x1B, {"Deck Status", operands<DeckInfo>}},
    {0x1A, {"Give Deck Status", operands<StatusRequest>}},
    {0x41, {"Play", operands<PlayMode>}},
    {0x08, {"Give Tuner Device Status", operands<StatusRequest>}},
    {0x92, {"Select Analogue Service", operands<Three(AnalogBroadcastType, AnalogFrequency, BroadcastSystem)>}},
    {0x93, {"Select Digital Service", operands<DigitalServiceIdentification>}},
    {0x07, {"Tuner Device Status", operands<TunerDeviceInfo>}},
    {0x06, {"Tuner Step Decrement", operands<None>}},
    {0x05, {"Tuner Step Increment", operands<None>}},
    {0x87, {"Device Vendor ID", operands<VendorId>}},
    {0x8C, {"Give Device Vendor ID", operands<None>}},
    {0x89, {"Vendor Command", operands<VendorSpecificData>}},
    {0xA0, {"Vendor Command With ID", operands<Two(VendorId, VendorSpecificData)>}},
    {0x8A, {"Vendor Remote Button Down", operands<VendorSpecificRCCode>}},
    {0x8B, {"Vendor Remote Button Up", operands<None>}},
    {0x64, {"Set OSD String", operands<Two(DisplayControl, OsdString)>}},
    {0x46, {"Give OSD Name", operands<None>}},
    {0x47, {"Set OSD Name", operands<OsdName>}},
    {0x8D, {"Menu Request", operands<MenuRequestType>}},
    {0x8E, {"Menu Status", operands<MenuState>}},
    {0x44, {"User Control Pressed", operands<UICommand>}},
    {0x45, {"User Control Released", operands<None>}},
    {0x8F, {"Give Device Power Status", operands<None>}},
    {0x90, {"Report Power Status", operands<PowerStatus>}},
    {0x00, {"Feature Abort", operands<Two(FeatureOpcode, AbortReason)>}},
    {0xFF, {"Abort", operands<None>}},
    {0x71, {"Give Audio Status", operands<None>}},
    {0x7D, {"Give System Audio Mode Status", operands<None>}},
    {0x7A, {"Report Audio Status", operands<AudioStatus>}},
    {0xA3, {"Report Short Audio Descriptor", operands<ShortAudioDescriptor>}},
    {0xA4, {"Request Short Audio Descriptor", operands<AudioFormat>}},
    {0x72, {"Set System Audio Mode", operands<SystemAudioStatus>}},
    {0x70, {"System Audio Mode Request", operands<PhysicalAddress>}},
    {0x7E, {"System Audio Mode Status", operands<SystemAudioStatus>}},
    {0x9A, {"Set Audio Rate", operands<AudioRate>}},
    {0xC0, {"Initiate ARC", operands<None>}},
    {0xC1, {"Report ARC Initiated", operands<None>}},
    {0xC2, {"Report ARC Terminated", operands<None>}},
    {0xC3, {"Request ARC Initiation", operands<None>}},
    {0xC4, {"Request ARC Termination", operands<None>}},
    {0xC5, {"Terminate ARC", operands<None>}},
    {0xF8, {"CDC Message", operands<None>}}};

static std::string opcode_name(uint8_t opcode) {
  auto it = cec_opcode_table.find(opcode);
  if (it == cec_opcode_table.end()) {
    return "<?>";
  }
  return std::string("<") + it->second.name + ">";
}

/**
 * Entry function 'decode' to call for full decode of a CEC frame
 */
static std::string decode(const Message *frame) {
  auto it = cec_opcode_table.find(frame->opcode());
  if (it == cec_opcode_table.end()) {
    return std::string("<?>");
  }
  std::string op_name = std::string("<") + it->second.name + ">";
  const OperandChain op_arguments = it->second.decode_f({"", 2}, frame);  // convert operands to string
  return op_name + op_arguments.text;
}

}  // namespace decoder
}  // namespace hdmi_cec
}  // namespace esphome
