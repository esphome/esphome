#pragma once
// Fake HLK-FM22x face module on a UART cross-wired to the hlk_fm22x component's UART (115200 8N1).
// Frames: EF AA type len_hi len_lo data... xor(type..data). Replies are type 0x00 [cmd, result, ...],
// notes type 0x01 [note id, ...]. Runs in its own FreeRTOS task; the YAML scenario flips the public fields.
#include <atomic>
#include <cstdio>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

namespace fm22sim {
static const char *const TAG = "fm22sim";

enum ScanMode : int {
  SCAN_MATCH = 0,       // face 0 "Admin", admin
  SCAN_REJECT = 1,      // result 1
  SCAN_TIMEOUT = 2,     // result 0x0D after the requested timeout (capped by scan_ms)
  SCAN_SILENT = 3,      // never answers
  SCAN_MATCH_PLAIN = 4  // face 1 "Alice", not admin
};

struct Face {
  bool used;
  uint8_t admin;
  char name[32];
};

struct Module {
  esphome::uart::UARTComponent *u{nullptr};
  // Scenario knobs
  std::atomic<bool> alive{true};  // false: swallow everything, answer nothing
  std::atomic<int> scan_mode{SCAN_MATCH};
  std::atomic<int> scan_ms{800};         // time a scan or enrollment step takes
  std::atomic<bool> reset_aborts{true};  // RESET during a scan/enroll answers it with ABORTED first
  std::atomic<bool> known_face{false};   // ENROLL_ITG with allow_duplicate 0 is refused
  std::atomic<int> delete_ms{300};
  std::atomic<int> inject{0};  // 1 bad checksum, 2 garbage, 3 partial frame, 4 eye note, 5 unknown note,
                               // 6 ready note (restart), 7 restart after 1500 ms of silence
  // Counters for the report
  std::atomic<int> rx_frames{0}, rx_bad{0};

  Face faces[100]{};
  uint8_t dir_mask{0};
  // Running long command
  uint8_t busy_cmd{0};
  uint32_t busy_until{0};
  uint32_t note_at{0};
  int note_step{0};
  uint8_t enroll_dir{0}, enroll_admin{0};
  char enroll_name[32]{};
  // Delayed short reply
  uint8_t late_cmd{0};
  uint32_t late_at{0};
  uint8_t late_buf[40];
  size_t late_len{0};
  uint32_t silent_until{0};
  bool ready_pending{false};

  // RX parser
  uint8_t rx[300];
  size_t rxn{0};
  uint32_t rx_last{0};

  void init_faces() {
    memset(faces, 0, sizeof(faces));
    set_face(0, "Admin", 1);
    set_face(1, "Alice", 0);
    set_face(2, "Bob", 0);
  }
  void set_face(int id, const char *name, uint8_t admin) {
    faces[id].used = true;
    faces[id].admin = admin;
    memset(faces[id].name, 0, 32);
    strncpy(faces[id].name, name, 31);
  }
  int count() {
    int n = 0;
    for (auto &f : faces)
      n += f.used;
    return n;
  }
  int free_id() {
    for (int i = 0; i < 100; i++)
      if (!faces[i].used)
        return i;
    return -1;
  }

  void frame(uint8_t type, const uint8_t *d, size_t n, bool corrupt = false) {
    uint8_t h[5] = {0xEF, 0xAA, type, (uint8_t) (n >> 8), (uint8_t) n};
    uint8_t ck = type ^ h[3] ^ h[4];
    for (size_t i = 0; i < n; i++)
      ck ^= d[i];
    if (corrupt)
      ck ^= 0x5A;
    u->write_array(h, 5);
    if (n)
      u->write_array(d, n);
    u->write_byte(ck);
    u->flush();
  }
  void reply(uint8_t cmd, uint8_t result, const uint8_t *d = nullptr, size_t n = 0) {
    uint8_t b[260];
    b[0] = cmd;
    b[1] = result;
    if (n)
      memcpy(b + 2, d, n);
    ESP_LOGI(TAG, "MOD TX reply cmd 0x%02X result %u len %u", cmd, result, (unsigned) (n + 2));
    frame(0x00, b, n + 2);
  }
  void note(const uint8_t *d, size_t n) { frame(0x01, d, n); }
  void face_state_note(int16_t state) {
    uint8_t d[17];
    d[0] = 0x01;
    int16_t v[8] = {state, 120, 80, 360, 400, -5, 3, 1};
    for (int i = 0; i < 8; i++) {
      d[1 + 2 * i] = v[i] & 0xFF;  // little endian
      d[2 + 2 * i] = (uint16_t) v[i] >> 8;
    }
    ESP_LOGI(TAG, "MOD TX note face state %d", state);
    note(d, sizeof(d));
  }
  void record_reply(uint8_t cmd, int id) {
    uint8_t d[36];
    d[0] = id >> 8;
    d[1] = id;
    memcpy(d + 2, faces[id].name, 32);
    d[34] = faces[id].admin;
    d[35] = 0;  // unlock status
    reply(cmd, 0, d, sizeof(d));
  }

  // Finish the running scan/enroll with a result.
  void finish_busy(uint8_t result) {
    uint8_t cmd = busy_cmd;
    busy_cmd = 0;
    if (cmd == 0x12) {
      if (result != 0) {
        reply(cmd, result);
        return;
      }
      int id = scan_mode == SCAN_MATCH_PLAIN ? 1 : 0;
      record_reply(cmd, id);
      return;
    }
    // enrollments
    if (result != 0) {
      reply(cmd, result);
      return;
    }
    if (cmd == 0x13) {
      dir_mask |= enroll_dir;
      if (dir_mask != 0x1F) {
        uint8_t d[3] = {0xFF, 0xFF, dir_mask};
        reply(cmd, 0, d, 3);
        return;
      }
    }
    int id = free_id();
    set_face(id, enroll_name, enroll_admin);
    dir_mask = 0;
    uint8_t d[3] = {(uint8_t) (id >> 8), (uint8_t) id, 0x1F};
    reply(cmd, 0, d, 3);
  }

  void handle(uint8_t cmd, const uint8_t *d, size_t n) {
    char hx[3 * 12 + 1];
    size_t k = 0;
    for (size_t i = 0; i < n && i < 12; i++)
      k += snprintf(hx + k, sizeof(hx) - k, "%02X ", d[i]);
    hx[k] = 0;
    ESP_LOGI(TAG, "MOD RX cmd 0x%02X len %u: %s", cmd, (unsigned) n, hx);
    rx_frames++;
    if (!alive || esphome::millis() < silent_until) {
      ESP_LOGI(TAG, "MOD (not answering)");
      return;
    }
    uint32_t now = esphome::millis();
    switch (cmd) {
      case 0x11: {  // GET_STATUS
        uint8_t s = busy_cmd ? 1 : 0;
        reply(cmd, 0, &s, 1);
        break;
      }
      case 0x30: {  // GET_VERSION, NUL padded
        uint8_t v[32] = {0};
        strcpy((char *) v, "FM225_V3.0.4_SIM");
        reply(cmd, 0, v, 32);
        break;
      }
      case 0x93: {  // GET_SN: 29 chars then junk, as seen on a real module
        uint8_t v[32];
        memcpy(v, "HLKFM225SIM0123456789ABCDEFGH", 29);
        v[29] = 0xFF;
        v[30] = 0xA5;
        v[31] = 0x08;
        reply(cmd, 0, v, 32);
        break;
      }
      case 0x24: {  // GET_ALL_USERID: count + 100 ids = 201 data bytes (203 with cmd/result)
        uint8_t v[201] = {0};
        int c = 0;
        for (int i = 0; i < 100; i++)
          if (faces[i].used) {
            v[1 + 2 * c] = 0;
            v[2 + 2 * c] = i;
            c++;
          }
        v[0] = c;
        reply(cmd, 0, v, sizeof(v));
        break;
      }
      case 0x12:  // VERIFY
        busy_cmd = cmd;
        note_step = 0;
        note_at = now + 100;
        if (scan_mode == SCAN_TIMEOUT)
          busy_until = now + std::min<int>(n >= 2 ? d[1] * 1000 : 10000, scan_ms);
        else
          busy_until = now + scan_ms;
        break;
      case 0x13:  // ENROLL: admin, name[32], direction, timeout
      case 0x1D:  // ENROLL_SINGLE
      case 0x26:  // ENROLL_ITG: admin, name[32], direction, type, allow_duplicate, timeout, 3 reserved
        if (n < 35) {
          reply(cmd, 6);
          break;
        }
        enroll_admin = d[0];
        memcpy(enroll_name, d + 1, 32);
        enroll_dir = d[33];
        if (cmd == 0x26 && n >= 36 && d[35] == 0 && known_face) {
          busy_cmd = cmd;
          busy_until = now + 300;
          note_step = 99;
          ESP_LOGI(TAG, "MOD will refuse duplicate face");
          break;
        }
        busy_cmd = cmd;
        note_step = 0;
        note_at = now + 100;
        busy_until = now + scan_ms;
        break;
      case 0x20: {  // DELETE_FACE
        int id = n >= 2 ? (d[0] << 8 | d[1]) : -1;
        late_cmd = cmd;
        late_at = now + delete_ms;
        late_len = 0;
        if (id >= 0 && id < 100 && faces[id].used) {
          faces[id].used = false;
          late_buf[0] = 0;
        } else {
          late_buf[0] = 8;
        }
        break;
      }
      case 0x21:
        for (auto &f : faces)
          f.used = false;
        late_cmd = cmd;
        late_at = now + delete_ms;
        late_buf[0] = 0;
        late_len = 0;
        break;
      case 0x22: {  // GET_USER_INFO
        int id = n >= 2 ? (d[0] << 8 | d[1]) : -1;
        if (id >= 0 && id < 100 && faces[id].used)
          record_reply(cmd, id);
        else
          reply(cmd, 8);
        break;
      }
      case 0x23:  // FACE_RESET
        dir_mask = 0;
        reply(cmd, 0);
        break;
      case 0x10:  // RESET
        if (busy_cmd) {
          if (reset_aborts)
            finish_busy(2);
          else
            busy_cmd = 0;
        }
        reply(cmd, 0);
        break;
      default:
        reply(cmd, 6);
        break;
    }
  }

  void parse(uint8_t b) {
    uint32_t now = esphome::millis();
    if (rxn > 0 && now - rx_last > 100)
      rxn = 0;
    rx_last = now;
    if (rxn == 0 && b != 0xEF)
      return;
    if (rxn == 1 && b != 0xAA) {
      rxn = b == 0xEF ? 1 : 0;
      return;
    }
    rx[rxn++] = b;
    if (rxn >= 5) {
      size_t len = (rx[3] << 8) | rx[4];
      if (len > sizeof(rx) - 6) {
        rxn = 0;
        return;
      }
      if (rxn == 5 + len + 1) {
        uint8_t ck = 0;
        for (size_t i = 2; i < 5 + len; i++)
          ck ^= rx[i];
        if (ck == rx[5 + len])
          handle(rx[2], rx + 5, len);
        else {
          rx_bad++;
          ESP_LOGW(TAG, "MOD RX bad checksum");
        }
        rxn = 0;
      }
    }
  }

  void tick() {
    uint8_t b;
    while (u->available()) {
      u->read_byte(&b);
      parse(b);
    }
    uint32_t now = esphome::millis();
    int inj = inject.exchange(0);
    if (inj == 1) {
      uint8_t d[3] = {0x11, 0, 0};
      ESP_LOGI(TAG, "MOD TX bad checksum frame");
      frame(0x00, d, 3, true);
    } else if (inj == 2) {
      const uint8_t g[] = {0x00, 0x12, 0xEF, 0x55, 0xAA, 0xFF, 0x00};
      ESP_LOGI(TAG, "MOD TX garbage");
      u->write_array(g, sizeof(g));
    } else if (inj == 3) {
      const uint8_t p[] = {0xEF, 0xAA, 0x00, 0x00, 0x26, 0x12, 0x00};
      ESP_LOGI(TAG, "MOD TX partial frame");
      u->write_array(p, sizeof(p));
    } else if (inj == 4) {
      uint8_t d[2] = {0x04, 0x01};
      ESP_LOGI(TAG, "MOD TX eye note");
      note(d, 2);
    } else if (inj == 5) {
      uint8_t d[2] = {0x07, 0x00};
      ESP_LOGI(TAG, "MOD TX unknown note");
      note(d, 2);
    } else if (inj == 6 || inj == 7) {
      ESP_LOGI(TAG, "MOD restart%s", inj == 7 ? " (1500 ms dark)" : "");
      busy_cmd = 0;
      late_cmd = 0;
      dir_mask = 0;
      silent_until = now + (inj == 7 ? 1500 : 50);
      ready_pending = true;
    }
    if (ready_pending && now >= silent_until) {
      ready_pending = false;
      uint8_t d[1] = {0x00};
      ESP_LOGI(TAG, "MOD TX note ready");
      note(d, 1);
    }
    if (!alive)
      return;
    if (late_cmd && now >= late_at) {
      uint8_t c = late_cmd;
      late_cmd = 0;
      reply(c, late_buf[0]);
    }
    if (busy_cmd) {
      if (note_step < 3 && now >= note_at) {
        static const int16_t STATES[3] = {1, 6, 0};  // no face, too far away, face detected
        face_state_note(STATES[note_step++]);
        note_at = now + 150;
      }
      if (now >= busy_until) {
        uint8_t result = 0;
        if (note_step == 99)
          result = 0x0A;
        else if (busy_cmd == 0x12 && scan_mode == SCAN_REJECT)
          result = 1;
        else if (busy_cmd == 0x12 && scan_mode == SCAN_TIMEOUT)
          result = 0x0D;
        if (busy_cmd == 0x12 && scan_mode == SCAN_SILENT)
          busy_cmd = 0;
        else
          finish_busy(result);
      }
    }
  }

  static void task(void *arg) {
    auto *m = static_cast<Module *>(arg);
    while (true) {
      m->tick();
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
  void start(esphome::uart::UARTComponent *uart, bool send_ready) {
    u = uart;
    init_faces();
    if (send_ready)
      ready_pending = true;
    xTaskCreatePinnedToCore(task, "fm22sim", 6144, this, 5, nullptr, 0);
  }
};

inline Module mod;
}  // namespace fm22sim
