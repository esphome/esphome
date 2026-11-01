"""Drive homeassistant buttons over a plaintext native API connection and print every action call the device sends.

usage: habdrive.py HOST

Subscribes to Home Assistant actions, presses "HA button" and "HA input" 3 times each, presses "HA wizard" before and
after setting its wizard input (WizardInputSetRequest, key = FNV-1 hash of the input's id), then once after an
invalid set that the device must reject. Matches ha-button-common.yaml.
"""

import argparse
import socket
import struct
import sys
import time

WIZARD_INPUT_ID = "hab_wiz"


def fnv1_hash(text: str) -> int:
    value = 2166136261
    for ch in text:
        value = (value * 16777619) & 0xFFFFFFFF
        value ^= ord(ch)
    return value


def varint(v: int) -> bytes:
    out = bytearray()
    while True:
        b = v & 0x7F
        v >>= 7
        out.append(b | 0x80 if v else b)
        if not v:
            return bytes(out)


def read_varint(buf: bytes, pos: int) -> tuple[int, int]:
    value = shift = 0
    while True:
        b = buf[pos]
        pos += 1
        value |= (b & 0x7F) << shift
        if not b & 0x80:
            return value, pos
        shift += 7


def fields(payload: bytes) -> list[tuple[int, object]]:
    out = []
    pos = 0
    while pos < len(payload):
        key, pos = read_varint(payload, pos)
        num, wire = key >> 3, key & 7
        if wire == 0:
            value, pos = read_varint(payload, pos)
        elif wire == 2:
            length, pos = read_varint(payload, pos)
            value = payload[pos : pos + length]
            pos += length
        elif wire == 5:
            value = payload[pos : pos + 4]
            pos += 4
        else:
            raise ValueError(f"wire type {wire}")
        out.append((num, value))
    return out


def frame(msg_type: int, payload: bytes) -> bytes:
    return b"\x00" + varint(len(payload)) + varint(msg_type) + payload


def f_bytes(num: int, data: bytes) -> bytes:
    return varint((num << 3) | 2) + varint(len(data)) + data


def f_varint(num: int, value: int) -> bytes:
    return varint(num << 3) + varint(value)


def f_fixed32(num: int, value: int) -> bytes:
    return varint((num << 3) | 5) + struct.pack("<I", value)


class Client:
    def __init__(self, host: str) -> None:
        self.sock = socket.create_connection((host, 6053), timeout=10)
        self.buf = bytearray()

    def send(self, msg_type: int, payload: bytes = b"") -> None:
        self.sock.sendall(frame(msg_type, payload))

    def pump(self, secs: float, until: int | None = None) -> list[tuple[int, bytes]]:
        """Read frames for secs, answering pings; stop early at a frame of type until."""
        end = time.time() + secs
        got = []
        while time.time() < end:
            self.sock.settimeout(max(0.05, end - time.time()))
            try:
                chunk = self.sock.recv(4096)
            except TimeoutError:
                break
            if not chunk:
                raise ConnectionError("the device closed the API connection")
            self.buf.extend(chunk)
            while len(self.buf) >= 3:
                try:
                    length, pos = read_varint(self.buf, 1)
                    msg_type, pos = read_varint(self.buf, pos)
                except IndexError:
                    break  # the header itself is split across reads
                if len(self.buf) < pos + length:
                    break
                got.append((msg_type, bytes(self.buf[pos : pos + length])))
                del self.buf[: pos + length]
                if msg_type == 7:  # PingRequest
                    self.send(8)
                if msg_type == until:
                    return got
        return got


def print_actions(frames: list[tuple[int, bytes]]) -> list[str]:
    """Print the action calls in frames and return them as "service entity_id=..." strings."""
    actions = []
    for msg_type, payload in frames:
        if msg_type != 35:  # HomeassistantActionRequest
            continue
        service, data = "", []
        for num, value in fields(payload):
            if num == 1:
                service = value.decode()
            elif num == 2:
                kv = dict(fields(value))
                data.append(f"{kv.get(1, b'').decode()}={kv.get(2, b'').decode()}")
        actions.append(f"{service} {' '.join(data)}")
        print(f"  ACTION {actions[-1]}")
    if not actions:
        print("  no action")
    return actions


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("host")
    c = Client(parser.parse_args().host)
    # The hello reply can take more than a second; wait for each reply type instead of a fixed time.
    c.send(1, f_bytes(1, b"habdrive") + f_varint(2, 1) + f_varint(3, 14))
    c.pump(10, until=2)
    c.send(3)
    c.pump(2)
    c.send(11)  # ListEntitiesRequest
    keys = {}
    for msg_type, payload in c.pump(10, until=19):
        if msg_type == 61:  # ListEntitiesButtonResponse
            d = dict(fields(payload))
            keys[d[3].decode()] = struct.unpack("<I", d[2])[0]
    print("buttons", sorted(keys))
    missing = {"HA button", "HA input", "HA wizard"} - set(keys)
    if missing:
        sys.exit(
            f"FAIL: the device does not list {sorted(missing)} (internal: false set?)"
        )
    c.send(34)  # SubscribeHomeassistantServicesRequest
    c.pump(1)

    failures = []

    def press(name: str, label: str, expected: str | None) -> None:
        """Press name; expected is the one action call it must send, or None for no call."""
        print(f"{label}: press {name}")
        c.send(62, f_fixed32(1, keys[name]))
        actions = print_actions(c.pump(1.5))
        if actions != ([expected] if expected else []):
            failures.append(
                f"{label}: got {actions}, expected {expected or 'no action'}"
            )

    def wizard(entity_id: str) -> None:
        print(f"wizard set {WIZARD_INPUT_ID} = {entity_id!r}")
        c.send(
            158,
            f_fixed32(1, fnv1_hash(WIZARD_INPUT_ID)) + f_bytes(2, entity_id.encode()),
        )
        c.pump(1)

    doorbell = "button.press entity_id=button.doorbell"
    for r in range(3):
        press("HA button", f"R{r} T1", doorbell)
        press("HA input", f"R{r} T2", "input_button.press entity_id=input_button.chime")
    press("HA wizard", "T3 wizard unset", None)
    # A wizard set has no reply; the next press shows whether the device took it
    wizard("input_button.wizard_one")
    press(
        "HA wizard",
        "T4 wizard input_button",
        "input_button.press entity_id=input_button.wizard_one",
    )
    wizard("button.wizard_two")
    press("HA wizard", "T5 wizard button", "button.press entity_id=button.wizard_two")
    wizard("not_an_entity")  # rejected, the previous entity stays
    press(
        "HA wizard",
        "T6 wizard after invalid set",
        "button.press entity_id=button.wizard_two",
    )
    press("HA button", "T7 again", doorbell)
    if failures:
        print("FAIL: " + "; ".join(failures))
        sys.exit(1)
    print("PASS")


if __name__ == "__main__":
    main()
