"""Generate MiBeacon, ATC and PVVX advertisements, a C++ header that sends them and the expected publish counts.
Usage: gen.py [--ota]
Without --ota: frames.hpp for xbinject.yaml (scan results injected into the tracker, A4:C1:38 addresses).
With --ota: adv_frames.hpp for xbadv.yaml (a second ESP32 advertises them, C4:C1:38 random static addresses).
Both write expected.json ({"base": {...}, "pr": {...}}) for count.py."""

import json
from pathlib import Path
import struct
import sys

from cryptography.hazmat.primitives.ciphers.aead import AESCCM

OTA = "--ota" in sys.argv[1:]
CYCLES = 40
K1 = bytes.fromhex("6c0d1a3e5b7f29c4a18e90d2f3b46a57")
K2 = bytes.fromhex("0f1e2d3c4b5a69788796a5b4c3d2e1f0")
# Random static addresses need the top two bits set, so the over the air run uses C4 instead of A4.
PREFIX = "C4C138" if OTA else "A4C138"
MACS = {
    k: f"{PREFIX}0000{i + 10:02X}" for i, k in enumerate("ABCDEFG")
}  # ...0A to ...10
MAC = {k: bytes.fromhex(v) for k, v in MACS.items()}
MIDX = {k: i for i, k in enumerate(MACS)}


def mibeacon_plain(dev, pid, cnt, obj):
    return (
        bytes([0x50, 0x20])
        + struct.pack("<H", pid)
        + bytes([cnt])
        + MAC[dev][::-1]
        + obj
    )


def mibeacon_enc(dev, pid, cnt, obj, key, short, corrupt=False):
    ext = bytes([cnt, 0x00, 0x00])
    if short:
        hdr = bytes([0x48, 0x59]) + struct.pack("<H", pid) + bytes([cnt])
    else:
        hdr = (
            bytes([0x58, 0x58]) + struct.pack("<H", pid) + bytes([cnt]) + MAC[dev][::-1]
        )
    nonce = MAC[dev][::-1] + hdr[2:5] + ext
    ct = AESCCM(key, tag_length=4).encrypt(nonce, obj, b"\x11")
    body, tag = ct[:-4], ct[-4:]
    if corrupt:
        tag = bytes([tag[0] ^ 0xFF]) + tag[1:]
    out = hdr + body + ext + tag
    assert len(out) == (19 if short else 18 + len(obj)), len(out)
    return out


def t16(v):  # 0x1004 temperature object
    return bytes([0x04, 0x10, 0x02]) + struct.pack("<h", round(v * 10))


def th(t, h):  # 0x100D temperature + humidity
    return bytes([0x0D, 0x10, 0x04]) + struct.pack("<hh", round(t * 10), round(h * 10))


frames = []  # (dev, kind 0 = FE95 / 1 = 181A, data, tag for the expectation)
frames.append(("D", 0, mibeacon_plain("D", 0x0347, 0, t16(11.1)), "D_boot"))
steps = [len(frames)]
for k in range(1, CYCLES + 1):
    frames.append(("B", 0, mibeacon_plain("B", 0x01AA, k, th(20 + k / 10, 40)), "B"))
    frames.append(("C", 0, mibeacon_plain("C", 0x0098, k, t16(21 + k / 10)), "C"))
    frames.append(
        ("A", 0, mibeacon_enc("A", 0x055B, k + 100, t16(22 + k / 10), K1, False), "A")
    )
    if k % 2 == 0:
        bad = mibeacon_enc("A", 0x055B, k + 150, t16(60 + k / 10), K1, False, True)
        frames.append(("A", 0, bad, "A_bad"))
        good = mibeacon_enc("A", 0x055B, k + 150, t16(60 + k / 10), K1, False)
        frames.append(("A", 0, good, "A_after_bad"))
    frames.append(
        ("E", 0, mibeacon_enc("E", 0x1203, k + 50, th(23 + k / 10, 50), K2, True), "E")
    )
    frames.append(("D", 0, mibeacon_plain("D", 0x0347, k + 200, t16(24 + k / 10)), "D"))
    atc = (
        MAC["F"]
        + struct.pack(">h", round((26 + k / 10) * 10))
        + bytes([45, 90])
        + struct.pack(">H", 2950)
    )
    frames.append(("F", 1, atc + bytes([k]), "F"))
    pv = (
        MAC["G"][::-1]
        + struct.pack("<hHH", round((27 + k / 10) * 100), 4600, 3010)
        + bytes([88, k, 0])
    )
    frames.append(("G", 1, pv, "G"))
    steps.append(len(frames))


def simulate(pr):
    """Base keeps one counter shared by every device and checks it before the tag; the PR keeps one per device."""
    last_shared = 0
    last = {}
    pub = {}
    for dev, kind, data, tag in frames:
        if kind == 1:
            cnt = data[12 if dev == "F" else 13]
            if last.get(("x", dev)) == cnt:
                continue
            last["x", dev] = cnt
        elif pr:
            if last.get(dev) == data[4] or tag == "A_bad":
                continue
            last[dev] = data[4]
        else:
            if last_shared == data[4]:
                continue
            last_shared = data[4]
            if tag == "A_bad":
                continue
        pub[tag] = pub.get(tag, 0) + 1
    return pub


expected = {"base": simulate(False), "pr": simulate(True)}
print("base", expected["base"])
print("pr  ", expected["pr"])
Path("expected.json").write_text(json.dumps(expected), encoding="utf-8")


def c_rows(name, rows):
    out = [f"static const uint8_t {name}[{len(rows)}][{len(rows[0][0])}] = {{"]
    out += [
        "  {" + ",".join(f"0x{b:02X}" for b in row) + "}," + (f"  // {c}" if c else "")
        for row, c in rows
    ]
    return "\n".join([*out, "};", ""])


mac_rows = [(MAC[k], "") for k in MACS]
frame_rows = [
    ((bytes([MIDX[d], kind, len(data)]) + data).ljust(28, b"\0"), tag)
    for d, kind, data, tag in frames
]

INJECT = """
inline void xb_fill(esphome::esp32_ble::BLEScanResult &r, const uint8_t *mac, bool fe95, const uint8_t *data,
                    uint8_t len) {
  memcpy(r.bda, mac, 6);
  r.ble_addr_type = 0;
  r.rssi = -50;
  uint8_t n = 0;
  r.ble_adv[n++] = 2;
  r.ble_adv[n++] = 0x01;
  r.ble_adv[n++] = 0x06;
  r.ble_adv[n++] = len + 3;
  r.ble_adv[n++] = 0x16;
  r.ble_adv[n++] = fe95 ? 0x95 : 0x1A;
  r.ble_adv[n++] = fe95 ? 0xFE : 0x18;
  memcpy(r.ble_adv + n, data, len);
  r.adv_data_len = n + len;
  r.scan_rsp_len = 0;
  r.search_evt = ESP_GAP_SEARCH_INQ_RES_EVT;
}

inline void xb_inject(size_t first, size_t last) {
  for (size_t i = first; i < last; i++) {
    const uint8_t *row = XB_FRAMES[i];
    esphome::esp32_ble::BLEScanResult r{};
    xb_fill(r, XB_MACS[row[0]], row[1] == 0, row + 3, row[2]);
    esphome::esp32_ble_tracker::global_esp32_ble_tracker->gap_scan_event_handler(r);
  }
}

inline void xb_hex(const char *label, const std::vector<uint8_t> &v) {
  char buf[80];
  size_t n = 0;
  for (size_t i = 0; i < v.size() && n + 3 < sizeof(buf); i++)
    n += snprintf(buf + n, sizeof(buf) - n, "%02X", v[i]);
  ESP_LOGI("probe", "%s %s", label, buf);
}

// Parse one encrypted frame with the listener directly and show whether it rewrote the shared service data.
inline void xb_rewrite_probe(esphome::ble_device_base::ESPBTDeviceListener *listener) {
  esphome::esp32_ble::BLEScanResult r{};
  xb_fill(r, XB_MACS[0], true, XB_PROBE, sizeof(XB_PROBE));
  esphome::ble_device_base::ESPBTDevice device;
  device.parse_scan_rst(r);
  xb_hex("before", device.get_service_datas()[0].data);
  bool ok = listener->parse_device(device);
  ESP_LOGI("probe", "parse_device returned %d", ok);
  xb_hex("after ", device.get_service_datas()[0].data);
}
"""

ADVERTISE = """
// Sends where a GAP call returned an error when it was queued. Failures the controller reports later in the GAP
// completion events are not counted; those frames show up as missing publishes in count.py.
static uint32_t af_failed = 0;

// Returns false (and counts it) when the address, the data or the start was rejected.
inline bool af_send(int i) {
  const uint8_t *row = AF_FRAMES[i];
  uint8_t adv[31];
  uint8_t n = 0;
  adv[n++] = 2;
  adv[n++] = 0x01;
  adv[n++] = 0x06;
  adv[n++] = row[2] + 3;
  adv[n++] = 0x16;
  adv[n++] = row[1] == 0 ? 0x95 : 0x1A;
  adv[n++] = row[1] == 0 ? 0xFE : 0x18;
  memcpy(adv + n, row + 3, row[2]);
  n += row[2];
  esp_ble_gap_stop_advertising();  // fails harmlessly when nothing is advertising yet
  uint8_t addr[6];
  memcpy(addr, AF_MACS[row[0]], 6);
  esp_err_t e1 = esp_ble_gap_set_rand_addr(addr);
  esp_err_t e2 = esp_ble_gap_config_adv_data_raw(adv, n);
  esp_ble_adv_params_t p{};
  p.adv_int_min = 0x20;  // 20 ms
  p.adv_int_max = 0x20;
  p.adv_type = ADV_TYPE_NONCONN_IND;
  p.own_addr_type = BLE_ADDR_TYPE_RANDOM;
  p.channel_map = ADV_CHNL_ALL;
  p.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  esp_err_t e3 = esp_ble_gap_start_advertising(&p);
  if (e1 == ESP_OK && e2 == ESP_OK && e3 == ESP_OK)
    return true;
  af_failed++;
  ESP_LOGW("af", "frame %d send failed: rand_addr %d adv_data %d start %d", i, e1, e2, e3);
  return false;
}

inline void af_stop() { esp_ble_gap_stop_advertising(); }
"""

if OTA:
    head = '#pragma once\n#include <esp_gap_ble_api.h>\n#include <cstring>\n#include "esphome/core/log.h"\n'
    body = c_rows("AF_MACS", mac_rows) + c_rows("AF_FRAMES", frame_rows)
    body += f"static constexpr int AF_COUNT = {len(frames)};\n" + ADVERTISE
    Path("adv_frames.hpp").write_text(head + body, encoding="utf-8")
else:
    head = '#pragma once\n#include <cstring>\n#include "esphome/components/esp32_ble/ble_scan_result.h"\n'
    probe = mibeacon_enc("A", 0x055B, 0xF5, t16(33.3), K1, False)
    body = c_rows("XB_MACS", mac_rows) + c_rows("XB_FRAMES", frame_rows)
    body += (
        f"static const uint16_t XB_STEPS[{len(steps)}] = {{"
        + ",".join(map(str, steps))
        + "};\n"
    )
    body += f"static constexpr size_t XB_NUM_STEPS = {len(steps)};\n"
    body += (
        "static const uint8_t XB_PROBE[] = {"
        + ",".join(f"0x{b:02X}" for b in probe)
        + "};\n"
    )
    Path("frames.hpp").write_text(head + body + INJECT, encoding="utf-8")
print("frames", len(frames), "longest", max(len(d) for _, _, d, _ in frames))
