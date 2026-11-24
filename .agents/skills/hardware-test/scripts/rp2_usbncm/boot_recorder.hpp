#pragma once
// Boot recorder for USB NCM Ethernet on the RP2040. Everything lives in .uninitialized_data, survives a watchdog
// reboot and is printed on the next boot, the panic() message included. Link flags are in usbncm-rec.yaml (--wrap of
// panic and the TinyUSB hooks).
// br_start() from on_boot (normal boot) or on_safe_mode (safe mode) starts a 200 ms alarm-pool timer that records:
//  - main loop: heartbeat from a 100 ms interval (normal boot) and App.get_loop_component_start_time() (any boot)
//  - alarm pool: the timer callback itself (its count and time)
//  - USB: mounted/suspended/CDC flags, SIE status, event/task counters, NCM data IN starts/completes,
//         time of SET_INTERFACE alt 1 on the NCM data interface
// plus a ring of USB events (EV lines: code 10 netd_reset, 11 netd_open, 12 class request, 13 transfer done,
// 20 accepted transfer start, 30 can_xmit change, 31 xmit, 40+n DCD event n,
// 48 interface SETUP request, a=bRequest b=wValue)
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

#define BR_UNINIT __attribute__((section(".uninitialized_data.boot_recorder")))
static constexpr uint32_t BR_MAGIC = 0xD1A6D1AA;
static constexpr uint32_t BR_BOOT_MAGIC = 0xB0075EF0;
static constexpr int BR_STACK_WORDS = 256;
static constexpr int BR_RING = 160;  // one snapshot every 2 s

struct BrSnap {
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
BR_UNINIT volatile uint32_t br_magic;
BR_UNINIT volatile uint32_t br_ms;
BR_UNINIT volatile uint32_t br_lr;
BR_UNINIT char br_msg[120];
BR_UNINIT volatile uint32_t br_boot_magic;
BR_UNINIT volatile uint32_t br_safe_shutdown;
BR_UNINIT volatile uint32_t br_safe_mode;
BR_UNINIT volatile uint32_t br_started_ms;
BR_UNINIT volatile uint32_t br_alt1_ms;
BR_UNINIT volatile uint32_t br_first_in_start_ms;
BR_UNINIT volatile uint32_t br_first_in_cmpl_ms;
BR_UNINIT volatile uint32_t br_pos;
BR_UNINIT BrSnap br_ring[BR_RING];
BR_UNINIT volatile uint32_t br_stall_ms;
BR_UNINIT volatile uint32_t br_stall_loop_ms;
BR_UNINIT volatile uint32_t br_stall_sp;
BR_UNINIT volatile uint32_t br_stall_ipsr;
BR_UNINIT uint32_t br_stall_stack[BR_STACK_WORDS];
struct BrEvt {
  uint32_t us;
  uint8_t code, a;
  uint16_t b;
  uint32_t c;
};
static constexpr int BR_EVR = 128;
BR_UNINIT volatile uint32_t br_evpos;
BR_UNINIT BrEvt br_events[BR_EVR];
}
static BrEvt br_prev_events[BR_EVR];
static uint32_t br_prev_evpos;
static volatile int br_last_can = -1;
static void br_mark(uint8_t code, uint8_t a = 0, uint16_t b = 0, uint32_t c = 0) {
  uint32_t st = save_and_disable_interrupts();
  uint32_t i = br_evpos++;
  if (i < BR_EVR) {
    br_events[i].us = time_us_32();
    br_events[i].code = code;
    br_events[i].a = a;
    br_events[i].b = b;
    br_events[i].c = c;
  }
  restore_interrupts(st);
}

static volatile uint32_t br_hb = 0, br_tx = 0, br_dcd_ev = 0, br_tasks = 0, br_tcb = 0;
static volatile uint32_t br_in_start = 0, br_in_cmpl = 0, br_out_cmpl = 0;
static volatile uint32_t br_last_loop_val = 0, br_last_loop_change = 0;
static repeating_timer_t br_timer;
static bool br_timer_on = false;
static constexpr uint8_t BR_NCM_IN = 0x83, BR_NCM_OUT = 0x03;

struct BrPrev {
  bool valid, safe_shutdown, safe_mode, panic;
  uint32_t started_ms, alt1_ms, first_in_start_ms, first_in_cmpl_ms, pos;
  uint32_t stall_ms, stall_loop_ms, stall_sp, stall_ipsr;
};
static BrPrev br_prev{};
static BrSnap br_prev_ring[BR_RING];
static uint32_t br_prev_stall_stack[BR_STACK_WORDS];

__attribute__((constructor(101))) static void br_ctor() {
  if (br_boot_magic == BR_BOOT_MAGIC && br_pos < 0x10000000) {
    br_prev.valid = true;
    br_prev.safe_shutdown = br_safe_shutdown == 1;
    br_prev.safe_mode = br_safe_mode == 1;
    br_prev.started_ms = br_started_ms;
    br_prev.alt1_ms = br_alt1_ms;
    br_prev.first_in_start_ms = br_first_in_start_ms;
    br_prev.first_in_cmpl_ms = br_first_in_cmpl_ms;
    br_prev.pos = br_pos;
    br_prev.stall_ms = br_stall_ms;
    br_prev.stall_loop_ms = br_stall_loop_ms;
    br_prev.stall_sp = br_stall_sp;
    br_prev.stall_ipsr = br_stall_ipsr;
    memcpy(br_prev_ring, (const void *) br_ring, sizeof(br_prev_ring));
    memcpy(br_prev_stall_stack, br_stall_stack, sizeof(br_prev_stall_stack));
    br_prev_evpos = br_evpos < 0x10000000 ? br_evpos : 0;
    memcpy(br_prev_events, (const void *) br_events, sizeof(br_prev_events));
  }
  br_evpos = 0;
  br_prev.panic = br_magic == BR_MAGIC;
  br_magic = 0;
  br_boot_magic = BR_BOOT_MAGIC;
  br_safe_shutdown = 0;
  br_safe_mode = 0;
  br_started_ms = 0;
  br_alt1_ms = 0;
  br_first_in_start_ms = 0;
  br_first_in_cmpl_ms = 0;
  br_pos = 0;
  br_stall_ms = 0;
}

static inline uint32_t br_ipsr_now() {
  uint32_t v;
  __asm volatile("mrs %0, ipsr" : "=r"(v));
  return v & 0x3F;
}

static void br_copy_stack(uint32_t *dst, volatile uint32_t *sp_out) {
  uint32_t sp;
  __asm volatile("mov %0, sp" : "=r"(sp));
  *sp_out = sp;
  for (int i = 0; i < BR_STACK_WORDS; i++) {
    uint32_t a = sp + 4 * i;
    dst[i] = a < 0x20042000 ? *(volatile uint32_t *) a : 0;
  }
}

static bool br_timer_cb(repeating_timer_t *) {
  uint32_t now = to_ms_since_boot(get_absolute_time());
  static uint32_t last_snap = 0;
  br_tcb++;
  uint32_t loop_ms = esphome::App.get_loop_component_start_time();
  if (loop_ms != br_last_loop_val) {
    br_last_loop_val = loop_ms;
    br_last_loop_change = now;
  } else if (br_stall_ms == 0 && now < 30000 && now - br_last_loop_change >= 3000) {
    br_stall_ms = now;
    br_stall_loop_ms = loop_ms;
    br_stall_ipsr = br_ipsr_now();
    br_copy_stack(br_stall_stack, &br_stall_sp);
  }
  if (now - last_snap >= 2000) {
    last_snap = now;
    BrSnap &s = br_ring[br_pos % BR_RING];
    s.ms = now;
    s.loop_ms = loop_ms;
    s.hb = br_hb;
    s.sie = usb_hw->sie_status;
    s.flags =
        (tud_mounted() ? 1 : 0) | (tud_suspended() ? 2 : 0) | (tud_cdc_connected() ? 4 : 0) | (br_safe_mode ? 8 : 0);
    s.sof = (uint16_t) usb_hw->sof_rd;
    s.tx = br_tx;
    s.ev = br_dcd_ev;
    s.tasks = br_tasks;
    s.in_start = br_in_start;
    s.in_cmpl = br_in_cmpl;
    s.out_cmpl = br_out_cmpl;
    s.tcb = br_tcb;
    br_pos++;
  }
  return true;
}

extern "C" {
void __real_panic(const char *fmt, ...);
void __wrap_panic(const char *fmt, ...) {
  br_lr = (uint32_t) __builtin_return_address(0);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(br_msg, sizeof(br_msg), fmt ? fmt : "(null)", ap);
  va_end(ap);
  br_ms = millis();
  br_magic = BR_MAGIC;
  __real_panic("%s", br_msg);
  while (true) {
  }
}
void __real_tud_network_xmit(void *ref, uint16_t arg);
void __wrap_tud_network_xmit(void *ref, uint16_t arg) {
  br_tx++;
  br_mark(0x31, 0, ((struct pbuf *) ref)->tot_len);
  __real_tud_network_xmit(ref, arg);
}
void __real_dcd_event_handler(dcd_event_t const *event, bool in_isr);
void __wrap_dcd_event_handler(dcd_event_t const *event, bool in_isr) {
  if (event->event_id != DCD_EVENT_SOF)
    br_dcd_ev++;
  if (event->event_id >= DCD_EVENT_BUS_RESET && event->event_id <= DCD_EVENT_RESUME && event->event_id != DCD_EVENT_SOF)
    br_mark(0x40 + event->event_id, in_isr);
  if (event->event_id == DCD_EVENT_SETUP_RECEIVED) {
    const tusb_control_request_t *r = &event->setup_received;
    // SET_INTERFACE to alternate setting 1 (only the NCM data interface has one)
    if (r->bmRequestType == 0x01 && r->bRequest == 0x0B && r->wValue == 1 && br_alt1_ms == 0)
      br_alt1_ms = millis();
    if ((r->bmRequestType & 0x1F) == 0x01)  // interface requests only
      br_mark(0x48, r->bRequest, r->wValue, r->wIndex | (r->bmRequestType << 16));
  } else if (event->event_id == DCD_EVENT_XFER_COMPLETE) {
    if (event->xfer_complete.ep_addr == BR_NCM_IN) {
      if (br_in_cmpl++ == 0)
        br_first_in_cmpl_ms = millis();
    } else if (event->xfer_complete.ep_addr == BR_NCM_OUT) {
      br_out_cmpl++;
    }
  }
  __real_dcd_event_handler(event, in_isr);
}
void __real_netd_reset(uint8_t rhport);
void __wrap_netd_reset(uint8_t rhport) {
  br_mark(0x10);
  __real_netd_reset(rhport);
}
uint16_t __real_netd_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len);
uint16_t __wrap_netd_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
  uint16_t r = __real_netd_open(rhport, itf_desc, max_len);
  br_mark(0x11, itf_desc->bInterfaceNumber, r);
  return r;
}
bool __real_netd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request);
bool __wrap_netd_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
  bool r = __real_netd_control_xfer_cb(rhport, stage, request);
  if (stage == CONTROL_STAGE_SETUP)
    br_mark(0x12, request->bRequest | (r ? 0x80 : 0), request->wValue,
            request->wIndex | (request->bmRequestType << 16));
  return r;
}
bool __real_netd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes);
bool __wrap_netd_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
  br_mark(0x13, ep_addr, (uint16_t) xferred_bytes, result);
  return __real_netd_xfer_cb(rhport, ep_addr, result, xferred_bytes);
}
bool __real_usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes);
bool __wrap_usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes) {
  bool r = __real_usbd_edpt_xfer(rhport, ep_addr, buffer, total_bytes);
  if (ep_addr == BR_NCM_IN && r && br_in_start++ == 0)
    br_first_in_start_ms = millis();
  if (r && (ep_addr == BR_NCM_IN || ep_addr == BR_NCM_OUT || ep_addr == 0x84))
    br_mark(0x20, ep_addr, total_bytes);
  return r;
}
bool __real_tud_network_can_xmit(uint16_t size);
bool __wrap_tud_network_can_xmit(uint16_t size) {
  bool r = __real_tud_network_can_xmit(size);
  if ((int) r != br_last_can) {
    br_last_can = r;
    br_mark(0x30, r, size);
  }
  return r;
}
void __real_tud_task_ext(uint32_t timeout_ms, bool in_isr);
void __wrap_tud_task_ext(uint32_t timeout_ms, bool in_isr) {
  br_tasks++;
  __real_tud_task_ext(timeout_ms, in_isr);
}
}

inline void br_start() {
  if (br_timer_on)
    return;
  br_timer_on = true;
  br_started_ms = millis();
  br_last_loop_change = to_ms_since_boot(get_absolute_time());
  add_repeating_timer_ms(-200, br_timer_cb, nullptr, &br_timer);
}
inline void br_enter_safe_mode() {
  br_safe_mode = 1;
  br_start();
}
inline void br_beat() { br_hb++; }
inline void br_on_shutdown() { br_safe_shutdown = 1; }

// TAG is EV (previous boot, read by evring.py) or CEV (this boot)
inline void br_dump_events(const char *tag, const BrEvt *ev, uint32_t n) {
  for (uint32_t k = 0; k < n; k++) {
    const BrEvt &e = ev[k];
    ESP_LOGI("pdiag", "%s %03u t=%u.%03u ms code=%02X a=%02X b=%04X c=%08X", tag, (unsigned) k,
             (unsigned) (e.us / 1000), (unsigned) (e.us % 1000), (unsigned) e.code, (unsigned) e.a, (unsigned) e.b,
             (unsigned) e.c);
  }
}
inline void br_report() {
  static int reports = 0;
  reports++;
  if (br_in_start == 0 && reports <= 2) {
    // the NCM data interface has not carried an IN transfer on this boot: show this boot's events live
    uint32_t n = br_evpos < BR_EVR ? br_evpos : BR_EVR;
    ESP_LOGW("pdiag", "THIS BOOT has no NCM IN transfer at %u ms; CURRENT EVENTS %u (first %u kept)",
             (unsigned) millis(), (unsigned) br_evpos, (unsigned) n);
    br_dump_events("CEV", (const BrEvt *) br_events, n);
  }
  if (br_prev.panic) {
    br_msg[sizeof(br_msg) - 1] = 0;
    ESP_LOGE("pdiag", "previous boot panicked at %u ms, caller 0x%08X: %s", (unsigned) br_ms, (unsigned) br_lr, br_msg);
  }
  if (!br_prev.valid) {
    ESP_LOGI("pdiag", "previous boot: no recorder data");
    return;
  }
  ESP_LOGI("pdiag", "previous boot: %s, %s, recorder started at %u ms, %u snapshots, stall=%u",
           br_prev.safe_shutdown ? "safe shutdown" : "NO SAFE SHUTDOWN",
           br_prev.safe_mode ? "SAFE MODE" : "normal mode", (unsigned) br_prev.started_ms, (unsigned) br_prev.pos,
           (unsigned) br_prev.stall_ms);
  ESP_LOGI("pdiag", "previous boot USB: alt1 at %u ms, first NCM IN start %u ms, first NCM IN complete %u ms",
           (unsigned) br_prev.alt1_ms, (unsigned) br_prev.first_in_start_ms, (unsigned) br_prev.first_in_cmpl_ms);
  // on every report, so an API log subscription that starts late still sees one complete block
  uint32_t ne = br_prev_evpos < BR_EVR ? br_prev_evpos : BR_EVR;
  ESP_LOGI("pdiag", "EVENTS %u recorded (first %u kept)", (unsigned) br_prev_evpos, (unsigned) ne);
  br_dump_events("EV", br_prev_events, ne);
  if (reports > 2 || (br_prev.safe_shutdown && br_prev.stall_ms == 0))
    return;
  uint32_t n = br_prev.pos < BR_RING ? br_prev.pos : BR_RING;
  for (uint32_t k = br_prev.pos - n; k < br_prev.pos; k++) {
    const BrSnap &s = br_prev_ring[k % BR_RING];
    ESP_LOGI("pdiag", "SNAP %u ms=%u loop=%u hb=%u tcb=%u sie=%08X sof=%u fl=%X tx=%u ev=%u task=%u in=%u/%u out=%u",
             (unsigned) k, (unsigned) s.ms, (unsigned) s.loop_ms, (unsigned) s.hb, (unsigned) s.tcb, (unsigned) s.sie,
             (unsigned) s.sof, (unsigned) s.flags, (unsigned) s.tx, (unsigned) s.ev, (unsigned) s.tasks,
             (unsigned) s.in_start, (unsigned) s.in_cmpl, (unsigned) s.out_cmpl);
  }
  if (br_prev.stall_ms) {
    ESP_LOGE("pdiag", "STALLCTX ms=%u loop=%u sp=0x%08X ipsr=%u", (unsigned) br_prev.stall_ms,
             (unsigned) br_prev.stall_loop_ms, (unsigned) br_prev.stall_sp, (unsigned) br_prev.stall_ipsr);
    for (int i = 0; i < BR_STACK_WORDS; i += 8) {
      const uint32_t *w = &br_prev_stall_stack[i];
      ESP_LOGE("pdiag", "SSTK %03X: %08X %08X %08X %08X %08X %08X %08X %08X", i * 4, (unsigned) w[0], (unsigned) w[1],
               (unsigned) w[2], (unsigned) w[3], (unsigned) w[4], (unsigned) w[5], (unsigned) w[6], (unsigned) w[7]);
    }
  }
}
