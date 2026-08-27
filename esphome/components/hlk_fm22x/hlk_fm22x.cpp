#include "hlk_fm22x.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>
#include <cstring>

namespace esphome::hlk_fm22x {

static const char *const TAG = "hlk_fm22x";
static constexpr uint32_t BAUD_RATE = 115200;
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
static constexpr size_t FACE_RECORD_MIN_SIZE = 4 + HLK_FM22X_NAME_SIZE;
static constexpr size_t FACE_RECORD_ADMIN_OFFSET = FACE_RECORD_MIN_SIZE;
static constexpr size_t ENROLL_REPLY_MIN_SIZE = 5;  // command + result + face_id + direction
// Enroll payload shared by ENROLL, ENROLL_SINGLE and ENROLL_ITG
static constexpr size_t ENROLL_ADMIN_OFFSET = 0;
static constexpr size_t ENROLL_NAME_OFFSET = 1;
static constexpr size_t ENROLL_DIRECTION_OFFSET = ENROLL_NAME_OFFSET + HLK_FM22X_NAME_SIZE;
static constexpr size_t ENROLL_TIMEOUT_OFFSET = ENROLL_DIRECTION_OFFSET + 1;
static constexpr size_t ENROLL_SIZE = ENROLL_TIMEOUT_OFFSET + 1;
static constexpr size_t ENROLL_ITG_TYPE_OFFSET = ENROLL_DIRECTION_OFFSET + 1;
static constexpr size_t ENROLL_ITG_DUPLICATE_OFFSET = ENROLL_ITG_TYPE_OFFSET + 1;
static constexpr size_t ENROLL_ITG_TIMEOUT_OFFSET = ENROLL_ITG_DUPLICATE_OFFSET + 1;
static constexpr size_t ENROLL_ITG_RESERVED_SIZE = 3;
static constexpr size_t ENROLL_ITG_SIZE = ENROLL_ITG_TIMEOUT_OFFSET + 1 + ENROLL_ITG_RESERVED_SIZE;
static_assert(ENROLL_ITG_SIZE == HLK_FM22X_MAX_COMMAND_SIZE, "command buffer must fit the largest enroll payload");
// Leave room for the terminating NUL in the module's 32 byte name field
static constexpr size_t MAX_NAME_LENGTH = HLK_FM22X_NAME_SIZE - 1;
static constexpr uint8_t READ_OUT_STAGE_DONE = 4;
static constexpr int16_t FACE_ID_INCOMPLETE = -1;
static constexpr int16_t FACE_STATE_IDLE = -1;

/// Length of the leading run of printable ASCII in a fixed width text field.
///
/// The module pads these fields to a fixed width and is not consistent about what it pads
/// with: names come back NUL padded, but the serial number is padded with arbitrary bytes
/// (0xFF and friends). Those bytes are not valid UTF-8, and publishing them produces a
/// state string that clients such as Home Assistant refuse to decode, so stop at the first
/// byte that cannot be part of a plain text string.
static size_t printable_length(const char *text, size_t max_length) {
  size_t length = 0;
  while (length < max_length) {
    const auto c = static_cast<unsigned char>(text[length]);
    if (c < 0x20 || c > 0x7E) {
      break;
    }
    length++;
  }
  while (length > 0 && text[length - 1] == ' ') {
    length--;
  }
  return length;
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

static const LogString *module_status_to_string(uint8_t status) {
  switch (status) {
    case HlkFm22xModuleStatus::MODULE_STATUS_STANDBY:
      return LOG_STR("standby");
    case HlkFm22xModuleStatus::MODULE_STATUS_BUSY:
      return LOG_STR("busy");
    case HlkFm22xModuleStatus::MODULE_STATUS_ERROR:
      return LOG_STR("error");
    case HlkFm22xModuleStatus::MODULE_STATUS_INVALID:
      return LOG_STR("not initialized");
    default:
      return LOG_STR("unknown");
  }
}

static const char *face_state_to_string(int16_t state) {
  switch (state) {
    case FACE_STATE_IDLE:
      return "Idle";
    case HlkFm22xFaceState::FACE_STATE_NORMAL:
      return "Face detected";
    case HlkFm22xFaceState::FACE_STATE_NO_FACE:
      return "No face";
    case HlkFm22xFaceState::FACE_STATE_TOO_HIGH:
      return "Too high";
    case HlkFm22xFaceState::FACE_STATE_TOO_LOW:
      return "Too low";
    case HlkFm22xFaceState::FACE_STATE_TOO_LEFT:
      return "Too far left";
    case HlkFm22xFaceState::FACE_STATE_TOO_RIGHT:
      return "Too far right";
    case HlkFm22xFaceState::FACE_STATE_TOO_FAR:
      return "Too far away";
    case HlkFm22xFaceState::FACE_STATE_TOO_CLOSE:
      return "Too close";
    case HlkFm22xFaceState::FACE_STATE_EYEBROW_OCCLUSION:
      return "Eyebrows covered";
    case HlkFm22xFaceState::FACE_STATE_EYE_OCCLUSION:
      return "Eyes covered";
    case HlkFm22xFaceState::FACE_STATE_FACE_OCCLUSION:
      return "Face covered";
    case HlkFm22xFaceState::FACE_STATE_DIRECTION_ERROR:
      return "Wrong direction";
    case HlkFm22xFaceState::FACE_STATE_EYES_OPEN:
      return "Eyes open";
    case HlkFm22xFaceState::FACE_STATE_EYES_CLOSED:
      return "Eyes closed";
    case HlkFm22xFaceState::FACE_STATE_EYES_UNKNOWN:
      return "Eye state unknown";
    default:
      return "Unknown";
  }
}

void HlkFm22xComponent::setup() {
  this->set_enrolling_(false);
  this->set_scanning_(false);
  this->publish_face_state_(FACE_STATE_IDLE);
  // Drop anything the module sent before we started listening
  while (this->available() > 0) {
    this->read();
  }
  this->start_read_out_();
}

void HlkFm22xComponent::loop() {
  this->check_timeouts_();
  this->read_frames_();
  this->send_next_command_();
}

void HlkFm22xComponent::enroll_face(const std::string &name, HlkFm22xFaceDirection direction, bool admin,
                                    uint8_t timeout_s, bool allow_duplicate) {
  if (name.length() > MAX_NAME_LENGTH) {
    ESP_LOGE(TAG, "enroll_face(): name too long '%s' (max %u bytes)", name.c_str(), (unsigned) MAX_NAME_LENGTH);
    this->enrollment_failed_callback_.call(HlkFm22xResult::FAILED4_INVALIDPARAM);
    return;
  }
  if (timeout_s == 0) {
    timeout_s = HLK_FM22X_DEFAULT_TIMEOUT_S;
  }
  const bool single_frame = direction == HlkFm22xFaceDirection::FACE_DIRECTION_UNDEFINED;
  ESP_LOGI(TAG, "Enrolling '%s'%s, %s, timeout %us", name.c_str(), admin ? " as admin" : "",
           single_frame ? "single frame" : "one direction", timeout_s);
  const uint16_t reply_timeout_s = timeout_s + ALGORITHM_TIMEOUT_MARGIN_S;

  uint8_t data[HLK_FM22X_MAX_COMMAND_SIZE]{};
  data[ENROLL_ADMIN_OFFSET] = admin ? 1 : 0;
  std::copy(name.begin(), name.end(), data + ENROLL_NAME_OFFSET);
  data[ENROLL_DIRECTION_OFFSET] = (uint8_t) direction;

  if (!allow_duplicate) {
    // Only the integrated enroll command can refuse a face that is already enrolled
    data[ENROLL_ITG_TYPE_OFFSET] = single_frame ? ENROLL_TYPE_SINGLE : ENROLL_TYPE_INTERACTIVE;
    data[ENROLL_ITG_DUPLICATE_OFFSET] = 0;
    data[ENROLL_ITG_TIMEOUT_OFFSET] = timeout_s;
    this->enqueue_(HlkFm22xCommand::ENROLL_ITG, data, ENROLL_ITG_SIZE, reply_timeout_s);
    return;
  }
  data[ENROLL_TIMEOUT_OFFSET] = timeout_s;
  this->enqueue_(single_frame ? HlkFm22xCommand::ENROLL_SINGLE : HlkFm22xCommand::ENROLL, data, ENROLL_SIZE,
                 reply_timeout_s);
}

void HlkFm22xComponent::scan_face(uint8_t timeout_s) {
  if (timeout_s == 0) {
    timeout_s = HLK_FM22X_DEFAULT_TIMEOUT_S;
  }
  ESP_LOGI(TAG, "Scanning for a face, timeout %us", timeout_s);
  const uint8_t data[] = {0, timeout_s};  // the first byte asks the module not to power down afterwards
  this->enqueue_(HlkFm22xCommand::VERIFY, data, sizeof(data), timeout_s + ALGORITHM_TIMEOUT_MARGIN_S);
}

void HlkFm22xComponent::cancel() {
  this->drop_queued_(is_scan_or_enroll_);
  if (is_scan_or_enroll_(this->pending_command_)) {
    const bool enrolling = this->pending_command_ != HlkFm22xCommand::VERIFY;
    ESP_LOGI(TAG, "Cancelling %s", enrolling ? "enrollment" : "scan");
    // RESET makes the module stop what it is doing; FACE_RESET then forgets the directions captured so far
    this->interrupt_with_(HlkFm22xCommand::RESET);
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
  this->enqueue_face_id_command_(HlkFm22xCommand::DELETE_FACE, face_id);
}

void HlkFm22xComponent::delete_all_faces() {
  ESP_LOGI(TAG, "Deleting all faces");
  this->enqueue_(HlkFm22xCommand::DELETE_ALL_FACES);
}

void HlkFm22xComponent::get_face_details(int16_t face_id) {
  ESP_LOGD(TAG, "Requesting details of face %d", face_id);
  this->enqueue_face_id_command_(HlkFm22xCommand::GET_FACE_DETAILS, face_id);
}

void HlkFm22xComponent::reset() {
  ESP_LOGI(TAG, "Resetting module");
  if (is_scan_or_enroll_(this->pending_command_)) {
    this->interrupt_with_(HlkFm22xCommand::RESET);
    return;
  }
  this->enqueue_(HlkFm22xCommand::RESET);
}

bool HlkFm22xComponent::is_scan_or_enroll_(HlkFm22xCommand command) {
  return command == HlkFm22xCommand::VERIFY || command == HlkFm22xCommand::ENROLL ||
         command == HlkFm22xCommand::ENROLL_SINGLE || command == HlkFm22xCommand::ENROLL_ITG;
}

bool HlkFm22xComponent::is_read_out_(HlkFm22xCommand command) {
  return command == HlkFm22xCommand::GET_STATUS || command == HlkFm22xCommand::GET_VERSION ||
         command == HlkFm22xCommand::GET_SERIAL_NUMBER || command == HlkFm22xCommand::GET_ALL_FACE_IDS;
}

uint16_t HlkFm22xComponent::default_timeout_s_(HlkFm22xCommand command) {
  if (command == HlkFm22xCommand::DELETE_FACE || command == HlkFm22xCommand::DELETE_ALL_FACES) {
    return DELETE_TIMEOUT_S;
  }
  return COMMAND_TIMEOUT_S;
}

bool HlkFm22xComponent::parse_face_record_(const uint8_t *data, size_t length, int16_t &face_id, const char *&name,
                                           size_t &name_length, bool &admin) {
  if (length < FACE_RECORD_MIN_SIZE) {
    return false;
  }
  face_id = (int16_t) encode_uint16(data[2], data[3]);
  name = reinterpret_cast<const char *>(data + 4);
  name_length = printable_length(name, HLK_FM22X_NAME_SIZE);
  admin = length > FACE_RECORD_ADMIN_OFFSET && data[FACE_RECORD_ADMIN_OFFSET] != 0;
  return true;
}

bool HlkFm22xComponent::enqueue_(HlkFm22xCommand command, const uint8_t *data, size_t size, uint16_t timeout_s) {
  if (size > HLK_FM22X_MAX_COMMAND_SIZE) {
    ESP_LOGE(TAG, "Command 0x%02X payload too large: %zu bytes", command, size);
    return false;
  }
  QueuedCommand queued{};
  queued.command = command;
  queued.size = (uint8_t) size;
  queued.timeout_s = timeout_s != 0 ? timeout_s : default_timeout_s_(command);
  if (size > 0) {
    memcpy(queued.data, data, size);
  }
  if (!this->queue_.push(queued)) {
    ESP_LOGE(TAG, "Command queue full, dropping command 0x%02X", command);
    return false;
  }
  return true;
}

void HlkFm22xComponent::enqueue_face_id_command_(HlkFm22xCommand command, int16_t face_id) {
  const uint8_t data[] = {(uint8_t) (face_id >> 8), (uint8_t) (face_id & 0xFF)};
  this->enqueue_(command, data, sizeof(data));
}

void HlkFm22xComponent::drop_queued_(bool (*predicate)(HlkFm22xCommand)) {
  // The ring buffer has no erase: take every entry out once and put back the ones that stay
  const size_t count = this->queue_.size();
  for (size_t i = 0; i < count; i++) {
    const QueuedCommand queued = this->queue_.front();
    this->queue_.pop();
    if (!predicate(queued.command)) {
      this->queue_.push(queued);
    }
  }
}

void HlkFm22xComponent::send_next_command_() {
  if (this->pending_command_ != HlkFm22xCommand::NONE || this->queue_.empty()) {
    return;
  }
  const QueuedCommand &queued = this->queue_.front();
  this->write_frame_(queued.command, queued.data, queued.size);
  this->pending_command_ = queued.command;
  this->pending_sent_ms_ = millis();
  this->pending_timeout_ms_ = queued.timeout_s * 1000UL;
  if (queued.command == HlkFm22xCommand::VERIFY) {
    this->set_scanning_(true);
  } else if (is_scan_or_enroll_(queued.command)) {
    this->set_enrolling_(true);
  }
  this->queue_.pop();
}

void HlkFm22xComponent::write_frame_(HlkFm22xCommand command, const uint8_t *data, size_t size) {
  ESP_LOGV(TAG, "Sending command 0x%02X with %zu bytes", command, size);
  const uint8_t header[] = {(uint8_t) (START_CODE >> 8), (uint8_t) (START_CODE & 0xFF), (uint8_t) command,
                            (uint8_t) (size >> 8), (uint8_t) (size & 0xFF)};
  // The checksum covers everything after the start code
  uint8_t checksum = header[2] ^ header[3] ^ header[4];
  for (size_t i = 0; i < size; i++) {
    checksum ^= data[i];
  }
  this->write_array(header, sizeof(header));
  if (size > 0) {
    this->write_array(data, size);
  }
  this->write_byte(checksum);
}

void HlkFm22xComponent::interrupt_with_(HlkFm22xCommand command) {
  // The module is busy with a scan or enrollment; this command jumps the queue and the
  // running scan or enrollment gets a grace period to answer once the module has confirmed the stop
  this->hold_interrupted_(this->pending_command_);
  this->write_frame_(command, nullptr, 0);
  this->pending_command_ = command;
  this->pending_sent_ms_ = millis();
  this->pending_timeout_ms_ = COMMAND_TIMEOUT_S * 1000UL;
}

void HlkFm22xComponent::hold_interrupted_(HlkFm22xCommand command) {
  // Only one interrupted scan or enrollment can wait for its reply; an older one is reported as aborted
  this->clear_interrupted_(true);
  this->interrupted_command_ = command;
  this->interrupted_deadline_set_ = false;
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
  size_t available = this->available();
  if (available == 0) {
    return;
  }
  this->recv_last_byte_ms_ = millis();
  uint8_t buffer[32];
  while (available > 0) {
    const size_t to_read = std::min(available, sizeof(buffer));
    if (!this->read_array(buffer, to_read)) {
      break;
    }
    available -= to_read;
    for (size_t i = 0; i < to_read; i++) {
      this->process_byte_(buffer[i]);
    }
  }
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
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(HLK_FM22X_MAX_RESPONSE_SIZE)];
  ESP_LOGV(TAG, "Received type 0x%02X, %u bytes: %s", this->recv_type_, this->recv_length_,
           format_hex_pretty_to(hex_buf, this->recv_buf_.data(), stored));
#endif
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
        info[i] = (int16_t) encode_uint16(data[2 + 2 * i], data[1 + 2 * i]);
      }
      ESP_LOGV(TAG, "Face state: %s (%d), left: %d, top: %d, right: %d, bottom: %d, yaw: %d, pitch: %d, roll: %d",
               face_state_to_string(info[0]), info[0], info[1], info[2], info[3], info[4], info[5], info[6], info[7]);
      this->publish_face_state_(info[0]);
      this->face_info_callback_.call(info[0], info[1], info[2], info[3], info[4], info[5], info[6], info[7]);
      break;
    }
    case HlkFm22xNoteType::NOTE_READY:
      ESP_LOGI(TAG, "Module ready");
      if (this->pending_command_ != HlkFm22xCommand::NONE) {
        if (is_scan_or_enroll_(this->pending_command_)) {
          // The module may still answer the scan or enrollment it was running; give it a moment
          this->hold_interrupted_(this->pending_command_);
          this->interrupted_deadline_ms_ = millis() + INTERRUPT_GRACE_MS;
          this->interrupted_deadline_set_ = true;
        }
        // Whatever else was sent before the module (re)started will not be answered
        this->pending_command_ = HlkFm22xCommand::NONE;
      }
      this->retry_scheduled_ = false;
      if (this->read_out_done_) {
        // Version, serial number and stored faces do not change on a restart; only the status is worth a look
        this->enqueue_(HlkFm22xCommand::GET_STATUS);
      } else {
        this->start_read_out_();
      }
      break;
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
    this->interrupted_command_ = HlkFm22xCommand::NONE;
    this->interrupted_deadline_set_ = false;
  } else if (is_scan_or_enroll_(command)) {
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
        ESP_LOGD(TAG, "Module status: %s (%u)", LOG_STR_ARG(module_status_to_string(data[2])), data[2]);
        if (this->status_sensor_ != nullptr) {
          this->status_sensor_->publish_state(data[2]);
        }
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
        if (this->face_count_sensor_ != nullptr) {
          this->face_count_sensor_->publish_state(data[2]);
        }
      }
      break;
    case HlkFm22xCommand::DELETE_FACE:
    case HlkFm22xCommand::DELETE_ALL_FACES:
      if (result == HlkFm22xResult::SUCCEEDED) {
        ESP_LOGI(TAG, "%s deleted", command == HlkFm22xCommand::DELETE_FACE ? "Face" : "All faces");
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
        this->interrupted_deadline_ms_ = millis() + INTERRUPT_GRACE_MS;
        this->interrupted_deadline_set_ = true;
      }
      this->enqueue_(HlkFm22xCommand::GET_STATUS);
      break;
    default:
      ESP_LOGW(TAG, "Unhandled reply for command 0x%02X", command);
      break;
  }

  // Only one command is ever outstanding, so an expected read-out reply belongs to the running read-out chain
  if (expected && is_read_out_(command)) {
    this->continue_read_out_();
  }
}

void HlkFm22xComponent::handle_scan_reply_(uint8_t result, const uint8_t *data, size_t length) {
  this->set_scanning_(false);
  this->publish_face_state_(FACE_STATE_IDLE);
  if (result == HlkFm22xResult::SUCCEEDED) {
    int16_t face_id;
    const char *name;
    size_t name_length;
    bool admin;
    if (!parse_face_record_(data, length, face_id, name, name_length, admin)) {
      ESP_LOGE(TAG, "Scan reply too short: %zu bytes", length);
      this->face_scan_invalid_callback_.call(HlkFm22xResult::FAILED4_UNKNOWNREASON);
      return;
    }
    ESP_LOGI(TAG, "Face matched: ID %d, name '%.*s'%s", face_id, (int) name_length, name, admin ? " (admin)" : "");
    if (this->last_face_id_sensor_ != nullptr) {
      this->last_face_id_sensor_->publish_state(face_id);
    }
    if (this->last_face_name_text_sensor_ != nullptr) {
      this->last_face_name_text_sensor_->publish_state(name, name_length);
    }
    this->face_scan_matched_callback_.call(face_id, std::string(name, name_length), admin);
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
  this->set_enrolling_(false);
  this->publish_face_state_(FACE_STATE_IDLE);
  if (result != HlkFm22xResult::SUCCEEDED) {
    this->enrollment_failed_callback_.call(result);
    return;
  }
  if (length < ENROLL_REPLY_MIN_SIZE) {
    ESP_LOGE(TAG, "Enrollment reply too short: %zu bytes", length);
    this->enrollment_failed_callback_.call(HlkFm22xResult::FAILED4_UNKNOWNREASON);
    return;
  }
  const auto face_id = (int16_t) encode_uint16(data[2], data[3]);
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
  int16_t face_id;
  const char *name;
  size_t name_length;
  bool admin;
  if (!parse_face_record_(data, length, face_id, name, name_length, admin)) {
    ESP_LOGE(TAG, "Face details reply too short: %zu bytes", length);
    return;
  }
  ESP_LOGD(TAG, "Face %d: name '%.*s'%s", face_id, (int) name_length, name, admin ? " (admin)" : "");
  this->face_details_callback_.call(face_id, std::string(name, name_length), admin);
}

void HlkFm22xComponent::publish_text_(text_sensor::TextSensor *text_sensor, const uint8_t *data, size_t length) {
  const char *text = reinterpret_cast<const char *>(data);
  const size_t text_length = printable_length(text, length);
  ESP_LOGD(TAG, "Module reports: %.*s", (int) text_length, text);
  if (text_sensor != nullptr) {
    text_sensor->publish_state(text, text_length);
  }
}

void HlkFm22xComponent::check_timeouts_() {
  const uint32_t now = millis();
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

  if (this->retry_scheduled_ && (int32_t) (now - this->retry_at_ms_) >= 0) {
    this->retry_scheduled_ = false;
    if (this->pending_command_ == HlkFm22xCommand::NONE && this->queue_.empty()) {
      this->start_read_out_();
    }
  }
}

void HlkFm22xComponent::finish_failed_(HlkFm22xCommand command, uint8_t error) {
  switch (command) {
    case HlkFm22xCommand::VERIFY:
      this->set_scanning_(false);
      this->publish_face_state_(FACE_STATE_IDLE);
      this->face_scan_invalid_callback_.call(error);
      break;
    case HlkFm22xCommand::ENROLL:
    case HlkFm22xCommand::ENROLL_SINGLE:
    case HlkFm22xCommand::ENROLL_ITG:
      this->set_enrolling_(false);
      this->publish_face_state_(FACE_STATE_IDLE);
      this->enrollment_failed_callback_.call(error);
      break;
    default:
      break;
  }
}

void HlkFm22xComponent::start_read_out_() {
  // Never let two read-out chains run at the same time
  this->drop_queued_(is_read_out_);
  this->read_out_stage_ = 0;
  this->continue_read_out_();
}

void HlkFm22xComponent::continue_read_out_() {
  // Read status, version, serial number and face count one after the other, skipping what nobody asked for
  while (this->read_out_stage_ < READ_OUT_STAGE_DONE) {
    const uint8_t stage = this->read_out_stage_++;
    switch (stage) {
      case 0:
        this->enqueue_(HlkFm22xCommand::GET_STATUS);
        return;
      case 1:
        if (this->version_text_sensor_ != nullptr) {
          this->enqueue_(HlkFm22xCommand::GET_VERSION);
          return;
        }
        break;
      case 2:
        if (this->serial_number_text_sensor_ != nullptr) {
          this->enqueue_(HlkFm22xCommand::GET_SERIAL_NUMBER);
          return;
        }
        break;
      case 3:
        if (this->face_count_sensor_ != nullptr) {
          this->enqueue_(HlkFm22xCommand::GET_ALL_FACE_IDS);
          return;
        }
        break;
      default:
        break;
    }
  }
  this->read_out_done_ = true;
}

void HlkFm22xComponent::refresh_face_count_() {
  if (this->face_count_sensor_ != nullptr) {
    this->enqueue_(HlkFm22xCommand::GET_ALL_FACE_IDS);
  }
}

void HlkFm22xComponent::set_enrolling_(bool enrolling) {
  if (this->enrolling_binary_sensor_ == nullptr) {
    return;
  }
  if (this->enrolling_binary_sensor_->has_state() && this->enrolling_binary_sensor_->state == enrolling) {
    return;
  }
  this->enrolling_binary_sensor_->publish_state(enrolling);
}

void HlkFm22xComponent::set_scanning_(bool scanning) {
  if (this->scanning_binary_sensor_ == nullptr) {
    return;
  }
  if (this->scanning_binary_sensor_->has_state() && this->scanning_binary_sensor_->state == scanning) {
    return;
  }
  this->scanning_binary_sensor_->publish_state(scanning);
}

void HlkFm22xComponent::publish_face_state_(int16_t state) {
  if (state == this->last_face_state_) {
    return;
  }
  this->last_face_state_ = state;
  if (this->face_state_text_sensor_ != nullptr) {
    this->face_state_text_sensor_->publish_state(face_state_to_string(state));
  }
}

void HlkFm22xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HLK-FM22X:");
  this->check_uart_settings(BAUD_RATE);
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
