#pragma once

#include "esphome/components/rx8111/rx8111.h"
#include "esphome/core/base_automation.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <vector>

namespace alarm_test {
using namespace esphome;
using namespace esphome::rx8111;
inline time_t mock_now = 1784289610;
inline unsigned sync_calls = 0;

inline void require(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAILED: %s\n", message);
    std::exit(1);
  }
}

class RegisterBus : public i2c::I2CBus {
 public:
  std::array<uint8_t, 256> regs{};
  std::vector<uint8_t> writes;
  int fail_read{-1};
  int fail_write{-1};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *out, size_t out_size, uint8_t *in,
                             size_t in_size) override {
    require(out_size > 0, "register address required");
    const uint8_t reg = out[0];
    if (in_size > 0) {
      if (reg == this->fail_read)
        return i2c::ERROR_TIMEOUT;
      for (size_t i = 0; i < in_size; i++)
        in[i] = this->regs[reg + i];
    } else {
      if (reg == this->fail_write)
        return i2c::ERROR_TIMEOUT;
      this->writes.push_back(reg);
      for (size_t i = 1; i < out_size; i++)
        this->regs[reg + i - 1] = out[i];
    }
    return i2c::ERROR_OK;
  }
};

template<typename RTC> void attach(RTC &rtc, RegisterBus &bus) {
  rtc.set_i2c_bus(&bus);
  rtc.set_i2c_address(0x32);
}

template<typename RTC> void test_set(uint8_t control, uint8_t flags) {
  RTC startup_rtc;
  RegisterBus startup_bus;
  attach(startup_rtc, startup_bus);
  startup_bus.regs[flags] = 0x38;
  startup_rtc.set_alarm_minute(0);
  startup_rtc.set_alarm_enabled(true);
  startup_rtc.setup();
  require(!startup_rtc.is_failed(), "startup programming succeeds");
  require((startup_bus.regs[control] & 0x08) != 0, "startup still enables AIE");
  require(startup_bus.regs[flags] == 0x38, "startup programming does not acknowledge AF");
  for (uint8_t enabled : {0x00, 0x08}) {
    RTC rtc;
    RegisterBus bus;
    attach(rtc, bus);
    bus.regs[control] = enabled | 0x10;
    bus.regs[flags] = 0x38;
    require(rtc.set_alarm_runtime(0xFF, 15, 12, 0xFF, 5), "set succeeds even when AIE is off");
    require(bus.regs[control] == (enabled | 0x10), "set preserves all control bits");
    require(bus.regs[flags] == 0x38, "set preserves all flags");
    require(bus.regs[RX8111_REG_MIN_ALARM] == 0x15, "set programs BCD minute");
    require(bus.regs[RX8111_REG_WD_ALARM] == 0x05, "set programs day");
    for (auto reg : bus.writes)
      require(reg != control && reg != flags, "set never writes control or flag register");
    SetAlarmAction<> action;
    action.set_parent(&rtc);
    action.set_hour([]() -> uint16_t { return 6; });
    action.play();
    require(bus.regs[RX8111_REG_HOUR_ALARM] == 0x06, "templated hour programmed");
    require(bus.regs[RX8111_REG_MIN_ALARM] == RX8111_ALARM_AE, "omitted fields become don't-care");
  }
}

template<typename RTC> void test_unset(uint8_t control_reg, uint8_t flag_reg, uint8_t extension_reg) {
  for (uint8_t control : {0x00, 0x08, 0x3C}) {
    for (uint8_t flags : {0x00, 0x08, 0x3C}) {
      for (uint8_t extension : {0xD7, 0xDF}) {
        RTC rtc;
        RegisterBus bus;
        attach(rtc, bus);
        bus.regs.fill(0x15);
        bus.regs[control_reg] = control;
        bus.regs[flag_reg] = flags;
        bus.regs[extension_reg] = extension;
        auto expected = bus.regs;
        expected[RX8111_REG_WD_ALARM] = 0;
        expected[extension_reg] &= ~RX8111_EXT_WADA;
        require(rtc.unset_alarm(), "unset succeeds");
        require(bus.regs == expected, "unset changes only WEEK and WADA");
        require(bus.writes == std::vector<uint8_t>{RX8111_REG_WD_ALARM, extension_reg},
                "empty mask is written before changing comparison mode");
        UnsetAlarmAction<> action;
        action.set_parent(&rtc);
        action.play();
        require(bus.regs == expected, "repeated unset is idempotent");
        require(rtc.set_alarm_runtime(0xFF, 30, 12, 0xFF, 20), "rearm date after unset");
        require(bus.regs[RX8111_REG_WD_ALARM] == 0x20, "date restored");
        require((bus.regs[extension_reg] & RX8111_EXT_WADA) != 0, "day mode restored");
        require(bus.regs[flag_reg] == flags, "rearm preserves AF and EVF");
        require(bus.regs[control_reg] == control, "rearm preserves AIE and EIE");
        require(rtc.unset_alarm(), "unset date alarm again");
        require(rtc.set_alarm_runtime(0xFF, 0, 8, 3, 0xFF), "rearm weekday after unset");
        require(bus.regs[RX8111_REG_WD_ALARM] == 0x04, "weekday mask restored");
        require((bus.regs[extension_reg] & RX8111_EXT_WADA) == 0, "weekday mode restored");
        require(rtc.unset_alarm(), "unset before relative schedule");
        rtc.set_alarm_enabled(true);
        ScheduleAlarmInAction<> relative;
        relative.set_parent(&rtc);
        relative.set_seconds(60);
        relative.play();
        require(bus.regs[RX8111_REG_WD_ALARM] == 0x17, "relative schedule works after unset");
        rtc.clear_alarm_flag();
        require(bus.regs[flag_reg] == (flags & ~RX8111_FLAG_AF), "explicit clear remains separate");
      }
    }
  }
}

template<typename RTC> void test_failures(uint8_t control_reg, uint8_t flag_reg, uint8_t extension_reg) {
  for (int failure : {0, 1, 2}) {
    RTC rtc;
    RegisterBus bus;
    attach(rtc, bus);
    bus.regs.fill(0x3C);
    const auto before = bus.regs;
    if (failure == 0)
      bus.fail_read = extension_reg;
    else
      bus.fail_write = failure == 1 ? RX8111_REG_WD_ALARM : extension_reg;
    require(!rtc.unset_alarm(), "unset reports I2C failure");
    require(bus.regs[control_reg] == before[control_reg], "failed unset preserves controls");
    require(bus.regs[flag_reg] == before[flag_reg], "failed unset preserves flags");
    if (failure < 2)
      require(bus.regs == before, "early failure does not write registers");
    bus.fail_read = bus.fail_write = -1;
    require(rtc.unset_alarm(), "retry succeeds");
  }
}

template<typename RTC>
void test_clock_and_battery(uint8_t flag_reg, uint8_t invalid_mask, uint8_t power_reg, uint8_t charge_mask,
                            uint8_t backup_mask) {
  RTC rtc;
  RegisterBus bus;
  attach(rtc, bus);
  mock_now = 1784289610;
  rtc.write_time();
  const auto valid_clock = bus.regs;
  bus.regs[power_reg] = charge_mask | backup_mask | 0x01;
  bus.regs[flag_reg] = invalid_mask | 0x18;
  mock_now -= 3600;
  const unsigned before = sync_calls;
  rtc.setup();
  require((bus.regs[power_reg] & (charge_mask | backup_mask)) == 0,
          "backup false explicitly disables retained charging and backup");
  require((bus.regs[power_reg] & 0x01) != 0, "unrelated power settings preserved");
  require((bus.regs[flag_reg] & invalid_mask) == invalid_mask, "setup retains invalid-time flags");
  require(sync_calls == before, "invalidated but plausible clock never syncs");
  rtc.read_time();
  require(sync_calls == before, "repeated reads do not clear invalidity");
  bus.fail_write = RX8111_REG_SEC;
  rtc.write_time();
  require((bus.regs[flag_reg] & invalid_mask) != 0, "failed write keeps invalidity");
  bus.fail_write = -1;
  rtc.write_time();
  require((bus.regs[flag_reg] & invalid_mask) == 0, "successful trusted write clears invalidity");
  require((bus.regs[flag_reg] & 0x18) == 0x18, "time write preserves AF and TF");
  mock_now -= 3600;
  rtc.read_time();
  require(sync_calls == before + 1, "valid clock synchronizes after trusted write");
  const unsigned valid_syncs = sync_calls;
  mock_now -= 7200;
  for (uint8_t weekday : {0, 3, 0x80, 0x81}) {
    bus.regs[RX8111_REG_WEEK] = weekday;
    rtc.read_time();
    require(sync_calls == valid_syncs, "zero, multiple and reserved weekday bits rejected");
  }
  bus.regs = valid_clock;
  bus.regs[RX8111_REG_SEC] = 0x1A;
  rtc.read_time();
  require(sync_calls == valid_syncs, "invalid BCD rejected");
  bus.regs = valid_clock;
  bus.regs[RX8111_REG_MONTH] = 0x02;
  bus.regs[RX8111_REG_DAY] = 0x31;
  rtc.read_time();
  require(sync_calls == valid_syncs, "February 31 rejected");
  bus.regs = valid_clock;
  rtc.set_battery_backup(true);
  rtc.set_battery_charging(false);
  bus.regs[power_reg] |= charge_mask;
  rtc.setup();
  require((bus.regs[power_reg] & backup_mask) != 0 && (bus.regs[power_reg] & charge_mask) == 0,
          "backup enabled without charging");
}

inline void test_callback_rearm() {
  for (uint8_t mask : {RX8111_FLAG_AF, RX8111_FLAG_TF, RX8111_FLAG_EVF}) {
    RX8111Component rtc;
    RegisterBus bus;
    attach(rtc, bus);
    rtc.write_time();
    Trigger<> trigger;
    unsigned calls = 0;
    bool clear_inside = false;
    auto clear = [&]() {
      if (mask == RX8111_FLAG_AF)
        rtc.clear_alarm_flag();
      else if (mask == RX8111_FLAG_TF)
        rtc.clear_timer_flag();
      else
        rtc.clear_event_flag();
    };
    LambdaAction<> callback([&]() {
      calls++;
      if (clear_inside)
        clear();
    });
    Automation<> automation(&trigger);
    automation.add_action(&callback);
    if (mask == RX8111_FLAG_AF)
      rtc.add_on_alarm_callback(&trigger);
    else if (mask == RX8111_FLAG_TF)
      rtc.add_on_timer_callback(&trigger);
    else
      rtc.add_on_event_callback(&trigger);
    bus.regs[RX8111_REG_FLAG] = mask;
    rtc.update();
    rtc.update();
    require(calls == 1, "latched callback deduplicated");
    bus.fail_write = RX8111_REG_FLAG;
    clear();
    rtc.update();
    require(calls == 1, "failed clear does not rearm callback");
    bus.fail_write = -1;
    clear();
    bus.regs[RX8111_REG_FLAG] |= mask;
    rtc.update();
    require(calls == 2, "event rearmed without observing cleared hardware flag");
    clear();
    clear_inside = true;
    bus.regs[RX8111_REG_FLAG] |= mask;
    rtc.update();
    bus.regs[RX8111_REG_FLAG] |= mask;
    rtc.update();
    require(calls == 4, "clear inside callback survives update snapshot");
  }
}

inline void test_regressions() {
  test_clock_and_battery<RX8111Component>(RX8111_REG_FLAG, RX8111_FLAG_POR | RX8111_FLAG_VLF | RX8111_FLAG_XST,
                                          RX8111_REG_PWRSWITCH, RX8111_PWR_CHGEN, RX8111_PWR_INIEN);
  test_callback_rearm();
  for (unsigned clock = 0; clock < 4; clock++) {
    RX8111Component timer_rtc;
    RegisterBus timer_bus;
    attach(timer_rtc, timer_bus);
    timer_rtc.set_timer_enabled(true);
    timer_rtc.set_timer_count(0x123456);
    timer_rtc.set_timer_clock(static_cast<TimerClock>(clock));
    timer_rtc.setup();
    require(timer_bus.regs[RX8111_REG_TIMER0] == 0x56 && timer_bus.regs[RX8111_REG_TIMER1] == 0x34 &&
                timer_bus.regs[RX8111_REG_TIMER2] == 0x12 &&
                (timer_bus.regs[RX8111_REG_EXTENSION] & (RX8111_EXT_TE | RX8111_EXT_TSEL_MASK)) ==
                    (RX8111_EXT_TE | clock) &&
                (timer_bus.regs[RX8111_REG_CONTROL] & RX8111_CTRL_TIE),
            "RX8111 production timer setup");
  }
}

inline void run() {
  test_set<RX8111Component>(RX8111_REG_CONTROL, RX8111_REG_FLAG);
  test_unset<RX8111Component>(RX8111_REG_CONTROL, RX8111_REG_FLAG, RX8111_REG_EXTENSION);
  test_failures<RX8111Component>(RX8111_REG_CONTROL, RX8111_REG_FLAG, RX8111_REG_EXTENSION);
  test_regressions();
  std::puts("Alarm action tests passed");
  std::exit(0);
}
}  // namespace alarm_test

extern "C" time_t __wrap_time(time_t *out) {
  if (out != nullptr)
    *out = alarm_test::mock_now;
  return alarm_test::mock_now;
}
extern "C" int __wrap_settimeofday(const timeval *value, const struct timezone *) {
  alarm_test::sync_calls++;
  alarm_test::mock_now = value->tv_sec;
  return 0;
}
