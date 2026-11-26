#pragma once
// Frame builders for the LengthFrameReader injection harness (ld2410 data frames and tuya datapoint reports).
#include <cstdint>
#include <string>
#include <vector>
#include "esphome/core/log.h"

namespace lfh {

using Bytes = std::vector<uint8_t>;

inline std::string seen;  // values handled during the current case
inline void record(int v) { seen += std::to_string(v) + ","; }

inline void add(Bytes &out, std::initializer_list<uint8_t> b) { out.insert(out.end(), b); }
inline void add(Bytes &out, const Bytes &b) { out.insert(out.end(), b.begin(), b.end()); }

// LD2410 basic periodic frame: F4F3F2F1, len 0x0D, 02 AA, state, moving dist LE, moving energy, still dist LE,
// still energy, detect dist LE, 55 00, F8F7F6F5. Bytes 9..12 can be overridden to plant marker bytes.
inline Bytes ld_raw(uint8_t b9, uint8_t b10, uint8_t b11, uint8_t b12, bool bad_footer = false,
                    bool bad_check = false) {
  return Bytes{0xF4,
               0xF3,
               0xF2,
               0xF1,
               0x0D,
               0x00,
               0x02,
               0xAA,
               0x01,
               b9,
               b10,
               b11,
               b12,
               0x00,
               0x20,
               0x2C,
               0x01,
               0x55,
               static_cast<uint8_t>(bad_check ? 0x07 : 0x00),
               0xF8,
               0xF7,
               0xF6,
               static_cast<uint8_t>(bad_footer ? 0x00 : 0xF5)};
}
inline Bytes ld(uint16_t v, bool bad_footer = false, bool bad_check = false) {
  return ld_raw(v & 0xFF, v >> 8, 0x30, 0x40, bad_footer, bad_check);
}

// Tuya datapoint report (cmd 07, version 03) of dp 101 as a 4 byte integer, sum8 checksum
inline Bytes tu(uint32_t v, bool bad_ck = false) {
  Bytes f{0x55,
          0xAA,
          0x03,
          0x07,
          0x00,
          0x08,
          0x65,
          0x02,
          0x00,
          0x04,
          static_cast<uint8_t>(v >> 24),
          static_cast<uint8_t>(v >> 16),
          static_cast<uint8_t>(v >> 8),
          static_cast<uint8_t>(v)};
  uint8_t s = 0;
  for (auto b : f)
    s += b;
  f.push_back(bad_ck ? static_cast<uint8_t>(s + 1) : s);
  return f;
}

inline Bytes head(size_t n, const Bytes &b) { return Bytes(b.begin(), b.begin() + n); }

// ---- ld2410 cases ----
inline Bytes ld_a() {  // noise between frames
  Bytes o;
  add(o, {0x00, 0x11, 0xF4, 0xF3, 0x22, 0xFD, 0xFC, 0x33, 0x55, 0xAA, 0xF8, 0xF7, 0xF6, 0xF5, 0x04, 0x03, 0x02, 0x01});
  add(o, ld(101));
  add(o, {0xF4});
  add(o, ld(102));
  add(o, {0xFD, 0xFC, 0xFB});
  add(o, ld(103));
  add(o, {0xF4, 0xF3, 0xF2, 0xF1, 0xF4, 0xF3});  // broken header whose length bytes are header bytes
  add(o, ld(104));
  add(o, {0x00, 0x00, 0x00});
  add(o, ld(105));
  return o;
}
inline Bytes ld_b() {  // rejected frame, good frame right behind it
  Bytes o;
  add(o, ld(111, true));  // bad footer
  add(o, ld(112));
  add(o, ld(113, false, true));  // bad check byte (the handler drops it)
  add(o, ld(114));
  add(o, head(12, ld(115)));  // cut off frame
  add(o, ld(116));
  add(o, {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xA0, 0x01, 0x00, 0x00, 0x04, 0x03, 0x02, 0x00});  // cmd, bad footer
  add(o, ld(117));
  return o;
}
inline Bytes ld_c1() {  // length 200 then two frames (then silence, then one more frame)
  Bytes o;
  add(o, {0xF4, 0xF3, 0xF2, 0xF1, 0xC8, 0x00});
  add(o, ld(121));
  add(o, ld(122));
  return o;
}
inline Bytes ld_c2() {  // length 28 (38 byte frame, fits the buffer) then two frames
  Bytes o;
  add(o, {0xF4, 0xF3, 0xF2, 0xF1, 0x1C, 0x00});
  add(o, ld(124));
  add(o, ld(125));
  return o;
}
inline Bytes ld_c3() {  // length 31 (41 bytes) then one frame: the bad frame is still open when the line goes quiet
  Bytes o;
  add(o, {0xF4, 0xF3, 0xF2, 0xF1, 0x1F, 0x00});
  add(o, ld(127));
  return o;
}
inline Bytes ld_d() {  // good frames whose payload holds footer and header bytes
  Bytes o;
  add(o, ld_raw(0xF8, 0xF7, 0xF6, 0xF5));  // data footer inside: moving 0xF7F8 = 63480
  add(o, ld_raw(0x04, 0x03, 0x02, 0x01));  // command footer inside: moving 0x0304 = 772
  add(o, ld_raw(0xF4, 0xF3, 0xF2, 0xF1));  // data header inside: moving 0xF3F4 = 62452
  add(o, ld_raw(0xFD, 0xFC, 0xFB, 0xFA));  // command header inside: moving 0xFCFD = 64765
  add(o, ld(131));
  return o;
}

// ---- tuya cases ----
inline Bytes tu_a() {
  Bytes o;
  add(o, {0x00, 0x11, 0xAA, 0x55, 0x00});
  add(o, tu(201));
  add(o, {0x55});  // lone first header byte right before a frame
  add(o, tu(202));
  add(o, {0x55, 0xAA});  // whole header right before a frame: its version/cmd/length come from the frame
  add(o, tu(203));
  add(o, {0x00, 0x00, 0x00});
  add(o, tu(204));
  return o;
}
inline Bytes tu_b() {
  Bytes o;
  add(o, tu(211, true));  // bad checksum
  add(o, tu(212));
  add(o, head(8, tu(213)));  // cut off frame
  add(o, tu(214));
  return o;
}
inline Bytes tu_c1() {  // length 200 (fits 255) then two frames, then silence
  Bytes o;
  add(o, {0x55, 0xAA, 0x03, 0x07, 0x00, 0xC8});
  add(o, tu(221));
  add(o, tu(222));
  return o;
}
inline Bytes tu_c2() {  // length 1024 then two frames, then silence
  Bytes o;
  add(o, {0x55, 0xAA, 0x03, 0x07, 0x04, 0x00});
  add(o, tu(224));
  add(o, tu(225));
  return o;
}
inline Bytes tu_d() {  // payload holds a header
  Bytes o;
  add(o, tu(0x55AA0307));  // 1437205255
  add(o, tu(0x0055AA03));  // 5614083
  add(o, tu(231));
  return o;
}

// One step of a case script: write the bytes, wait, then log the values seen as CASE <name>
struct Step {
  const char *name;
  Bytes (*make)();
  uint32_t wait_ms;
};
inline const std::vector<Step> LD_STEPS{
    {"LD A", ld_a, 1500},         {"LD B", ld_b, 1500},
    {"LD C1-quiet", ld_c1, 1000}, {"LD C1-next", [] { return ld(123); }, 1500},
    {"LD C2-quiet", ld_c2, 1000}, {"LD C2-next", [] { return ld(126); }, 1500},
    {"LD C3-quiet", ld_c3, 1000}, {"LD C3-next", [] { return ld(128); }, 1500},
    {"LD D", ld_d, 1500},
};
inline const std::vector<Step> TU_STEPS{
    {"TU A", tu_a, 1500},         {"TU B", tu_b, 1500},
    {"TU C1-quiet", tu_c1, 1000}, {"TU C1-next", [] { return tu(223); }, 1500},
    {"TU C2-quiet", tu_c2, 1000}, {"TU C2-next", [] { return tu(226); }, 1500},
    {"TU D", tu_d, 1500},
};
inline const std::vector<Step> &steps(bool tuya) { return tuya ? TU_STEPS : LD_STEPS; }

inline void report(const char *name) {
  ESP_LOGI("lfh", "CASE %s seen=%s", name, seen.c_str());
  seen.clear();
}

}  // namespace lfh
