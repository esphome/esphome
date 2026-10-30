#include "hlk_fm22x.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/optional.h"
#include "esphome/core/progmem.h"
#include <algorithm>

namespace esphome::hlk_fm22x {

ESPHOME_LOG_TAG(TAG, "hlk_fm22x");
// Longest pause allowed between two bytes of one frame before the parser looks for a new frame
static constexpr uint32_t FRAME_TIMEOUT_MS = 100;
// The module answers most commands within 2 s and deletes within 5 s
static constexpr uint16_t COMMAND_TIMEOUT_S = 3;
static constexpr uint16_t DELETE_TIMEOUT_S = 10;
// Added on top of the timeout handed to the module for scans and enrollments
static constexpr uint16_t ALGORITHM_TIMEOUT_MARGIN_S = 5;
// Time an interrupted scan or enrollment gets to send its own reply
static constexpr uint32_t INTERRUPT_GRACE_MS = 500;
// Retry interval while the module does not answer, for example because it is still booting
static constexpr uint32_t RETRY_INTERVAL_MS = 5000;
// Unanswered commands in a row before the component reports an error instead of a warning
static constexpr uint8_t TIMEOUTS_BEFORE_ERROR = 3;
static constexpr size_t FACE_STATE_NOTE_SIZE = 17;  // note id + 8 x int16
static constexpr size_t FACE_STATE_FIELDS = 8;
// Scan and face details replies: command(1) + result(1) + face_id(2) + name(32), then the admin flag
static constexpr size_t FACE_RECORD_SIZE = 4 + HLK_FM22X_NAME_SIZE;
static constexpr size_t ENROLL_REPLY_MIN_SIZE = 5;  // command + result + face_id + direction
// Leave room for the terminating NUL in the module's 32 byte name field
static constexpr size_t MAX_NAME_LENGTH = HLK_FM22X_NAME_SIZE - 1;
static constexpr uint8_t READ_OUT_STAGE_DONE = 4;
static constexpr int16_t FACE_ID_INCOMPLETE = -1;
static constexpr int16_t FACE_STATE_IDLE = -1;

/// Number of leading bytes that are printable ASCII (0x20 to 0x7E).
static size_t printable_ascii_length(const char *text, size_t max_length) {
  size_t length = 0;
  while (length < max_length) {
    const auto c = static_cast<unsigned char>(text[length]);
    if (c < 0x20 || c > 0x7E) {
      break;
    }
    length++;
  }
  return length;
}

/// Length of the text in a fixed width field sent by the module, without padding or trailing spaces.
/// The padding is NUL or arbitrary bytes that are not valid UTF-8, so stop at the first unprintable byte.
static size_t printable_length(const char *text, size_t max_length) {
  size_t length = printable_ascii_length(text, max_length);
  while (length > 0 && text[length - 1] == ' ') {
    length--;
  }
  return length;
}

static bool is_scan_or_enroll(HlkFm22xCommand command) {
  return command == HlkFm22xCommand::VERIFY || command == HlkFm22xCommand::ENROLL ||
         command == HlkFm22xCommand::ENROLL_SINGLE || command == HlkFm22xCommand::ENROLL_ITG;
}

static bool is_read_out(HlkFm22xCommand command) {
  return command == HlkFm22xCommand::GET_STATUS || command == HlkFm22xCommand::GET_VERSION ||
         command == HlkFm22xCommand::GET_SERIAL_NUMBER || command == HlkFm22xCommand::GET_ALL_FACE_IDS;
}

static uint16_t default_timeout_s(HlkFm22xCommand command) {
  if (command == HlkFm22xCommand::DELETE_FACE || command == HlkFm22xCommand::DELETE_ALL_FACES) {
    return DELETE_TIMEOUT_S;
  }
  return COMMAND_TIMEOUT_S;
}

struct FaceRecord {
  int16_t face_id;
  const char *name;
  size_t name_length;
  bool admin;
};

static optional<FaceRecord> parse_face_record(const uint8_t *data, size_t length) {
  if (length < FACE_RECORD_SIZE) {
    return nullopt;
  }
  const char *name = reinterpret_cast<const char *>(data + 4);
  return FaceRecord{encode_value<int16_t>(&data[2]), name, printable_length(name, HLK_FM22X_NAME_SIZE),
                    length > FACE_RECORD_SIZE && data[FACE_RECORD_SIZE] != 0};
}

static const LogString *result_to_string(uint8_t result) {
  switch (result) {
    case HlkFm22xResult::SUCCEEDED:
      return LOG_STR("success");
    case HlkFm22xResult::REJECTED:
      return LOG_STR("command rejected");
    case HlkFm22xResult::ABORTED:
      return LOG_STR("aborted");
    case HlkFm22xResult::FAILED4_CAMERA:
      return LOG_STR("camera failed");
    case HlkFm22xResult::FAILED4_UNKNOWNREASON:
      return LOG_STR("unknown error");
    case HlkFm22xResult::FAILED4_INVALIDPARAM:
      return LOG_STR("invalid parameter");
    case HlkFm22xResult::FAILED4_NOMEMORY:
      return LOG_STR("out of memory");
    case HlkFm22xResult::FAILED4_UNKNOWNUSER:
      return LOG_STR("unknown face");
    case HlkFm22xResult::FAILED4_MAXUSER:
      return LOG_STR("face storage full");
    case HlkFm22xResult::FAILED4_FACEENROLLED:
      return LOG_STR("face already enrolled");
    case HlkFm22xResult::FAILED4_LIVENESSCHECK:
      return LOG_STR("liveness check failed");
    case HlkFm22xResult::FAILED4_TIMEOUT:
      return LOG_STR("timeout");
    case HlkFm22xResult::FAILED4_AUTHORIZATION:
      return LOG_STR("authorization failed");
    case HlkFm22xResult::FAILED4_READ_FILE:
      return LOG_STR("read file failed");
    case HlkFm22xResult::FAILED4_WRITE_FILE:
      return LOG_STR("write file failed");
    case HlkFm22xResult::FAILED4_NO_ENCRYPT:
      return LOG_STR("encryption required");
    case HlkFm22xResult::FAILED4_NO_RGBIMAGE:
      return LOG_STR("no RGB image");
    case HlkFm22xResult::FAILED4_JPGPHOTO_LARGE:
      return LOG_STR("photo too large");
    case HlkFm22xResult::FAILED4_JPGPHOTO_SMALL:
      return LOG_STR("photo too small");
    default:
      return LOG_STR("unknown result");
  }
}

PROGMEM_STRING_TABLE(ModuleStatusStrings, "standby", "busy", "error", "not initialized", "unknown");

// Indexed by the face state plus one, so FACE_STATE_IDLE comes first
PROGMEM_STRING_TABLE(FaceStateStrings, "Idle", "Face detected", "No face", "Too high", "Too low", "Too far left",
                     "Too far right", "Too far away", "Too close", "Eyebrows covered", "Eyes covered", "Face covered",
                     "Wrong direction", "Eyes open", "Eyes closed", "Eye state unknown", "Unknown");

static uint8_t face_state_index(int16_t state) {
  return state >= FACE_STATE_IDLE && state + 1 < FaceStateStrings::LAST_INDEX ? state + 1
                                                                              : FaceStateStrings::LAST_INDEX;
}

void HlkFm22xComponent::setup() {
  this->set_busy_(HlkFm22xCommand::ENROLL, false);
  this->set_busy_(HlkFm22xCommand::VERIFY, false);
  // Drop anything the module sent before we started listening
  this->discard_available();
  this->start_read_out_();
}

void HlkFm22xComponent::loop() {
  // Read first so a frame that is already waiting in the UART buffer is never discarded as stale
  this->read_frames_();
  this->check_timeouts_();
  this->send_next_command_();
}

void HlkFm22xComponent::enroll_face(const std::string &name, HlkFm22xFaceDirection direction, bool admin,
                                    uint8_t timeout_s, bool allow_duplicate) {
  // Templated names skip the configuration validator, so the same limits are enforced here
  if (name.length() > MAX_NAME_LENGTH || printable_ascii_length(name.c_str(), name.length()) != name.length()) {
    ESP_LOGE(TAG, "enroll_face(): name must be at most %u printable ASCII characters", (unsigned) MAX_NAME_LENGTH);
    this->enrollment_failed_callback_.call(HlkFm22xResult::FAILED4_INVALIDPARAM);
    return;
  }
  // Only one enrollment fits in pending_enroll_ until it has been sent
  if (this->enroll_queued_()) {
    ESP_LOGE(TAG, "An enrollment is already waiting to be sent");
    this->enrollment_failed_callback_.call(HlkFm22xResult::FAILED4_NOMEMORY);
    return;
  }
  const bool single_frame = direction == HlkFm22xFaceDirection::FACE_DIRECTION_UNDEFINED;
  ESP_LOGI(TAG, "Enrolling '%s'%s, %s, timeout %us", name.c_str(),
           admin ? LOG_STR_LITERAL(" as admin") : LOG_STR_LITERAL(""),
           single_frame ? LOG_STR_LITERAL("single frame") : LOG_STR_LITERAL("one direction"), timeout_s);

  HlkFm22xCommand command;
  this->pending_enroll_ = {};
  if (!allow_duplicate) {
    auto &payload = this->pending_enroll_.itg;
    payload.admin = admin ? 1 : 0;
    std::copy(name.begin(), name.end(), payload.name);
    payload.direction = direction;
    payload.type = single_frame ? ENROLL_TYPE_SINGLE : ENROLL_TYPE_INTERACTIVE;
    payload.allow_duplicate = 0;
    command = HlkFm22xCommand::ENROLL_ITG;
  } else {
    auto &payload = this->pending_enroll_.basic;
    payload.admin = admin ? 1 : 0;
    std::copy(name.begin(), name.end(), payload.name);
    payload.direction = direction;
    command = single_frame ? HlkFm22xCommand::ENROLL_SINGLE : HlkFm22xCommand::ENROLL;
  }
  if (!this->enqueue_(command, 0, timeout_s)) {
    this->enrollment_failed_callback_.call(HlkFm22xResult::FAILED4_NOMEMORY);
  }
}

void HlkFm22xComponent::scan_face(uint8_t timeout_s) {
  ESP_LOGI(TAG, "Scanning for a face, timeout %us", timeout_s);
  if (!this->enqueue_(HlkFm22xCommand::VERIFY, 0, timeout_s)) {
    this->face_scan_invalid_callback_.call(HlkFm22xResult::FAILED4_NOMEMORY);
  }
}

void HlkFm22xComponent::cancel() {
  this->drop_queued_(is_scan_or_enroll);
  if (is_scan_or_enroll(this->pending_command_)) {
    const bool enrolling = this->pending_command_ != HlkFm22xCommand::VERIFY;
    ESP_LOGI(TAG, "Cancelling %s", enrolling ? LOG_STR_LITERAL("enrollment") : LOG_STR_LITERAL("scan"));
    // RESET makes the module stop what it is doing; FACE_RESET then forgets the directions captured so far
    this->interrupt_with_reset_();
    if (enrolling) {
      this->enqueue_(HlkFm22xCommand::FACE_RESET);
    }
    return;
  }
  ESP_LOGI(TAG, "Clearing enrollment state");
  this->enqueue_(HlkFm22xCommand::FACE_RESET);
}

void HlkFm22xComponent::delete_face(int16_t face_id) {
  ESP_LOGI(TAG, "Deleting face %d", face_id);
  this->enqueue_(HlkFm22xCommand::DELETE_FACE, face_id);
}

void HlkFm22xComponent::delete_all_faces() {
  ESP_LOGI(TAG, "Deleting all faces");
  this->enqueue_(HlkFm22xCommand::DELETE_ALL_FACES);
}

void HlkFm22xComponent::get_face_details(int16_t face_id) {
  ESP_LOGD(TAG, "Requesting details of face %d", face_id);
  this->enqueue_(HlkFm22xCommand::GET_FACE_DETAILS, face_id);
}

void HlkFm22xComponent::reset() {
  ESP_LOGI(TAG, "Resetting module");
  if (is_scan_or_enroll(this->pending_command_)) {
    this->interrupt_with_reset_();
    return;
  }
  this->enqueue_(HlkFm22xCommand::RESET);
}

bool HlkFm22xComponent::enqueue_(HlkFm22xCommand command, uint16_t arg, uint8_t timeout_s) {
  if (timeout_s == 0 && is_scan_or_enroll(command)) {
    timeout_s = HLK_FM22X_DEFAULT_TIMEOUT_S;
  }
  if (!this->queue_.push(QueuedCommand{command, timeout_s, arg})) {
    ESP_LOGE(TAG, "Command queue full, dropping command 0x%02X", command);
    return false;
  }
  return true;
}

bool HlkFm22xComponent::enroll_queued_() {
  for (const QueuedCommand &queued : this->queue_) {
    if (is_scan_or_enroll(queued.command) && queued.command != HlkFm22xCommand::VERIFY) {
      return true;
    }
  }
  return false;
}

void HlkFm22xComponent::drop_queued_(bool (*predicate)(HlkFm22xCommand)) {
  // The ring buffer has no erase: take every entry out once and put back the ones that stay
  HlkFm22xCommand dropped[HLK_FM22X_COMMAND_QUEUE_SIZE]{};
  size_t dropped_count = 0;
  const size_t count = this->queue_.size();
  for (size_t i = 0; i < count; i++) {
    const QueuedCommand queued = this->queue_.front();
    this->queue_.pop();
    if (predicate(queued.command)) {
      dropped[dropped_count++] = queued.command;
    } else {
      this->queue_.push(queued);
    }
  }
  // Reported once the queue is back in order, in case a handler queues a new command
  for (size_t i = 0; i < dropped_count; i++) {
    this->report_failed_(dropped[i], HlkFm22xResult::ABORTED);
  }
}

void HlkFm22xComponent::send_next_command_() {
  if (this->pending_command_ != HlkFm22xCommand::NONE || this->queue_.empty()) {
    return;
  }
  const QueuedCommand queued = this->queue_.front();
  this->queue_.pop();
  switch (queued.command) {
    case HlkFm22xCommand::VERIFY: {
      const uint8_t data[] = {0, queued.timeout_s};  // the first byte asks the module not to power down afterwards
      this->write_frame_(queued.command, data, sizeof(data));
      break;
    }
    case HlkFm22xCommand::ENROLL:
    case HlkFm22xCommand::ENROLL_SINGLE:
      this->pending_enroll_.basic.timeout_s = queued.timeout_s;
      this->write_frame_(queued.command, reinterpret_cast<const uint8_t *>(&this->pending_enroll_.basic),
                         sizeof(EnrollPayload));
      break;
    case HlkFm22xCommand::ENROLL_ITG:
      this->pending_enroll_.itg.timeout_s = queued.timeout_s;
      this->write_frame_(queued.command, reinterpret_cast<const uint8_t *>(&this->pending_enroll_.itg),
                         sizeof(EnrollItgPayload));
      break;
    case HlkFm22xCommand::DELETE_FACE:
    case HlkFm22xCommand::GET_FACE_DETAILS: {
      const uint8_t data[] = {(uint8_t) (queued.arg >> 8), (uint8_t) (queued.arg & 0xFF)};
      this->write_frame_(queued.command, data, sizeof(data));
      break;
    }
    default:
      this->write_frame_(queued.command, nullptr, 0);
      break;
  }
  this->pending_command_ = queued.command;
  this->pending_sent_ms_ = App.get_loop_component_start_time();
  const uint16_t reply_timeout_s = is_scan_or_enroll(queued.command) ? queued.timeout_s + ALGORITHM_TIMEOUT_MARGIN_S
                                                                     : default_timeout_s(queued.command);
  this->pending_timeout_ms_ = reply_timeout_s * 1000UL;
  this->set_busy_(queued.command, true);
}

void HlkFm22xComponent::write_frame_(HlkFm22xCommand command, const uint8_t *data, size_t size) {
  ESP_LOGV(TAG, "Sending command 0x%02X with %zu bytes", command, size);
  const uint8_t header[] = {(uint8_t) (START_CODE >> 8), (uint8_t) (START_CODE & 0xFF), (uint8_t) command,
                            (uint8_t) (size >> 8), (uint8_t) (size & 0xFF)};
  // The checksum covers everything after the start code
  const uint8_t checksum = xor_bytes(data, size, xor_bytes(header + 2, sizeof(header) - 2));
  this->write_array(header, sizeof(header));
  if (size > 0) {
    this->write_array(data, size);
  }
  this->write_byte(checksum);
}

void HlkFm22xComponent::interrupt_with_reset_() {
  // The module is busy with a scan or enrollment; the reset jumps the queue and the
  // running scan or enrollment gets a grace period to answer once the module has confirmed the stop
  this->hold_interrupted_(this->pending_command_);
  this->write_frame_(HlkFm22xCommand::RESET, nullptr, 0);
  this->pending_command_ = HlkFm22xCommand::RESET;
  this->pending_sent_ms_ = App.get_loop_component_start_time();
  this->pending_timeout_ms_ = COMMAND_TIMEOUT_S * 1000UL;
}

void HlkFm22xComponent::hold_interrupted_(HlkFm22xCommand command) {
  // Only one interrupted scan or enrollment can wait for its reply; an older one is reported as aborted
  this->clear_interrupted_(true);
  this->interrupted_command_ = command;
  this->interrupted_deadline_set_ = false;
}

void HlkFm22xComponent::start_interrupt_grace_() {
  this->interrupted_deadline_ms_ = App.get_loop_component_start_time() + INTERRUPT_GRACE_MS;
  this->interrupted_deadline_set_ = true;
}

void HlkFm22xComponent::clear_interrupted_(bool aborted) {
  if (this->interrupted_command_ == HlkFm22xCommand::NONE) {
    return;
  }
  const HlkFm22xCommand command = this->interrupted_command_;
  this->interrupted_command_ = HlkFm22xCommand::NONE;
  this->interrupted_deadline_set_ = false;
  if (aborted) {
    this->finish_failed_(command, HlkFm22xResult::ABORTED);
  }
}

void HlkFm22xComponent::read_frames_() {
  if (this->available() == 0) {
    return;
  }
  this->recv_last_byte_ms_ = App.get_loop_component_start_time();
  this->read_available<32>([this](uint8_t byte) { this->process_byte_(byte); });
}

void HlkFm22xComponent::process_byte_(uint8_t byte) {
  switch (this->recv_state_) {
    case RecvState::RECV_STATE_SYNC_1:
      if (byte == (uint8_t) (START_CODE >> 8)) {
        this->recv_state_ = RecvState::RECV_STATE_SYNC_2;
      }
      break;
    case RecvState::RECV_STATE_SYNC_2:
      if (byte == (uint8_t) (START_CODE & 0xFF)) {
        this->recv_checksum_ = 0;
        this->recv_state_ = RecvState::RECV_STATE_TYPE;
      } else if (byte != (uint8_t) (START_CODE >> 8)) {
        this->recv_state_ = RecvState::RECV_STATE_SYNC_1;
      }
      break;
    case RecvState::RECV_STATE_TYPE:
      this->recv_type_ = byte;
      this->recv_checksum_ ^= byte;
      this->recv_state_ = RecvState::RECV_STATE_LENGTH_HIGH;
      break;
    case RecvState::RECV_STATE_LENGTH_HIGH:
      this->recv_length_ = (uint16_t) byte << 8;
      this->recv_checksum_ ^= byte;
      this->recv_state_ = RecvState::RECV_STATE_LENGTH_LOW;
      break;
    case RecvState::RECV_STATE_LENGTH_LOW:
      this->recv_length_ |= byte;
      this->recv_checksum_ ^= byte;
      this->recv_index_ = 0;
      this->recv_state_ = this->recv_length_ > 0 ? RecvState::RECV_STATE_DATA : RecvState::RECV_STATE_CHECKSUM;
      break;
    case RecvState::RECV_STATE_DATA:
      this->recv_checksum_ ^= byte;
      // Keep what fits; long replies only need their first bytes but must be checksummed in full
      if (this->recv_index_ < HLK_FM22X_MAX_RESPONSE_SIZE) {
        this->recv_buf_[this->recv_index_] = byte;
      }
      if (++this->recv_index_ >= this->recv_length_) {
        this->recv_state_ = RecvState::RECV_STATE_CHECKSUM;
      }
      break;
    case RecvState::RECV_STATE_CHECKSUM:
      this->recv_state_ = RecvState::RECV_STATE_SYNC_1;
      if (byte != this->recv_checksum_) {
        ESP_LOGE(TAG, "Invalid checksum. Calculated: 0x%02X, received: 0x%02X", this->recv_checksum_, byte);
        break;
      }
      this->handle_frame_();
      break;
  }
}

void HlkFm22xComponent::handle_frame_() {
  const size_t stored = std::min<size_t>(this->recv_length_, HLK_FM22X_MAX_RESPONSE_SIZE);
  ESP_LOGV_HEX(TAG, this->recv_buf_.data(), stored, HLK_FM22X_MAX_RESPONSE_SIZE, "Received type 0x%02X, %u bytes: %s",
               this->recv_type_, this->recv_length_);
  switch (this->recv_type_) {
    case HlkFm22xResponseType::REPLY:
      this->handle_reply_(this->recv_buf_.data(), stored);
      break;
    case HlkFm22xResponseType::NOTE:
      this->handle_note_(this->recv_buf_.data(), stored);
      break;
    default:
      ESP_LOGW(TAG, "Unexpected response type: 0x%02X", this->recv_type_);
      break;
  }
}

void HlkFm22xComponent::handle_note_(const uint8_t *data, size_t length) {
  if (length < 1) {
    ESP_LOGE(TAG, "Empty note");
    return;
  }
  switch (data[0]) {
    case HlkFm22xNoteType::NOTE_FACE_STATE: {
      if (length < FACE_STATE_NOTE_SIZE) {
        ESP_LOGE(TAG, "Face state note too short: %zu bytes", length);
        break;
      }
      // Unlike the rest of the protocol these fields are sent with the low byte first
      int16_t info[FACE_STATE_FIELDS];
      for (size_t i = 0; i < FACE_STATE_FIELDS; i++) {
        info[i] = encode_value_le<int16_t>(&data[1 + 2 * i]);
      }
      ESP_LOGV(TAG, "Face state: %s (%d), left: %d, top: %d, right: %d, bottom: %d, yaw: %d, pitch: %d, roll: %d",
               LOG_STR_ARG(FaceStateStrings::get_log_str(face_state_index(info[0]), FaceStateStrings::LAST_INDEX)),
               info[0], info[1], info[2], info[3], info[4], info[5], info[6], info[7]);
      this->publish_face_state_(info[0]);
      this->face_info_callback_.call(info[0], info[1], info[2], info[3], info[4], info[5], info[6], info[7]);
      break;
    }
    case HlkFm22xNoteType::NOTE_READY: {
      ESP_LOGI(TAG, "Module ready");
      // Whatever was sent before the module (re)started will not be answered
      const HlkFm22xCommand lost = this->pending_command_;
      this->pending_command_ = HlkFm22xCommand::NONE;
      if (is_scan_or_enroll(lost)) {
        this->hold_interrupted_(lost);
      } else if (lost != HlkFm22xCommand::NONE && lost != HlkFm22xCommand::RESET && !is_read_out(lost)) {
        // The read-out repeats below and a reset has done its job either way; anything else is worth knowing
        ESP_LOGW(TAG, "Module restarted, command 0x%02X was not answered", lost);
      }
      // The module may still answer the scan or enrollment it was running, also one a cancel's RESET interrupted;
      // give it a moment
      if (this->interrupted_command_ != HlkFm22xCommand::NONE) {
        this->start_interrupt_grace_();
      }
      this->retry_scheduled_ = false;
      if (!this->read_out_done_) {
        this->start_read_out_();
        break;
      }
      // Version, serial number and stored faces do not change on a restart, so only the status is worth a
      // look, unless a delete was cut short and the face count may be off
      this->enqueue_(HlkFm22xCommand::GET_STATUS);
      if (lost == HlkFm22xCommand::DELETE_FACE || lost == HlkFm22xCommand::DELETE_ALL_FACES) {
        this->refresh_face_count_();
      }
      break;
    }
    case HlkFm22xNoteType::NOTE_UNKNOWN_ERROR:
      ESP_LOGE(TAG, "Module reported an unknown error");
      break;
    case HlkFm22xNoteType::NOTE_OTA_DONE:
      ESP_LOGI(TAG, "Module firmware update finished");
      break;
    case HlkFm22xNoteType::NOTE_EYE_STATE:
      ESP_LOGD(TAG, "Eye state: %u", length > 1 ? data[1] : 0);
      break;
    case HlkFm22xNoteType::NOTE_AUTHORIZATION_FAILED:
      ESP_LOGE(TAG, "Module license check failed");
      break;
    default:
      ESP_LOGW(TAG, "Unhandled note: 0x%02X", data[0]);
      break;
  }
}

void HlkFm22xComponent::handle_reply_(const uint8_t *data, size_t length) {
  if (length < 2) {
    ESP_LOGE(TAG, "Reply too short: %zu bytes", length);
    return;
  }
  const auto command = (HlkFm22xCommand) data[0];
  const uint8_t result = data[1];

  bool expected = true;
  if (command == this->pending_command_) {
    this->pending_command_ = HlkFm22xCommand::NONE;
  } else if (command == this->interrupted_command_) {
    this->clear_interrupted_(false);
  } else if (is_scan_or_enroll(command)) {
    // Its result has already been reported as a timeout or abort
    ESP_LOGW(TAG, "Unexpected reply for command 0x%02X, ignoring", command);
    return;
  } else {
    // For example a delete that took longer than expected; its outcome is still worth recording
    ESP_LOGD(TAG, "Late reply for command 0x%02X", command);
    expected = false;
  }

  // The module answered, so it is alive
  this->consecutive_timeouts_ = 0;
  this->status_clear_warning();
  this->status_clear_error();

  if (result != HlkFm22xResult::SUCCEEDED) {
    ESP_LOGW(TAG, "Command 0x%02X failed: %s (%u)", command, LOG_STR_ARG(result_to_string(result)), result);
  }

  switch (command) {
    case HlkFm22xCommand::VERIFY:
      this->handle_scan_reply_(result, data, length);
      break;
    case HlkFm22xCommand::ENROLL:
    case HlkFm22xCommand::ENROLL_SINGLE:
    case HlkFm22xCommand::ENROLL_ITG:
      this->handle_enroll_reply_(result, data, length);
      break;
    case HlkFm22xCommand::GET_FACE_DETAILS:
      this->handle_face_details_reply_(result, data, length);
      break;
    case HlkFm22xCommand::GET_STATUS:
      if (result == HlkFm22xResult::SUCCEEDED && length >= 3) {
        ESP_LOGD(TAG, "Module status: %s (%u)",
                 LOG_STR_ARG(ModuleStatusStrings::get_log_str(data[2], ModuleStatusStrings::LAST_INDEX)), data[2]);
        PUBLISH_STATE(this->status_sensor_, data[2]);
      }
      break;
    case HlkFm22xCommand::GET_VERSION:
    case HlkFm22xCommand::GET_SERIAL_NUMBER:
      if (result == HlkFm22xResult::SUCCEEDED) {
        this->publish_text_(
            command == HlkFm22xCommand::GET_VERSION ? this->version_text_sensor_ : this->serial_number_text_sensor_,
            data + 2, length - 2);
      }
      break;
    case HlkFm22xCommand::GET_ALL_FACE_IDS:
      if (result == HlkFm22xResult::SUCCEEDED && length >= 3) {
        ESP_LOGD(TAG, "%u faces enrolled", data[2]);
        PUBLISH_STATE(this->face_count_sensor_, data[2]);
      }
      break;
    case HlkFm22xCommand::DELETE_FACE:
    case HlkFm22xCommand::DELETE_ALL_FACES:
      if (result == HlkFm22xResult::SUCCEEDED) {
        ESP_LOGI(TAG, "%s deleted",
                 command == HlkFm22xCommand::DELETE_FACE ? LOG_STR_LITERAL("Face") : LOG_STR_LITERAL("All faces"));
      }
      this->refresh_face_count_();
      break;
    case HlkFm22xCommand::FACE_RESET:
      ESP_LOGD(TAG, "Enrollment state cleared");
      break;
    case HlkFm22xCommand::RESET:
      ESP_LOGD(TAG, "Module reset");
      if (this->interrupted_command_ != HlkFm22xCommand::NONE) {
        // The interrupted scan or enrollment gets a short grace period to report that it was aborted
        this->start_interrupt_grace_();
      }
      this->enqueue_(HlkFm22xCommand::GET_STATUS);
      break;
    default:
      ESP_LOGW(TAG, "Unhandled reply for command 0x%02X", command);
      break;
  }

  // Only one command is ever outstanding, so an expected read-out reply belongs to the running read-out chain
  if (expected && is_read_out(command)) {
    this->continue_read_out_();
  }
}

void HlkFm22xComponent::handle_scan_reply_(uint8_t result, const uint8_t *data, size_t length) {
  this->set_busy_(HlkFm22xCommand::VERIFY, false);
  if (result == HlkFm22xResult::SUCCEEDED) {
    const auto record = parse_face_record(data, length);
    if (!record.has_value()) {
      ESP_LOGE(TAG, "Scan reply too short: %zu bytes", length);
      this->face_scan_invalid_callback_.call(HlkFm22xResult::FAILED4_UNKNOWNREASON);
      return;
    }
    ESP_LOGI(TAG, "Face matched: ID %d, name '%.*s'%s", record->face_id, (int) record->name_length, record->name,
             record->admin ? LOG_STR_LITERAL(" (admin)") : LOG_STR_LITERAL(""));
    PUBLISH_STATE(this->last_face_id_sensor_, record->face_id);
    PUBLISH_STATE(this->last_face_name_text_sensor_, record->name, record->name_length);
    if (!this->face_scan_matched_callback_.empty()) {
      this->face_scan_matched_callback_.call(record->face_id, std::string(record->name, record->name_length),
                                             record->admin);
    }
    return;
  }
  // The module answers REJECTED for a face it does not know; every other result is a failed scan
  if (result == HlkFm22xResult::REJECTED) {
    ESP_LOGI(TAG, "Face not recognized");
    this->face_scan_unmatched_callback_.call();
    return;
  }
  this->face_scan_invalid_callback_.call(result);
}

void HlkFm22xComponent::handle_enroll_reply_(uint8_t result, const uint8_t *data, size_t length) {
  this->set_busy_(HlkFm22xCommand::ENROLL, false);
  if (result != HlkFm22xResult::SUCCEEDED) {
    this->enrollment_failed_callback_.call(result);
    return;
  }
  if (length < ENROLL_REPLY_MIN_SIZE) {
    ESP_LOGE(TAG, "Enrollment reply too short: %zu bytes", length);
    this->enrollment_failed_callback_.call(HlkFm22xResult::FAILED4_UNKNOWNREASON);
    return;
  }
  const auto face_id = encode_value<int16_t>(&data[2]);
  uint8_t direction = data[4];
  const bool complete = face_id != FACE_ID_INCOMPLETE;
  if (complete) {
    // A stored face always counts as captured from all directions, also after a single frame enrollment
    direction = HLK_FM22X_ALL_DIRECTIONS;
    ESP_LOGI(TAG, "Face enrolled: ID %d", face_id);
  } else {
    ESP_LOGI(TAG, "Direction captured, directions so far: 0x%02X", direction);
  }
  this->enrollment_done_callback_.call(face_id, direction);
  if (complete) {
    this->refresh_face_count_();
  }
}

void HlkFm22xComponent::handle_face_details_reply_(uint8_t result, const uint8_t *data, size_t length) {
  if (result != HlkFm22xResult::SUCCEEDED) {
    return;
  }
  const auto record = parse_face_record(data, length);
  if (!record.has_value()) {
    ESP_LOGE(TAG, "Face details reply too short: %zu bytes", length);
    return;
  }
  ESP_LOGD(TAG, "Face %d: name '%.*s'%s", record->face_id, (int) record->name_length, record->name,
           record->admin ? LOG_STR_LITERAL(" (admin)") : LOG_STR_LITERAL(""));
  if (!this->face_details_callback_.empty()) {
    this->face_details_callback_.call(record->face_id, std::string(record->name, record->name_length), record->admin);
  }
}

void HlkFm22xComponent::publish_text_(text_sensor::TextSensor *text_sensor, const uint8_t *data, size_t length) {
  const char *text = reinterpret_cast<const char *>(data);
  const size_t text_length = printable_length(text, length);
  ESP_LOGD(TAG, "Module reports: %.*s", (int) text_length, text);
  PUBLISH_STATE(text_sensor, text, text_length);
}

void HlkFm22xComponent::check_timeouts_() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->recv_state_ != RecvState::RECV_STATE_SYNC_1 && now - this->recv_last_byte_ms_ > FRAME_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Incomplete frame discarded");
    this->recv_state_ = RecvState::RECV_STATE_SYNC_1;
  }

  if (this->pending_command_ != HlkFm22xCommand::NONE && now - this->pending_sent_ms_ >= this->pending_timeout_ms_) {
    const HlkFm22xCommand command = this->pending_command_;
    this->pending_command_ = HlkFm22xCommand::NONE;
    if (this->consecutive_timeouts_ < TIMEOUTS_BEFORE_ERROR) {
      this->consecutive_timeouts_++;
    }
    if (this->consecutive_timeouts_ >= TIMEOUTS_BEFORE_ERROR) {
      // Only complain once; the retry below keeps checking whether the module comes back
      if (!this->status_has_error()) {
        ESP_LOGE(TAG, "Module is not responding");
        this->status_set_error();
      }
    } else {
      ESP_LOGW(TAG, "Command 0x%02X timed out", command);
      this->status_set_warning();
    }
    this->finish_failed_(command, HlkFm22xResult::FAILED4_TIMEOUT);
    // A stop that was never confirmed leaves no hope for the interrupted scan or enrollment either
    this->clear_interrupted_(true);
    // Check again later whether the module is alive; this also restarts an interrupted read-out
    this->retry_at_ms_ = now + RETRY_INTERVAL_MS;
    this->retry_scheduled_ = true;
  }

  if (this->interrupted_deadline_set_ && (int32_t) (now - this->interrupted_deadline_ms_) >= 0) {
    this->clear_interrupted_(true);
  }

  // The retry waits for other commands to finish rather than being used up while they run
  if (this->retry_scheduled_ && this->pending_command_ == HlkFm22xCommand::NONE && this->queue_.empty() &&
      (int32_t) (now - this->retry_at_ms_) >= 0) {
    this->retry_scheduled_ = false;
    this->start_read_out_();
  }
}

void HlkFm22xComponent::finish_failed_(HlkFm22xCommand command, uint8_t error) {
  this->set_busy_(command, false);
  this->report_failed_(command, error);
}

void HlkFm22xComponent::report_failed_(HlkFm22xCommand command, uint8_t error) {
  if (command == HlkFm22xCommand::VERIFY) {
    this->face_scan_invalid_callback_.call(error);
  } else if (is_scan_or_enroll(command)) {
    this->enrollment_failed_callback_.call(error);
  }
}

void HlkFm22xComponent::start_read_out_() {
  // Never let two read-out chains run at the same time
  this->drop_queued_(is_read_out);
  this->read_out_stage_ = 0;
  this->continue_read_out_();
}

void HlkFm22xComponent::continue_read_out_() {
  // Read status, version, serial number and face count one after the other, skipping what nobody asked for
  while (this->read_out_stage_ < READ_OUT_STAGE_DONE) {
    const HlkFm22xCommand command = this->read_out_command_(this->read_out_stage_++);
    if (command == HlkFm22xCommand::NONE) {
      continue;
    }
    if (!this->enqueue_(command)) {
      // No room right now; the read-out starts over once the queue has drained
      this->retry_at_ms_ = App.get_loop_component_start_time();
      this->retry_scheduled_ = true;
    }
    return;
  }
  this->read_out_done_ = true;
}

HlkFm22xCommand HlkFm22xComponent::read_out_command_(uint8_t stage) const {
  switch (stage) {
    case 0:
      return HlkFm22xCommand::GET_STATUS;
    case 1:
      return this->version_text_sensor_ != nullptr ? HlkFm22xCommand::GET_VERSION : HlkFm22xCommand::NONE;
    case 2:
      return this->serial_number_text_sensor_ != nullptr ? HlkFm22xCommand::GET_SERIAL_NUMBER : HlkFm22xCommand::NONE;
    case 3:
      return this->face_count_sensor_ != nullptr ? HlkFm22xCommand::GET_ALL_FACE_IDS : HlkFm22xCommand::NONE;
    default:
      return HlkFm22xCommand::NONE;
  }
}

void HlkFm22xComponent::refresh_face_count_() {
  if (this->face_count_sensor_ != nullptr) {
    this->enqueue_(HlkFm22xCommand::GET_ALL_FACE_IDS);
  }
}

void HlkFm22xComponent::set_busy_(HlkFm22xCommand command, bool busy) {
  if (!is_scan_or_enroll(command)) {
    return;
  }
  PUBLISH_STATE(command == HlkFm22xCommand::VERIFY ? this->scanning_binary_sensor_ : this->enrolling_binary_sensor_,
                busy);
  if (!busy) {
    this->publish_face_state_(FACE_STATE_IDLE);
  }
}

void HlkFm22xComponent::publish_face_state_(int16_t state) {
  if (state == this->last_face_state_) {
    return;
  }
  this->last_face_state_ = state;
  PUBLISH_STATE(this->face_state_text_sensor_,
                FaceStateStrings::get_progmem_str(face_state_index(state), FaceStateStrings::LAST_INDEX));
}

void HlkFm22xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HLK-FM22X:");
  LOG_BINARY_SENSOR("  ", "Enrolling", this->enrolling_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Scanning", this->scanning_binary_sensor_);
  LOG_SENSOR("  ", "Face Count", this->face_count_sensor_);
  LOG_SENSOR("  ", "Status", this->status_sensor_);
  LOG_SENSOR("  ", "Last Face ID", this->last_face_id_sensor_);
  LOG_TEXT_SENSOR("  ", "Version", this->version_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Serial Number", this->serial_number_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Last Face Name", this->last_face_name_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Face State", this->face_state_text_sensor_);
  if (this->version_text_sensor_ != nullptr && this->version_text_sensor_->has_state()) {
    ESP_LOGCONFIG(TAG, "  Firmware: %s", this->version_text_sensor_->get_state().c_str());
  }
  if (this->serial_number_text_sensor_ != nullptr && this->serial_number_text_sensor_->has_state()) {
    ESP_LOGCONFIG(TAG, "  Serial: %s", this->serial_number_text_sensor_->get_state().c_str());
  }
  if (this->face_count_sensor_ != nullptr && this->face_count_sensor_->has_state()) {
    ESP_LOGCONFIG(TAG, "  Enrolled faces: %u", (unsigned) this->face_count_sensor_->get_state());
  }
}

}  // namespace esphome::hlk_fm22x
