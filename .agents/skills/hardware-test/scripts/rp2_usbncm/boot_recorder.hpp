#pragma once
// Boot recorder for USB NCM Ethernet on the RP2040. Everything lives in .uninitialized_data, survives a watchdog
// reboot and is printed on the next boot, the panic() message included. Link flags are in usbncm-rec.yaml (--wrap of
// panic and the TinyUSB hooks).
// pd5_start() from on_boot (normal boot) or on_safe_mode (safe mode) starts a 200 ms alarm-pool timer that records:
//  - main loop: heartbeat from a 100 ms interval (normal boot) and App.get_loop_component_start_time() (any boot)
//  - alarm pool: the timer callback itself (its count and time)
//  - USB: mounted/suspended/CDC flags, SIE status, event/task counters, NCM data IN starts/completes,
//         time of SET_INTERFACE alt 1 on the NCM data interface
// plus a ring of USB events (EV lines: code 10 netd_reset, 11 netd_open, 12 class request, 13 transfer done,
// 20 transfer start, 30 can_xmit change, 31 xmit, 40+n DCD event n, 48 interface SETUP request, a=bRequest b=wValue)
// and a stack copy if the loop timestamp stops for 3 s. The stack copy stops at 0x20042000 (RP2040 RAM end).
#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <hardware/sync.h>
#include <hardware/structs/usb.h>
#include <hardware/structs/timer.h>
#include <pico/time.h>
#include <tusb.h>
#include <device/dcd.h>
#include "esphome/core/application.h"
#include <device/usbd_pvt.h>
#include <lwip/pbuf.h>

#define PD_UNINIT __attribute__((section(".uninitialized_data.pdiag6")))
static constexpr uint32_t PD5_MAGIC = 0xD1A6D1AA;
static constexpr uint32_t PD5_BOOT_MAGIC = 0xB0075EF0;
static constexpr int PD5_STACK_WORDS = 256;
static constexpr int PD5_RING = 160;  // one snapshot every 2 s

struct Pd5Snap {
  uint32_t ms;       // timer callback time
  uint32_t loop_ms;  // App.get_loop_component_start_time()
  uint32_t hb;       // 100 ms interval heartbeat (0 in safe mode)
  uint32_t sie;
  uint8_t flags;  // 1 mounted, 2 suspended, 4 cdc connected, 8 safe mode
  uint8_t pad;
  uint16_t sof;
  uint32_t tx, ev, tasks, in_start, in_cmpl, out_cmpl, tcb;
};

extern "C" {
PD_UNINIT volatile uint32_t pd5_magic;
PD_UNINIT volatile uint32_t pd5_ms;
PD_UNINIT volatile uint32_t pd5_lr;
PD_UNINIT volatile uint32_t pd5_sp;
PD_UNINIT volatile uint32_t pd5_ipsr;
PD_UNINIT char pd5_msg[120];
PD_UNINIT uint32_t pd5_stack[PD5_STACK_WORDS];
PD_UNINIT volatile uint32_t pd5_boot_magic;
PD_UNINIT volatile uint32_t pd5_safe_shutdown;
PD_UNINIT volatile uint32_t pd5_safe_mode;
PD_UNINIT volatile uint32_t pd5_started_ms;
PD_UNINIT volatile uint32_t pd5_alt1_ms;
PD_UNINIT volatile uint32_t pd5_first_in_start_ms;
PD_UNINIT volatile uint32_t pd5_first_in_cmpl_ms;
PD_UNINIT volatile uint32_t pd5_pos;
PD_UNINIT Pd5Snap pd5_ring[PD5_RING];
PD_UNINIT volatile uint32_t pd5_stall_ms;
PD_UNINIT volatile uint32_t pd5_stall_loop_ms;
PD_UNINIT volatile uint32_t pd5_stall_sp;
PD_UNINIT volatile uint32_t pd5_stall_ipsr;
PD_UNINIT uint32_t pd5_stall_stack[PD5_STACK_WORDS];
struct Pd6Evt {
  uint32_t us;
  uint8_t code, a;
  uint16_t b;
  uint32_t c;
};
static constexpr int PD6_EVR = 128;
PD_UNINIT volatile uint32_t pd6_evpos;
PD_UNINIT Pd6Evt pd6_ev[PD6_EVR];
}
static Pd6Evt pd6_prev_ev[PD6_EVR];
static uint32_t pd6_prev_evpos;
static volatile int pd6_last_can = -1;
static void pd6_mark(uint8_t code, uint8_t a = 0, uint16_t b = 0, uint32_t c = 0) {
  uint32_t st = save_and_disable_interrupts();
  uint32_t i = pd6_evpos++;
  if (i < PD6_EVR) {
    pd6_ev[i].us = time_us_32();
    pd6_ev[i].code = code;
    pd6_ev[i].a = a;
    pd6_ev[i].b = b;
    pd6_ev[i].c = c;
  }
  restore_interrupts(st);
}

static volatile uint32_t pd5_hb = 0, pd5_tx = 0, pd5_ev = 0, pd5_tasks = 0, pd5_tcb = 0;
static volatile uint32_t pd5_in_start = 0, pd5_in_cmpl = 0, pd5_out_cmpl = 0;
static volatile uint32_t pd5_last_loop_val = 0, pd5_last_loop_change = 0;
static repeating_timer_t pd5_timer;
static bool pd5_timer_on = false;
static constexpr uint8_t PD5_NCM_IN = 0x83, PD5_NCM_OUT = 0x03;

struct Pd5Prev {
  bool valid, safe_shutdown, safe_mode, panic;
  uint32_t started_ms, alt1_ms, first_in_start_ms, first_in_cmpl_ms, pos;
  uint32_t stall_ms, stall_loop_ms, stall_sp, stall_ipsr;
};
static Pd5Prev pd5_prev{};
static Pd5Snap pd5_prev_ring[PD5_RING];
static uint32_t pd5_prev_stall_stack[PD5_STACK_WORDS];

__attribute__((constructor(101))) static void pd5_ctor() {
  if (pd5_boot_magic == PD5_BOOT_MAGIC && pd5_pos < 0x10000000) {
    pd5_prev.valid = true;
    pd5_prev.safe_shutdown = pd5_safe_shutdown == 1;
    pd5_prev.safe_mode = pd5_safe_mode == 1;
    pd5_prev.started_ms = pd5_started_ms;
    pd5_prev.alt1_ms = pd5_alt1_ms;
    pd5_prev.first_in_start_ms = pd5_first_in_start_ms;
    pd5_prev.first_in_cmpl_ms = pd5_first_in_cmpl_ms;
    pd5_prev.pos = pd5_pos;
    pd5_prev.stall_ms = pd5_stall_ms;
    pd5_prev.stall_loop_ms = pd5_stall_loop_ms;
    pd5_prev.stall_sp = pd5_stall_sp;
    pd5_prev.stall_ipsr = pd5_stall_ipsr;
    memcpy(pd5_prev_ring, (const void *) pd5_ring, sizeof(pd5_prev_ring));
    memcpy(pd5_prev_stall_stack, pd5_stall_stack, sizeof(pd5_prev_stall_stack));
    pd6_prev_evpos = pd6_evpos < 0x10000000 ? pd6_evpos : 0;
    memcpy(pd6_prev_ev, (const void *) pd6_ev, sizeof(pd6_prev_ev));
  }
  pd6_evpos = 0;
  pd5_prev.panic = pd5_magic == PD5_MAGIC;
  pd5_magic = 0;
  pd5_boot_magic = PD5_BOOT_MAGIC;
  pd5_safe_shutdown = 0;
  pd5_safe_mode = 0;
  pd5_started_ms = 0;
  pd5_alt1_ms = 0;
  pd5_first_in_start_ms = 0;
  pd5_first_in_cmpl_ms = 0;
  pd5_pos = 0;
  pd5_stall_ms = 0;
}

static inline uint32_t pd5_ipsr_now() {
  uint32_t v;
  __asm volatile("mrs %0, ipsr" : "=r"(v));
  return v & 0x3F;
}

static void pd5_copy_stack(uint32_t *dst, volatile uint32_t *sp_out) {
  uint32_t sp;
  __asm volatile("mov %0, sp" : "=r"(sp));
  *sp_out = sp;
  for (int i = 0; i < PD5_STACK_WORDS; i++) {
    uint32_t a = sp + 4 * i;
    dst[i] = a < 0x20042000 ? *(volatile uint32_t *) a : 0;
  }
}

static bool pd5_timer_cb(repeating_timer_t *) {
  uint32_t now = to_ms_since_boot(get_absolute_time());
  static uint32_t last_snap = 0;
  pd5_tcb++;
  uint32_t loop_ms = esphome::App.get_loop_component_start_time();
  if (loop_ms != pd5_last_loop_val) {
    pd5_last_loop_val = loop_ms;
    pd5_last_loop_change = now;
  } else if (pd5_stall_ms == 0 && now < 30000 && now - pd5_last_loop_change >= 3000) {
    pd5_stall_ms = now;
    pd5_stall_loop_ms = loop_ms;
    pd5_stall_ipsr = pd5_ipsr_now();
    pd5_copy_stack(pd5_stall_stack, &pd5_stall_sp);
  }
  if (now - last_snap >= 2000) {
    last_snap = now;
    Pd5Snap &s = pd5_ring[pd5_pos % PD5_RING];
    s.ms = now;
    s.loop_ms = loop_ms;
    s.hb = pd5_hb;
    s.sie = usb_hw->sie_status;
    s.flags =
        (tud_mounted() ? 1 : 0) | (tud_suspended() ? 2 : 0) | (tud_cdc_connected() ? 4 : 0) | (pd5_safe_mode ? 8 : 0);
    s.sof = (uint16_t) usb_hw->sof_rd;
    s.tx = pd5_tx;
    s.ev = pd5_ev;
    s.tasks = pd5_tasks;
    s.in_start = pd5_in_start;
    s.in_cmpl = pd5_in_cmpl;
    s.out_cmpl = pd5_out_cmpl;
    s.tcb = pd5_tcb;
    pd5_pos++;
  }
  return true;
}

extern "C" {
void __real_panic(const char *fmt, ...);
void __wrap_panic(const char *fmt, ...) {
  pd5_ipsr = pd5_ipsr_now();
  pd5_lr = (uint32_t) __builtin_return_address(0);
  pd5_copy_stack(pd5_stack, &pd5_sp);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(pd5_msg, sizeof(pd5_msg), fmt ? fmt : "(null)", ap);
  va_end(ap);
  pd5_ms = millis();
  pd5_magic = PD5_MAGIC;
  __real_panic("%s", pd5_msg);
  while (true) {
  }
}
void __real_tud_network_xmit(void *ref, uint16_t arg);
void __wrap_tud_network_xmit(void *ref, uint16_t arg) {
  pd5_tx++;
  pd6_mark(0x31, 0, ((struct pbuf *) ref)->tot_len);
  __real_tud_network_xmit(ref, arg);
}
bool __real_dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes);
bool __wrap_dcd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes) {
  if (ep_addr == PD5_NCM_IN) {
    if (pd5_in_start++ == 0)
      pd5_first_in_start_ms = millis();
  }
  return __real_dcd_edpt_xfer(rhport, ep_addr, buffer, total_bytes);
}
void __real_dcd_event_handler(dcd_event_t const *event, bool in_isr);
void __wrap_dcd_event_handler(dcd_event_t const *event, bool in_isr) {
  if (event->event_id != DCD_EVENT_SOF)
    pd5_ev++;
  if (event->event_id >= DCD_EVENT_BUS_RESET && event->event_id <= DCD_EVENT_RESUME && event->event_id != DCD_EVENT_SOF)
    pd6_mark(0x40 + event->event_id, in_isr);
  if (event->event_id == DCD_EVENT_SETUP_RECEIVED) {
    const tusb_control_request_t *r = &event->setup_received;
    // SET_INTERFACE to alternate setting 1 (only the NCM data interface has one)
    if (r->bmRequestType == 0x01 && r->bRequest == 0x0B && r->wValue == 1 && pd5_alt1_ms == 0)
      pd5_alt1_ms = millis();
    if ((r->bmRequestType & 0x1F) == 0x01)  // interface requests only
      pd6_mark(0x48, r->bRequest, r->wValue, r->wIndex | (r->bmRequestType << 16));
  } else if (event->event_id == DCD_EVENT_XFER_COMPLETE) {
    if (event->xfer_complete.ep_addr == PD5_NCM_IN) {
      if (pd5_in_cmpl++ == 0)
        pd5_first_in_cmpl_ms = millis();
    } else if (event->xfer_complete.ep_addr == PD5_NCM_OUT) {
      pd5_out_cmpl++;
    }
  }
  __real_dcd_event_handler(event, in_isr);
}
void __real_netd_reset(uint8_t rhport);
void __wrap_netd_reset(uint8_t rhport) {
  pd6_mark(0x10);
  __real_netd_reset(rhport);
}
uint16_t __real_netd_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len);
uint16_t __wrap_netd_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
  uint16_t r = __real_netd_open(rhport, itf_desc, max_len);
  pd6_mark(0x11, itf_desc->bInterfaceNumber, r);
  return r;
}
bool __real_netd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request);
bool __wrap_netd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  bool r = __real_netd_control_xfer_cb(rhport, stage, request);
  if (stage == CONTROL_STAGE_SETUP)
    pd6_mark(0x12, request->bRequest | (r ? 0x80 : 0), request->wValue,
             request->wIndex | (request->bmRequestType << 16));
  return r;
}
bool __real_netd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
bool __wrap_netd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  pd6_mark(0x13, ep_addr, (uint16_t) xferred_bytes, result);
  return __real_netd_xfer_cb(rhport, ep_addr, result, xferred_bytes);
}
bool __real_usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes);
bool __wrap_usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes) {
  bool r = __real_usbd_edpt_xfer(rhport, ep_addr, buffer, total_bytes);
  if (ep_addr == PD5_NCM_IN || ep_addr == PD5_NCM_OUT || ep_addr == 0x84)
    pd6_mark(0x20, ep_addr, total_bytes, r);
  return r;
}
bool __real_tud_network_can_xmit(uint16_t size);
bool __wrap_tud_network_can_xmit(uint16_t size) {
  bool r = __real_tud_network_can_xmit(size);
  if ((int) r != pd6_last_can) {
    pd6_last_can = r;
    pd6_mark(0x30, r, size, millis());
  }
  return r;
}
void __real_tud_task_ext(uint32_t timeout_ms, bool in_isr);
void __wrap_tud_task_ext(uint32_t timeout_ms, bool in_isr) {
  pd5_tasks++;
  __real_tud_task_ext(timeout_ms, in_isr);
}
}

inline void pd5_start() {
  if (pd5_timer_on)
    return;
  pd5_timer_on = true;
  pd5_started_ms = millis();
  pd5_last_loop_change = to_ms_since_boot(get_absolute_time());
  add_repeating_timer_ms(-200, pd5_timer_cb, nullptr, &pd5_timer);
}
inline void pd5_enter_safe_mode() {
  pd5_safe_mode = 1;
  pd5_start();
}
inline void pd5_beat() { pd5_hb++; }
inline void pd5_on_shutdown() { pd5_safe_shutdown = 1; }

inline void pd6_dump_current() {
  // NCM data interface never carried an IN transfer on this boot: show this boot's events live
  uint32_t n6 = pd6_evpos < PD6_EVR ? pd6_evpos : PD6_EVR;
  ESP_LOGW("pdiag", "THIS BOOT has no NCM IN transfer at %u ms; CURRENT EVENTS %u (first %u kept)", (unsigned) millis(),
           (unsigned) pd6_evpos, (unsigned) n6);
  for (uint32_t k = 0; k < n6; k++) {
    const Pd6Evt &e = pd6_ev[k];
    ESP_LOGW("pdiag", "CEV %03u t=%u.%03u ms code=%02X a=%02X b=%04X c=%08X", (unsigned) k, (unsigned) (e.us / 1000),
             (unsigned) (e.us % 1000), (unsigned) e.code, (unsigned) e.a, (unsigned) e.b, (unsigned) e.c);
  }
}
inline void pd5_report() {
  static int reports = 0;
  reports++;
  if (pd5_in_start == 0 && reports <= 2)
    pd6_dump_current();
  if (pd5_prev.panic) {
    pd5_msg[sizeof(pd5_msg) - 1] = 0;
    ESP_LOGE("pdiag", "previous boot panicked at %u ms, caller 0x%08X: %s", (unsigned) pd5_ms, (unsigned) pd5_lr,
             pd5_msg);
  }
  if (!pd5_prev.valid) {
    ESP_LOGI("pdiag", "previous boot: no recorder data");
    return;
  }
  ESP_LOGI("pdiag", "previous boot: %s, %s, recorder started at %u ms, %u snapshots, stall=%u",
           pd5_prev.safe_shutdown ? "safe shutdown" : "NO SAFE SHUTDOWN",
           pd5_prev.safe_mode ? "SAFE MODE" : "normal mode", (unsigned) pd5_prev.started_ms, (unsigned) pd5_prev.pos,
           (unsigned) pd5_prev.stall_ms);
  ESP_LOGI("pdiag", "previous boot USB: alt1 at %u ms, first NCM IN start %u ms, first NCM IN complete %u ms",
           (unsigned) pd5_prev.alt1_ms, (unsigned) pd5_prev.first_in_start_ms, (unsigned) pd5_prev.first_in_cmpl_ms);
  // on every report, so an API log subscription that starts late still sees one complete block
  uint32_t n6 = pd6_prev_evpos < PD6_EVR ? pd6_prev_evpos : PD6_EVR;
  ESP_LOGI("pdiag", "EVENTS %u recorded (first %u kept)", (unsigned) pd6_prev_evpos, (unsigned) n6);
  for (uint32_t k = 0; k < n6; k++) {
    const Pd6Evt &e = pd6_prev_ev[k];
    ESP_LOGI("pdiag", "EV %03u t=%u.%03u ms code=%02X a=%02X b=%04X c=%08X", (unsigned) k, (unsigned) (e.us / 1000),
             (unsigned) (e.us % 1000), (unsigned) e.code, (unsigned) e.a, (unsigned) e.b, (unsigned) e.c);
  }
  if (reports > 2 || (pd5_prev.safe_shutdown && pd5_prev.stall_ms == 0))
    return;
  uint32_t n = pd5_prev.pos < PD5_RING ? pd5_prev.pos : PD5_RING;
  for (uint32_t k = pd5_prev.pos - n; k < pd5_prev.pos; k++) {
    const Pd5Snap &s = pd5_prev_ring[k % PD5_RING];
    ESP_LOGI("pdiag", "SNAP %u ms=%u loop=%u hb=%u tcb=%u sie=%08X sof=%u fl=%X tx=%u ev=%u task=%u in=%u/%u out=%u",
             (unsigned) k, (unsigned) s.ms, (unsigned) s.loop_ms, (unsigned) s.hb, (unsigned) s.tcb, (unsigned) s.sie,
             (unsigned) s.sof, (unsigned) s.flags, (unsigned) s.tx, (unsigned) s.ev, (unsigned) s.tasks,
             (unsigned) s.in_start, (unsigned) s.in_cmpl, (unsigned) s.out_cmpl);
  }
  if (pd5_prev.stall_ms) {
    ESP_LOGE("pdiag", "STALLCTX ms=%u hb=%u sp=0x%08X ipsr=%u", (unsigned) pd5_prev.stall_ms,
             (unsigned) pd5_prev.stall_loop_ms, (unsigned) pd5_prev.stall_sp, (unsigned) pd5_prev.stall_ipsr);
    for (int i = 0; i < PD5_STACK_WORDS; i += 8) {
      const uint32_t *w = &pd5_prev_stall_stack[i];
      ESP_LOGE("pdiag", "SSTK %03X: %08X %08X %08X %08X %08X %08X %08X %08X", i * 4, (unsigned) w[0], (unsigned) w[1],
               (unsigned) w[2], (unsigned) w[3], (unsigned) w[4], (unsigned) w[5], (unsigned) w[6], (unsigned) w[7]);
    }
  }
}
