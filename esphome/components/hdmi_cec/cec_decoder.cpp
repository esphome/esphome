#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <array>
#include <cstdio>

#include "hdmi_cec.h"
#include "cec_decoder.h"

namespace esphome {
namespace hdmi_cec {

const std::array<const char *, 0x77> Decoder::UI_Commands = {
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
const std::array<const char *, 0x11> Decoder::audio_formats = {
    "reserved", "LPCM", "AC3",    "MPEG-1",           "MP3",       "MPEG-2",  "AAC",       "DTS", "ATRAC",
    "DSD",      "DD+",  "DTS-HD", "MAT/Dolby TrueHD", "DST Audio", "WMA Pro", "Extension?"};
const std::array<const char *, 8> Decoder::audio_samplerates = {"32", "44.1", "48",  "88",
                                                                "96", "176",  "192", "Reserved"};

template<uint32_t OPERANDS> bool Decoder::do_operand() {
  if (OPERANDS <= 0xFF) {
    // generic function called for single operand of unkown type and length
    return append_operand(".");
  } else {
    return do_operand<OPERANDS & 0xFF>() && do_operand<(OPERANDS >> 8u)>();
  }
}

/**
 * List of specialised operand decode functions, one function per operand type.
 * (Not fully complete, might be extended later)
 */
template<> bool Decoder::do_operand<Decoder::None>() { return append_operand(""); }

template<> bool Decoder::do_operand<Decoder::AbortReason>() {
  const static std::array<const char *, 7> names = {"Unrecognized opcode",
                                                    "Not in correct mode to respond",
                                                    "Cannot provide source",
                                                    "Invalid operand",
                                                    "Refused",
                                                    "Unable to determine"};
  return append_operand<names.size()>(names);
}

template<> bool Decoder::do_operand<Decoder::AudioFormat>() {
  // this type of operand comes in a sequence, until exhausted
  bool ok = true;
  while (ok && (offset_ < frame_.size())) {
    ok = append_operand<audio_formats.size()>(audio_formats);
  }
  return ok;
}

template<> bool Decoder::do_operand<Decoder::AudioStatus>() {
  char line[20];
  if (offset_ < frame_.size()) {
    uint8_t field = frame_[offset_];
    std::sprintf(line, "Mute=%d,Vol=%02X", (field >> 7), (field & 0x7f));
    return append_operand(line);
  } else {
    return append_operand("?");
  }
}

template<> bool Decoder::do_operand<Decoder::DeviceType>() {
  const static std::array<const char *, 9> names = {"TV]",           "Recording Device]", "Reserved",
                                                    "Tuner",       "Playback Device", "Audio System",
                                                    "Pure CEC Switch", "Video Processor"};
  return append_operand<names.size()>(names);
}

template<> bool Decoder::do_operand<Decoder::DisplayControl>() {
  const static std::array<const char *, 9> names = {"Default Time", "Until cleared", "Clear previous",
                                                    "Reserved"};
  return append_operand<names.size()>(names);
}

template<> bool Decoder::do_operand<Decoder::FeatureOpcode>() {
  if (offset_ >= frame_.size()) {
    return false;
  }
  uint8_t opcode = frame_[offset_];
  return append_operand(find_opcode_name(opcode));
}

template<> bool Decoder::do_operand<Decoder::OsdString>() {
  char line[20];  // frame size() is at most 16
  // copy with typecast and append '\0' char
  std::snprintf(line, frame_.size() - offset_ + 1, "%s", reinterpret_cast<const char *>(frame_.data() + offset_));
  offset_ = frame_.size();
  return append_operand(line);
}

template<> bool Decoder::do_operand<Decoder::PhysicalAddress>() {
  // Exception: if this is an operand of <System Audio Mode Request> 0x70, then this operand is
  // merely optional, and its absence means 'Off'
  if (frame_.at(1) == 0x70 && offset_ >= frame_.size()) {
    return append_operand("Off");
  }
  if (offset_ + 1 >= frame_.size()) {
    return append_operand("?", 2);
  }
  char line[16];
  std::sprintf(line, "PA=%02x%02x", frame_[offset_], frame_[offset_ + 1]);
  return append_operand(line, 2);
}

template<> bool Decoder::do_operand<Decoder::PowerStatus>() {
  const static std::array<const char *, 5> names = {"On", "Standby", "Standby->On", "On->Standby"};
  return append_operand<names.size()>(names);
}

template<> bool Decoder::do_operand<Decoder::ShortAudioDescriptor>() {
  // the frame can have a sequence of these operands, count is not fixed;
  // each such operand takes 3 bytes in the frame
  std::array<char, 100> line;
  uint32_t pos = 0;
  bool ok = true;
  while (ok && (offset_ + 2 < frame_.size())) {
    const uint8_t *descriptor = frame_.data() + offset_;
    uint8_t format = (descriptor[0] >> 3) & 0x0F;
    pos = std::sprintf(&line[0], "%s", audio_formats[format]);
    pos += std::sprintf(&line[pos], ",num_channels=%d", (descriptor[0] & 0x07));
    uint8_t rates = descriptor[1];
    for (int bit = 0; rates; bit++, rates >>= 1) {
      if (rates & 0x1) {
        // show support of various audio sample rates
        pos += std::sprintf(&line[pos], ",%skHz", audio_samplerates[bit]);
      }
    }
    if (format == 1) {
      // for LPCM format
      uint8_t widths = descriptor[2] & 0x7;
      for (int i = 0; widths; i++, widths >>= 1) {
        if (widths & 0x1) {
          // show support of audio samble bit widths of 16, 20, and/or 24
          pos += std::sprintf(&line[pos], ",%dbits", (16 + 4 * i));
        }
      }
    }
    ok = append_operand(&line[0], 3);
  }
  // TODO: Further descriptor 'extensions' not yet decoded
  return ok;
}

template<> bool Decoder::do_operand<Decoder::SystemAudioStatus>() {
  const static std::array<const char *, 3> names = {"Off", "On"};
  return append_operand<names.size()>(names);
}

template<> bool Decoder::do_operand<Decoder::UICommand>() {
  uint8_t command = frame_[offset_];
  bool ok = append_operand<UI_Commands.size()>(UI_Commands);
  if (!ok) {
    return false;
  }
  // out of the 100+ UI commands, a few exceptional UI commands have appended an extra parameter:
  switch (command) {
    case 0x56:
      return do_operand<UIBroadcastType>();
    case 0x57:
      return do_operand<UISoundPresentationControl>();
    case 0x60:
      return do_operand<PlayMode>();
    case 0x67:
      return do_operand<ChannelIdentifier>();
    case 0x68:
      return do_operand<UIFunctionMedia>();
    case 0x69:
      return do_operand<UIFunctionSelectAVInput>();
    case 0x6A:
      return do_operand<UIFunctionSelectAudioInput>();
    default:
      return ok;
  }
}

template<> bool Decoder::do_operand<Decoder::VendorId>() {
  if (offset_ + 2 >= frame_.size()) {
    return append_operand("?", 3);
  }
  uint32_t id = (uint32_t) (frame_[offset_]) << 16 | (uint32_t) (frame_[offset_ + 1]) << 8 | frame_[offset_ + 2];
  auto it = vendor_ids.find(id);
  if (it == vendor_ids.end()) {
    // if the hdmi-cec vendor id is not in our list, the id value itself is printed.
    char line[12];
    sprintf(line, "ID=%06x", id);
    return append_operand(line, 3);
  }
  return append_operand(it->second, 3);
}

const Decoder::CecOpcodeTable Decoder::cec_opcode_table = {
    // opcode,   name,     operands
    {0x04, {"Image View On", &Decoder::do_operand<None>}},
    {0x00, {"Feature Abort", &Decoder::do_operand<Two(FeatureOpcode, AbortReason)>}},
    {0x0D, {"Text View On", &Decoder::do_operand<None>}},
    {0x9D, {"Inactive Source", &Decoder::do_operand<PhysicalAddress>}},
    {0x85, {"Request Active Source", &Decoder::do_operand<None>}},
    {0x80, {"Routing Change", &Decoder::do_operand<Two(PhysicalAddress, PhysicalAddress)>}},
    {0x81, {"Routing Information", &Decoder::do_operand<PhysicalAddress>}},
    {0x86, {"Set Stream Path", &Decoder::do_operand<PhysicalAddress>}},
    {0x36, {"Standby", &Decoder::do_operand<None>}},
    {0x0B, {"Record Off", &Decoder::do_operand<None>}},
    {0x09, {"Record On", &Decoder::do_operand<RecordSource>}},
    {0x0A, {"Record Status", &Decoder::do_operand<RecordStatusInfo>}},
    {0x0F, {"Record TV Screen", &Decoder::do_operand<None>}},
    {0x33, {"Clear Analogue Timer", &Decoder::do_operand<Two(StartDateTime, Duration)>}},
    {0x99, {"Clear Digital Timer", &Decoder::do_operand<Two(StartDateTime, Duration)>}},
    {0xA1, {"Clear External Timer", &Decoder::do_operand<Two(StartDateTime, Duration)>}},
    {0x34, {"Set Analogue Timer", &Decoder::do_operand<Two(StartDateTime, Duration)>}},
    {0x97, {"Set Digital Timer", &Decoder::do_operand<Two(StartDateTime, Duration)>}},
    {0xA2, {"Set External Timer", &Decoder::do_operand<Two(StartDateTime, Duration)>}},
    {0x67, {"Set Timer Program Title", &Decoder::do_operand<ProgramTitleString>}},
    {0x43, {"Timer Cleared Status", &Decoder::do_operand<TimerClearedStatusData>}},
    {0x35, {"Timer Status", &Decoder::do_operand<TimerStatusData>}},
    {0x9E, {"CEC Version", &Decoder::do_operand<CecVersion>}},
    {0x9F, {"Get CEC Version", &Decoder::do_operand<None>}},
    {0x83, {"Give Physical Address", &Decoder::do_operand<None>}},
    {0x91, {"Get Menu Language", &Decoder::do_operand<None>}},
    {0x84, {"Report Physical Address", &Decoder::do_operand<Two(PhysicalAddress, DeviceType)>}},
    {0x32, {"Set Menu Language", &Decoder::do_operand<Language>}},
    {0x42, {"Deck Control", &Decoder::do_operand<DeckControlMode>}},
    {0x1B, {"Deck Status", &Decoder::do_operand<DeckInfo>}},
    {0x1A, {"Give Deck Status", &Decoder::do_operand<StatusRequest>}},
    {0x41, {"Play", &Decoder::do_operand<PlayMode>}},
    {0x08, {"Give Tuner Device Status", &Decoder::do_operand<StatusRequest>}},
    {0x92,
     {"Select Analogue Service", &Decoder::do_operand<Three(AnalogBroadcastType, AnalogFrequency, BroadcastSystem)>}},
    {0x93, {"Select Digital Service", &Decoder::do_operand<DigitalServiceIdentification>}},
    {0x07, {"Tuner Device Status", &Decoder::do_operand<TunerDeviceInfo>}},
    {0x06, {"Tuner Step Decrement", &Decoder::do_operand<None>}},
    {0x05, {"Tuner Step Increment", &Decoder::do_operand<None>}},
    {0x87, {"Device Vendor ID", &Decoder::do_operand<VendorId>}},
    {0x8C, {"Give Device Vendor ID", &Decoder::do_operand<None>}},
    {0x89, {"Vendor Command", &Decoder::do_operand<VendorSpecificData>}},
    {0xA0, {"Vendor Command With ID", &Decoder::do_operand<Two(VendorId, VendorSpecificData)>}},
    {0x8A, {"Vendor Remote Button Down", &Decoder::do_operand<VendorSpecificRCCode>}},
    {0x8B, {"Vendor Remote Button Up", &Decoder::do_operand<None>}},
    {0x64, {"Set OSD String", &Decoder::do_operand<Two(DisplayControl, OsdString)>}},
    {0x46, {"Give OSD Name", &Decoder::do_operand<None>}},
    {0x47, {"Set OSD Name", &Decoder::do_operand<OsdName>}},
    {0x8D, {"Menu Request", &Decoder::do_operand<MenuRequestType>}},
    {0x8E, {"Menu Status", &Decoder::do_operand<MenuState>}},
    {0x44, {"User Control Pressed", &Decoder::do_operand<UICommand>}},
    {0x45, {"User Control Released", &Decoder::do_operand<None>}},
    {0x8F, {"Give Device Power Status", &Decoder::do_operand<None>}},
    {0x90, {"Report Power Status", &Decoder::do_operand<PowerStatus>}},
    {0x00, {"Feature Abort", &Decoder::do_operand<Two(FeatureOpcode, AbortReason)>}},
    {0xFF, {"Abort", &Decoder::do_operand<None>}},
    {0x71, {"Give Audio Status", &Decoder::do_operand<None>}},
    {0x7D, {"Give System Audio Mode Status", &Decoder::do_operand<None>}},
    {0x7A, {"Report Audio Status", &Decoder::do_operand<AudioStatus>}},
    {0xA3, {"Report Short Audio Descriptor", &Decoder::do_operand<ShortAudioDescriptor>}},
    {0xA4, {"Request Short Audio Descriptor", &Decoder::do_operand<AudioFormat>}},
    {0x72, {"Set System Audio Mode", &Decoder::do_operand<SystemAudioStatus>}},
    {0x70, {"System Audio Mode Request", &Decoder::do_operand<PhysicalAddress>}},
    {0x7E, {"System Audio Mode Status", &Decoder::do_operand<SystemAudioStatus>}},
    {0x9A, {"Set Audio Rate", &Decoder::do_operand<AudioRate>}},
    {0xC0, {"Initiate ARC", &Decoder::do_operand<None>}},
    {0xC1, {"Report ARC Initiated", &Decoder::do_operand<None>}},
    {0xC2, {"Report ARC Terminated", &Decoder::do_operand<None>}},
    {0xC3, {"Request ARC Initiation", &Decoder::do_operand<None>}},
    {0xC4, {"Request ARC Termination", &Decoder::do_operand<None>}},
    {0xC5, {"Terminate ARC", &Decoder::do_operand<None>}},
    {0xF8, {"CDC Message", &Decoder::do_operand<None>}}};

const std::map<uint32_t, const char *> Decoder::vendor_ids = {
    {0x000039, "Toshiba"}, {0x0000F0, "Samsung"},     {0x0005CD, "Denon"},         {0x000678, "Maranz"},
    {0x000982, "Loewe"},   {0x0009B0, "Onkyo"},       {0x000CB8, "Medion"},        {0x000CE7, "Toshiba"},
    {0x0010FA, "Apple"},   {0x001582, "Pulse Eight"}, {0x001950, "Harman Kardon"}, {0x001A11, "Google"},
    {0x0020C7, "Akai"},    {0x002467, "AOC"},         {0x008045, "Panasonic"},     {0x00903E, "Philips"},
    {0x009053, "Daewoo"},  {0x00A0DE, "Yamaha"},      {0x00D0D5, "Grundig"},       {0x00E036, "Pioneer"},
    {0x00E091, "LG"},      {0x08001F, "Sharp"},       {0x080046, "Sony"},          {0x18C086, "Broadcom"},
    {0x534850, "Sharp"},   {0x6B746D, "Vizio"},       {0x8065E9, "Benq"},          {0x9C645E, "Harman Kardon"}};

std::string Decoder::address_decode(uint8_t my_address) const {
  auto addr2str = [my_address](int addr) -> std::string {
    return (addr == my_address) ? "ME" : (addr == 0) ? "TV" : std::to_string(addr);
  };
  std::string dest = (frame_.is_broadcast()) ? std::string("ALL") : addr2str(frame_.destination_addr());
  return std::string("(") + addr2str(frame_.initiator_addr()) + "->" + dest + ")";
}

const char *Decoder::find_opcode_name(uint32_t opcode) const {
  auto it = cec_opcode_table.find(opcode);
  if (it == cec_opcode_table.end()) {
    return "?";
  }
  return it->second.name;
}

/**
 * Entry function 'decode' to call for full decode of a CEC frame
 */
std::string Decoder::decode(uint8_t my_address) {
  // src and dest fields
  std::string result = address_decode(my_address);
  // opcode field
  auto it = cec_opcode_table.find(frame_.opcode());
  if (it == cec_opcode_table.end()) {
    return std::string("<?>");
  }
  result += std::string("<") + it->second.name + ">";
  // operand fields
  length_ = 0;  // currently accumulated length of text of operands
  offset_ = 2;  // location in frame of first operand to decode
  OperandDecode_f f = it->second.decode_f;
  (this->*f)();
  result += &line_[0];
  return result;
}

}  // namespace hdmi_cec
}  // namespace esphome
