#define ESP_LOGW(...) ((void) 0)
#define ESP_LOGI(...) ((void) 0)
#define ESP_LOGD(...) ((void) 0)
#define ESP_LOGV(...) ((void) 0)
#define ESP_LOGCONFIG(...) ((void) 0)
#define LOG_WATER_HEATER(...) ((void) 0)
#include "esphome/components/haier/water_heater/hon_water_heater.cpp"
#include <cassert>
using namespace esphome;
class Heater : public haier::HonWaterHeater {
 public:
  using HonWaterHeater::control;
  using HonWaterHeater::poll_status_;
  using HonWaterHeater::pending_off_;
  using HonWaterHeater::command_count_;
  using HonWaterHeater::waiting_control_;
  using HonWaterHeater::network_pending_;
  using HonWaterHeater::waiting_poll_;
  using WaterHeater::mode_;
  void status(bool powered, bool unknown = false, bool eco = false) {
    uint8_t core[61]{};
    core[0] = 60;
    core[7] = 2;
    core[8] = 0x6D;
    core[9] = 1;
    core[10] = 42;
    core[11] = 5;
    core[23] = 43;
    core[46] = 16;
    core[47] = powered ? 17 : 16;
    if (unknown || eco) {
      core[46] = 0;
      core[47] = powered ? 1 : 0;
    }
    if (eco)
      core[49] = 4;
    for (size_t i = 0; i < 60; i++)
      core[60] += core[i];
    parser_.feed(255);
    parser_.feed(255);
    for (auto b : core) {
      if (parser_.feed(b))
        process_frame_();
    }
  }
  bool last_is_off() {
    auto &b = sent.back();
    return b.size() == 15 && b[9] == 1 && b[10] == 0x5D && b[11] == 1 && b[13] == 0;
  }
};
int main() {
  // Combined Off and temperature must work before the first received status.
  {
    Heater h;
    water_heater::WaterHeaterCall c;
    c.mode = water_heater::WATER_HEATER_MODE_OFF;
    c.temperature = 40;
    h.control(c);
    h.loop();
    assert(h.last_is_off());
  }
  // Off survives timeout of an in-flight power-on and cancels the unsent BPS command.
  {
    Heater h;
    App.now = 100;
    h.status(false);
    water_heater::WaterHeaterCall c;
    c.mode = water_heater::WATER_HEATER_MODE_PERFORMANCE;
    h.control(c);
    h.loop();
    assert(h.command_count_ == 2);
    water_heater::WaterHeaterCall off;
    off.on = false;
    off.temperature = 999;
    h.control(off);
    assert(h.pending_off_ && h.command_count_ == 1);
    App.now = 5101;
    h.loop();
    assert(h.last_is_off() && !h.pending_off_);
    assert(h.sent.size() == 2);
  }
  // A valid reply also releases the current exchange and sends Off next.
  {
    Heater h;
    App.now = 100;
    h.status(false);
    water_heater::WaterHeaterCall c;
    c.mode = water_heater::WATER_HEATER_MODE_PERFORMANCE;
    h.control(c);
    h.loop();
    water_heater::WaterHeaterCall off;
    off.on = false;
    h.control(off);
    App.now = 200;
    h.status(true);
    h.loop();
    assert(h.last_is_off());
    assert(h.sent.size() == 2);
  }
  // Connectivity changes only on valid status and timeout, not repeated on every loop.
  {
    Heater h;
    binary_sensor::BinarySensor health;
    h.set_communication_sensor(&health);
    App.now = 100;
    h.setup();
    assert(!health.states.back() && h.warning);
    h.status(false);
    assert(health.states.back() && !h.warning);
    App.now = 20099;
    h.loop();
    assert(health.states.size() == 2);
    App.now = 20100;
    h.loop();
    assert(!health.states.back() && h.warning);
    h.loop();
    assert(health.states.size() == 3);
    h.status(false);
    assert(health.states.back() && !h.warning && health.states.size() == 4);
  }
  // A timed-out poll must release pending network updates, then polling resumes.
  {
    Heater h;
    App.now = 100;
    h.network_pending_ = false;
    h.poll_status_();
    assert(h.waiting_poll_);
    h.network_pending_ = true;
    App.now = 5099;
    h.loop();
    assert(h.sent.size() == 1);
    App.now = 5100;
    h.poll_status_();
    h.loop();
    assert(!h.waiting_poll_ && h.sent.size() == 2 && h.sent.back()[9] == 0xF7);
    App.now = 7201;
    h.loop();
    App.now = 10100;
    h.poll_status_();
    assert(h.waiting_poll_ && h.sent.back()[9] == 0x70);
  }
  // Off can release an unanswered poll exactly at its timeout.
  {
    Heater h;
    App.now = 100;
    h.network_pending_ = false;
    h.poll_status_();
    water_heater::WaterHeaterCall off;
    off.on = false;
    h.control(off);
    App.now = 5100;
    h.loop();
    assert(h.last_is_off());
  }
  // Unknown powered-on modes never retain Off; recognized active modes survive Off.
  {
    Heater h;
    App.now = 100;
    h.status(true, true);
    assert(h.is_on() && h.mode_ == water_heater::WATER_HEATER_MODE_ELECTRIC);
    h.status(true, false, true);
    assert(h.mode_ == water_heater::WATER_HEATER_MODE_ECO);
    h.status(false, true);
    assert(h.mode_ == water_heater::WATER_HEATER_MODE_OFF);
    h.status(true, true);
    assert(h.is_on() && h.mode_ == water_heater::WATER_HEATER_MODE_ECO);
    h.status(true);
    assert(h.mode_ == water_heater::WATER_HEATER_MODE_ELECTRIC);
  }
  // A configured timeout overrides the default and recovery remains automatic.
  {
    Heater h;
    binary_sensor::BinarySensor health;
    h.set_communication_sensor(&health);
    h.set_communication_timeout(7500);
    App.now = 100;
    h.setup();
    h.status(false);
    App.now = 7599;
    h.loop();
    assert(health.states.back());
    App.now = 7600;
    h.loop();
    assert(!health.states.back());
    h.status(false);
    assert(health.states.back());
  }
}
