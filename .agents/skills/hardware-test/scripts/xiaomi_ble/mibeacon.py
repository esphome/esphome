"""MiBeacon frame builders and the C++ scan result injector shared by gen.py and gen_all.py."""

import struct

from cryptography.hazmat.primitives.ciphers.aead import AESCCM


def plain(mac, pid, cnt, obj, fc=0x50):
    """Unencrypted MiBeacon; the MAC is included when fc has bit 0x10."""
    hdr = bytes([fc, 0x20]) + struct.pack("<H", pid) + bytes([cnt])
    return hdr + (mac[::-1] if fc & 0x10 else b"") + obj


def enc(mac, pid, cnt, obj, key, short=False, fc=None, version=0x59, corrupt=False):
    """Encrypted MiBeacon v4/v5 (AES-CCM, tag 4, AAD 0x11): long with the MAC (fc 0x58), short without (fc 0x48).
    corrupt flips the first tag byte."""
    ext = bytes([cnt, 0x00, 0x00])
    hdr = (
        bytes([fc or (0x48 if short else 0x58), version])
        + struct.pack("<H", pid)
        + bytes([cnt])
    )
    if not short:
        hdr += mac[::-1]
    nonce = mac[::-1] + hdr[2:5] + ext
    ct = AESCCM(key, tag_length=4).encrypt(nonce, obj, b"\x11")
    body, tag = ct[:-4], ct[-4:]
    if corrupt:
        tag = bytes([tag[0] ^ 0xFF]) + tag[1:]
    return hdr + body + ext + tag


def o(t, data):
    """One MiBeacon object: type, length, data."""
    return struct.pack("<H", t) + bytes([len(data)]) + data


def t16(v):
    """0x1004 temperature object."""
    return o(0x1004, struct.pack("<h", round(v * 10)))


def th(t, h):
    """0x100D temperature and humidity object."""
    return o(0x100D, struct.pack("<hh", round(t * 10), round(h * 10)))


def c_rows(name, rows):
    """A C byte table from (bytes, comment) rows of equal length."""
    out = [f"static const uint8_t {name}[{len(rows)}][{len(rows[0][0])}] = {{"]
    out += [
        "  {" + ",".join(f"0x{b:02X}" for b in row) + "}," + (f"  // {c}" if c else "")
        for row, c in rows
    ]
    return "\n".join([*out, "};", ""])


# Fills a scan result with flags plus one service data field (16 bit uuid) and hands it to the tracker.
XB_FILL = """
inline void xb_fill(esphome::esp32_ble::BLEScanResult &r, const uint8_t *mac, uint16_t uuid, const uint8_t *data,
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
  r.ble_adv[n++] = uuid & 0xFF;
  r.ble_adv[n++] = uuid >> 8;
  memcpy(r.ble_adv + n, data, len);
  r.adv_data_len = n + len;
  r.scan_rsp_len = 0;
  r.search_evt = ESP_GAP_SEARCH_INQ_RES_EVT;
}

inline void xb_inject_one(const uint8_t *mac, uint16_t uuid, const uint8_t *data, uint8_t len) {
  esphome::esp32_ble::BLEScanResult r{};
  xb_fill(r, mac, uuid, data, len);
  esphome::esp32_ble_tracker::global_esp32_ble_tracker->gap_scan_event_handler(r);
}
"""
