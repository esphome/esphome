#include "ld2420.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

/*
Configure commands - little endian

No command can exceed 64 bytes, otherwise they would need be to be split up into multiple sends.

All send command frames will have:
  Header = FD FC FB FA, Bytes 0 - 3, uint32_t 0xFAFBFCFD
  Length, bytes 4 - 5, uint16_t 0x0002, must be at least 2 for the command byte if no addon data.
  Command bytes 6 - 7, uint16_t
  Footer = 04 03 02 01 - uint32_t 0x01020304, Always last 4 Bytes.
Receive
  Error bytes 8-9 uint16_t, 0 = success, all other positive values = error

Enable config mode:
Send:
  UART Tx: FD FC FB FA 04 00 FF 00 02 00 04 03 02 01
  Command = FF 00 - uint16_t 0x00FF
  Protocol version = 02 00, can be 1 or 2 - uint16_t 0x0002
Reply:
  UART Rx: FD FC FB FA 06 00 FF 01 00 00 02 00 04 03 02 01

Disable config mode:
Send:
  UART Tx: FD FC FB FA 02 00 FE 00 04 03 02 01
  Command = FE 00 - uint16_t 0x00FE
Receive:
  UART Rx: FD FC FB FA 04 00 FE 01 00 00 04 03 02 01

Configure system parameters:

UART Tx: FD FC FB FA 08 00 12 00 00 00 64 00 00 00 04 03 02 01  Set system parms
Command = 12 00 - uint16_t 0x0012, Param
There are three documented parameters for modes:
  00 64 = Basic status mode
    This mode outputs text as presence "ON" or  "OFF" and "Range XXXX"
    where XXXX is a decimal value for distance in cm
  00 04 = Energy output mode
    This mode outputs detailed signal energy values for each gate and the target distance.
    The data format consist of the following.
    Header HH, Length LL, Presence PP, Distance DD, 16 Gate Energies EE, Footer FF
    HH HH HH HH LL LL PP DD DD EE EE .. 16x   .. FF FF FF FF
    F4 F3 F2 F1 23 00 00 00 00 00 00 .. .. .. .. F8 F7 F6 F5
  00 00 = debug output mode
    This mode outputs detailed values consisting of 20 Dopplers, 16 Ranges for a total 20 * 16 * 4 bytes
    The data format consist of the following.
    Header HH, Doppler DD, Range RR, Footer FF
    HH HH HH HH DD DD DD DD .. 20x   .. RR RR RR RR .. 16x   .. FF FF FF FF
    AA BF 10 14 00 00 00 00 .. .. .. .. 00 00 00 00 .. .. .. .. FD FC FB FA

Configure gate sensitivity parameters:
UART Tx: FD FC FB FA 0E 00 07 00 10 00 60 EA 00 00 20 00 60 EA 00 00 04 03 02 01
Command = 12 00 - uint16_t 0x0007
Gate 0 high thresh = 10 00 uint16_t 0x0010, Threshold value = 60 EA 00 00 uint32_t 0x0000EA60
Gate 0 low thresh = 20 00 uint16_t 0x0020, Threshold value = 60 EA 00 00 uint32_t 0x0000EA60
*/

namespace esphome::ld2420 {

static const char *const TAG = "ld2420";

// Local const's
static constexpr uint16_t REFRESH_RATE_MS = 1000;
static constexpr uint32_t CMD_ACK_TIMEOUT_MS = 1000;
static constexpr uint8_t CMD_MAX_RETRIES = 3;

// Startup state machine timing. The module starts transmitting ~3.5 s after a
// power cycle; the first listen window is roughly three times that to be
// safe, and the shorter retry window still stays above the boot silence in
// case the module reset itself between attempts.
static constexpr uint32_t STARTUP_LISTEN_TIMEOUT_MS = 10000;
static constexpr uint32_t STARTUP_RETRY_LISTEN_MS = 5000;
static constexpr uint32_t STARTUP_LISTEN_SETTLE_MS = 500;
static constexpr uint8_t STARTUP_SEQUENCE_MAX_RETRIES = 3;
// Minimum reply data lengths for the startup reads: the limits read returns
// three values and each gate read returns two, four bytes each plus the four
// status bytes counted in the reply length field
static constexpr uint8_t REPLY_MIN_LEN_LIMITS = 16;
static constexpr uint8_t REPLY_MIN_LEN_GATE = 12;

// Command sets
static constexpr uint16_t CMD_DISABLE_CONF = 0x00FE;
static constexpr uint16_t CMD_ENABLE_CONF = 0x00FF;
static constexpr uint16_t CMD_PARM_HIGH_TRESH = 0x0012;
static constexpr uint16_t CMD_PARM_LOW_TRESH = 0x0021;
static constexpr uint16_t CMD_PROTOCOL_VER = 0x0002;
static constexpr uint16_t CMD_READ_ABD_PARAM = 0x0008;
static constexpr uint16_t CMD_READ_REG_ADDR = 0x0020;
static constexpr uint16_t CMD_READ_REGISTER = 0x0002;
static constexpr uint16_t CMD_READ_SERIAL_NUM = 0x0011;
static constexpr uint16_t CMD_READ_SYS_PARAM = 0x0013;
static constexpr uint16_t CMD_READ_VERSION = 0x0000;
static constexpr uint16_t CMD_RESTART = 0x0068;
static constexpr uint16_t CMD_SYSTEM_MODE = 0x0000;
static constexpr uint16_t CMD_SYSTEM_MODE_GR = 0x0003;
static constexpr uint16_t CMD_SYSTEM_MODE_MTT = 0x0001;
static constexpr uint16_t CMD_SYSTEM_MODE_SIMPLE = 0x0064;
static constexpr uint16_t CMD_SYSTEM_MODE_DEBUG = 0x0000;
static constexpr uint16_t CMD_SYSTEM_MODE_ENERGY = 0x0004;
static constexpr uint16_t CMD_SYSTEM_MODE_VS = 0x0002;
static constexpr uint16_t CMD_WRITE_ABD_PARAM = 0x0007;
static constexpr uint16_t CMD_WRITE_REGISTER = 0x0001;
static constexpr uint16_t CMD_WRITE_SYS_PARAM = 0x0012;

static constexpr uint8_t CMD_ABD_DATA_REPLY_SIZE = 0x04;
static constexpr uint8_t CMD_ABD_DATA_REPLY_START = 0x0A;
static constexpr uint8_t CMD_MAX_BYTES = 0x64;
static constexpr uint8_t CMD_REG_DATA_REPLY_SIZE = 0x02;

static constexpr uint8_t LD2420_ERROR_NONE = 0x00;
static constexpr uint8_t LD2420_ERROR_TIMEOUT = 0x02;
static constexpr uint8_t LD2420_ERROR_UNKNOWN = 0x01;

// Register address values
static constexpr uint16_t CMD_MIN_GATE_REG = 0x0000;
static constexpr uint16_t CMD_MAX_GATE_REG = 0x0001;
static constexpr uint16_t CMD_TIMEOUT_REG = 0x0004;
static constexpr uint16_t CMD_GATE_MOVE_THRESH[TOTAL_GATES] = {0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015,
                                                               0x0016, 0x0017, 0x0018, 0x0019, 0x001A, 0x001B,
                                                               0x001C, 0x001D, 0x001E, 0x001F};
static constexpr uint16_t CMD_GATE_STILL_THRESH[TOTAL_GATES] = {0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025,
                                                                0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B,
                                                                0x002C, 0x002D, 0x002E, 0x002F};
static constexpr uint32_t FACTORY_MOVE_THRESH[TOTAL_GATES] = {60000, 30000, 400, 250, 250, 250, 250, 250,
                                                              250,   250,   250, 250, 250, 250, 250, 250};
static constexpr uint32_t FACTORY_STILL_THRESH[TOTAL_GATES] = {40000, 20000, 200, 200, 200, 200, 200, 150,
                                                               150,   100,   100, 100, 100, 100, 100, 100};
static constexpr uint16_t FACTORY_TIMEOUT = 120;
static constexpr uint16_t FACTORY_MIN_GATE = 1;
static constexpr uint16_t FACTORY_MAX_GATE = 12;

// COMMAND_BYTE Header & Footer
static constexpr uint32_t CMD_FRAME_FOOTER = 0x01020304;
static constexpr uint32_t CMD_FRAME_HEADER = 0xFAFBFCFD;
static constexpr uint32_t DEBUG_FRAME_FOOTER = 0xFAFBFCFD;
static constexpr uint32_t DEBUG_FRAME_HEADER = 0x1410BFAA;
static constexpr uint32_t ENERGY_FRAME_FOOTER = 0xF5F6F7F8;
static constexpr uint32_t ENERGY_FRAME_HEADER = 0xF1F2F3F4;
static constexpr int CALIBRATE_VERSION_MIN = 154;
static constexpr uint8_t CMD_FRAME_COMMAND = 6;
static constexpr uint8_t CMD_FRAME_DATA_LENGTH = 4;
static constexpr uint8_t CMD_FRAME_STATUS = 7;
static constexpr uint8_t CMD_ERROR_WORD = 8;
static constexpr uint8_t ENERGY_SENSOR_START = 9;
static constexpr uint8_t CALIBRATE_REPORT_INTERVAL = 4;
static const char *const OP_NORMAL_MODE_STRING = "Normal";
static const char *const OP_SIMPLE_MODE_STRING = "Simple";

// Memory-efficient lookup tables
struct StringToUint8 {
  const char *str;
  const uint8_t value;
};

static constexpr StringToUint8 OP_MODE_BY_STR[] = {
    {"Normal", OP_NORMAL_MODE},
    {"Calibrate", OP_CALIBRATE_MODE},
    {"Simple", OP_SIMPLE_MODE},
};

static constexpr const char *ERR_MESSAGE[] = {
    "None",
    "Unknown",
    "Timeout",
};

// Helper function for lookups
template<size_t N> uint8_t find_uint8(const StringToUint8 (&arr)[N], const std::string &str) {
  for (const auto &entry : arr) {
    if (str == entry.str) {
      return entry.value;
    }
  }
  return 0xFF;  // Not found
}

static uint8_t calc_checksum(void *data, size_t size) {
  uint8_t checksum = 0;
  uint8_t *data_bytes = (uint8_t *) data;
  for (size_t i = 0; i < size; i++) {
    checksum ^= data_bytes[i];  // XOR operation
  }
  return checksum;
}

static int32_t get_firmware_int(const char *version_string) {
  // Convert "v1.5.4" -> 154 by skipping 'v' and '.', accumulating digits
  const char *p = (*version_string == 'v') ? version_string + 1 : version_string;
  int32_t result = 0;
  for (; *p != '\0'; p++) {
    if (*p == '.')
      continue;
    if (*p < '0' || *p > '9')
      return 0;
    result = result * 10 + (*p - '0');
  }
  return result;
}

void LD2420Component::dump_config() {
  // Setup no longer blocks, so the config dump usually runs before the
  // version is read; do not present the "v0.0.0" placeholder as real
  const int32_t firmware = ld2420::get_firmware_int(this->firmware_ver_);
  ESP_LOGCONFIG(TAG,
                "LD2420:\n"
                "  Firmware version: %7s",
                firmware > 0 ? this->firmware_ver_ : "unknown");
#ifdef USE_NUMBER
  ESP_LOGCONFIG(TAG, "Number:");
  LOG_NUMBER("  ", "Gate Timeout:", this->gate_timeout_number_);
  LOG_NUMBER("  ", "Gate Max Distance:", this->max_gate_distance_number_);
  LOG_NUMBER("  ", "Gate Min Distance:", this->min_gate_distance_number_);
  LOG_NUMBER("  ", "Gate Select:", this->gate_select_number_);
  for (uint8_t gate = 0; gate < TOTAL_GATES; gate++) {
    LOG_NUMBER("  ", "Gate Move Threshold:", this->gate_move_threshold_numbers_[gate]);
    LOG_NUMBER("  ", "Gate Still Threshold::", this->gate_still_threshold_numbers_[gate]);
  }
#endif
#ifdef USE_BUTTON
  LOG_BUTTON("  ", "Apply Config:", this->apply_config_button_);
  LOG_BUTTON("  ", "Revert Edits:", this->revert_config_button_);
  LOG_BUTTON("  ", "Factory Reset:", this->factory_reset_button_);
  LOG_BUTTON("  ", "Restart Module:", this->restart_module_button_);
#endif
#ifdef USE_SELECT
  ESP_LOGCONFIG(TAG, "Select:");
  LOG_SELECT("  ", "Operating Mode", this->operating_selector_);
#endif
  if (firmware > 0 && firmware < CALIBRATE_VERSION_MIN) {
    ESP_LOGW(TAG, "Firmware version %s and older supports Simple Mode only", this->firmware_ver_);
  }
}

void LD2420Component::setup() { this->begin_startup_(); }

void LD2420Component::begin_startup_() {
  // Default to energy mode so the stream parser can frame data from a module
  // that kept streaming across a soft restart, before the mode is negotiated.
  this->system_mode_ = CMD_SYSTEM_MODE_ENERGY;
  this->startup_sequence_retries_ = 0;
  this->config_read_complete_ = false;
  this->begin_listen_();
}

void LD2420Component::begin_listen_() {
  this->phase_start_ms_ = millis();
  this->startup_state_ = StartupState::STARTUP_STATE_LISTEN_SETTLE;
}

void LD2420Component::drain_rx_() {
  uint8_t buf[MAX_LINE_LENGTH];
  size_t avail;
  while ((avail = this->available()) > 0) {
    if (!this->read_array(buf, std::min(avail, sizeof(buf)))) {
      ESP_LOGV(TAG, "Failed to drain the receive buffer");
      break;
    }
  }
  this->buffer_pos_ = 0;
}

// Builds the command frame for the current startup state; returns false when
// the state has no associated command
bool LD2420Component::build_startup_frame_(CmdFrameT &frame) {
  switch (this->startup_state_) {
    case StartupState::STARTUP_STATE_ENTER_CONFIG:
      this->build_config_mode_frame_(frame, true);
      return true;
    case StartupState::STARTUP_STATE_READ_LIMITS:
      this->build_min_max_timeout_frame_(frame);
      return true;
    case StartupState::STARTUP_STATE_READ_VERSION:
      this->build_version_frame_(frame);
      return true;
    case StartupState::STARTUP_STATE_READ_GATES:
      this->build_gate_threshold_frame_(frame, this->startup_gate_);
      return true;
    case StartupState::STARTUP_STATE_SET_MODE:
      this->build_system_mode_frame_(frame, this->startup_target_mode_);
      return true;
    case StartupState::STARTUP_STATE_EXIT_CONFIG:
      this->build_config_mode_frame_(frame, false);
      return true;
    default:
      return false;
  }
}

void LD2420Component::send_startup_cmd_() {
  CmdFrameT frame;
  if (!this->build_startup_frame_(frame)) {
    // Programming error: a command state without a frame would otherwise look
    // exactly like a module timeout
    ESP_LOGE(TAG, "No command frame for startup state %u", (unsigned) this->startup_state_);
    return;
  }
  // Discard anything still buffered (including a late reply to a previous
  // send of the same command) so a stale ack cannot be matched to this one.
  // READ_LIMITS and all gate reads share the same command byte, so a late
  // reply accepted for the wrong request would shift every following gate's
  // thresholds by one.
  this->drain_rx_();
  this->startup_cmd_ = (uint8_t) frame.command;
  this->cmd_reply_.ack = false;
  this->cmd_reply_.error = 0;
  // A short reply acks without filling every data word; zero them so stale
  // values from the previous command cannot be stored as this command's data
  memset(this->cmd_reply_.data, 0, sizeof(this->cmd_reply_.data));
  this->write_cmd_frame_(frame);
  this->phase_start_ms_ = millis();
}

void LD2420Component::start_startup_cmd_(StartupState state) {
  this->startup_state_ = state;
  this->startup_cmd_attempts_ = 1;
  this->send_startup_cmd_();
}

// Common ack handling for the startup commands: returns true once the reply to
// the current startup frame arrived; resends on timeout, and after too many
// failed sends either restarts the whole sequence or gives up with a warning.
// A reply shorter than min_data_len is treated like silence so a truncated
// read cannot be stored as zeroed configuration.
bool LD2420Component::startup_ack_check_(uint8_t min_data_len) {
  if (this->cmd_reply_.ack && this->cmd_reply_.command == this->startup_cmd_ &&
      this->cmd_reply_.length >= min_data_len) {
    return true;
  }
  if (this->cmd_reply_.error > 0) {
    // The module explicitly rejected the command; log why instead of letting
    // it look like silence. The normal retry cadence still applies.
    this->handle_cmd_error(this->cmd_reply_.error);
    this->cmd_reply_.error = 0;
  }
  if (millis() - this->phase_start_ms_ <= CMD_ACK_TIMEOUT_MS) {
    return false;
  }
  if (this->startup_cmd_attempts_ < CMD_MAX_RETRIES) {
    this->startup_cmd_attempts_++;
    ESP_LOGV(TAG, "No reply to startup command %2X; resending", this->startup_cmd_);
    this->send_startup_cmd_();
    return false;
  }
  this->abort_startup_cmd_();
  if (this->startup_sequence_retries_ < STARTUP_SEQUENCE_MAX_RETRIES) {
    this->startup_sequence_retries_++;
    ESP_LOGW(TAG, "Module setup attempt %u failed; retrying", this->startup_sequence_retries_);
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
    this->begin_listen_();
    return false;
  }
  this->abandon_startup_();
  return false;
}

// Gives up on configuration but keeps parsing the stream; a module that is
// still streaming keeps publishing sensor data even without a config read.
void LD2420Component::abandon_startup_() {
  ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  if (ld2420::get_firmware_int(this->firmware_ver_) == 0) {
    // Old firmware streams text frames that are only parsed in simple mode;
    // without a version read the mode was never negotiated, so such a module
    // will not publish sensor data either.
    ESP_LOGE(TAG, "Firmware version and operating mode were never read");
  } else if (this->startup_state_ == StartupState::STARTUP_STATE_SET_MODE) {
    ESP_LOGE(TAG, "Operating mode write was not acknowledged; sensor data may not be parsed");
  }
  // Keep the editable config in sync with what was actually read so a later
  // Apply Config cannot write values that were never read from the module
  memcpy(&this->new_config, &this->current_config, sizeof(this->current_config));
#ifdef USE_NUMBER
  // Publish whatever was read before giving up so the number entities show
  // values next to the warning status instead of staying unknown forever
  this->init_gate_config_numbers();
#endif
  this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
  this->startup_state_ = StartupState::STARTUP_STATE_RUNNING;
}

void LD2420Component::abort_startup_cmd_() {
  // If the module already acknowledged config mode it stops streaming until
  // config mode is exited, so send the exit command blind before abandoning
  // the sequence; otherwise the stream never resumes and neither passive
  // parsing nor the next listen phase would ever see data. This is also sent
  // when config mode was never acknowledged: the ack may merely have been
  // lost, and the frame is harmless to a module that is not in config mode.
  CmdFrameT frame;
  this->build_config_mode_frame_(frame, false);
  this->write_cmd_frame_(frame);
}

void LD2420Component::loop_startup_(bool got_data) {
  switch (this->startup_state_) {
    case StartupState::STARTUP_STATE_LISTEN_SETTLE:
      // Bytes can already be in flight when the listen phase starts: the tail
      // of a frame the module was transmitting when it was told to restart,
      // stale data buffered before setup, or the ack to the blind config mode
      // exit. Discard everything received during this settle window so only
      // data the module sends afterwards counts as proof that it is up and
      // streaming. The state runs at least one drain pass even when the main
      // loop stalls past the whole window, so bytes that arrived before the
      // listen phase can never be mistaken for fresh data.
      this->drain_rx_();
      if (millis() - this->phase_start_ms_ >= STARTUP_LISTEN_SETTLE_MS) {
        this->phase_start_ms_ = millis();
        this->startup_state_ = StartupState::STARTUP_STATE_LISTEN;
      }
      return;

    case StartupState::STARTUP_STATE_LISTEN:
      // The module locks up until power cycled if it receives data before it
      // has sent its first frame after powering on, so wait until it has
      // provably transmitted before sending anything. (A full-frame check
      // cannot serve as that proof: old-firmware text frames are only
      // recognized once the operating mode is known, which requires the very
      // handshake this phase gates.) A module stuck in some other state
      // stays quiet, so fall through after the listen window.
      if (!got_data) {
        const uint32_t listen_timeout_ms =
            this->startup_sequence_retries_ == 0 ? STARTUP_LISTEN_TIMEOUT_MS : STARTUP_RETRY_LISTEN_MS;
        if (millis() - this->phase_start_ms_ < listen_timeout_ms) {
          return;
        }
        ESP_LOGW(TAG, "No data received from the module; attempting configuration anyway");
      }
      // Drop any partial frame so the ack parser starts clean
      this->drain_rx_();
      this->start_startup_cmd_(StartupState::STARTUP_STATE_ENTER_CONFIG);
      return;

    case StartupState::STARTUP_STATE_ENTER_CONFIG:
      if (!this->startup_ack_check_()) {
        return;
      }
      this->start_startup_cmd_(StartupState::STARTUP_STATE_READ_LIMITS);
      return;

    case StartupState::STARTUP_STATE_READ_LIMITS:
      if (!this->startup_ack_check_(REPLY_MIN_LEN_LIMITS)) {
        return;
      }
      this->current_config.min_gate = (uint16_t) this->cmd_reply_.data[0];
      this->current_config.max_gate = (uint16_t) this->cmd_reply_.data[1];
      this->current_config.timeout = (uint16_t) this->cmd_reply_.data[2];
      this->start_startup_cmd_(StartupState::STARTUP_STATE_READ_VERSION);
      return;

    case StartupState::STARTUP_STATE_READ_VERSION: {
      if (!this->startup_ack_check_()) {
        return;
      }
      std::string fw_str(this->firmware_ver_);
      for (auto &listener : this->listeners_) {
        listener->on_fw_version(fw_str);
      }
      this->startup_gate_ = 0;
      this->start_startup_cmd_(StartupState::STARTUP_STATE_READ_GATES);
      return;
    }

    case StartupState::STARTUP_STATE_READ_GATES:
      if (!this->startup_ack_check_(REPLY_MIN_LEN_GATE)) {
        return;
      }
      this->current_config.move_thresh[this->startup_gate_] = this->cmd_reply_.data[0];
      this->current_config.still_thresh[this->startup_gate_] = this->cmd_reply_.data[1];
      if (++this->startup_gate_ < TOTAL_GATES) {
        this->start_startup_cmd_(StartupState::STARTUP_STATE_READ_GATES);
        return;
      }
      this->config_read_complete_ = true;
      memcpy(&this->new_config, &this->current_config, sizeof(this->current_config));
      if (ld2420::get_firmware_int(this->firmware_ver_) < CALIBRATE_VERSION_MIN) {
        this->set_operating_mode(OP_SIMPLE_MODE_STRING);
#ifdef USE_SELECT
        if (this->operating_selector_ != nullptr) {
          this->operating_selector_->publish_state(OP_SIMPLE_MODE_STRING);
        }
#endif
        this->startup_target_mode_ = CMD_SYSTEM_MODE_SIMPLE;
        ESP_LOGW(TAG, "Firmware version %s and older supports Simple Mode only", this->firmware_ver_);
      } else {
        this->startup_target_mode_ = CMD_SYSTEM_MODE_ENERGY;
#ifdef USE_SELECT
        if (this->operating_selector_ != nullptr) {
          this->operating_selector_->publish_state(OP_NORMAL_MODE_STRING);
        }
#endif
      }
#ifdef USE_NUMBER
      this->init_gate_config_numbers();
#endif
      this->start_startup_cmd_(StartupState::STARTUP_STATE_SET_MODE);
      return;

    case StartupState::STARTUP_STATE_SET_MODE:
      if (!this->startup_ack_check_()) {
        return;
      }
      // Switch the parser only after the module acknowledged the mode write,
      // so both sides stay in the same mode when the write is never acked
      this->set_mode_(this->startup_target_mode_);
      this->start_startup_cmd_(StartupState::STARTUP_STATE_EXIT_CONFIG);
      return;

    case StartupState::STARTUP_STATE_EXIT_CONFIG:
      if (!this->startup_ack_check_()) {
        return;
      }
      this->status_clear_warning();
      this->startup_state_ = StartupState::STARTUP_STATE_RUNNING;
      ESP_LOGI(TAG, "Module setup complete; firmware %s", this->firmware_ver_);
      return;

    case StartupState::STARTUP_STATE_RUNNING:
      return;
  }
}

// Common precondition for the button actions: the startup handshake must have
// finished, and actions that write configuration additionally require that
// every limit and gate threshold was actually read (setup may have given up
// partway through; writing the unread config to the module's NVM would wipe
// its stored thresholds).
bool LD2420Component::action_allowed_(bool needs_config) {
  if (this->startup_state_ != StartupState::STARTUP_STATE_RUNNING) {
    ESP_LOGW(TAG, "Module is still starting up; ignoring");
    return false;
  }
  if (needs_config && !this->config_read_complete_) {
    ESP_LOGW(TAG, "Module configuration was never fully read; ignoring");
    return false;
  }
  return true;
}

void LD2420Component::apply_config_action() {
  if (!this->action_allowed_(true)) {
    return;
  }
  const uint8_t checksum = calc_checksum(&this->new_config, sizeof(this->new_config));
  if (checksum == calc_checksum(&this->current_config, sizeof(this->current_config))) {
    ESP_LOGD(TAG, "No configuration change detected");
    return;
  }
  ESP_LOGD(TAG, "Reconfiguring");
  if (this->set_config_mode(true) == LD2420_ERROR_TIMEOUT) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  uint8_t error = this->set_min_max_distances_timeout(this->new_config.max_gate, this->new_config.min_gate,
                                                      this->new_config.timeout);
  for (uint8_t gate = 0; gate < TOTAL_GATES; gate++) {
    delay_microseconds_safe(125);
    error |= this->set_gate_threshold(gate);
  }
  if (error == LD2420_ERROR_NONE) {
    // Only adopt the new values as current once every write was acknowledged
    memcpy(&current_config, &new_config, sizeof(new_config));
  }
#ifdef USE_NUMBER
  this->init_gate_config_numbers();
#endif
  this->set_system_mode(this->system_mode_);
  // Disable config mode to save the new values in the LD2420 nvm
  if (this->set_config_mode(false) == LD2420_ERROR_NONE && error == LD2420_ERROR_NONE) {
    this->status_clear_warning();
  } else {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
  }
  this->set_operating_mode(OP_NORMAL_MODE_STRING);
}

void LD2420Component::factory_reset_action() {
  if (!this->action_allowed_(true)) {
    return;
  }
  ESP_LOGD(TAG, "Setting factory defaults");
  if (this->set_config_mode(true) == LD2420_ERROR_TIMEOUT) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
    return;
  }
  uint8_t error = this->set_min_max_distances_timeout(FACTORY_MAX_GATE, FACTORY_MIN_GATE, FACTORY_TIMEOUT);
#ifdef USE_NUMBER
  this->gate_timeout_number_->state = FACTORY_TIMEOUT;
  this->min_gate_distance_number_->state = FACTORY_MIN_GATE;
  this->max_gate_distance_number_->state = FACTORY_MAX_GATE;
#endif
  for (uint8_t gate = 0; gate < TOTAL_GATES; gate++) {
    this->new_config.move_thresh[gate] = FACTORY_MOVE_THRESH[gate];
    this->new_config.still_thresh[gate] = FACTORY_STILL_THRESH[gate];
    delay_microseconds_safe(125);
    error |= this->set_gate_threshold(gate);
  }
  if (error == LD2420_ERROR_NONE) {
    memcpy(&this->current_config, &this->new_config, sizeof(this->new_config));
  }
  this->set_system_mode(this->system_mode_);
  if (this->set_config_mode(false) == LD2420_ERROR_NONE && error == LD2420_ERROR_NONE) {
    this->status_clear_warning();
  } else {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->status_set_warning(ESP_LOG_MSG_COMM_FAIL);
  }
#ifdef USE_NUMBER
  this->init_gate_config_numbers();
  this->refresh_gate_config_numbers();
#endif
}

void LD2420Component::restart_module_action() {
  if (!this->action_allowed_(false)) {
    return;
  }
  ESP_LOGD(TAG, "Restarting");
  this->send_module_restart();
  // The module is silent while it boots and locks up if it receives data
  // before it has sent its first frame, so re-run the listen-first startup
  // sequence instead of transmitting into the boot window.
  this->begin_startup_();
}

void LD2420Component::revert_config_action() {
  if (!this->action_allowed_(false)) {
    return;
  }
  memcpy(&this->new_config, &this->current_config, sizeof(this->current_config));
#ifdef USE_NUMBER
  this->init_gate_config_numbers();
#endif
  ESP_LOGD(TAG, "Reverted config number edits");
}

void LD2420Component::loop() {
  // If there is a active send command do not process it here, the send command call will handle it.
  if (this->cmd_active_) {
    return;
  }
  const bool got_data = this->read_batch_(this->buffer_data_);
  if (this->startup_state_ != StartupState::STARTUP_STATE_RUNNING) {
    this->loop_startup_(got_data);
  }
}

void LD2420Component::update_radar_data(uint16_t const *gate_energy, uint8_t sample_number) {
  for (uint8_t gate = 0; gate < TOTAL_GATES; ++gate) {
    this->radar_data[gate][sample_number] = gate_energy[gate];
  }
  this->total_sample_number_counter++;
}

void LD2420Component::auto_calibrate_sensitivity() {
  // Calculate average and peak values for each gate
  const float move_factor = gate_move_sensitivity_factor + 1;
  const float still_factor = (gate_still_sensitivity_factor / 2) + 1;
  for (uint8_t gate = 0; gate < TOTAL_GATES; ++gate) {
    uint32_t sum = 0;
    uint16_t peak = 0;

    for (uint8_t sample_number = 0; sample_number < CALIBRATE_SAMPLES; ++sample_number) {
      // Calculate average
      sum += this->radar_data[gate][sample_number];

      // Calculate max value
      if (this->radar_data[gate][sample_number] > peak) {
        peak = this->radar_data[gate][sample_number];
      }
    }

    // Store average and peak values
    this->gate_avg[gate] = sum / CALIBRATE_SAMPLES;
    if (this->gate_peak[gate] < peak) {
      this->gate_peak[gate] = peak;
    }

    uint32_t calculated_value =
        (static_cast<uint32_t>(this->gate_peak[gate]) + (move_factor * static_cast<uint32_t>(this->gate_peak[gate])));
    this->new_config.move_thresh[gate] = static_cast<uint16_t>(calculated_value <= 65535 ? calculated_value : 65535);
    calculated_value =
        (static_cast<uint32_t>(this->gate_peak[gate]) + (still_factor * static_cast<uint32_t>(this->gate_peak[gate])));
    this->new_config.still_thresh[gate] = static_cast<uint16_t>(calculated_value <= 65535 ? calculated_value : 65535);
  }
}

void LD2420Component::report_gate_data() {
  for (uint8_t gate = 0; gate < TOTAL_GATES; ++gate) {
    // Output results
    ESP_LOGI(TAG, "Gate: %2d Avg: %5d Peak: %5d", gate, this->gate_avg[gate], this->gate_peak[gate]);
  }
  ESP_LOGI(TAG, "Total samples: %d", this->total_sample_number_counter);
}

void LD2420Component::set_operating_mode(const char *state) {
  // If unsupported firmware ignore mode select
  if (ld2420::get_firmware_int(firmware_ver_) >= CALIBRATE_VERSION_MIN) {
    this->current_operating_mode = find_uint8(OP_MODE_BY_STR, state);
    // Entering Auto Calibrate we need to clear the previous data collection
#ifdef USE_SELECT
    if (this->operating_selector_ != nullptr) {
      this->operating_selector_->publish_state(state);
    }
#endif
    if (current_operating_mode == OP_CALIBRATE_MODE) {
      this->set_calibration_(true);
      for (uint8_t gate = 0; gate < TOTAL_GATES; gate++) {
        this->gate_avg[gate] = 0;
        this->gate_peak[gate] = 0;
        for (uint8_t i = 0; i < CALIBRATE_SAMPLES; i++) {
          this->radar_data[gate][i] = 0;
        }
        this->total_sample_number_counter = 0;
      }
    } else {
      // Set the current data back so we don't have new data that can be applied in error.
      if (this->get_calibration_()) {
        memcpy(&this->new_config, &this->current_config, sizeof(this->current_config));
      }
      this->set_calibration_(false);
    }
  } else {
    this->current_operating_mode = OP_SIMPLE_MODE;
#ifdef USE_SELECT
    if (this->operating_selector_ != nullptr) {
      this->operating_selector_->publish_state(OP_SIMPLE_MODE_STRING);
    }
#endif
  }
}

void LD2420Component::readline_(int rx_data, uint8_t *buffer, int len) {
  if (rx_data < 0) {
    return;  // No data available
  }
  if (this->buffer_pos_ < len - 1) {
    buffer[this->buffer_pos_++] = rx_data;
    buffer[this->buffer_pos_] = 0;
  } else {
    // We should never get here, but just in case...
    ESP_LOGW(TAG, "Max command length exceeded; ignoring");
    this->buffer_pos_ = 0;
  }
  if (this->buffer_pos_ < 4) {
    return;  // Not enough data to process yet
  }
  if (memcmp(&buffer[this->buffer_pos_ - 4], &CMD_FRAME_FOOTER, sizeof(CMD_FRAME_FOOTER)) == 0) {
    this->cmd_active_ = false;  // Set command state to inactive after response
    this->handle_ack_data_(buffer, this->buffer_pos_);
    this->buffer_pos_ = 0;
  } else if ((buffer[this->buffer_pos_ - 2] == 0x0D && buffer[this->buffer_pos_ - 1] == 0x0A) &&
             (this->get_mode_() == CMD_SYSTEM_MODE_SIMPLE)) {
    this->handle_simple_mode_(buffer, this->buffer_pos_);
    this->buffer_pos_ = 0;
  } else if ((memcmp(&buffer[this->buffer_pos_ - 4], &ENERGY_FRAME_FOOTER, sizeof(ENERGY_FRAME_FOOTER)) == 0) &&
             (this->get_mode_() == CMD_SYSTEM_MODE_ENERGY)) {
    this->handle_energy_mode_(buffer, this->buffer_pos_);
    this->buffer_pos_ = 0;
  }
}

void LD2420Component::handle_energy_mode_(uint8_t *buffer, int len) {
  uint8_t index = 6;  // Start at presence byte position
  uint16_t range;
  const uint8_t elements = sizeof(this->gate_energy_) / sizeof(this->gate_energy_[0]);
  if (len < static_cast<int>(index + 1 + sizeof(range) + elements * sizeof(this->gate_energy_[0]))) {
    ESP_LOGW(TAG, "Energy frame too short: %d bytes", len);
    return;
  }
  this->set_presence_(buffer[index]);
  index++;
  memcpy(&range, &buffer[index], sizeof(range));
  index += sizeof(range);
  this->set_distance_(range);
  for (uint8_t i = 0; i < elements; i++) {  // NOLINT
    memcpy(&this->gate_energy_[i], &buffer[index], sizeof(this->gate_energy_[0]));
    index += sizeof(this->gate_energy_[0]);
  }

  if (this->current_operating_mode == OP_CALIBRATE_MODE) {
    this->update_radar_data(gate_energy_, this->sample_number_counter);
    this->sample_number_counter++;
    if (this->sample_number_counter >= CALIBRATE_SAMPLES) {
      this->sample_number_counter = 0;
    }
  }

  // Resonable refresh rate for home assistant database size health
  const int32_t current_millis = App.get_loop_component_start_time();
  if (current_millis - this->last_periodic_millis < REFRESH_RATE_MS) {
    return;
  }
  this->last_periodic_millis = current_millis;
  for (auto &listener : this->listeners_) {
    listener->on_distance(this->get_distance_());
    listener->on_presence(this->get_presence_());
    listener->on_energy(this->gate_energy_, sizeof(this->gate_energy_) / sizeof(this->gate_energy_[0]));
  }

  if (this->current_operating_mode == OP_CALIBRATE_MODE) {
    this->auto_calibrate_sensitivity();
    if (current_millis - this->report_periodic_millis > REFRESH_RATE_MS * CALIBRATE_REPORT_INTERVAL) {
      this->report_periodic_millis = current_millis;
      this->report_gate_data();
    }
  }
}

void LD2420Component::handle_simple_mode_(const uint8_t *inbuf, int len) {
  const uint8_t bufsize = 16;
  uint8_t index{0};
  uint8_t pos{0};
  char *endptr{nullptr};
  char outbuf[bufsize]{0};
  while (true) {
    if (pos >= 2 && inbuf[pos - 2] == 'O' && inbuf[pos - 1] == 'F' && inbuf[pos] == 'F') {
      this->set_presence_(false);
    } else if (pos >= 1 && inbuf[pos - 1] == 'O' && inbuf[pos] == 'N') {
      this->set_presence_(true);
    }
    if (inbuf[pos] >= '0' && inbuf[pos] <= '9') {
      if (index < bufsize - 1) {
        outbuf[index++] = inbuf[pos];
      }
    }
    if (pos < len - 1) {
      pos++;
    } else {
      break;
    }
  }
  outbuf[index] = '\0';
  if (index > 1) {
    this->set_distance_(strtol(outbuf, &endptr, 10));
  }

  if (this->get_mode_() == CMD_SYSTEM_MODE_SIMPLE) {
    // Resonable refresh rate for home assistant database size health
    const int32_t current_millis = App.get_loop_component_start_time();
    if (current_millis - this->last_normal_periodic_millis < REFRESH_RATE_MS) {
      return;
    }
    this->last_normal_periodic_millis = current_millis;
    for (auto &listener : this->listeners_)
      listener->on_distance(this->get_distance_());
    for (auto &listener : this->listeners_)
      listener->on_presence(this->get_presence_());
  }
}

bool LD2420Component::read_batch_(std::span<uint8_t, MAX_LINE_LENGTH> buffer) {
  // Read all available bytes in batches to reduce UART call overhead.
  size_t avail = this->available();
  const bool got_data = avail > 0;
  uint8_t buf[MAX_LINE_LENGTH];
  while (avail > 0) {
    size_t to_read = std::min(avail, sizeof(buf));
    if (!this->read_array(buf, to_read)) {
      break;
    }
    avail -= to_read;

    for (size_t i = 0; i < to_read; i++) {
      this->readline_(buf[i], buffer.data(), buffer.size());
    }
  }
  return got_data;
}

void LD2420Component::handle_ack_data_(uint8_t *buffer, int len) {
  this->cmd_reply_.command = buffer[CMD_FRAME_COMMAND];
  this->cmd_reply_.length = buffer[CMD_FRAME_DATA_LENGTH];
  uint16_t data_pos = 0;
  if (this->cmd_reply_.length > CMD_MAX_BYTES) {
    ESP_LOGW(TAG, "Reply frame too long");
    return;
  } else if (this->cmd_reply_.length < 2) {
    ESP_LOGW(TAG, "Command frame too short");
    return;
  }
  memcpy(&this->cmd_reply_.error, &buffer[CMD_ERROR_WORD], sizeof(this->cmd_reply_.error));
  const char *result = this->cmd_reply_.error ? "failure" : "success";
  if (this->cmd_reply_.error > 0) {
    return;
  };
  this->cmd_reply_.ack = true;
  switch ((uint16_t) this->cmd_reply_.command) {
    case (CMD_ENABLE_CONF):
      ESP_LOGV(TAG, "Set config enable: CMD = %2X %s", CMD_ENABLE_CONF, result);
      break;
    case (CMD_DISABLE_CONF):
      ESP_LOGV(TAG, "Set config disable: CMD = %2X %s", CMD_DISABLE_CONF, result);
      break;
    case (CMD_READ_REGISTER): {
      ESP_LOGV(TAG, "Read register: CMD = %2X %s", CMD_READ_REGISTER, result);
      // TODO Read/Write register is not implemented yet, this will get flushed out to a proper header file
      data_pos = 0x0A;
      uint16_t reg_count = std::min<uint16_t>((buffer[CMD_FRAME_DATA_LENGTH] - 4) / CMD_REG_DATA_REPLY_SIZE,
                                              sizeof(this->cmd_reply_.data) / sizeof(this->cmd_reply_.data[0]));
      for (uint16_t i = 0; i < reg_count; i++) {
        memcpy(&this->cmd_reply_.data[i], &buffer[data_pos + i * CMD_REG_DATA_REPLY_SIZE], CMD_REG_DATA_REPLY_SIZE);
      }
      break;
    }
    case (CMD_WRITE_REGISTER):
      ESP_LOGV(TAG, "Write register: CMD = %2X %s", CMD_WRITE_REGISTER, result);
      break;
    case (CMD_WRITE_ABD_PARAM):
      ESP_LOGV(TAG, "Write gate parameter(s): %2X %s", CMD_WRITE_ABD_PARAM, result);
      break;
    case (CMD_READ_ABD_PARAM): {
      ESP_LOGV(TAG, "Read gate parameter(s): %2X %s", CMD_READ_ABD_PARAM, result);
      data_pos = CMD_ABD_DATA_REPLY_START;
      uint16_t abd_count = std::min<uint16_t>((buffer[CMD_FRAME_DATA_LENGTH] - 4) / CMD_ABD_DATA_REPLY_SIZE,
                                              sizeof(this->cmd_reply_.data) / sizeof(this->cmd_reply_.data[0]));
      for (uint16_t i = 0; i < abd_count; i++) {
        memcpy(&this->cmd_reply_.data[i], &buffer[data_pos + i * CMD_ABD_DATA_REPLY_SIZE],
               sizeof(this->cmd_reply_.data[i]));
      }
      break;
    }
    case (CMD_WRITE_SYS_PARAM):
      ESP_LOGV(TAG, "Set system parameter(s): %2X %s", CMD_WRITE_SYS_PARAM, result);
      break;
    case (CMD_READ_VERSION): {
      uint8_t ver_len = std::min<uint8_t>(buffer[10], sizeof(this->firmware_ver_) - 1);
      memcpy(this->firmware_ver_, &buffer[12], ver_len);
      this->firmware_ver_[ver_len] = '\0';
      ESP_LOGV(TAG, "Firmware version: %s %s", this->firmware_ver_, result);
      break;
    }
    default:
      break;
  }
}

void LD2420Component::write_cmd_frame_(const CmdFrameT &frame) {
  uint8_t cmd_buffer[MAX_LINE_LENGTH];
  uint16_t length = 0;
  const uint16_t frame_data_bytes = frame.data_length + 2;  // Always add two bytes for the cmd size

  memcpy(&cmd_buffer[length], &frame.header, sizeof(frame.header));
  length += sizeof(frame.header);

  memcpy(&cmd_buffer[length], &frame_data_bytes, sizeof(frame.data_length));
  length += sizeof(frame.data_length);

  memcpy(&cmd_buffer[length], &frame.command, sizeof(frame.command));
  length += sizeof(frame.command);

  memcpy(&cmd_buffer[length], frame.data, frame.data_length);
  length += frame.data_length;

  memcpy(&cmd_buffer[length], &frame.footer, sizeof(frame.footer));
  length += sizeof(frame.footer);
  this->write_array(cmd_buffer, length);
}

int LD2420Component::send_cmd_from_array(CmdFrameT frame) {
  uint32_t start_millis = millis();
  uint8_t error = 0;
  uint8_t ack_buffer[MAX_LINE_LENGTH];
  this->cmd_reply_.ack = false;
  if (frame.command != CMD_RESTART) {
    this->cmd_active_ = true;
  }  // Restart does not reply, thus no ack state required
  uint8_t retry = CMD_MAX_RETRIES;
  while (retry) {
    this->write_cmd_frame_(frame);

    error = 0;
    if (frame.command == CMD_RESTART) {
      return 0;  // restart does not reply exit now
    }

    while (!this->cmd_reply_.ack) {
      while (this->available()) {
        this->readline_(this->read(), ack_buffer, sizeof(ack_buffer));
      }
      delay_microseconds_safe(1450);
      // Wait on an Rx from the LD2420 for up to 3 1 second loops, otherwise it could trigger a WDT.
      if ((millis() - start_millis) > CMD_ACK_TIMEOUT_MS) {
        start_millis = millis();
        error = LD2420_ERROR_TIMEOUT;
        retry--;
        break;
      }
    }
    if (this->cmd_reply_.ack) {
      retry = 0;
    }
    if (this->cmd_reply_.error > 0) {
      this->handle_cmd_error(this->cmd_reply_.error);
    }
  }
  // On ack the reply parser already cleared this; clear it here as well so an
  // exhausted retry loop cannot leave loop() skipping all processing forever.
  this->cmd_active_ = false;
  return error;
}

void LD2420Component::build_config_mode_frame_(CmdFrameT &frame, bool enable) {
  frame.data_length = 0;
  frame.header = CMD_FRAME_HEADER;
  frame.command = enable ? CMD_ENABLE_CONF : CMD_DISABLE_CONF;
  if (enable) {
    memcpy(&frame.data[0], &CMD_PROTOCOL_VER, sizeof(CMD_PROTOCOL_VER));
    frame.data_length += sizeof(CMD_PROTOCOL_VER);
  }
  frame.footer = CMD_FRAME_FOOTER;
}

uint8_t LD2420Component::set_config_mode(bool enable) {
  CmdFrameT cmd_frame;
  this->build_config_mode_frame_(cmd_frame, enable);
  ESP_LOGV(TAG, "Sending set config %s command: %2X", enable ? "enable" : "disable", cmd_frame.command);
  return this->send_cmd_from_array(cmd_frame);
}

// Sends a restart and set system running mode to normal
void LD2420Component::send_module_restart() { this->ld2420_restart(); }

void LD2420Component::ld2420_restart() {
  CmdFrameT cmd_frame;
  cmd_frame.data_length = 0;
  cmd_frame.header = CMD_FRAME_HEADER;
  cmd_frame.command = CMD_RESTART;
  cmd_frame.footer = CMD_FRAME_FOOTER;
  ESP_LOGV(TAG, "Sending restart command: %2X", cmd_frame.command);
  this->send_cmd_from_array(cmd_frame);
}

void LD2420Component::set_reg_value(uint16_t reg, uint16_t value) {
  CmdFrameT cmd_frame;
  cmd_frame.data_length = 0;
  cmd_frame.header = CMD_FRAME_HEADER;
  cmd_frame.command = CMD_WRITE_REGISTER;
  memcpy(&cmd_frame.data[cmd_frame.data_length], &reg, CMD_REG_DATA_REPLY_SIZE);
  cmd_frame.data_length += 2;
  memcpy(&cmd_frame.data[cmd_frame.data_length], &value, CMD_REG_DATA_REPLY_SIZE);
  cmd_frame.data_length += 2;
  cmd_frame.footer = CMD_FRAME_FOOTER;
  ESP_LOGV(TAG, "Sending write register %4X command: %2X data = %4X", reg, cmd_frame.command, value);
  this->send_cmd_from_array(cmd_frame);
}

void LD2420Component::handle_cmd_error(uint16_t error) {
  if (error < std::size(ERR_MESSAGE)) {
    ESP_LOGE(TAG, "Command failed: %s", ERR_MESSAGE[error]);
  } else {
    // The error word comes from the device reply frame; unknown codes must not index ERR_MESSAGE
    ESP_LOGE(TAG, "Command failed: error 0x%04X", error);
  }
}

void LD2420Component::build_gate_threshold_frame_(CmdFrameT &frame, uint8_t gate) {
  frame.data_length = 0;
  frame.header = CMD_FRAME_HEADER;
  frame.command = CMD_READ_ABD_PARAM;
  memcpy(&frame.data[frame.data_length], &CMD_GATE_MOVE_THRESH[gate], sizeof(CMD_GATE_MOVE_THRESH[gate]));
  frame.data_length += 2;
  memcpy(&frame.data[frame.data_length], &CMD_GATE_STILL_THRESH[gate], sizeof(CMD_GATE_STILL_THRESH[gate]));
  frame.data_length += 2;
  frame.footer = CMD_FRAME_FOOTER;
  ESP_LOGV(TAG, "Sending read gate %d high/low threshold command: %2X", gate, frame.command);
}

void LD2420Component::build_min_max_timeout_frame_(CmdFrameT &frame) {
  frame.data_length = 0;
  frame.header = CMD_FRAME_HEADER;
  frame.command = CMD_READ_ABD_PARAM;
  memcpy(&frame.data[frame.data_length], &CMD_MIN_GATE_REG,
         sizeof(CMD_MIN_GATE_REG));  // Register: global min detect gate number
  frame.data_length += sizeof(CMD_MIN_GATE_REG);
  memcpy(&frame.data[frame.data_length], &CMD_MAX_GATE_REG,
         sizeof(CMD_MAX_GATE_REG));  // Register: global max detect gate number
  frame.data_length += sizeof(CMD_MAX_GATE_REG);
  memcpy(&frame.data[frame.data_length], &CMD_TIMEOUT_REG,
         sizeof(CMD_TIMEOUT_REG));  // Register: global delay time
  frame.data_length += sizeof(CMD_TIMEOUT_REG);
  frame.footer = CMD_FRAME_FOOTER;
  ESP_LOGV(TAG, "Sending read gate min max and timeout command: %2X", frame.command);
}

void LD2420Component::build_system_mode_frame_(CmdFrameT &frame, uint16_t mode) {
  uint16_t unknown_parm = 0x0000;
  frame.data_length = 0;
  frame.header = CMD_FRAME_HEADER;
  frame.command = CMD_WRITE_SYS_PARAM;
  memcpy(&frame.data[frame.data_length], &CMD_SYSTEM_MODE, sizeof(CMD_SYSTEM_MODE));
  frame.data_length += sizeof(CMD_SYSTEM_MODE);
  memcpy(&frame.data[frame.data_length], &mode, sizeof(mode));
  frame.data_length += sizeof(mode);
  memcpy(&frame.data[frame.data_length], &unknown_parm, sizeof(unknown_parm));
  frame.data_length += sizeof(unknown_parm);
  frame.footer = CMD_FRAME_FOOTER;
  ESP_LOGV(TAG, "Sending write system mode command: %2X", frame.command);
}

void LD2420Component::set_system_mode(uint16_t mode) {
  CmdFrameT cmd_frame;
  this->build_system_mode_frame_(cmd_frame, mode);
  if (this->send_cmd_from_array(cmd_frame) == 0) {
    this->set_mode_(mode);
  }
}

void LD2420Component::build_version_frame_(CmdFrameT &frame) {
  frame.data_length = 0;
  frame.header = CMD_FRAME_HEADER;
  frame.command = CMD_READ_VERSION;
  frame.footer = CMD_FRAME_FOOTER;
  ESP_LOGV(TAG, "Sending read firmware version command: %2X", frame.command);
}

uint8_t LD2420Component::set_min_max_distances_timeout(uint32_t max_gate_distance,
                                                       uint32_t min_gate_distance,  // NOLINT
                                                       uint32_t timeout) {
  // Header H, Length L, Register R, Value V, Footer F
  //                        |Min Gate         |Max Gate         |Timeout          |
  // HH HH HH HH LL LL CC CC RR RR VV VV VV VV RR RR VV VV VV VV RR RR VV VV VV VV FF FF FF FF
  // FD FC FB FA 14 00 07 00 00 00 01 00 00 00 01 00 09 00 00 00 04 00 0A 00 00 00 04 03 02 01 e.g.

  CmdFrameT cmd_frame;
  cmd_frame.data_length = 0;
  cmd_frame.header = CMD_FRAME_HEADER;
  cmd_frame.command = CMD_WRITE_ABD_PARAM;
  memcpy(&cmd_frame.data[cmd_frame.data_length], &CMD_MIN_GATE_REG,
         sizeof(CMD_MIN_GATE_REG));  // Register: global min detect gate number
  cmd_frame.data_length += sizeof(CMD_MIN_GATE_REG);
  memcpy(&cmd_frame.data[cmd_frame.data_length], &min_gate_distance, sizeof(min_gate_distance));
  cmd_frame.data_length += sizeof(min_gate_distance);
  memcpy(&cmd_frame.data[cmd_frame.data_length], &CMD_MAX_GATE_REG,
         sizeof(CMD_MAX_GATE_REG));  // Register: global max detect gate number
  cmd_frame.data_length += sizeof(CMD_MAX_GATE_REG);
  memcpy(&cmd_frame.data[cmd_frame.data_length], &max_gate_distance, sizeof(max_gate_distance));
  cmd_frame.data_length += sizeof(max_gate_distance);
  memcpy(&cmd_frame.data[cmd_frame.data_length], &CMD_TIMEOUT_REG,
         sizeof(CMD_TIMEOUT_REG));  // Register: global delay time
  cmd_frame.data_length += sizeof(CMD_TIMEOUT_REG);
  memcpy(&cmd_frame.data[cmd_frame.data_length], &timeout, sizeof(timeout));
  ;
  cmd_frame.data_length += sizeof(timeout);
  cmd_frame.footer = CMD_FRAME_FOOTER;

  ESP_LOGV(TAG, "Sending write gate min max and timeout command: %2X", cmd_frame.command);
  return this->send_cmd_from_array(cmd_frame);
}

uint8_t LD2420Component::set_gate_threshold(uint8_t gate) {
  // Header H, Length L, Command C, Register R, Value V, Footer F
  // HH HH HH HH LL LL CC CC RR RR VV VV VV VV RR RR VV VV VV VV FF FF FF FF
  // FD FC FB FA 14 00 07 00 10 00 00 FF 00 00 00 01 00 0F 00 00 04 03 02 01

  uint16_t move_threshold_gate = CMD_GATE_MOVE_THRESH[gate];
  uint16_t still_threshold_gate = CMD_GATE_STILL_THRESH[gate];
  CmdFrameT cmd_frame;
  cmd_frame.data_length = 0;
  cmd_frame.header = CMD_FRAME_HEADER;
  cmd_frame.command = CMD_WRITE_ABD_PARAM;
  memcpy(&cmd_frame.data[cmd_frame.data_length], &move_threshold_gate, sizeof(move_threshold_gate));
  cmd_frame.data_length += sizeof(move_threshold_gate);
  memcpy(&cmd_frame.data[cmd_frame.data_length], &this->new_config.move_thresh[gate],
         sizeof(this->new_config.move_thresh[gate]));
  cmd_frame.data_length += sizeof(this->new_config.move_thresh[gate]);
  memcpy(&cmd_frame.data[cmd_frame.data_length], &still_threshold_gate, sizeof(still_threshold_gate));
  cmd_frame.data_length += sizeof(still_threshold_gate);
  memcpy(&cmd_frame.data[cmd_frame.data_length], &this->new_config.still_thresh[gate],
         sizeof(this->new_config.still_thresh[gate]));
  cmd_frame.data_length += sizeof(this->new_config.still_thresh[gate]);
  cmd_frame.footer = CMD_FRAME_FOOTER;
  ESP_LOGV(TAG, "Sending set gate %4X sensitivity command: %2X", gate, cmd_frame.command);
  return this->send_cmd_from_array(cmd_frame);
}

#ifdef USE_NUMBER
void LD2420Component::init_gate_config_numbers() {
  if (this->gate_timeout_number_ != nullptr) {
    this->gate_timeout_number_->publish_state(static_cast<uint16_t>(this->current_config.timeout));
  }
  if (this->gate_select_number_ != nullptr) {
    this->gate_select_number_->publish_state(0);
  }
  if (this->min_gate_distance_number_ != nullptr) {
    this->min_gate_distance_number_->publish_state(static_cast<uint16_t>(this->current_config.min_gate));
  }
  if (this->max_gate_distance_number_ != nullptr) {
    this->max_gate_distance_number_->publish_state(static_cast<uint16_t>(this->current_config.max_gate));
  }
  if (this->gate_move_sensitivity_factor_number_ != nullptr) {
    this->gate_move_sensitivity_factor_number_->publish_state(this->gate_move_sensitivity_factor);
  }
  if (this->gate_still_sensitivity_factor_number_ != nullptr) {
    this->gate_still_sensitivity_factor_number_->publish_state(this->gate_still_sensitivity_factor);
  }
  for (uint8_t gate = 0; gate < TOTAL_GATES; gate++) {
    if (this->gate_still_threshold_numbers_[gate] != nullptr) {
      this->gate_still_threshold_numbers_[gate]->publish_state(
          static_cast<uint16_t>(this->current_config.still_thresh[gate]));
    }
    if (this->gate_move_threshold_numbers_[gate] != nullptr) {
      this->gate_move_threshold_numbers_[gate]->publish_state(
          static_cast<uint16_t>(this->current_config.move_thresh[gate]));
    }
  }
}

void LD2420Component::refresh_gate_config_numbers() {
  this->gate_timeout_number_->publish_state(this->new_config.timeout);
  this->min_gate_distance_number_->publish_state(this->new_config.min_gate);
  this->max_gate_distance_number_->publish_state(this->new_config.max_gate);
}

#endif

}  // namespace esphome::ld2420
