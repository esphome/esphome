#include "ubx_protocol.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time.h"
#include <algorithm>
#include <cstring>

namespace esphome::ubx {

static void checksum(const uint8_t *data, size_t len, uint8_t &a, uint8_t &b) {
  for (size_t i = 0; i < len; i++) {
    a += data[i];
    b += a;
  }
}

size_t build_frame(uint8_t msg_class, uint8_t msg_id, const uint8_t *payload, uint16_t len, uint8_t *out) {
  out[0] = UBX_SYNC_1;
  out[1] = UBX_SYNC_2;
  out[2] = msg_class;
  out[3] = msg_id;
  put_le16(out + 4, len);
  if (len > 0)
    memcpy(out + 6, payload, len);
  uint8_t a = 0, b = 0;
  checksum(out + 2, len + 4, a, b);
  out[6 + len] = a;
  out[7 + len] = b;
  return len + UBX_FRAME_OVERHEAD;
}

// Parser states; payload bytes are state 6 until length_ is reached
enum : uint8_t {
  STATE_SYNC_1,
  STATE_SYNC_2,
  STATE_CLASS,
  STATE_ID,
  STATE_LENGTH_1,
  STATE_LENGTH_2,
  STATE_PAYLOAD,
  STATE_CK_A,
  STATE_CK_B,
};

bool FrameParser::feed(uint8_t byte) {
  switch (this->state_) {
    case STATE_SYNC_1:
      if (byte == UBX_SYNC_1)
        this->state_ = STATE_SYNC_2;
      return false;
    case STATE_SYNC_2:
      this->state_ = byte == UBX_SYNC_2 ? STATE_CLASS : (byte == UBX_SYNC_1 ? STATE_SYNC_2 : STATE_SYNC_1);
      this->ck_a_ = 0;
      this->ck_b_ = 0;
      return false;
    case STATE_CLASS:
    case STATE_ID:
    case STATE_LENGTH_1:
      this->header_[this->state_ - STATE_CLASS] = byte;
      checksum(&byte, 1, this->ck_a_, this->ck_b_);
      this->state_++;
      return false;
    case STATE_LENGTH_2:
      this->header_[3] = byte;
      checksum(&byte, 1, this->ck_a_, this->ck_b_);
      this->frame_length_ = encode_value_le<uint16_t>(this->header_ + 2);
      this->length_ = std::min(this->frame_length_, UBX_MAX_PAYLOAD);
      this->pos_ = 0;
      // Longer than any UBX message: a false sync in other data
      if (this->frame_length_ > UBX_MAX_FRAME_PAYLOAD) {
        this->state_ = STATE_SYNC_1;
      } else {
        this->state_ = this->frame_length_ == 0 ? STATE_CK_A : STATE_PAYLOAD;
      }
      return false;
    case STATE_PAYLOAD:
      // A longer frame keeps its start; the rest only goes into the checksum
      if (this->pos_ < this->length_)
        this->payload_[this->pos_] = byte;
      this->pos_++;
      checksum(&byte, 1, this->ck_a_, this->ck_b_);
      if (this->pos_ == this->frame_length_)
        this->state_ = STATE_CK_A;
      return false;
    case STATE_CK_A:
      this->state_ = byte == this->ck_a_ ? STATE_CK_B : STATE_SYNC_1;
      return false;
    default:
      this->state_ = STATE_SYNC_1;
      return byte == this->ck_b_;
  }
}

uint16_t parse_protocol_version(const uint8_t *payload, uint16_t len) {
  // swVersion (30) and hwVersion (10), then 30-byte extensions such as "PROTVER=18.00" or "PROTVER 14.00"
  static constexpr char KEY[] = "PROTVER";
  for (uint16_t off = 40; off + 30 <= len; off += 30) {
    const char *ext = reinterpret_cast<const char *>(payload + off);
    if (strncmp(ext, KEY, sizeof(KEY) - 1) != 0)
      continue;
    // major.minor after the key and its separator, within the extension
    const char *c = ext + sizeof(KEY);
    const char *end = ext + 30;
    uint16_t major = 0;
    uint16_t minor = 0;
    while (c < end && *c >= '0' && *c <= '9')
      major = major * 10 + (*c++ - '0');
    if (c < end && *c == '.') {
      c++;
      while (c < end && *c >= '0' && *c <= '9')
        minor = minor * 10 + (*c++ - '0');
    }
    return major * 100 + minor;
  }
  return 0;
}

using gnss::GNSSData;

/// A ground speed in mm/s as knots times 1000
static uint32_t knots_e3(int64_t mm_per_s) {
  return mm_per_s <= 0 ? 0 : static_cast<uint32_t>((mm_per_s * 3600 + 926) / 1852);
}
/// A heading in 1e-5 degrees as 1e-2 degrees, 360 as 0
static uint16_t course_e2(int32_t heading_e5) {
  const int32_t e2 = (heading_e5 + 500) / 1000;
  return static_cast<uint16_t>(e2 >= 36000 || e2 < 0 ? 0 : e2);
}

static void set_position(GNSSData &data, const uint8_t *lon, const uint8_t *height_msl, const uint8_t *h_acc) {
  data.longitude_e7 = encode_value_le<int32_t>(lon);
  data.latitude_e7 = encode_value_le<int32_t>(lon + 4);
  data.altitude_e3 = encode_value_le<int32_t>(height_msl);  // mm
  data.horizontal_accuracy_e3 = encode_value_le<uint32_t>(h_acc);
  data.vertical_accuracy_e3 = encode_value_le<uint32_t>(h_acc + 4);
  data.updated |= gnss::GNSS_FIELD_LOCATION | gnss::GNSS_FIELD_ALTITUDE | gnss::GNSS_FIELD_HORIZONTAL_ACCURACY |
                  gnss::GNSS_FIELD_VERTICAL_ACCURACY;
}

bool decode_nav(uint8_t msg_id, const uint8_t *p, uint16_t len, uint64_t now_ms, GNSSData &data, bool &fix) {
  switch (msg_id) {
    case UBX_NAV_PVT: {
      // 84 bytes in protocol 14, 92 from 15; the fields read here are the same
      if (len < 84)
        return false;
      const uint8_t fix_type = p[20];
      fix = fix_type >= 2 && fix_type <= 4 && (p[21] & 0x01) != 0;
      data.satellites = p[23];
      set_position(data, p + 24, p + 36, p + 40);
      data.speed_e3 = knots_e3(encode_value_le<int32_t>(p + 60));
      data.course_e2 = course_e2(encode_value_le<int32_t>(p + 64));
      data.updated |= gnss::GNSS_FIELD_SATELLITES | gnss::GNSS_FIELD_SPEED | gnss::GNSS_FIELD_COURSE;
      return true;
    }
    case UBX_NAV_POSLLH:
      if (len < 28)
        return false;
      set_position(data, p + 4, p + 16, p + 20);
      return true;
    case UBX_NAV_VELNED:
      if (len < 36)
        return false;
      data.speed_e3 = knots_e3(static_cast<int64_t>(encode_value_le<uint32_t>(p + 20)) * 10);  // cm/s
      data.course_e2 = course_e2(encode_value_le<int32_t>(p + 24));
      data.updated |= gnss::GNSS_FIELD_SPEED | gnss::GNSS_FIELD_COURSE;
      return true;
    case UBX_NAV_SOL:
      if (len < 52)
        return false;
      fix = p[10] >= 2 && p[10] <= 4 && (p[11] & 0x01) != 0;
      data.satellites = p[47];
      data.updated |= gnss::GNSS_FIELD_SATELLITES;
      return true;
    case UBX_NAV_DOP:
      if (len < 18)
        return false;
      data.hdop_e2 = encode_value_le<uint16_t>(p + 12);
      data.updated |= gnss::GNSS_FIELD_HDOP;
      return true;
    case UBX_NAV_TIMEUTC: {
      if (len < 20)
        return false;
      // validUTC: the receiver knows the current leap seconds
      if ((p[19] & 0x04) == 0)
        return true;
      data.year = encode_value_le<uint16_t>(p + 12);
      data.month = p[14];
      data.day = p[15];
      data.hour = p[16];
      data.minute = p[17];
      data.second = p[18];
      ESPTime t{};
      t.year = data.year;
      t.month = data.month;
      t.day_of_month = data.day;
      t.hour = data.hour;
      t.minute = data.minute;
      t.second = data.second;
      t.recalc_timestamp_utc(false);
      data.utc = t.timestamp;
      data.utc_millis = now_ms;
      data.updated |= gnss::GNSS_FIELD_TIME | gnss::GNSS_FIELD_DATE;
      return true;
    }
    default:
      return false;
  }
}

}  // namespace esphome::ubx
