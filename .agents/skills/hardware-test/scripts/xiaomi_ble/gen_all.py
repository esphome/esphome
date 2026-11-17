"""One device per Xiaomi platform (20 listener-only platforms, the three that stay components, plus ATC/PVVX):
writes frames_all.hpp (injected by xball.yaml), xball_entities.yaml (the platform configs, each entity logs
"PUB <dev>.<key> <value>" on every publish) and expected_all.json (publishes per entity) for count_all.py.
Usage: gen_all.py [S3|DEVKIT]   (only changes the comment in the yaml)
Every frame is injected twice in a row: MiBeacon devices drop the repeat by frame counter, the FD50/181D/181A
parsers without a counter publish it again. Binary sensors only publish on a change (trigger_on_initial_state on)."""

import json
from pathlib import Path
import struct

from cryptography.hazmat.primitives.ciphers.aead import AESCCM

CYCLES = 10
KEY = bytes.fromhex("6c0d1a3e5b7f29c4a18e90d2f3b46a57")
KEYHEX = KEY.hex()


def mac(i):
    return bytes.fromhex(f"A4C138001{i:03X}"[:12])


def plain(m, pid, cnt, obj, fc=0x50):
    hdr = bytes([fc, 0x20]) + struct.pack("<H", pid) + bytes([cnt])
    return hdr + (m[::-1] if fc & 0x10 else b"") + obj


def enc(m, pid, cnt, obj, short=False, fc=None):
    """MiBeacon v4/v5 encrypted frame; long = with MAC (fc 0x58), short = without (fc 0x48)."""
    ext = bytes([cnt, 0x00, 0x00])
    if short:
        hdr = bytes([fc or 0x48, 0x59]) + struct.pack("<H", pid) + bytes([cnt])
    else:
        hdr = (
            bytes([fc or 0x58, 0x59]) + struct.pack("<H", pid) + bytes([cnt]) + m[::-1]
        )
    nonce = m[::-1] + hdr[2:5] + ext
    ct = AESCCM(KEY, tag_length=4).encrypt(nonce, obj, b"\x11")
    return hdr + ct[:-4] + ext + ct[-4:]


def o(t, data):
    return struct.pack("<H", t) + bytes([len(data)]) + data


def t16(v):
    return o(0x1004, struct.pack("<h", round(v * 10)))


def th(t, h):
    return o(0x100D, struct.pack("<hh", round(t * 10), round(h * 10)))


# name: (platform, kind, yaml body, sensors {key: publishes-per-new-frame or "bin"}, frame builder k -> [(uuid, data)])
DEVS = {}


def dev(name, platform, body, ents, build, moved=True, hub=None):
    DEVS[name] = {
        "platform": platform,
        "body": body,
        "ents": ents,
        "build": build,
        "moved": moved,
        "hub": hub,
    }


FE95, FD50, S181D, S181A, S181B = 0xFE95, 0xFD50, 0x181D, 0x181A, 0x181B
# moved platforms (sensor)
dev(
    "cgd1",
    "xiaomi_cgd1",
    "bindkey",
    {"temperature": "s"},
    lambda m, k: [(FE95, enc(m, 0x0576, k, t16(30 + k / 10)))],
)
dev(
    "cgdk2",
    "xiaomi_cgdk2",
    "bindkey",
    {"temperature": "s"},
    lambda m, k: [(FE95, enc(m, 0x066F, k, t16(31 + k / 10)))],
)
dev(
    "cgg1",
    "xiaomi_cgg1",
    "",
    {"temperature": "s", "humidity": "s"},
    lambda m, k: [(FE95, plain(m, 0x0347, k, th(32 + k / 10, 40 + k)))],
)
dev(
    "gcls002",
    "xiaomi_gcls002",
    "",
    {"temperature": "s"},
    lambda m, k: [(FE95, plain(m, 0x03BC, k, t16(33 + k / 10)))],
)
dev(
    "hhccjcy01",
    "xiaomi_hhccjcy01",
    "",
    {"temperature": "s", "moisture": "s"},
    lambda m, k: [
        (FE95, plain(m, 0x0098, 2 * k, t16(34 + k / 10))),
        (FE95, plain(m, 0x0098, 2 * k + 1, o(0x1008, bytes([k + 10])))),
    ],
)
dev(
    "hhccjcy10",
    "xiaomi_hhccjcy10",
    "",
    {
        "temperature": "r",
        "moisture": "r",
        "illuminance": "r",
        "conductivity": "r",
        "battery_level": "r",
    },
    lambda m, k: [
        (
            FD50,
            bytes([k + 20])
            + struct.pack(">h", 350 + k)
            + (1000 + k).to_bytes(3, "big")
            + bytes([90 - k])
            + struct.pack(">H", 300 + k),
        )
    ],
)
dev(
    "hhccpot002",
    "xiaomi_hhccpot002",
    "",
    {"moisture": "s"},
    lambda m, k: [(FE95, plain(m, 0x015D, k, o(0x1008, bytes([k + 30]))))],
)
dev(
    "jqjcy01ym",
    "xiaomi_jqjcy01ym",
    "",
    {"formaldehyde": "s"},
    lambda m, k: [(FE95, plain(m, 0x02DF, k, o(0x1010, struct.pack("<H", k))))],
)
dev(
    "lywsd02",
    "xiaomi_lywsd02",
    "",
    {"temperature": "s"},
    lambda m, k: [(FE95, plain(m, 0x045B, k, t16(36 + k / 10)))],
)
dev(
    "lywsd02mmc",
    "xiaomi_lywsd02mmc",
    "bindkey",
    {"temperature": "s", "humidity": "s"},
    lambda m, k: [(FE95, enc(m, 0x2542, k, th(37 + k / 10, 50 + k), short=True))],
)
dev(
    "lywsd03mmc",
    "xiaomi_lywsd03mmc",
    "bindkey",
    {"temperature": "s"},
    lambda m, k: [(FE95, enc(m, 0x055B, k, t16(38 + k / 10)))],
)
dev(
    "lywsdcgq",
    "xiaomi_lywsdcgq",
    "",
    {"temperature": "s", "humidity": "s"},
    lambda m, k: [(FE95, plain(m, 0x01AA, k, th(39 + k / 10, 60 + k)))],
)
dev(
    "mhoc303",
    "xiaomi_mhoc303",
    "",
    {"temperature": "s", "humidity": "s"},
    lambda m, k: [(FE95, plain(m, 0x06D3, k, th(40 + k / 10, 70 + k)))],
)
dev(
    "mhoc401",
    "xiaomi_mhoc401",
    "bindkey",
    {"temperature": "s"},
    lambda m, k: [(FE95, enc(m, 0x0387, k, t16(41 + k / 10)))],
)
dev(
    "miscale",
    "xiaomi_miscale",
    "",
    {"weight": "r"},
    lambda m, k: [
        (
            S181D,
            bytes([0x22])
            + struct.pack("<H", 14000 + 20 * k)
            + bytes([0xE8, 0x07, 11, 14, 12, 0, k]),
        )
    ],
)
dev(
    "xmwsdj04mmc",
    "xiaomi_xmwsdj04mmc",
    "bindkey",
    {"temperature": "s", "humidity": "s"},
    lambda m, k: [(FE95, enc(m, 0x1203, k, th(42 + k / 10, 45 + k), short=True))],
)
# moved platforms (binary_sensor; the main entity is the platform itself)
dev(
    "cgpr1",
    "xiaomi_cgpr1",
    "bindkey",
    {"": "b", "idle_time": "s"},
    lambda m, k: [
        (
            FE95,
            enc(
                m,
                0x0A83,
                k,
                o(0x1017, struct.pack("<I", 0 if k % 2 else 60 * k)),
                short=True,
            ),
        )
    ],
)
dev(
    "mccgq02hl",
    "xiaomi_mccgq02hl",
    "bindkey",
    {"": "b"},
    lambda m, k: [(FE95, enc(m, 0x098B, k, o(0x0019, bytes([k % 2]))))],
)
dev(
    "mjyd02yla",
    "xiaomi_mjyd02yla",
    "bindkey",
    {"": "z", "light": "b"},
    lambda m, k: [(FE95, enc(m, 0x07F6, k, o(0x1018, bytes([k % 2]))))],
)
dev(
    "wx08zm",
    "xiaomi_wx08zm",
    "",
    {"": "b", "tablet": "s"},
    lambda m, k: [
        (
            FE95,
            plain(
                m, 0x040A, k, o(0x1012, bytes([k % 2])) + o(0x1013, bytes([100 - k]))
            ),
        )
    ],
)
# still components
dev(
    "mue4094rt",
    "xiaomi_mue4094rt",
    "",
    {"": "m"},
    lambda m, k: [
        (FE95, plain(m, 0x03DD, k, o(0x000F, (200 + k).to_bytes(3, "little")), fc=0x40))
    ],
    moved=False,
)
dev(
    "rtcgq02lm",
    "xiaomi_rtcgq02lm",
    "bindkey",
    {"light": "b", "battery_level": "s"},
    lambda m, k: [
        (FE95, enc(m, 0x0A8D, 2 * k, o(0x1018, bytes([k % 2])))),
        (FE95, enc(m, 0x0A8D, 2 * k + 1, o(0x100A, bytes([80 + k])))),
    ],
    moved=False,
    hub=True,
)


def body_scale(m, k):
    obj = o(
        0x4E16,
        bytes([1])
        + struct.pack("<I", 7000 + 10 * k)
        + struct.pack("<I", 1700000000 + k),
    )
    assert len(obj) == 12
    return [(FE95, enc(m, 0x1234, k, obj, short=True))]


dev(
    "body_scale",
    "xiaomi_body_scale",
    "bindkey",
    {"weight": "s", "profile_id": "s"},
    body_scale,
    moved=False,
)
dev(
    "atc",
    "atc_mithermometer",
    "",
    {"temperature": "s"},
    lambda m, k: [
        (
            S181A,
            m
            + struct.pack(">h", 250 + k)
            + bytes([45, 90])
            + struct.pack(">H", 2950)
            + bytes([k]),
        )
    ],
    moved=None,
)
dev(
    "pvvx",
    "pvvx_mithermometer",
    "",
    {"temperature": "s"},
    lambda m, k: [
        (S181A, m[::-1] + struct.pack("<hHH", 2600 + k, 4600, 3010) + bytes([88, k, 0]))
    ],
    moved=None,
)

MACS = {n: mac(i + 1) for i, n in enumerate(DEVS)}
frames = []  # (dev index, uuid, data, cycle)
for k in range(1, CYCLES + 1):
    for i, (n, d) in enumerate(DEVS.items()):
        for uuid, data in d["build"](MACS[n], k):
            assert (
                len(data) <= 24
                or uuid != FE95
                or not (data[0] & 0x08)
                or n == "mccgq02hl"
            ), (n, len(data))
            frames.append((i, uuid, data))
            frames.append((i, uuid, data))  # repeat: dropped by counter-based dedup

# Expected publishes per entity
exp = {}
for n, d in DEVS.items():
    nf = len(d["build"](MACS[n], 1))
    for key, kind in d["ents"].items():
        ent = f"{n}.{key or 'main'}"
        if kind == "s":
            per = 1 if n != "hhccjcy01" else 1
            exp[ent] = CYCLES * per
        elif kind == "r":  # no counter: the repeat publishes again
            exp[ent] = CYCLES * 2
        elif (
            kind == "b"
        ):  # alternating value, dedup on change; mccgq02hl starts closed = the initial off state
            exp[ent] = CYCLES - (n == "mccgq02hl")
        elif kind == "z":  # configured, no frame sets it
            exp[ent] = 0
        elif kind == "m":  # mue4094rt: motion true once, false 5 s after the last frame
            exp[ent] = 2
    del nf
Path("expected_all.json").write_text(
    json.dumps(exp, indent=1, sort_keys=True), encoding="utf-8"
)

rows = [f"static const uint8_t XA_MACS[{len(MACS)}][6] = {{"]
rows += [
    "  {" + ",".join(f"0x{b:02X}" for b in m) + "},  // " + n for n, m in MACS.items()
]
rows += ["};", f"static const uint8_t XA_FRAMES[{len(frames)}][28] = {{"]
for i, uuid, data in frames:
    row = bytes([i, uuid & 0xFF, uuid >> 8, len(data)]) + data
    rows.append("  {" + ",".join(f"0x{b:02X}" for b in row.ljust(28, b"\0")) + "},")
per = len(frames) // CYCLES
rows += [
    "};",
    f"static constexpr size_t XA_COUNT = {len(frames)};",
    f"static constexpr size_t XA_PER_CYCLE = {per};",
]
HDR = '#pragma once\n#include <cstring>\n#include "esphome/components/esp32_ble/ble_scan_result.h"\n'
INJECT = """
inline void xa_inject(size_t first, size_t last) {
  for (size_t i = first; i < last && i < XA_COUNT; i++) {
    const uint8_t *row = XA_FRAMES[i];
    esphome::esp32_ble::BLEScanResult r{};
    memcpy(r.bda, XA_MACS[row[0]], 6);
    r.ble_addr_type = 0;
    r.rssi = -50;
    uint8_t n = 0, len = row[3];
    r.ble_adv[n++] = 2;
    r.ble_adv[n++] = 0x01;
    r.ble_adv[n++] = 0x06;
    r.ble_adv[n++] = len + 3;
    r.ble_adv[n++] = 0x16;
    r.ble_adv[n++] = row[1];
    r.ble_adv[n++] = row[2];
    memcpy(r.ble_adv + n, row + 4, len);
    r.adv_data_len = n + len;
    r.scan_rsp_len = 0;
    r.search_evt = ESP_GAP_SEARCH_INQ_RES_EVT;
    esphome::esp32_ble_tracker::global_esp32_ble_tracker->gap_scan_event_handler(r);
  }
}
"""
Path("frames_all.hpp").write_text(
    HDR + "\n".join(rows) + "\n" + INJECT, encoding="utf-8"
)


def macstr(m):
    return ":".join(f"{b:02X}" for b in m)


def ent_yaml(ent, kind, ind):
    """name plus a publish log at indent ind."""
    sp = " " * ind
    if kind in ("b", "m", "z"):
        return (
            f"{sp}trigger_on_initial_state: true\n{sp}on_state:\n"
            f'{sp}  - lambda: \'ESP_LOGI("pub", "PUB {ent} %d", x);\'\n'
        )
    return f'{sp}on_value:\n{sp}  - lambda: \'ESP_LOGI("pub", "PUB {ent} %.3f", x);\'\n'


sensor, binary, top = [], [], []
for n, d in DEVS.items():
    m = macstr(MACS[n])
    common = f"    mac_address: {m}\n" + (
        f"    bindkey: {KEYHEX}\n" if d["body"] == "bindkey" else ""
    )
    subs = {key: kind for key, kind in d["ents"].items() if key}
    if d["hub"]:
        top.append(f"{d['platform']}:\n  - id: hub_{n}\n" + common)
        for kinds, lst in (({"b"}, binary), ({"s", "r"}, sensor)):
            blk = f"  - platform: {d['platform']}\n    id: hub_{n}\n"
            for key, kind in subs.items():
                if kind in kinds:
                    blk += f'    {key}:\n      name: "{n} {key}"\n' + ent_yaml(
                        f"{n}.{key}", kind, 6
                    )
            lst.append(blk)
        continue
    blk = f"  - platform: {d['platform']}\n" + common
    if "" in d["ents"]:
        blk += f'    name: "{n} main"\n' + ent_yaml(f"{n}.main", d["ents"][""], 4)
    for key, kind in subs.items():
        blk += f'    {key}:\n      name: "{n} {key}"\n' + ent_yaml(
            f"{n}.{key}", kind, 6
        )
    (binary if "" in d["ents"] else sensor).append(blk)

out = (
    "".join(top)
    + "\nsensor:\n"
    + "".join(sensor)
    + "\nbinary_sensor:\n"
    + "".join(binary)
)
Path("xball_entities.yaml").write_text(out, encoding="utf-8")
print(
    "devices", len(DEVS), "frames", len(frames), "per cycle", per, "entities", len(exp)
)
