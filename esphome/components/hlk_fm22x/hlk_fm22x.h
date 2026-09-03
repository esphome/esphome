#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace esphome::hlk_fm22x {

static const uint16_t START_CODE = 0xEFAA;
static constexpr size_t HLK_FM22X_NAME_SIZE = 32;
// Largest reply that is parsed in full: command(1) + result(1) + face_id(2) + name(32) + admin(1) + unlock_status(1).
// Longer replies (the list of enrolled face IDs) are checksummed in full but only their first bytes are kept.
static constexpr size_t HLK_FM22X_MAX_RESPONSE_SIZE = 38;
// Largest command payload: admin(1) + name(32) + direction(1) + type(1) + duplicate(1) + timeout(1) + reserved(3)
static constexpr size_t HLK_FM22X_MAX_COMMAND_SIZE = 40;
static constexpr size_t HLK_FM22X_COMMAND_QUEUE_SIZE = 6;
static constexpr uint8_t HLK_FM22X_DEFAULT_TIMEOUT_S = 10;
// Direction bitmask reported once a face has been captured from all five directions
static constexpr uint8_t HLK_FM22X_ALL_DIRECTIONS = 0x1F;

enum HlkFm22xCommand : uint8_t {
  NONE = 0x00,
  RESET = 0x10,
  GET_STATUS = 0x11,
  VERIFY = 0x12,
  ENROLL = 0x13,
  ENROLL_SINGLE = 0x1D,
  DELETE_FACE = 0x20,
  DELETE_ALL_FACES = 0x21,
  GET_FACE_DETAILS = 0x22,
  FACE_RESET = 0x23,
  GET_ALL_FACE_IDS = 0x24,
  ENROLL_ITG = 0x26,
  GET_VERSION = 0x30,
  GET_SERIAL_NUMBER = 0x93,
};

enum HlkFm22xResponseType : uint8_t {
  REPLY = 0x00,
  NOTE = 0x01,
  IMAGE = 0x02,
};

enum HlkFm22xNoteType : uint8_t {
  NOTE_READY = 0x00,
  NOTE_FACE_STATE = 0x01,
  NOTE_UNKNOWN_ERROR = 0x02,
  NOTE_OTA_DONE = 0x03,
  NOTE_EYE_STATE = 0x04,
  NOTE_AUTHORIZATION_FAILED = 0x08,
};

enum HlkFm22xResult : uint8_t {
  SUCCEEDED = 0x00,
  REJECTED = 0x01,
  ABORTED = 0x02,
  FAILED4_CAMERA = 0x04,
  FAILED4_UNKNOWNREASON = 0x05,
  FAILED4_INVALIDPARAM = 0x06,
  FAILED4_NOMEMORY = 0x07,
  FAILED4_UNKNOWNUSER = 0x08,
  FAILED4_MAXUSER = 0x09,
  FAILED4_FACEENROLLED = 0x0A,
  FAILED4_LIVENESSCHECK = 0x0C,
  FAILED4_TIMEOUT = 0x0D,
  FAILED4_AUTHORIZATION = 0x0E,
  FAILED4_READ_FILE = 0x13,
  FAILED4_WRITE_FILE = 0x14,
  FAILED4_NO_ENCRYPT = 0x15,
  FAILED4_NO_RGBIMAGE = 0x17,
  FAILED4_JPGPHOTO_LARGE = 0x18,
  FAILED4_JPGPHOTO_SMALL = 0x19,
};

enum HlkFm22xModuleStatus : uint8_t {
  MODULE_STATUS_STANDBY = 0x00,
  MODULE_STATUS_BUSY = 0x01,
  MODULE_STATUS_ERROR = 0x02,
  MODULE_STATUS_INVALID = 0x03,
};

enum HlkFm22xFaceDirection : uint8_t {
  FACE_DIRECTION_UNDEFINED = 0x00,
  FACE_DIRECTION_MIDDLE = 0x01,
  FACE_DIRECTION_RIGHT = 0x02,
  FACE_DIRECTION_LEFT = 0x04,
  FACE_DIRECTION_DOWN = 0x08,
  FACE_DIRECTION_UP = 0x10,
};

enum HlkFm22xEnrollType : uint8_t {
  ENROLL_TYPE_INTERACTIVE = 0x00,
  ENROLL_TYPE_SINGLE = 0x01,
};

// Face state reported by the module while a scan or enrollment is running
enum HlkFm22xFaceState : int16_t {
  FACE_STATE_NORMAL = 0,
  FACE_STATE_NO_FACE = 1,
  FACE_STATE_TOO_HIGH = 2,
  FACE_STATE_TOO_LOW = 3,
  FACE_STATE_TOO_LEFT = 4,
  FACE_STATE_TOO_RIGHT = 5,
  FACE_STATE_TOO_FAR = 6,
  FACE_STATE_TOO_CLOSE = 7,
  FACE_STATE_EYEBROW_OCCLUSION = 8,
  FACE_STATE_EYE_OCCLUSION = 9,
  FACE_STATE_FACE_OCCLUSION = 10,
  FACE_STATE_DIRECTION_ERROR = 11,
  FACE_STATE_EYES_OPEN = 12,
  FACE_STATE_EYES_CLOSED = 13,
  FACE_STATE_EYES_UNKNOWN = 14,
};

class HlkFm22xComponent final : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_face_count_sensor(sensor::Sensor *face_count_sensor) { this->face_count_sensor_ = face_count_sensor; }
  void set_status_sensor(sensor::Sensor *status_sensor) { this->status_sensor_ = status_sensor; }
  void set_last_face_id_sensor(sensor::Sensor *last_face_id_sensor) {
    this->last_face_id_sensor_ = last_face_id_sensor;
  }
  void set_last_face_name_text_sensor(text_sensor::TextSensor *last_face_name_text_sensor) {
    this->last_face_name_text_sensor_ = last_face_name_text_sensor;
  }
  void set_enrolling_binary_sensor(binary_sensor::BinarySensor *enrolling_binary_sensor) {
    this->enrolling_binary_sensor_ = enrolling_binary_sensor;
  }
  void set_scanning_binary_sensor(binary_sensor::BinarySensor *scanning_binary_sensor) {
    this->scanning_binary_sensor_ = scanning_binary_sensor;
  }
  void set_version_text_sensor(text_sensor::TextSensor *version_text_sensor) {
    this->version_text_sensor_ = version_text_sensor;
  }
  void set_serial_number_text_sensor(text_sensor::TextSensor *serial_number_text_sensor) {
    this->serial_number_text_sensor_ = serial_number_text_sensor;
  }
  void set_face_state_text_sensor(text_sensor::TextSensor *face_state_text_sensor) {
    this->face_state_text_sensor_ = face_state_text_sensor;
  }
  template<typename F> void add_on_face_scan_matched_callback(F &&callback) {
    this->face_scan_matched_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_face_scan_unmatched_callback(F &&callback) {
    this->face_scan_unmatched_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_face_scan_invalid_callback(F &&callback) {
    this->face_scan_invalid_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_face_info_callback(F &&callback) {
    this->face_info_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_face_details_callback(F &&callback) {
    this->face_details_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_enrollment_done_callback(F &&callback) {
    this->enrollment_done_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_enrollment_failed_callback(F &&callback) {
    this->enrollment_failed_callback_.add(std::forward<F>(callback));
  }

  /// Start enrolling a face. With FACE_DIRECTION_UNDEFINED the face is captured from a single frame,
  /// otherwise one direction is captured per call and the face is stored once all five are done.
  /// With allow_duplicate set to false the module refuses to enroll a face it already knows.
  void enroll_face(const std::string &name, HlkFm22xFaceDirection direction, bool admin = false,
                   uint8_t timeout_s = HLK_FM22X_DEFAULT_TIMEOUT_S, bool allow_duplicate = true);
  /// Look for a face and try to match it against the enrolled faces.
  void scan_face(uint8_t timeout_s = HLK_FM22X_DEFAULT_TIMEOUT_S);
  /// Stop the running scan or enrollment and forget any directions captured so far.
  void cancel();
  void delete_face(int16_t face_id);
  void delete_all_faces();
  /// Ask for the name and admin flag of an enrolled face; the answer arrives through on_face_details.
  void get_face_details(int16_t face_id);
  /// Put the module back into standby. Anything the module was doing is stopped; queued commands still run.
  void reset();

 protected:
  struct QueuedCommand {
    HlkFm22xCommand command;
    uint8_t size;
    uint16_t timeout_s;  // how long to wait for the reply
    uint8_t data[HLK_FM22X_MAX_COMMAND_SIZE];
  };

  enum class RecvState : uint8_t {
    RECV_STATE_SYNC_1,
    RECV_STATE_SYNC_2,
    RECV_STATE_TYPE,
    RECV_STATE_LENGTH_HIGH,
    RECV_STATE_LENGTH_LOW,
    RECV_STATE_DATA,
    RECV_STATE_CHECKSUM,
  };

  bool enqueue_(HlkFm22xCommand command, const uint8_t *data = nullptr, size_t size = 0, uint16_t timeout_s = 0);
  void enqueue_face_id_command_(HlkFm22xCommand command, int16_t face_id);
  void drop_queued_(bool (*predicate)(HlkFm22xCommand));
  void send_next_command_();
  void write_frame_(HlkFm22xCommand command, const uint8_t *data, size_t size);
  void interrupt_with_(HlkFm22xCommand command);
  void hold_interrupted_(HlkFm22xCommand command);
  void clear_interrupted_(bool aborted);
  void read_frames_();
  void process_byte_(uint8_t byte);
  void handle_frame_();
  void handle_note_(const uint8_t *data, size_t length);
  void handle_reply_(const uint8_t *data, size_t length);
  void handle_scan_reply_(uint8_t result, const uint8_t *data, size_t length);
  void handle_enroll_reply_(uint8_t result, const uint8_t *data, size_t length);
  void handle_face_details_reply_(uint8_t result, const uint8_t *data, size_t length);
  void publish_text_(text_sensor::TextSensor *text_sensor, const uint8_t *data, size_t length);
  void check_timeouts_();
  void finish_failed_(HlkFm22xCommand command, uint8_t error);
  void start_read_out_();
  void continue_read_out_();
  void refresh_face_count_();
  void set_enrolling_(bool enrolling);
  void set_scanning_(bool scanning);
  void publish_face_state_(int16_t state);

  StaticRingBuffer<QueuedCommand, HLK_FM22X_COMMAND_QUEUE_SIZE> queue_;
  std::array<uint8_t, HLK_FM22X_MAX_RESPONSE_SIZE> recv_buf_{};
  RecvState recv_state_{RecvState::RECV_STATE_SYNC_1};
  uint8_t recv_type_{0};
  uint8_t recv_checksum_{0};
  uint16_t recv_length_{0};
  uint16_t recv_index_{0};
  uint32_t recv_last_byte_ms_{0};
  HlkFm22xCommand pending_command_{HlkFm22xCommand::NONE};
  // Scan or enrollment that was interrupted (cancel, reset or module restart) and may still answer
  HlkFm22xCommand interrupted_command_{HlkFm22xCommand::NONE};
  uint32_t pending_sent_ms_{0};
  uint32_t pending_timeout_ms_{0};
  uint32_t interrupted_deadline_ms_{0};
  uint32_t retry_at_ms_{0};
  bool interrupted_deadline_set_{false};
  bool retry_scheduled_{false};
  bool read_out_done_{false};
  uint8_t read_out_stage_{0};
  uint8_t consecutive_timeouts_{0};
  int16_t last_face_state_{INT16_MIN};
  sensor::Sensor *face_count_sensor_{nullptr};
  sensor::Sensor *status_sensor_{nullptr};
  sensor::Sensor *last_face_id_sensor_{nullptr};
  binary_sensor::BinarySensor *enrolling_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *scanning_binary_sensor_{nullptr};
  text_sensor::TextSensor *last_face_name_text_sensor_{nullptr};
  text_sensor::TextSensor *version_text_sensor_{nullptr};
  text_sensor::TextSensor *serial_number_text_sensor_{nullptr};
  text_sensor::TextSensor *face_state_text_sensor_{nullptr};
  LazyCallbackManager<void(uint8_t)> face_scan_invalid_callback_;
  LazyCallbackManager<void(int16_t, const std::string &, bool)> face_scan_matched_callback_;
  LazyCallbackManager<void()> face_scan_unmatched_callback_;
  LazyCallbackManager<void(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t)> face_info_callback_;
  LazyCallbackManager<void(int16_t, const std::string &, bool)> face_details_callback_;
  LazyCallbackManager<void(int16_t, uint8_t)> enrollment_done_callback_;
  LazyCallbackManager<void(uint8_t)> enrollment_failed_callback_;
};

template<typename... Ts> class EnrollmentAction final : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  TEMPLATABLE_VALUE(std::string, name)
  TEMPLATABLE_VALUE(uint8_t, direction)
  TEMPLATABLE_VALUE(bool, admin)
  TEMPLATABLE_VALUE(bool, allow_duplicate)

  void set_timeout_seconds(uint8_t timeout_s) { this->timeout_s_ = timeout_s; }

  void play(const Ts &...x) override {
    this->parent_->enroll_face(this->name_.value(x...), (HlkFm22xFaceDirection) this->direction_.value(x...),
                               this->admin_.value(x...), this->timeout_s_, this->allow_duplicate_.value_or(x..., true));
  }

 protected:
  uint8_t timeout_s_{HLK_FM22X_DEFAULT_TIMEOUT_S};
};

template<typename... Ts> class ScanAction final : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  void set_timeout_seconds(uint8_t timeout_s) { this->timeout_s_ = timeout_s; }

  void play(const Ts &...x) override { this->parent_->scan_face(this->timeout_s_); }

 protected:
  uint8_t timeout_s_{HLK_FM22X_DEFAULT_TIMEOUT_S};
};

template<typename... Ts> class CancelAction final : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->cancel(); }
};

template<typename... Ts> class DeleteAction final : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  TEMPLATABLE_VALUE(int16_t, face_id)

  void play(const Ts &...x) override { this->parent_->delete_face(this->face_id_.value(x...)); }
};

template<typename... Ts> class DeleteAllAction final : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->delete_all_faces(); }
};

template<typename... Ts> class GetFaceDetailsAction final : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  TEMPLATABLE_VALUE(int16_t, face_id)

  void play(const Ts &...x) override { this->parent_->get_face_details(this->face_id_.value(x...)); }
};

template<typename... Ts> class ResetAction final : public Action<Ts...>, public Parented<HlkFm22xComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->reset(); }
};

}  // namespace esphome::hlk_fm22x
