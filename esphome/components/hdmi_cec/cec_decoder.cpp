#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <array>

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include "hdmi_cec.h"
#include "cec_decoder.h"

namespace esphome {
namespace hdmi_cec {

// const std::array<const char *, 0x77> Decoder::UI_COMMANDS = {
// Unfortunately, the 'UI_COMMANDS' string table is too large to fit in the 'PROGMEM_STRING_TABLE' :-(
// Therefor the table is partitioned in sections"
PROGMEM_STRING_TABLE(UI_COMMANDS_0,
                     /* 0x00 = */ "Select", "Up", "Down", "Left", "Right", "Right-Up", "Right-Down", "Left-Up",
                     /* 0x08 = */ "Left-Down", "Root Menu", "Setup Menu", "Contents Menu", "Favorite Menu", "Exit",
                     "Reserved", "Reserved");
PROGMEM_STRING_TABLE(UI_COMMANDS_1,
                     /* 0x10 = */ "Media Top Menu", "Media Context-sensitive Menu", "Reserved", "Reserved", "Reserved",
                     "Reserved", "Reserved", "Reserved",
                     /* 0x18 = */ "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Number Entry Mode", "11",
                     "12");
PROGMEM_STRING_TABLE(UI_COMMANDS_2,
                     /* 0x20 = */ "0", "1", "2", "3", "4", "5", "6", "7",
                     /* 0x28 = */ "8", "9", "Dot", "Enter", "Clear", "Reserved", "Reserved", "Next Favorite");
PROGMEM_STRING_TABLE(UI_COMMANDS_3,

                     /* 0x30 = */ "Channel Up", "Channel Down", "Previous Channel", "Sound Select", "Input Select",
                     "Display Information", "Help", "Page Up",
                     /* 0x38 = */ "Page Down", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
                     "Reserved");
PROGMEM_STRING_TABLE(UI_COMMANDS_4,
                     /* 0x40 = */ "Power", "Volume Up", "Volume Down", "Mute", "Play", "Stop", "Pause", "Record",
                     /* 0x48 = */ "Rewind", "Fast forward", "Eject", "Forward", "Backward", "Stop-Record",
                     "Pause-Record", "Reserved");
PROGMEM_STRING_TABLE(UI_COMMANDS_5,
                     /* 0x50 = */ "Angle", "Sub picture", "Video on Demand", "Electronic Program Guide",
                     "Timer Programming", "Initial Configuration", "Select Broadcast Type", "Select Sound Presentation",
                     /* 0x58 = */ "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
                     "Reserved");
PROGMEM_STRING_TABLE(UI_COMMANDS_6,
                     /* 0x60 = */ "Play Func", "Pause-Play Func", "Record Func", "Pause-Record Func", "Stop Func",
                     "Mute Func", "Restore Volume Func", "Tune Func",
                     /* 0x68 = */ "Select Media Func", "Select A/V Input Func", "Select Audio Input Func",
                     "Power Toggle Func", "Power Off Func", "Power On Func", "Reserved", "Reserved");
PROGMEM_STRING_TABLE(UI_COMMANDS_7,
                     /* 0x70 = */ "Reserved", "F1 (Blue)", "F2 (Red)", "F3 (Green)", "F4 (Yellow)", "F5", "Data", "?");

// See 'short audio descriptor' in https://en.wikipedia.org/wiki/Extended_Display_Identification_Data
// const std::array<const char *, 0x11> Decoder::AUDIO_FORMATS = {
PROGMEM_STRING_TABLE(AUDIO_FORMATS, "reserved", "LPCM", "AC3", "MPEG-1", "MP3", "MPEG-2", "AAC", "DTS", "ATRAC", "DSD",
                     "DD+", "DTS-HD", "MAT/Dolby TrueHD", "DST Audio", "WMA Pro", "Extension?");

// const std::array<const char *, 8> Decoder::AUDIO_SAMPLERATES = {
PROGMEM_STRING_TABLE(AUDIO_SAMPLERATES, "32", "44.1", "48", "88", "96", "176", "192", "Reserved");

template<uint32_t OPERANDS> bool Decoder::do_operand_() {
  if (OPERANDS <= 0xFF) {
    // generic function called for single operand of unkown type and length
    return append_operand_(".");
  } else {
    // generic function to handle two or more operands
    return do_operand_<OPERANDS & 0xFF>() && do_operand_<(OPERANDS >> 8u)>();
  }
}

#define APPEND_LINE(...) (length_ = buf_append_printf(line_.data(), line_.size(), length_, __VA_ARGS__))

/**
 * List of specialised operand decode functions, one function per operand type.
 * (Not fully complete, might be extended later)
 */
template<> bool Decoder::do_operand_<Decoder::NONE>() { return false; }

PROGMEM_STRING_TABLE(ABORT_REASONS, "Unrecognized opcode", "Not in correct mode to respond", "Cannot provide source",
                     "Invalid operand", "Refused", "Unable to determine", "?");
template<> bool Decoder::do_operand_<Decoder::ABORT_REASON>() { return append_operand_<ABORT_REASONS>(); }

template<> bool Decoder::do_operand_<Decoder::AUDIO_FORMAT>() {
  // this type of operand comes in a sequence, until exhausted
  bool ok = true;
  while (ok && (offset_ < frame_.size())) {
    ok = append_operand_<AUDIO_FORMATS>();
  }
  return ok;
}

template<> bool Decoder::do_operand_<Decoder::AUDIO_STATUS>() {
  char line[20];
  if (offset_ < frame_.size()) {
    uint8_t field = frame_[offset_];
    std::snprintf(line, sizeof(line), "Mute=%d,Vol=%02X", (field >> 7), (field & 0x7f));
    return append_operand_(line);
  } else {
    return append_operand_("?");
  }
}

PROGMEM_STRING_TABLE(DEVICE_TYPES, "TV", "Recording Device", "Reserved", "Tuner", "Playback Device", "Audio System",
                     "CEC Switch", "Video Processor", "?");
template<> bool Decoder::do_operand_<Decoder::DEVICE_TYPE>() { return append_operand_<DEVICE_TYPES>(); }

PROGMEM_STRING_TABLE(DISPLAY_CONTROLS, "Default time", "Until cleared", "Clear previous", "Reserved", "?");
template<> bool Decoder::do_operand_<Decoder::DISPLAY_CONTROL>() { return append_operand_<DISPLAY_CONTROLS>(); }

template<> bool Decoder::do_operand_<Decoder::FEATURE_OPCODE>() {
  if (offset_ >= frame_.size()) {
    return false;
  }
  uint8_t opcode = frame_[offset_];
  return append_operand_(find_opcode_name_(opcode));
}

template<> bool Decoder::do_operand_<Decoder::OSD_STRING>() {
  char line[20];  // frame size() is at most 16
  unsigned int i;
  for (i = 0; i < frame_.size() - offset_; i++) {
    line[i] = (char) (frame_[offset_ + i]);
  }
  line[i] = '\0';           // append '\0' termination: that is likely NOT part of the frame
  offset_ = frame_.size();  // no useful data further in this frame
  return append_operand_(line);
}

template<> bool Decoder::do_operand_<Decoder::PHYSICAL_ADDRESS>() {
  // Exception: if this is an operand of <System Audio Mode Request> 0x70, then this operand is
  // merely optional, and its absence means 'Off'
  if (frame_.at(1) == 0x70 && offset_ >= frame_.size()) {
    return append_operand_("Off");
  }
  if (offset_ + 1 >= frame_.size()) {
    return append_operand_("?", 2);
  }
  char line[12];
  std::snprintf(line, sizeof(line), "%1x.%1x.%1x.%1x", (frame_[offset_] >> 4) & 0xF, frame_[offset_] & 0xF,
                (frame_[offset_ + 1] >> 4) & 0xF, frame_[offset_ + 1] & 0xF);
  return append_operand_(line, 2);
}

PROGMEM_STRING_TABLE(POWER_STATUSES, "On", "Standby", "Standby->On", "On->Standby", "?");
template<> bool Decoder::do_operand_<Decoder::POWER_STATUS>() { return append_operand_<POWER_STATUSES>(); }

template<> bool Decoder::do_operand_<Decoder::SHORT_AUDIO_DESCRIPTOR>() {
  // The frame can have a sequence of these operands, count is not fixed;
  // each such operand takes 3 bytes in the frame.
  // For a specification of this "Short Audio descriptor" format, see the Wikipedia page on
  // "Extended Display Identification Data (EDID)", section "Audio Data Blocks".
  std::array<char, 128> line;
  uint32_t pos = 0;
  bool ok = true;
  while (ok && (offset_ + 2 < frame_.size())) {
    const uint8_t descriptor = frame_[offset_];
    uint8_t format = (descriptor >> 3) & 0x0F;
    uint8_t num_channels = descriptor & 0x07;
    const LogString *s = AUDIO_FORMATS::get_log_str(format, AUDIO_FORMATS::LAST_INDEX);
#if defined(USE_ARDUINO)
    // Use Arduino extensions for strings in flash (snprintf_P and %S), required for ESP8266
    int n = snprintf_P(line_.data(), line_.size(), PSTR("%S,channels=%d"), (const char *) s, num_channels);
#else
    // Use the standard C library (with IDF, Host) assuming a unified ram&flash address space
    int n = snprintf(line_.data(), line_.size(), "%s,channels=%d", (const char *) s, num_channels);
#endif
    pos = std::min(pos + (uint32_t) std::max(n, 0), (uint32_t) (line.size() - 1));
    uint8_t rates = frame_[offset_ + 1];
    for (int bit = 0; rates && (pos < line.size() - 1); bit++, rates >>= 1) {
      if (rates & 0x1) {
        // show support of various audio sample rates
        const LogString *s = AUDIO_SAMPLERATES::get_log_str(bit, AUDIO_SAMPLERATES::LAST_INDEX);
#if defined(USE_ARDUINO)
        int n = snprintf_P(line_.data() + pos, line_.size() - pos, PSTR(",%SkHz"), (const char *) s);
#else
        int n = snprintf(line_.data() + pos, line_.size() - pos, ",%skHz", (const char *) s);
#endif
        pos = std::min(pos + (uint32_t) (std::max(n, 0)), (uint32_t) (line.size() - 1));
      }
    }
    if (format == 1) {
      // for LPCM format
      uint8_t widths = frame_[offset_ + 2] & 0x7;
      for (int i = 0; widths; i++, widths >>= 1) {
        if (widths & 0x1) {
          // show support of audio samble bit widths of 16, 20, and/or 24
          pos = buf_append_printf(&line[0], line.size(), pos, ",%dbits", (16 + 4 * i));
        }
      }
    }
    ok = append_operand_(line.data(), 3);
  }
  // Note: Further descriptor 'extensions' (for other formats) not yet decoded
  return ok;
}

PROGMEM_STRING_TABLE(SYSTEM_AUDIO_STATUSES, "Off", "On", "?");
template<> bool Decoder::do_operand_<Decoder::SYSTEM_AUDIO_STATUS>() {
  return append_operand_<SYSTEM_AUDIO_STATUSES>();
}

template<> bool Decoder::do_operand_<Decoder::UI_COMMAND>() {
  uint8_t command = frame_[offset_];
  uint8_t table_id = command >> 4;
  uint8_t table_inx = command & 0x0f;
  const LogString *s;
  switch (table_id) {
    case 0:
      s = UI_COMMANDS_0::get_log_str(table_inx, UI_COMMANDS_0::LAST_INDEX);
      break;
    case 1:
      s = UI_COMMANDS_1::get_log_str(table_inx, UI_COMMANDS_1::LAST_INDEX);
      break;
    case 2:
      s = UI_COMMANDS_2::get_log_str(table_inx, UI_COMMANDS_2::LAST_INDEX);
      break;
    case 3:
      s = UI_COMMANDS_3::get_log_str(table_inx, UI_COMMANDS_3::LAST_INDEX);
      break;
    case 4:
      s = UI_COMMANDS_4::get_log_str(table_inx, UI_COMMANDS_4::LAST_INDEX);
      break;
    case 5:
      s = UI_COMMANDS_5::get_log_str(table_inx, UI_COMMANDS_5::LAST_INDEX);
      break;
    case 6:
      s = UI_COMMANDS_6::get_log_str(table_inx, UI_COMMANDS_6::LAST_INDEX);
      break;
    case 7:
      s = UI_COMMANDS_7::get_log_str(table_inx, UI_COMMANDS_7::LAST_INDEX);
      break;
    default:
      s = LOG_STR("?");
  }
  bool ok = append_operand_(s);
  if (!ok) {
    return false;
  }
  // out of the 100+ UI commands, a few exceptional UI commands have appended an extra parameter:
  switch (command) {
    case 0x56:
      return do_operand_<UI_BROADCAST_TYPE>();
    case 0x57:
      return do_operand_<UI_SOUND_PRESENTATION_CONTROL>();
    case 0x60:
      return do_operand_<PLAY_MODE>();
    case 0x67:
      return do_operand_<CHANNEL_IDENTIFIER>();
    case 0x68:
      return do_operand_<UI_FUNCTION_MEDIA>();
    case 0x69:
      return do_operand_<UI_FUNCTION_SELECT_AV_INPUT>();
    case 0x6A:
      return do_operand_<UI_FUNCTION_SELECT_AUDIO_INPUT>();
    default:
      return ok;
  }
}

template<> bool Decoder::do_operand_<Decoder::VENDOR_ID>() {
  if (offset_ + 2 >= frame_.size()) {
    return append_operand_("?", 3);
  }
  uint32_t id = (uint32_t) (frame_[offset_]) << 16 | (uint32_t) (frame_[offset_ + 1]) << 8 | frame_[offset_ + 2];
  const char *const *name_lookup = VENDOR_IDS.find(id);
  const char *vendor_name = name_lookup ? (*name_lookup) : nullptr;
  if (!vendor_name) {
    // if the hdmi-cec vendor id is not in our list, the id value itself is printed.
    char line[12];
    std::snprintf(line, sizeof(line), "ID=%06x", id);
    return append_operand_(line, 3);
  }
  return append_operand_(vendor_name, 3);
}

PROGMEM_STRING_TABLE(CEC_VERSIONS, "?", "1.2", "1.2a", "1.3", "1.3a", "1.4", "2.0", "2.x", "2.x", "?");
template<> bool Decoder::do_operand_<Decoder::CEC_VERSION>() { return append_operand_<CEC_VERSIONS>(); }

// Must be given sorted on increasing opcode:
// opcode,   name,       function pointer with specification of its operands
#define CEC_OPCODE_LIST(X) \
  X(0x00, "Feature Abort", &Decoder::do_operand_<two(FEATURE_OPCODE, ABORT_REASON)>) \
  X(0x04, "Image View On", &Decoder::do_operand_<NONE>) \
  X(0x05, "Tuner Step Increment", &Decoder::do_operand_<NONE>) \
  X(0x06, "Tuner Step Decrement", &Decoder::do_operand_<NONE>) \
  X(0x07, "Tuner Device Status", &Decoder::do_operand_<TUNER_DEVICE_INFO>) \
  X(0x08, "Give Tuner Device Status", &Decoder::do_operand_<STATUS_REQUEST>) \
  X(0x09, "Record On", &Decoder::do_operand_<RECORD_SOURCE>) \
  X(0x0A, "Record Status", &Decoder::do_operand_<RECORD_STATUS_INFO>) \
  X(0x0D, "Text View On", &Decoder::do_operand_<NONE>) \
  X(0x0B, "Record Off", &Decoder::do_operand_<NONE>) \
  X(0x0F, "Record TV Screen", &Decoder::do_operand_<NONE>) \
  X(0x1A, "Give Deck Status", &Decoder::do_operand_<STATUS_REQUEST>) \
  X(0x1B, "Deck Status", &Decoder::do_operand_<DECK_INFO>) \
  X(0x32, "Set Menu Language", &Decoder::do_operand_<LANGUAGE>) \
  X(0x33, "Clear Analogue Timer", &Decoder::do_operand_<two(START_DATE_TIME, DURATION)>) \
  X(0x34, "Set Analogue Timer", &Decoder::do_operand_<two(START_DATE_TIME, DURATION)>) \
  X(0x35, "Timer Status", &Decoder::do_operand_<TIMER_STATUS_DATA>) \
  X(0x36, "Standby", &Decoder::do_operand_<NONE>) \
  X(0x41, "Play", &Decoder::do_operand_<PLAY_MODE>) \
  X(0x42, "Deck Control", &Decoder::do_operand_<DECK_CONTROL_MODE>) \
  X(0x43, "Timer Cleared Status", &Decoder::do_operand_<TIMER_CLEARED_STATUS_DATA>) \
  X(0x44, "User Control Pressed", &Decoder::do_operand_<UI_COMMAND>) \
  X(0x45, "User Control Released", &Decoder::do_operand_<NONE>) \
  X(0x46, "Give OSD Name", &Decoder::do_operand_<NONE>) \
  X(0x47, "Set OSD Name", &Decoder::do_operand_<OSD_NAME>) \
  X(0x64, "Set OSD String", &Decoder::do_operand_<two(DISPLAY_CONTROL, OSD_STRING)>) \
  X(0x67, "Set Timer Program Title", &Decoder::do_operand_<PROGRAM_TITLE_STRING>) \
  X(0x70, "System Audio Mode Request", &Decoder::do_operand_<PHYSICAL_ADDRESS>) \
  X(0x71, "Give Audio Status", &Decoder::do_operand_<NONE>) \
  X(0x72, "Set System Audio Mode", &Decoder::do_operand_<SYSTEM_AUDIO_STATUS>) \
  X(0x7A, "Report Audio Status", &Decoder::do_operand_<AUDIO_STATUS>) \
  X(0x7D, "Give System Audio Mode Status", &Decoder::do_operand_<NONE>) \
  X(0x7E, "System Audio Mode Status", &Decoder::do_operand_<SYSTEM_AUDIO_STATUS>) \
  X(0x80, "Routing Change", &Decoder::do_operand_<two(PHYSICAL_ADDRESS, PHYSICAL_ADDRESS)>) \
  X(0x81, "Routing Information", &Decoder::do_operand_<PHYSICAL_ADDRESS>) \
  X(0x82, "Active Source", &Decoder::do_operand_<PHYSICAL_ADDRESS>) \
  X(0x83, "Give Physical Address", &Decoder::do_operand_<NONE>) \
  X(0x84, "Report Physical Address", &Decoder::do_operand_<two(PHYSICAL_ADDRESS, DEVICE_TYPE)>) \
  X(0x85, "Request Active Source", &Decoder::do_operand_<NONE>) \
  X(0x86, "Set Stream Path", &Decoder::do_operand_<PHYSICAL_ADDRESS>) \
  X(0x87, "Device Vendor ID", &Decoder::do_operand_<VENDOR_ID>) \
  X(0x89, "Vendor Command", &Decoder::do_operand_<VENDOR_SPECIFIC_DATA>) \
  X(0x8A, "Vendor Remote Button Down", &Decoder::do_operand_<VENDOR_SPECIFIC_RC_CODE>) \
  X(0x8B, "Vendor Remote Button Up", &Decoder::do_operand_<NONE>) \
  X(0x8C, "Give Device Vendor ID", &Decoder::do_operand_<NONE>) \
  X(0x8D, "Menu Request", &Decoder::do_operand_<MENU_REQUEST_TYPE>) \
  X(0x8E, "Menu Status", &Decoder::do_operand_<MENU_STATE>) \
  X(0x8F, "Give Device Power Status", &Decoder::do_operand_<NONE>) \
  X(0x90, "Report Power Status", &Decoder::do_operand_<POWER_STATUS>) \
  X(0x91, "Get Menu Language", &Decoder::do_operand_<NONE>) \
  X(0x92, "Select Analogue Service", \
    &Decoder::do_operand_<three(ANALOG_BROADCAST_TYPE, ANALOG_FREQUENCY, BROADCAST_SYSTEM)>) \
  X(0x93, "Select Digital Service", &Decoder::do_operand_<DIGITAL_SERVICE_IDENTIFICATION>) \
  X(0x9A, "Set Audio Rate", &Decoder::do_operand_<AUDIO_RATE>) \
  X(0x9D, "Inactive Source", &Decoder::do_operand_<PHYSICAL_ADDRESS>) \
  X(0x97, "Set Digital Timer", &Decoder::do_operand_<two(START_DATE_TIME, DURATION)>) \
  X(0x99, "Clear Digital Timer", &Decoder::do_operand_<two(START_DATE_TIME, DURATION)>) \
  X(0x9E, "CEC Version", &Decoder::do_operand_<CEC_VERSION>) \
  X(0x9F, "Get CEC Version", &Decoder::do_operand_<NONE>) \
  X(0xA0, "Vendor Command With ID", &Decoder::do_operand_<two(VENDOR_ID, VENDOR_SPECIFIC_DATA)>) \
  X(0xA1, "Clear External Timer", &Decoder::do_operand_<two(START_DATE_TIME, DURATION)>) \
  X(0xA2, "Set External Timer", &Decoder::do_operand_<two(START_DATE_TIME, DURATION)>) \
  X(0xA3, "Report Short Audio Descriptor", &Decoder::do_operand_<SHORT_AUDIO_DESCRIPTOR>) \
  X(0xA4, "Request Short Audio Descriptor", &Decoder::do_operand_<AUDIO_FORMAT>) \
  X(0xC0, "Initiate ARC", &Decoder::do_operand_<NONE>) \
  X(0xC1, "Report ARC Initiated", &Decoder::do_operand_<NONE>) \
  X(0xC2, "Report ARC Terminated", &Decoder::do_operand_<NONE>) \
  X(0xC3, "Request ARC Initiation", &Decoder::do_operand_<NONE>) \
  X(0xC4, "Request ARC Termination", &Decoder::do_operand_<NONE>) \
  X(0xC5, "Terminate ARC", &Decoder::do_operand_<NONE>) \
  X(0xF8, "CDC Message", &Decoder::do_operand_<NONE>) \
  X(0xFF, "Abort", &Decoder::do_operand_<NONE>)

#ifdef ESP8266
#include <pgmspace.h>
// On ESP8266, we declare individual const strings in Flash
#define DECLARE_STR(op, name, func) static const char STR_##op[] PROGMEM = name;
CEC_OPCODE_LIST(DECLARE_STR)

// Then map the table to use those Flash pointers
#define TABLE_ENTRY(op, name, func) {op, {(const LogString *) STR_##op, func}},
#else
// On ESP32, rp2040, or Host, just use the string literal directly
#define TABLE_ENTRY(op, name, func) {op, {(const LogString *) name, func}},
#endif

// TODO: move the table itself also to flash for the ESP8266.
// That requires further #ifdefs : use of 'pgm_read_byte' in 'find' (to read the key from flash),
// as well as use of 'pgm_read_ptr(' to read the functionpointer from flash.
const Decoder::CecOpcodeTable Decoder::CEC_OPCODE_TABLE = {{CEC_OPCODE_LIST(TABLE_ENTRY)}};

const Decoder::VendorIdTable Decoder::VENDOR_IDS = {
    {// Must be given sorted on increasing id:
     {0x000039, "Toshiba"}, {0x0000F0, "Samsung"},     {0x0005CD, "Denon"},         {0x000678, "Maranz"},
     {0x000982, "Loewe"},   {0x0009B0, "Onkyo"},       {0x000CB8, "Medion"},        {0x000CE7, "Toshiba"},
     {0x0010FA, "Apple"},   {0x001582, "Pulse Eight"}, {0x001950, "Harman Kardon"}, {0x001A11, "Google"},
     {0x0020C7, "Akai"},    {0x002467, "AOC"},         {0x008045, "Panasonic"},     {0x00903E, "Philips"},
     {0x009053, "Daewoo"},  {0x00A0DE, "Yamaha"},      {0x00D0D5, "Grundig"},       {0x00E036, "Pioneer"},
     {0x00E091, "LG"},      {0x08001F, "Sharp"},       {0x080046, "Sony"},          {0x18C086, "Broadcom"},
     {0x534850, "Sharp"},   {0x6B746D, "Vizio"},       {0x8065E9, "Benq"},          {0x9C645E, "Harman Kardon"}}};

PROGMEM_STRING_TABLE(ADDRESS_NAMES, "TV", "RecordingDev1", "RecordingDev2", "Tuner1", "PlaybackDev1", "AudioSystem",
                     "Tuner2", "Tuner3", "PlaybackDev2", "RecordingDev3", "Tuner4", "PlaybackDev3", "Reserved",
                     "Reserved", "SpecificUse", "Unregistered", "?");
void Decoder::address_decode_() {
  const LogString *dst = (frame_.is_broadcast())
                             ? LOG_STR("All")
                             : ADDRESS_NAMES::get_log_str(frame_.destination_addr(), ADDRESS_NAMES::LAST_INDEX);
  const LogString *src = ADDRESS_NAMES::get_log_str(frame_.initiator_addr(), ADDRESS_NAMES::LAST_INDEX);
#if defined(USE_ARDUINO)
  // Use Arduino extensions for strings in flash (snprintf_P and %S), required for ESP8266
  incr_length_(snprintf_P(line_.data() + length_, line_.size() - length_, PSTR("%S to %S"), (const char *) src,
                          (const char *) dst));
#else
  // Use the standard C library (with IDF, Host) assuming a unified ram&flash address space
  incr_length_(
      snprintf(line_.data() + length_, line_.size() - length_, "%s to %s", (const char *) src, (const char *) dst));
#endif
}

const LogString *Decoder::find_opcode_name_(uint32_t opcode) const {
  const FrameType *frametype = CEC_OPCODE_TABLE.find(opcode);

  if (!frametype)
    return LOG_STR("?");

  return frametype->name_;
}

/**
 * Helper function to implement the 'do_operand' methods, to gather a textual representation.
 * Note: the use of '[]' corresponds to the notation used in the CEC standard description.
 * @return true if a further operand can be decoded, false otherwise
 */
bool Decoder::append_operand_(const char *word, uint8_t offset_incr /* default 1 */) {
  APPEND_LINE("[%s]", word);
  offset_ += offset_incr;
  return (length_ < line_.size() - 1) && (offset_ < frame_.size());
}
// similar method, in case the 'word' is stored in flash
bool Decoder::append_operand_(const LogString *word, uint8_t offset_incr /* default 1 */) {
#if defined(USE_ARDUINO)
  // Use Arduino extensions for strings in flash (snprintf_P and %S), required for ESP8266
  incr_length_(snprintf_P(line_.data() + length_, line_.size() - length_, PSTR("[%S]"), (const char *) word));
#else
  // Use the standard C library (with IDF, Host) assuming a unified ram&flash address space
  incr_length_(snprintf(line_.data() + length_, line_.size() - length_, "[%s]", (const char *) word));
#endif
  offset_ += offset_incr;
  return (length_ < line_.size() - 1) && (offset_ < frame_.size());
}

/**
 * Entry function 'decode' to call for full decode of a CEC frame
 */
const char *Decoder::decode() {
  // prepare text buffer as empty
  length_ = 0;         // currently accumulated length of text of operands
  line_[length_] = 0;  // initialise operand text to empty string

  // print src and dest fields into 'line_'
  address_decode_();

  // print opcode field
  const FrameType *frametype = nullptr;
  if (frame_.size() <= 1) {
    // Missing frame operation field?
    APPEND_LINE("Ping");
  } else {
    // find operation name
    frametype = CEC_OPCODE_TABLE.find(frame_.opcode());
    const LogString *opname = frametype ? frametype->name_ : LOG_STR("?");
    APPEND_LINE("<%s>", opname);
  }

  // convert operand fields to text:
  if (frametype) {
    offset_ = 2;  // location in frame of first operand, after address-byte and opcode-byte
    OperandDecode_f f = frametype->decode_f_;
    (this->*f)();  // call one of the above 'do_operand_<xx>' methods
  }
  return line_.data();
}

}  // namespace hdmi_cec
}  // namespace esphome
