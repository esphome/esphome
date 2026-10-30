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

class HlkFm22xComponent final : public Component, public uart::UARTDevice {
  SUB_SENSOR(face_count)
  SUB_SENSOR(status)
  SUB_SENSOR(last_face_id)
  SUB_TEXT_SENSOR(last_face_name)
  SUB_BINARY_SENSOR(enrolling)
  SUB_BINARY_SENSOR(scanning)
  SUB_TEXT_SENSOR(version)
  SUB_TEXT_SENSOR(serial_number)
  SUB_TEXT_SENSOR(face_state)

 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

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
  // Commands wait here until the module has answered the previous one. Only enrollments need a
  // payload larger than the argument, so theirs waits in pending_enroll_ (one at a time).
  struct QueuedCommand {
    HlkFm22xCommand command;
    uint8_t timeout_s;  // timeout handed to the module for scans and enrollments, 0 for other commands
    uint16_t arg;       // face id for commands that take one
  };

  // Payload of ENROLL and ENROLL_SINGLE
  struct EnrollPayload {
    uint8_t admin;
    uint8_t name[HLK_FM22X_NAME_SIZE];
    uint8_t direction;
    uint8_t timeout_s;
  };
  // Payload of ENROLL_ITG, the only enroll command that can refuse a face that is already enrolled
  struct EnrollItgPayload {
    uint8_t admin;
    uint8_t name[HLK_FM22X_NAME_SIZE];
    uint8_t direction;
    uint8_t type;
    uint8_t allow_duplicate;
    uint8_t timeout_s;
    uint8_t reserved[3];
  };
  static_assert(sizeof(EnrollItgPayload) == 40, "ENROLL_ITG payload is 40 bytes on the wire");

  enum class RecvState : uint8_t {
    RECV_STATE_SYNC_1,
    RECV_STATE_SYNC_2,
    RECV_STATE_TYPE,
    RECV_STATE_LENGTH_HIGH,
    RECV_STATE_LENGTH_LOW,
    RECV_STATE_DATA,
    RECV_STATE_CHECKSUM,
  };

  bool enqueue_(HlkFm22xCommand command, uint16_t arg = 0, uint8_t timeout_s = 0);
  bool enroll_queued_();
  void drop_queued_(bool (*predicate)(HlkFm22xCommand));
  void send_next_command_();
  void write_frame_(HlkFm22xCommand command, const uint8_t *data, size_t size);
  void interrupt_with_reset_();
  void hold_interrupted_(HlkFm22xCommand command);
  void start_interrupt_grace_();
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
  void report_failed_(HlkFm22xCommand command, uint8_t error);
  void start_read_out_();
  void continue_read_out_();
  HlkFm22xCommand read_out_command_(uint8_t stage) const;
  void refresh_face_count_();
  void set_busy_(HlkFm22xCommand command, bool busy);
  void publish_face_state_(int16_t state);

  uint32_t recv_last_byte_ms_{0};
  uint32_t pending_sent_ms_{0};
  uint32_t pending_timeout_ms_{0};
  uint32_t interrupted_deadline_ms_{0};
  uint32_t retry_at_ms_{0};
  LazyCallbackManager<void(uint8_t)> face_scan_invalid_callback_;
  LazyCallbackManager<void(int16_t, const std::string &, bool)> face_scan_matched_callback_;
  LazyCallbackManager<void()> face_scan_unmatched_callback_;
  LazyCallbackManager<void(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t)> face_info_callback_;
  LazyCallbackManager<void(int16_t, const std::string &, bool)> face_details_callback_;
  LazyCallbackManager<void(int16_t, uint8_t)> enrollment_done_callback_;
  LazyCallbackManager<void(uint8_t)> enrollment_failed_callback_;
  StaticRingBuffer<QueuedCommand, HLK_FM22X_COMMAND_QUEUE_SIZE> queue_;
  // The larger payload comes first so value initialization clears all of it
  union {
    EnrollItgPayload itg;
    EnrollPayload basic;
  } pending_enroll_{};
  std::array<uint8_t, HLK_FM22X_MAX_RESPONSE_SIZE> recv_buf_{};
  uint16_t recv_length_{0};
  uint16_t recv_index_{0};
  int16_t last_face_state_{INT16_MIN};
  RecvState recv_state_{RecvState::RECV_STATE_SYNC_1};
  uint8_t recv_type_{0};
  uint8_t recv_checksum_{0};
  HlkFm22xCommand pending_command_{HlkFm22xCommand::NONE};
  // Scan or enrollment that was interrupted (cancel, reset or module restart) and may still answer
  HlkFm22xCommand interrupted_command_{HlkFm22xCommand::NONE};
  uint8_t read_out_stage_{0};
  uint8_t consecutive_timeouts_{0};
  bool interrupted_deadline_set_{false};
  bool retry_scheduled_{false};
  bool read_out_done_{false};
};

}  // namespace esphome::hlk_fm22x
