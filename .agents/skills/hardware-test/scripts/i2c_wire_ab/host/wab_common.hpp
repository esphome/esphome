#pragma once
// Wire-level A/B harness for esphome#22780: every bus model emits the same token stream.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace wab {

// Token stream of the bus: "S", "Sr", "P", "AB+" (written byte, slave ACK), "AB-" (NACK), "rAB+"/"rAB-" (read byte,
// master ACK/NACK), "ABORT".
struct Wire {
  std::string text;
  bool open{false};
  void start() {
    text += open ? "Sr " : "S ";
    open = true;
  }
  void stop() {
    text += "P ";
    open = false;
  }
  void wbyte(uint8_t b, bool ack) {
    char t[8];
    snprintf(t, sizeof t, "%02X%c ", b, ack ? '+' : '-');
    text += t;
  }
  void rbyte(uint8_t b, bool ack) {
    char t[8];
    snprintf(t, sizeof t, "r%02X%c ", b, ack ? '+' : '-');
    text += t;
  }
  void note(const char *s) {
    text += s;
    text += ' ';
  }
  void clear() {
    text.clear();
    open = false;
  }
};
inline Wire &wire() {
  static Wire w;
  return w;
}
inline std::string &jobs_note() {
  static std::string s;
  return s;
}

// Slave model: present addresses ACK their address and up to ack_limit data bytes after it.
struct Slave {
  uint8_t present[4]{0x48, 0x1A, 0, 0};
  size_t ack_limit{SIZE_MAX};
  size_t written{0};  // data bytes written since the address byte
  size_t read_idx{0};
  bool is_present(uint8_t addr7) const {
    for (auto p : present)
      if (p != 0 && p == addr7)
        return true;
    return false;
  }
  // Called for the address byte; returns ACK
  bool address(uint8_t byte) {
    this->written = 0;
    this->read_idx = 0;
    return this->is_present(byte >> 1);
  }
  bool data(uint8_t) { return this->written++ < this->ack_limit; }
  uint8_t next_read() { return static_cast<uint8_t>(0xA0 + this->read_idx++); }
};
inline Slave &slave() {
  static Slave s;
  return s;
}

// Allocation counter (window opened around one bus call)
void alloc_window(bool on);
size_t alloc_count();

}  // namespace wab

// Member access to private fields of final classes (explicit instantiation ignores access checks)
template<typename Tag, typename Tag::type M> struct WabRob {
  friend typename Tag::type get(Tag) { return M; }
};
