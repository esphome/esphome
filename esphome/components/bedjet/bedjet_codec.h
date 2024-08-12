#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "bedjet_const.h"

namespace esphome {
namespace bedjet {

struct BedjetPacket {
  uint8_t data_length;
  BedjetCommand command;
  uint8_t data[2];
};

enum BedjetPacketFormat : uint8_t {
  PACKET_FORMAT_DEBUG = 0x05,    //  5
  PACKET_FORMAT_V3_HOME = 0x56,  // 86
};

enum BedjetPacketType : uint8_t {
  PACKET_TYPE_STATUS = 0x1,
  PACKET_TYPE_DEBUG = 0x2,
};

enum BedjetNotification : uint8_t {
  NOTIFY_NONE = 0,                    ///< No notification pending
  NOTIFY_FILTER = 1,                  ///< Clean Filter / Please check BedJet air filter and clean if necessary.
  NOTIFY_UPDATE = 2,                  ///< Firmware Update / A newer version of firmware is available.
  NOTIFY_UPDATE_FAIL = 3,             ///< Firmware Update / Unable to connect to the firmware update server.
  NOTIFY_BIO_FAIL_CLOCK_NOT_SET = 4,  ///< The specified sequence cannot be run because the clock is not set
  NOTIFY_BIO_FAIL_TOO_LONG = 5,  ///< The specified sequence cannot be run because it contains steps that would be too
                                 ///< long running from the current time.
  // Note: after handling a notification, send MAGIC_NOTIFY_ACK
};

/** The format of a BedJet V3 status packet. */
struct BedjetStatusPacket {
  // [0]
  bool is_partial : 8;  ///< `1` indicates that this is a partial packet, and more data can be read directly from the
                        ///< characteristic.
  BedjetPacketFormat packet_format : 8;  ///< BedjetPacketFormat::PACKET_FORMAT_V3_HOME for BedJet V3 status packet
                                         ///< format. BedjetPacketFormat::PACKET_FORMAT_DEBUG for debugging packets.
  uint8_t expecting_length : 8;      ///< The expected total length of the status packet after merging the extra packet.
  BedjetPacketType packet_type : 8;  ///< Typically BedjetPacketType::PACKET_TYPE_STATUS for BedJet V3 status packet.

  // [4]
  uint8_t time_remaining_hrs : 8;   ///< Hours remaining in program runtime
  uint8_t time_remaining_mins : 8;  ///< Minutes remaining in program runtime
  uint8_t time_remaining_secs : 8;  ///< Seconds remaining in program runtime

  // [7]
  uint8_t actual_temp_step : 8;  ///< Actual temp of the air blown by the BedJet fan; value represents `2 *
                                 ///< degrees_celsius`. See #bedjet_temp_to_c and #bedjet_temp_to_f
  uint8_t target_temp_step : 8;  ///< Target temp that the BedJet will try to heat to. See #actual_temp_step.

  // [9]
  BedjetMode mode : 8;  ///< BedJet operating mode.

  // [10]
  uint8_t fan_step : 8;  ///< BedJet fan speed; value is in the 0-19 range, representing 5% increments (5%-100%): `5 + 5
                         ///< * fan_step`
  uint8_t max_hrs : 8;   ///< Max hours of mode runtime
  uint8_t max_mins : 8;  ///< Max minutes of mode runtime
  uint8_t min_temp_step : 8;  ///< Min temp allowed in mode. See #actual_temp_step.
  uint8_t max_temp_step : 8;  ///< Max temp allowed in mode. See #actual_temp_step.

  // [15-16]
  uint16_t turbo_time : 16;  ///< Time remaining in BedjetMode::MODE_TURBO.

  // [17]
  uint8_t ambient_temp_step : 8;  ///< Current ambient air temp. This is the coldest air the BedJet can blow. See
                                  ///< #actual_temp_step.
  uint8_t shutdown_reason : 8;    ///< The reason for the last device shutdown.

  // [19-25]; the initial partial packet cuts off here after [19]

  uint8_t unused_1 : 8;  // Unknown [19] = 0x01
  uint8_t unused_2 : 8;  // Unknown [20] = 0x81
  uint8_t unused_3 : 8;  // Unknown [21] = 0x01

  // [22]: 0x2=is_dual_zone, ...?
  struct {
    int unused_1 : 1;       // 0x80
    int unused_2 : 1;       // 0x40
    int unused_3 : 1;       // 0x20
    int unused_4 : 1;       // 0x10
    int unused_5 : 1;       // 0x8
    int unused_6 : 1;       // 0x4
    bool is_dual_zone : 1;  /// Is part of a Dual Zone configuration
    int unused_7 : 1;       // 0x1
  } dual_zone_flags;

  uint8_t unused_4 : 8;  // Unknown 23-24 = 0x1310
  uint8_t unused_5 : 8;  // Unknown 23-24 = 0x1310
  uint8_t unused_6 : 8;  // Unknown 25 = 0x00

  // [26]
  //   0x18(24) = "Connection test has completed OK"
  //   0x1a(26) = "Firmware update is not needed"
  uint8_t update_phase : 8;  ///< The current status/phase of a firmware update.

  // [27]
  union {
    uint8_t flags_packed;
    struct {
      /* uint8_t */
      int unused_1 : 1;           // 0x80
      int unused_2 : 1;           // 0x40
      bool conn_test_passed : 1;  ///< (0x20) Bit is set `1` if the last connection test passed.
      bool leds_enabled : 1;      ///< (0x10) Bit is set `1` if the LEDs on the device are enabled.
      int unused_3 : 1;           // 0x08
      bool units_setup : 1;       ///< (0x04) Bit is set `1` if the device's units have been configured.
      int unused_4 : 1;           // 0x02
      bool beeps_muted : 1;       ///< (0x01) Bit is set `1` if the device's sound output is muted.
    } __attribute__((packed)) flags;
  };

  // [28] = (biorhythm?) sequence step
  uint8_t bio_sequence_step : 8;  /// Biorhythm sequence step number
  // [29] = notify_code:
  BedjetNotification notify_code : 8;  /// See BedjetNotification

  uint16_t unused_7 : 16;  // Unknown

} __attribute__((packed));

/** This class is responsible for encoding command packets and decoding status packets.
 *
 * Status Packets
 * ==============
 * The BedJet protocol depends on registering for notifications on the esphome::BedJet::BEDJET_SERVICE_UUID
 * characteristic. If the BedJet is on, it will send rapid updates as notifications. If it is off,
 * it generally will not notify of any status.
 *
 * As the BedJet V3's BedjetStatusPacket exceeds the buffer size allowed for BLE notification packets,
 * the notification packet will contain `BedjetStatusPacket::is_partial == 1`. When that happens, an additional
 * read of the esphome::BedJet::BEDJET_SERVICE_UUID characteristic will contain the second portion of the
 * full status packet.
 *
 * Command Packets
 * ===============
 * This class supports encoding a number of BedjetPacket commands:
 * - Button press
 *   This simulates a press of one of the BedjetButton values.
 *   - BedjetPacket#command = BedjetCommand::CMD_BUTTON
 *   - BedjetPacket#data [0] contains the BedjetButton value
 * - Set target temp
 *   This sets the BedJet's target temp to a concrete temperature value.
 *   - BedjetPacket#command = BedjetCommand::CMD_SET_TEMP
 *   - BedjetPacket#data [0] contains the BedJet temp value; see BedjetStatusPacket#actual_temp_step
 * - Set fan speed
 *   This sets the BedJet fan speed.
 *   - BedjetPacket#command = BedjetCommand::CMD_SET_FAN
 *   - BedjetPacket#data [0] contains the BedJet fan step in the range 0-19.
 * - Set current time
 *   The BedJet needs to have its clock set properly in order to run the biorhythm programs, which might
 *   contain time-of-day based step rules.
 *   - BedjetPacket#command = BedjetCommand::CMD_SET_CLOCK
 *   - BedjetPacket#data [0] is hours, [1] is minutes
 */
class BedjetCodec {
 public:
  BedjetPacket *get_button_request(BedjetButton button);
  BedjetPacket *get_set_target_temp_request(float temperature);
  BedjetPacket *get_set_fan_speed_request(uint8_t fan_step);
  BedjetPacket *get_set_time_request(uint8_t hour, uint8_t minute);
  BedjetPacket *get_set_runtime_remaining_request(uint8_t hour, uint8_t minute);

  bool decode_notify(const uint8_t *data, uint16_t length);
  void decode_extra(const uint8_t *data, uint16_t length);
  bool compare(const uint8_t *data, uint16_t length);

  inline bool has_status() { return this->status_packet_ != nullptr; }
  const BedjetStatusPacket *get_status_packet() const { return this->status_packet_; }
  void clear_status() { this->status_packet_ = nullptr; }

 protected:
  BedjetPacket *clean_packet_();

  uint8_t last_buffer_size_ = 0;

  BedjetPacket packet_;

  BedjetStatusPacket *status_packet_;
  BedjetStatusPacket buf_;
};

/// Converts a BedJet temp step into degrees Celsius.
float bedjet_temp_to_c(uint8_t temp);

}  // namespace bedjet
}  // namespace esphome
