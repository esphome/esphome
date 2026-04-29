#pragma once
//
// Dayton Audio DSP-408 wire protocol — direct port of dsp408-py/protocol.py.
// Reference: https://github.com/malaiwah/dsp408-py/blob/main/dsp408/protocol.py
//
// All field semantics, command codes, and blob offsets here are
// byte-exact to dsp408-py, which derived them from Windows GUI captures
// against a real DSP-408 (firmware MYDW-AV1.06).
//
// Wire format (64-byte HID report on EP 0x01 OUT / EP 0x82 IN):
//
//     offset  len  field           notes
//     0       4    magic           80 80 80 ee
//     4       1    direction       host->dev: a2 (READ) / a1 (WRITE)
//                                  dev->host: 53 (READ reply) / 51 (WRITE ack)
//     5       1    version         always 01
//     6       1    seq             8-bit sequence number
//     7       1    category        09 = state, 04 = output param, 03 = input
//     8..11   4    cmd             LE u32 command code
//     12..13  2    payload length  LE u16
//     14..N   len  payload         up to 48 bytes single-frame
//     14+len  1    checksum        XOR of bytes[4 .. 14+len-1]
//     15+len  1    end marker      aa
//     rest         padding         00...
//
// Multi-frame replies (e.g. cmd=0x77NN, 296 bytes): first frame carries
// 50 payload bytes WITHOUT chk/end; continuation frames are raw 64-byte
// chunks until the declared length is satisfied; the last continuation
// frame ends with chk + end + zero-pad. v0.1 only does single-frame
// reads/writes (master + per-channel basics) so multi-frame is not yet
// implemented here.

#include <cstdint>
#include <cstring>
#include <cstddef>

namespace esphome {
namespace dsp408 {

// USB IDs
static constexpr uint16_t DSP408_VID = 0x0483;
static constexpr uint16_t DSP408_PID = 0x5750;

// Frame fixed layout
static constexpr uint8_t FRAME_MAGIC0 = 0x80;
static constexpr uint8_t FRAME_MAGIC1 = 0x80;
static constexpr uint8_t FRAME_MAGIC2 = 0x80;
static constexpr uint8_t FRAME_MAGIC3 = 0xEE;
static constexpr uint8_t END_MARKER = 0xAA;
static constexpr uint8_t PROTO_VERSION = 0x01;
static constexpr size_t FRAME_SIZE = 64;
static constexpr size_t HEADER_SIZE = 14;  // bytes before payload

// Direction byte
static constexpr uint8_t DIR_CMD = 0xA2;        // host -> device, READ request
static constexpr uint8_t DIR_WRITE = 0xA1;      // host -> device, WRITE
static constexpr uint8_t DIR_RESP = 0x53;       // device -> host, READ reply
static constexpr uint8_t DIR_WRITE_ACK = 0x51;  // device -> host, WRITE ack

// Category byte
static constexpr uint8_t CAT_INPUT = 0x03;
static constexpr uint8_t CAT_STATE = 0x09;
static constexpr uint8_t CAT_PARAM = 0x04;

// Known command codes
static constexpr uint32_t CMD_CONNECT = 0xCC;
static constexpr uint32_t CMD_IDLE_POLL = 0x03;
static constexpr uint32_t CMD_GET_INFO = 0x04;
static constexpr uint32_t CMD_PRESET_NAME = 0x00;  // 15-byte ASCII
static constexpr uint32_t CMD_STATUS = 0x34;
static constexpr uint32_t CMD_GLOBAL_0X02 = 0x02;
static constexpr uint32_t CMD_MASTER = 0x05;  // alias for CMD_GLOBAL_0x05
static constexpr uint32_t CMD_GLOBAL_0X06 = 0x06;
static constexpr uint32_t CMD_WRITE_CHANNEL_BASE = 0x1F00;  // + ch (CAT_PARAM)
static constexpr uint32_t CMD_READ_CHANNEL_BASE = 0x7700;   // + ch (multi-frame)

// Per-channel routing matrix writes — 0x2100..0x2107 = Ch1..Ch8.
// 8-byte payload of uint8_t levels for IN1..IN8 (0x64 = ON / unity, 0x00 = OFF).
static constexpr uint32_t CMD_ROUTING_BASE = 0x2100;
// Mirror for IN9..IN16 — DSP-408 only has 4 physical inputs but the
// protocol surface includes the upper half from the sibling DSP-816 firmware.
static constexpr uint32_t CMD_ROUTING_HI_BASE = 0x2200;
static constexpr uint8_t ROUTING_ON = 0x64;
static constexpr uint8_t ROUTING_OFF = 0x00;

// Per-channel compressor write (firmware-inert in v1.06 but kept for parity).
static constexpr uint32_t CMD_WRITE_COMPRESSOR_BASE = 0x2300;

// Per-channel name write — 0x2400..0x2407, 8-byte ASCII payload (NUL-padded).
static constexpr uint32_t CMD_WRITE_CHANNEL_NAME_BASE = 0x2400;

// Crossover writes — 0x12000..0x12007 (named symbol; was a bare literal).
static constexpr uint32_t CMD_WRITE_CROSSOVER_BASE = 0x12000;

// 10-band parametric EQ writes:
//   cmd = CMD_WRITE_EQ_BAND_BASE + (band << 8) + channel
//   band = 0..9, channel = 0..7
// Payload (8 bytes):
//   [0..1] freq Hz LE16
//   [2..3] gain raw LE16 (dB = (raw - 600) / 10; same as channel volume)
//   [4]    bandwidth byte; Q ≈ 256 / b4_byte
//   [5..7] zeros
static constexpr uint32_t CMD_WRITE_EQ_BAND_BASE = 0x10000;

// EQ encoding constants
static constexpr float EQ_Q_BW_CONSTANT = 256.0f;
static constexpr int EQ_GAIN_RAW_OFFSET = 600;  // same as CHANNEL_VOL_OFFSET
static constexpr int EQ_GAIN_RAW_MIN = 0;       // -60 dB
static constexpr int EQ_GAIN_RAW_MAX = 1200;    // +60 dB envelope

// Master payload constants (decode of byte[0]):
//     dB = lvl_raw - 60   (0..66 raw = -60..+6 dB)
static constexpr int MASTER_LEVEL_OFFSET = 60;
static constexpr int MASTER_LEVEL_MIN = 0;
static constexpr int MASTER_LEVEL_MAX = 66;

// Per-channel volume:
//     dB = (raw - 600) / 10   (0..600 = -60..0 dB)
static constexpr int CHANNEL_VOL_MIN = 0;
static constexpr int CHANNEL_VOL_MAX = 600;
static constexpr int CHANNEL_VOL_OFFSET = 600;

// Delay (samples; firmware clamps at 359 -> 8.143 ms @ 44.1 kHz / 7.479 ms @ 48 kHz)
static constexpr int CHANNEL_DELAY_MIN = 0;
static constexpr int CHANNEL_DELAY_MAX = 359;

// Default per-channel speaker-role byte (blob[255] / write byte[7]).
// Values copied from dsp408-py/protocol.py CHANNEL_SUBIDX.
static constexpr uint8_t CHANNEL_SUBIDX_DEFAULT[8] = {
    0x01, 0x02, 0x03, 0x07, 0x08, 0x09, 0x0F, 0x12,
};

// 296-byte channel-state blob layout (returned by cmd=0x77NN reads).
// Offsets from dsp408-py/protocol.py — cross-verified against the
// Windows GUI captures on the dsp408-py reverse-engineering branch.
static constexpr size_t CHANNEL_BLOB_SIZE = 296;
static constexpr size_t OFF_MUTE = 248;
static constexpr size_t OFF_POLAR = 249;
static constexpr size_t OFF_GAIN = 250;      // u16 LE
static constexpr size_t OFF_DELAY = 252;     // u16 LE samples
static constexpr size_t OFF_BYTE_254 = 254;  // semantic unknown
static constexpr size_t OFF_SPK_TYPE = 255;
static constexpr size_t OFF_HPF_FREQ = 256;    // u16 LE Hz
static constexpr size_t OFF_HPF_FILTER = 258;  // 0=BW 1=Bessel 2=LR (3=alias)
static constexpr size_t OFF_HPF_SLOPE = 259;   // 0..7 = 6..48 dB/oct, 8=Off
static constexpr size_t OFF_LPF_FREQ = 260;
static constexpr size_t OFF_LPF_FILTER = 262;
static constexpr size_t OFF_LPF_SLOPE = 263;
static constexpr size_t OFF_MIXER = 264;       // 8 × u8 (IN1..IN8)
static constexpr size_t OFF_ALL_PASS_Q = 280;  // u16 LE
static constexpr size_t OFF_ATTACK_MS = 282;   // u16 LE
static constexpr size_t OFF_RELEASE_MS = 284;  // u16 LE
static constexpr size_t OFF_THRESHOLD = 286;
static constexpr size_t OFF_LINKGROUP = 287;
static constexpr size_t OFF_NAME = 288;  // 8 bytes ASCII
static constexpr size_t NAME_LEN = 8;

// EQ region (offsets 0..79; 10 bands × 8 bytes each)
static constexpr size_t EQ_BAND_COUNT = 10;
static constexpr size_t EQ_BAND_STRIDE = 8;
// Each band:
//   [0..1]  freq Hz LE16
//   [2..3]  gain raw LE16  (dB = (raw - 600) / 10)
//   [4]     bandwidth byte (Q ≈ 256 / b4_byte)
//   [5..7]  zeros

// XOR checksum over [direction .. last_payload_byte] inclusive.
inline uint8_t xor_checksum(const uint8_t *data, size_t len) {
  uint8_t c = 0;
  for (size_t i = 0; i < len; i++)
    c ^= data[i];
  return c;
}

// Build a single-frame 64-byte HID report.
//
// `data` may be up to 48 bytes (FRAME_SIZE - HEADER_SIZE - 2 for chk + end).
// On overflow returns false and leaves `out` unmodified.
//
// `out` is always cleared to 64 bytes of zero before population.
inline bool build_frame(uint8_t *out, uint8_t direction, uint8_t seq, uint32_t cmd, uint8_t category,
                        const uint8_t *data, size_t data_len) {
  if (data_len > FRAME_SIZE - HEADER_SIZE - 2)
    return false;
  std::memset(out, 0, FRAME_SIZE);
  out[0] = FRAME_MAGIC0;
  out[1] = FRAME_MAGIC1;
  out[2] = FRAME_MAGIC2;
  out[3] = FRAME_MAGIC3;
  out[4] = direction;
  out[5] = PROTO_VERSION;
  out[6] = seq;
  out[7] = category;
  out[8] = static_cast<uint8_t>(cmd & 0xFF);
  out[9] = static_cast<uint8_t>((cmd >> 8) & 0xFF);
  out[10] = static_cast<uint8_t>((cmd >> 16) & 0xFF);
  out[11] = static_cast<uint8_t>((cmd >> 24) & 0xFF);
  out[12] = static_cast<uint8_t>(data_len & 0xFF);
  out[13] = static_cast<uint8_t>((data_len >> 8) & 0xFF);
  if (data != nullptr && data_len > 0)
    std::memcpy(out + HEADER_SIZE, data, data_len);
  // XOR over [direction .. end-of-payload]
  uint8_t chk = xor_checksum(out + 4, HEADER_SIZE - 4 + data_len);
  out[HEADER_SIZE + data_len] = chk;
  out[HEADER_SIZE + data_len + 1] = END_MARKER;
  return true;
}

// Build a sequence of 64-byte HID reports for a multi-frame logical
// payload (when payload_len > 48). Mirrors dsp408-py's
// build_frames_multi() byte-exactly:
//
//   First frame:  header[14] + first_50_bytes_of_payload (no chk/end)
//   Continuation: 64-byte raw payload chunks
//   LAST cont:    payload + chk + END_MARKER + zero-pad
//
// The chk covers (header[4..14] = 10 bytes) + (full payload bytes).
//
// `out` is a flat buffer that receives FRAME_SIZE * num_frames bytes.
// Returns the number of frames produced (0 on failure). `out_capacity`
// is the size of the output buffer in bytes; we won't write past it.
//
// For the only multi-frame WRITE we currently use (full channel state,
// 296 bytes), this produces 5 frames = 320 bytes total.
inline size_t build_frames_multi(uint8_t *out, size_t out_capacity, uint8_t direction, uint8_t seq, uint32_t cmd,
                                 uint8_t category, const uint8_t *payload, size_t payload_len) {
  // Single-frame fast path
  if (payload_len <= FRAME_SIZE - HEADER_SIZE - 2) {
    if (out_capacity < FRAME_SIZE)
      return 0;
    if (!build_frame(out, direction, seq, cmd, category, payload, payload_len))
      return 0;
    return 1;
  }

  // Multi-frame layout
  static constexpr size_t MAX_IN_FIRST_MULTI = FRAME_SIZE - HEADER_SIZE;  // 50
  size_t total_frames = 1;                                                // first frame
  // Continuation frame count: ceil((payload_len - 50) / 64)
  size_t rest = payload_len - MAX_IN_FIRST_MULTI;
  total_frames += (rest + FRAME_SIZE - 1) / FRAME_SIZE;
  // Last cont's chk + end may overflow into one more frame if the
  // last cont was full (rare with our blob sizes; safer to budget).
  size_t bytes_in_last_cont = rest % FRAME_SIZE;
  if (bytes_in_last_cont == 0)
    bytes_in_last_cont = FRAME_SIZE;
  if (bytes_in_last_cont > FRAME_SIZE - 2) {
    // Need a spill frame for chk + end
    total_frames += 1;
  }
  if (out_capacity < total_frames * FRAME_SIZE)
    return 0;

  // Compute chk over header[4..14] + full payload
  uint8_t chk = 0;
  // First frame header bytes [4..14] = direction, ver, seq, cat, cmd_le32, len_le16
  uint8_t hdr_chk_bytes[10] = {
      direction,
      PROTO_VERSION,
      seq,
      category,
      static_cast<uint8_t>(cmd & 0xFF),
      static_cast<uint8_t>((cmd >> 8) & 0xFF),
      static_cast<uint8_t>((cmd >> 16) & 0xFF),
      static_cast<uint8_t>((cmd >> 24) & 0xFF),
      static_cast<uint8_t>(payload_len & 0xFF),
      static_cast<uint8_t>((payload_len >> 8) & 0xFF),
  };
  chk = xor_checksum(hdr_chk_bytes, sizeof(hdr_chk_bytes));
  chk ^= xor_checksum(payload, payload_len);

  // First frame
  uint8_t *p = out;
  std::memset(p, 0, FRAME_SIZE);
  p[0] = FRAME_MAGIC0;
  p[1] = FRAME_MAGIC1;
  p[2] = FRAME_MAGIC2;
  p[3] = FRAME_MAGIC3;
  std::memcpy(p + 4, hdr_chk_bytes, sizeof(hdr_chk_bytes));
  std::memcpy(p + HEADER_SIZE, payload, MAX_IN_FIRST_MULTI);
  size_t produced_payload = MAX_IN_FIRST_MULTI;
  p += FRAME_SIZE;

  // Continuation frames
  while (produced_payload < payload_len) {
    std::memset(p, 0, FRAME_SIZE);
    size_t remaining = payload_len - produced_payload;
    size_t take = remaining > FRAME_SIZE ? FRAME_SIZE : remaining;
    std::memcpy(p, payload + produced_payload, take);
    produced_payload += take;
    if (produced_payload == payload_len) {
      // This is the last cont frame — append chk + END_MARKER if room
      if (take + 2 <= FRAME_SIZE) {
        p[take] = chk;
        p[take + 1] = END_MARKER;
      } else {
        // Spill chk+end into the next frame
        p += FRAME_SIZE;
        std::memset(p, 0, FRAME_SIZE);
        p[0] = chk;
        p[1] = END_MARKER;
      }
    }
    p += FRAME_SIZE;
  }
  return total_frames;
}

// Parsed view of an inbound 64-byte frame. `payload` points into the
// caller-owned report buffer — copy it if you need to keep it past the
// transfer callback's lifetime.
struct ParsedFrame {
  bool valid;  // false if not a DSP-408 frame (magic mismatch / truncated)
  uint8_t direction;
  uint8_t seq;
  uint8_t category;
  uint32_t cmd;
  uint16_t payload_len;  // declared length; may exceed bytes_in_this_frame for multi-frame
  const uint8_t *payload;
  uint16_t payload_bytes_in_frame;  // bytes actually present in THIS 64-byte frame
  uint8_t checksum;
  bool checksum_ok;
  bool is_multi_frame_first;  // true if declared length > 48 bytes
};

inline ParsedFrame parse_frame(const uint8_t *raw, size_t raw_len) {
  ParsedFrame f{};
  f.valid = false;
  if (raw_len < HEADER_SIZE + 2)
    return f;
  if (raw[0] != FRAME_MAGIC0 || raw[1] != FRAME_MAGIC1 || raw[2] != FRAME_MAGIC2 || raw[3] != FRAME_MAGIC3)
    return f;
  if (raw[5] != PROTO_VERSION)
    return f;
  f.direction = raw[4];
  f.seq = raw[6];
  f.category = raw[7];
  f.cmd = static_cast<uint32_t>(raw[8]) | (static_cast<uint32_t>(raw[9]) << 8) |
          (static_cast<uint32_t>(raw[10]) << 16) | (static_cast<uint32_t>(raw[11]) << 24);
  f.payload_len = static_cast<uint16_t>(raw[12]) | (static_cast<uint16_t>(raw[13]) << 8);

  // Single-frame: header(14) + payload(<=48) + chk + end + zero-pad
  // Multi-frame first: header(14) + payload(50) — no chk/end here.
  size_t max_in_frame;
  size_t max_available;
  if (f.payload_len > FRAME_SIZE - HEADER_SIZE - 2) {
    f.is_multi_frame_first = true;
    max_in_frame = FRAME_SIZE - HEADER_SIZE;  // 50
    max_available = raw_len - HEADER_SIZE;
  } else {
    f.is_multi_frame_first = false;
    max_in_frame = FRAME_SIZE - HEADER_SIZE - 2;  // 48
    max_available = raw_len > HEADER_SIZE + 2 ? raw_len - HEADER_SIZE - 2 : 0;
  }
  size_t present = f.payload_len;
  if (present > max_in_frame)
    present = max_in_frame;
  if (present > max_available)
    present = max_available;
  f.payload = raw + HEADER_SIZE;
  f.payload_bytes_in_frame = static_cast<uint16_t>(present);

  if (!f.is_multi_frame_first) {
    size_t chk_pos = HEADER_SIZE + present;
    if (chk_pos < raw_len) {
      f.checksum = raw[chk_pos];
      f.checksum_ok = (xor_checksum(raw + 4, chk_pos - 4) == f.checksum);
    } else {
      f.checksum = 0;
      f.checksum_ok = false;
    }
  } else {
    f.checksum = 0;
    f.checksum_ok = false;  // validated only after multi-frame reassembly
  }

  f.valid = true;
  return f;
}

}  // namespace dsp408
}  // namespace esphome
