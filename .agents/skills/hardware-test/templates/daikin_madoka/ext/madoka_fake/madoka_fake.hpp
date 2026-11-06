#pragma once
#include <esp_gap_ble_api.h>
#include <cstring>
#include <vector>
#include "esphome/components/esp32_ble_server/ble_characteristic.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::madoka_fake {

static const char *const TAG = "madoka_fake";

inline const char *hex(const std::vector<uint8_t> &v) {
  static char buf[3 * 64 + 1];
  size_t o = 0;
  for (size_t i = 0; i < v.size() && o + 4 < sizeof(buf); i++)
    o += snprintf(buf + o, sizeof(buf) - o, "%02X ", v[i]);
  buf[o] = 0;
  return buf;
}

// Fault injection, chosen at run time
enum Fault : uint8_t {
  FAULT_NONE = 0,
  FAULT_DELAY_100,
  FAULT_DELAY_300,
  FAULT_FF_RECORD,      // an empty record (size 0xFF) ahead of the wanted records in every GET reply
  FAULT_OUT_OF_ORDER,   // the second chunk of a multi-chunk reply is sent before the first
  FAULT_DROP_SENSOR,    // no reply to GetSensorInformation
  FAULT_DROP_SETPOINT,  // no reply to GetSetpoint
  FAULT_SLOW_1S,        // every reply 1 s late
  FAULT_TAIL_LATE,      // chunks 2 on of a multi-chunk reply go out 120 ms late; later replies wait behind them
};
static constexpr uint32_t TAIL_DELAY_MS = 120;

class MadokaFake : public Component {
 public:
  void set_chars(esp32_ble_server::BLECharacteristic *nfy) { this->nfy_ = nfy; }
  void set_fault(uint8_t f) {
    this->fault_ = f;
    ESP_LOGI(TAG, "FAULT %u", f);
  }

  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
      case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(TAG, "PAIR security request, accepting");
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
      case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGI(TAG, "PAIR numeric comparison %06" PRIu32 ", confirming", param->ble_security.key_notif.passkey);
        esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
        break;
      case ESP_GAP_BLE_AUTH_CMPL_EVT:
        ESP_LOGI(TAG, "PAIR auth complete success=%d mode=0x%x fail=0x%x", param->ble_security.auth_cmpl.success,
                 param->ble_security.auth_cmpl.auth_mode, param->ble_security.auth_cmpl.fail_reason);
        break;
      default:
        break;
    }
  }

  // One write on the e112 characteristic: chunk id, then up to 19 message bytes
  void on_chunk(const std::vector<uint8_t> &chunk) {
    if (chunk.size() < 2)
      return;
    if (chunk[0] == 0)
      this->rx_.clear();
    this->rx_.insert(this->rx_.end(), chunk.begin() + 1, chunk.end());
    if (this->rx_.empty() || this->rx_.size() < this->rx_[0])
      return;
    std::vector<uint8_t> msg = this->rx_;
    this->rx_.clear();
    ESP_LOGI(TAG, "RX %s", hex(msg));
    this->handle_(msg);
  }

  // Device state, also readable from lambdas for the report
  uint8_t power{1}, mode{3}, fan_cool{3}, fan_heat{1}, indoor{22};
  uint16_t sp_cool{24 * 128}, sp_heat{20 * 128};
  uint32_t received{0}, replied{0};

 protected:
  static void rec1(std::vector<uint8_t> &m, uint8_t id, uint8_t v) { m.insert(m.end(), {id, 1, v}); }
  static void rec2(std::vector<uint8_t> &m, uint8_t id, uint16_t v) {
    m.insert(m.end(), {id, 2, (uint8_t) (v >> 8), (uint8_t) v});
  }
  // Value of record `id` in a request; nullptr when absent
  static const uint8_t *arg(const std::vector<uint8_t> &msg, uint8_t id, uint8_t *len) {
    size_t i = 4;
    while (i + 2 <= msg.size()) {
      uint8_t rid = msg[i], l = msg[i + 1];
      if (i + 2 + l > msg.size())
        return nullptr;
      if (rid == id) {
        *len = l;
        return &msg[i + 2];
      }
      i += 2 + l;
    }
    return nullptr;
  }

  void handle_(const std::vector<uint8_t> &msg) {
    this->received++;
    if (msg.size() < 4)
      return;
    const uint16_t cmd = (msg[2] << 8) | msg[3];
    std::vector<uint8_t> r = {0, 0, msg[2], msg[3]};
    if (this->fault_ == FAULT_FF_RECORD && (cmd & 0x4000) == 0)
      r.insert(r.end(), {0x1E, 0xFF});
    uint8_t l = 0;
    const uint8_t *v;
    switch (cmd) {
      case 0x0020:
        rec1(r, 0x20, this->power);
        break;
      case 0x0030:
        rec1(r, 0x1F, 0);
        rec1(r, 0x20, this->mode);
        break;
      case 0x0040:
        rec2(r, 0x20, this->sp_cool);
        rec2(r, 0x21, this->sp_heat);
        rec1(r, 0x30, 1);
        rec1(r, 0x31, 0);
        rec1(r, 0x32, 2);
        rec1(r, 0xA0, 18);
        rec1(r, 0xA1, 16);
        rec2(r, 0xA2, 18 * 128);
        rec2(r, 0xA3, 16 * 128);
        rec1(r, 0xA4, 0);
        rec1(r, 0xA5, 0);
        rec1(r, 0xB0, 32);
        rec1(r, 0xB1, 30);
        rec2(r, 0xB2, 32 * 128);
        rec2(r, 0xB3, 30 * 128);
        rec1(r, 0xB4, 0);
        rec1(r, 0xB5, 0);
        break;
      case 0x0050:
        rec1(r, 0x20, this->fan_cool);
        rec1(r, 0x21, this->fan_heat);
        break;
      case 0x0110:
        rec1(r, 0x40, this->indoor);
        rec2(r, 0x41, 0xFFFF);
        break;
      case 0x4020:
        if ((v = arg(msg, 0x20, &l)) && l == 1)
          this->power = v[0];
        r.insert(r.end(), {0x00, 0x00});
        break;
      case 0x4030:
        if ((v = arg(msg, 0x20, &l)) && l == 1)
          this->mode = v[0];
        r.insert(r.end(), {0x00, 0x00});
        break;
      case 0x4040:
        if ((v = arg(msg, 0x20, &l)) && l == 2)
          this->sp_cool = (v[0] << 8) | v[1];
        if ((v = arg(msg, 0x21, &l)) && l == 2)
          this->sp_heat = (v[0] << 8) | v[1];
        r.insert(r.end(), {0x00, 0x00});
        break;
      case 0x4050:
        if ((v = arg(msg, 0x20, &l)) && l == 1)
          this->fan_cool = v[0];
        if ((v = arg(msg, 0x21, &l)) && l == 1)
          this->fan_heat = v[0];
        r.insert(r.end(), {0x00, 0x00});
        break;
      default:
        r.insert(r.end(), {0x00, 0x00});
        break;
    }
    ESP_LOGI(TAG, "STATE power=%u mode=%u cool=%.2f heat=%.2f fan_cool=%u fan_heat=%u", this->power, this->mode,
             this->sp_cool / 128.0f, this->sp_heat / 128.0f, this->fan_cool, this->fan_heat);
    if ((this->fault_ == FAULT_DROP_SENSOR && cmd == 0x0110) ||
        (this->fault_ == FAULT_DROP_SETPOINT && cmd == 0x0040)) {
      ESP_LOGI(TAG, "DROP reply to 0x%04X", cmd);
      return;
    }
    r[0] = r.size();
    uint32_t delay_ms = this->fault_ == FAULT_DELAY_100   ? 100
                        : this->fault_ == FAULT_DELAY_300 ? 300
                        : this->fault_ == FAULT_SLOW_1S   ? 1000
                                                          : 0;
    if (delay_ms == 0) {
      this->send_(r);
    } else {
      this->set_timeout(delay_ms, [this, r]() { this->send_(r); });
    }
  }

  void send_(const std::vector<uint8_t> &msg) {
    // A device sends one message at a time, so a reply waits until a late tail is out
    if (this->tail_pending_) {
      this->held_.push_back(msg);
      ESP_LOGI(TAG, "HOLD reply 0x%02X%02X behind a late tail", msg[2], msg[3]);
      return;
    }
    std::vector<std::vector<uint8_t>> chunks;
    for (size_t i = 0, id = 0; i < msg.size(); i += 19, id++) {
      std::vector<uint8_t> c = {(uint8_t) id};
      c.insert(c.end(), msg.begin() + i, msg.begin() + std::min(msg.size(), i + 19));
      chunks.push_back(c);
    }
    if (this->fault_ == FAULT_OUT_OF_ORDER && chunks.size() > 1)
      std::swap(chunks[0], chunks[1]);
    size_t now = chunks.size();
    if (this->fault_ == FAULT_TAIL_LATE && chunks.size() > 2)
      now = 2;
    for (size_t i = 0; i < now; i++)
      this->notify_(chunks[i]);
    if (now < chunks.size()) {
      this->tail_pending_ = true;
      std::vector<std::vector<uint8_t>> tail(chunks.begin() + now, chunks.end());
      ESP_LOGI(TAG, "TAIL %u chunks of 0x%02X%02X deferred %u ms", (unsigned) tail.size(), msg[2], msg[3],
               (unsigned) TAIL_DELAY_MS);
      this->set_timeout(TAIL_DELAY_MS, [this, tail]() {
        for (auto &c : tail)
          this->notify_(c);
        ESP_LOGI(TAG, "TAIL sent");
        this->tail_pending_ = false;
        auto held = std::move(this->held_);
        this->held_.clear();
        for (auto &m : held)
          this->send_(m);
      });
    }
    this->replied++;
    ESP_LOGI(TAG, "TX %u chunks %s", (unsigned) chunks.size(), hex(msg));
  }

  void notify_(const std::vector<uint8_t> &c) {
    this->nfy_->set_value(std::vector<uint8_t>(c));
    this->nfy_->notify();
  }

  esp32_ble_server::BLECharacteristic *nfy_{nullptr};
  std::vector<uint8_t> rx_;
  std::vector<std::vector<uint8_t>> held_;
  bool tail_pending_{false};
  uint8_t fault_{FAULT_NONE};
};

}  // namespace esphome::madoka_fake
