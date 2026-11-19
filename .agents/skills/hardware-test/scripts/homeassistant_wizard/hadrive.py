"""Drive every homeassistant platform over a plaintext native API connection, playing Home Assistant.

usage: hadrive.py HOST [--no-light]   (exits 1 on any failed check)

Matches ha-all-common.yaml: one fixed entity_id entity and one wizard-linked entity per platform. The client
subscribes to entity states, Home Assistant actions and Home Assistant state subscriptions, then:
  A  sends Home Assistant states for the fixed entities (and for a wizard ID that is not set yet), commands every
     writer, and checks that the wizard-linked ones send nothing;
  B  sets every wizard input (WizardInputSetRequest), records the subscriptions the device sends for it, sends
     states for the new IDs and commands the writers;
  C  changes every wizard input to a second ID: old IDs must be ignored, new ones followed;
  D  sends an invalid ID, which the device must reject.
Prints canonical lines (SUB, STATE, ACTION, CHECK) so two builds can be diffed, and PASS or FAIL at the end.
"""

import argparse
from collections.abc import Callable
import pathlib
import struct
import sys
import time
from typing import NamedTuple

sys.path.insert(
    0, str(pathlib.Path(__file__).resolve().parents[1] / "homeassistant_button")
)
from habdrive import (  # noqa: E402
    Client as BaseClient,
    f_bytes,
    f_fixed32,
    f_varint,
    fields,
    fnv1_hash,
    varint,
)


def f_float(num: int, value: float) -> bytes:
    return varint((num << 3) | 5) + struct.pack("<f", value)


# ListEntities response type -> platform
LIST_TYPES = {
    12: "binary_sensor",
    15: "light",
    16: "sensor",
    17: "switch",
    18: "text_sensor",
    49: "number",
    52: "select",
    61: "button",
    97: "text",
}
STATE_TYPES = {
    21: "bool",
    24: "bool",
    25: "float",
    26: "bool",
    27: "str",
    50: "float",
    53: "str",
    98: "str",
}


class Client(BaseClient):
    def __init__(self, host: str) -> None:
        super().__init__(host)
        self.keys: dict[str, int] = {}
        self.names: dict[int, str] = {}
        self.failures: list[str] = []
        self.last: dict[
            str, str
        ] = {}  # the last state the device published, by entity name

    def login(
        self, name: str, connect_wait: float = 1, list_entities: bool = True
    ) -> None:
        """Hello (API 1.14) and Connect, then ListEntities into keys and names."""
        self.send(1, f_bytes(1, name) + f_varint(2, 1) + f_varint(3, 14))
        self.pump(10, until=2)
        self.send(3)
        self.pump(connect_wait)
        if not list_entities:
            return
        self.send(11)
        for msg_type, payload in self.pump(10, until=19):
            if msg_type in LIST_TYPES:
                d = dict(fields(payload))
                key, name = struct.unpack("<I", d[2])[0], d[3].decode()
                self.keys[name] = key
                self.names[key] = name

    def report(
        self, frames: list[tuple[int, bytes]], label: str
    ) -> dict[str, list[str]]:
        """Print SUB, STATE and ACTION lines for frames and return them by kind."""
        out = self.parse(frames)
        for line in out["STATE"]:
            name, _, value = line.partition(" = ")
            self.last[name] = value
        for kind in ("SUB", "STATE", "ACTION"):
            for line in sorted(out[kind]) if kind == "SUB" else out[kind]:
                print(f"  {label} {kind} {line}")
        return out

    def collect(
        self,
        label: str,
        done: Callable[[dict[str, list[str]]], bool] | None = None,
        timeout: float = 6.0,
        settle: float = 0.5,
    ) -> dict[str, list[str]]:
        """Read until done(parsed) holds (or for timeout without one), then settle more seconds for strays."""
        frames: list[tuple[int, bytes]] = []
        end = time.time() + timeout
        while time.time() < end:
            frames += self.pump(0.2)
            if done is not None and done(self.parse(frames)):
                break
        frames += self.pump(settle)
        return self.report(frames, label)

    def parse(self, frames: list[tuple[int, bytes]]) -> dict[str, list[str]]:
        out: dict[str, list[str]] = {"SUB": [], "STATE": [], "ACTION": []}
        for msg_type, payload in frames:
            f = fields(payload)
            d = dict(f)
            if msg_type == 39:  # SubscribeHomeAssistantStateResponse
                attr = d.get(2, b"").decode() or "-"
                line = f"{d.get(1, b'').decode()} {attr}{' once' if d.get(3) else ''}"
                out["SUB"].append(line)
            elif msg_type in STATE_TYPES:
                key = struct.unpack("<I", d[1])[0]
                kind = STATE_TYPES[msg_type]
                if kind == "float":
                    value = d.get(2)
                    value = (
                        f"{struct.unpack('<f', value)[0]:g}"
                        if value is not None
                        else "0"
                    )
                elif kind == "bool":
                    value = "ON" if d.get(2) else "OFF"
                else:
                    value = repr(d.get(2, b"").decode())
                if msg_type not in (24, 26) and d.get(
                    3
                ):  # field 3 of a light state is brightness
                    value += " missing"
                out["STATE"].append(f"{self.names.get(key, key)} = {value}")
            elif msg_type == 35:  # HomeassistantActionRequest
                service, data = "", []
                for num, value in f:
                    if num == 1:
                        service = value.decode()
                    elif num in (2, 3):
                        kv = dict(fields(value))
                        data.append(
                            f"{kv.get(1, b'').decode()}={kv.get(2, b'').decode()}"
                        )
                out["ACTION"].append(f"{service} {' '.join(data)}")
        return out

    def check(self, label: str, ok: bool, detail: str) -> None:
        print(f"  CHECK {label}: {'ok' if ok else 'FAIL'} {detail}")
        if not ok:
            self.failures.append(f"{label}: {detail}")

    def ha_state(self, entity_id: str, state: str, attribute: str = "") -> None:
        payload = f_bytes(1, entity_id) + f_bytes(2, state)
        if attribute:
            payload += f_bytes(3, attribute)
        self.send(40, payload)

    def wizard(self, input_id: str, entity_id: str) -> None:
        self.send(158, f_fixed32(1, fnv1_hash(input_id)) + f_bytes(2, entity_id))


def feed(c: Client, platform: str, eid: str, n: int) -> None:
    """Send what Home Assistant sends for a platform; n varies the values between rounds."""
    if platform == "sensor":
        c.ha_state(eid, f"{20 + n}.5")
    elif platform == "sensor_attr":
        c.ha_state(eid, "heat")  # the state itself must not reach an attribute sensor
        c.ha_state(eid, f"{30 + n}", "temperature")
    elif platform == "binary_sensor":
        c.ha_state(eid, "on" if n % 2 else "off")
    elif platform == "text_sensor":
        c.ha_state(eid, f"text{n}")
    elif platform == "number":
        c.ha_state(eid, "0", "min")
        c.ha_state(eid, "100", "max")
        c.ha_state(eid, "1", "step")
        c.ha_state(eid, f"{40 + n}")
    elif platform == "switch":
        c.ha_state(eid, "on" if n % 2 else "off")
    elif platform == "select":
        c.ha_state(eid, "['eco', 'comfort', 'away']", "options")
        c.ha_state(eid, ["eco", "comfort", "away"][n % 3])
    elif platform == "text":
        c.ha_state(eid, "0", "min")
        c.ha_state(eid, "100", "max")
        c.ha_state(eid, "text", "mode")
        c.ha_state(eid, f"note{n}")
    elif platform == "light":
        c.ha_state(eid, "['onoff']", "supported_color_modes")
        c.ha_state(eid, "onoff", "color_mode")
        c.ha_state(eid, "on" if n % 2 else "off")


def expected_state(platform: str, n: int) -> str | None:
    return {
        "sensor": f"{20 + n}.5",
        "sensor_attr": f"{30 + n}",
        "binary_sensor": "ON" if n % 2 else "OFF",
        "text_sensor": repr(f"text{n}"),
        "number": f"{40 + n}",
        "switch": "ON" if n % 2 else "OFF",
        "select": repr(["eco", "comfort", "away"][n % 3]),
        "text": repr(f"note{n}"),
        "light": "ON" if n % 2 else "OFF",
    }.get(platform)


class Row(NamedTuple):
    fix: str | None  # name of the fixed entity_id entity
    fix_id: str | None
    wiz: str | None  # name of the wizard-linked entity
    input_id: str | None  # ESPHome id of its wizard input
    platform: str
    one: str | None  # the first and second entity IDs the wizard input is set to
    two: str | None


# fmt: off
PLATFORMS = [
    Row("S fix", "sensor.outside", "S wiz", "s_wiz", "sensor", "sensor.wiz_one", "sensor.wiz_two"),
    Row(None, None, "SA wiz", "sa_wiz", "sensor_attr", "climate.wiz_one", "climate.wiz_two"),
    Row("B fix", "binary_sensor.door", "B wiz", "b_wiz", "binary_sensor", "binary_sensor.wiz_one",
        "binary_sensor.wiz_two"),
    Row("T fix", "sensor.weather", "T wiz", "t_wiz", "text_sensor", "sensor.wiz_text_one", "sensor.wiz_text_two"),
    Row("N fix", "input_number.target", "N wiz", "n_wiz", "number", "input_number.wiz_one", "number.wiz_two"),
    Row("SW fix", "switch.porch", "SW wiz", "sw_wiz", "switch", "input_boolean.wiz_one", "switch.wiz_two"),
    Row("SEL fix", "input_select.mode", "SEL wiz", "sel_wiz", "select", "input_select.wiz_one", "select.wiz_two"),
    Row("TX fix", "input_text.note", "TX wiz", "tx_wiz", "text", "input_text.wiz_one", "text.wiz_two"),
    Row("L fix", "light.kitchen", "L wiz", "l_wiz", "light", "light.wiz_one", "light.wiz_two"),
]
# fmt: on
# the fixed buttons come from ha-button-common.yaml
WIZARD_BUTTON = Row(
    None,
    None,
    "HA wizard",
    "hab_wiz",
    "button",
    "input_button.wiz_one",
    "button.wiz_two",
)

# The subscriptions each platform sends for one entity ID
SUBS = {
    "button": [],
    "sensor": ["{} -"],
    "sensor_attr": ["{} temperature"],
    "binary_sensor": ["{} -"],
    "text_sensor": ["{} -"],
    "number": ["{} -", "{} max once", "{} min once", "{} step once"],
    "switch": ["{} -"],
    "select": ["{} -", "{} options"],
    "text": ["{} -", "{} max once", "{} min once", "{} mode once"],
    "light": [
        "{} -",
        "{} brightness",
        "{} color_mode",
        "{} color_temp_kelvin",
        "{} max_color_temp_kelvin",
        "{} min_color_temp_kelvin",
        "{} rgb_color",
        "{} supported_color_modes",
    ],
}


def command(c: Client, name: str, platform: str, n: int) -> str | None:
    """Command a writer entity; returns the action it must send for entity ID {}, or None for a reader."""
    key = c.keys[name]
    if platform == "number":
        c.send(51, f_fixed32(1, key) + f_float(2, 10 + n))
        return "{d}.set_value entity_id={e} value=" + f"{10 + n}"
    if platform == "switch":
        state = n % 2 == 0  # the opposite of what feed() reported
        c.send(33, f_fixed32(1, key) + f_varint(2, int(state)))
        return f"homeassistant.turn_{'on' if state else 'off'} entity_id={{e}}"
    if platform == "select":
        option = ["eco", "comfort", "away"][(n + 1) % 3]
        c.send(54, f_fixed32(1, key) + f_bytes(2, option))
        return "{d}.select_option entity_id={e} option=" + option
    if platform == "text":
        c.send(99, f_fixed32(1, key) + f_bytes(2, f"cmd{n}"))
        return "{d}.set_value entity_id={e} value=" + f"cmd{n}"
    if platform == "light":
        state = n % 2 == 0
        c.send(32, f_fixed32(1, key) + f_varint(2, 1) + f_varint(3, int(state)))
        return f"light.turn_{'on' if state else 'off'} entity_id={{e}}"
    if platform == "button":
        c.send(62, f_fixed32(1, key))
        return "{d}.press entity_id={e}"
    return None


def action_for(template: str, eid: str) -> str:
    # number, select, text and button call the action of the entity's own domain (input_number.set_value, ...)
    if template.startswith("{d}"):
        return template.format(d=eid.split(".", maxsplit=1)[0], e=eid)
    return template.format(e=eid)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("host")
    parser.add_argument(
        "--no-light", action="store_true", help="the config has no wizard light"
    )
    args = parser.parse_args()
    c = Client(args.host)
    c.login("hadrive", connect_wait=2)
    print("entities", len(c.keys), sorted(c.keys))
    # only the wizard light may be left out (builds before #21908 cannot compile it)
    rows: list[Row] = []
    for row in [*PLATFORMS, WIZARD_BUTTON]:
        if row.wiz and row.wiz not in c.keys:
            c.check(
                f"{row.wiz} listed", args.no_light and row.wiz == "L wiz", "missing"
            )
            row = row._replace(wiz=None, input_id=None)
        if row.fix or row.wiz:
            rows.append(row)
    wizard = [row for row in rows if row.wiz]
    c.send(20)  # SubscribeStatesRequest
    c.report(c.pump(3), "INIT")
    c.send(34)  # SubscribeHomeassistantServicesRequest
    c.pump(1)
    c.send(38)  # SubscribeHomeAssistantStatesRequest
    expected = sorted(
        s.format(r.fix_id) for r in rows if r.fix for s in SUBS[r.platform]
    )
    subs = c.collect("A", lambda o: len(o["SUB"]) >= len(expected), 10, 2)["SUB"]
    c.check(
        "A subscriptions are the fixed entities only",
        sorted(subs) == expected,
        f"{len(subs)} subs",
    )

    def drive(
        label: str, entries: list[tuple[str, str | None, str]], n: int, stale: list[str]
    ) -> None:
        """entries: (name, entity ID or None if unset, platform). stale: IDs the device must ignore."""
        for eid in stale:
            c.ha_state(eid, "999")
            c.ha_state(eid, "999", "temperature")
        prev = dict(c.last)
        for _, eid, platform in entries:
            if eid:
                feed(c, platform, eid, n)
        wants = {
            f"{name} = {expected_state(p, n)}"
            for name, eid, p in entries
            if eid and expected_state(p, n)
        }
        got = c.collect(f"{label}-feed", lambda o: wants <= set(o["STATE"]), 8, 1.5)
        states = got["STATE"]
        for name, eid, platform in entries:
            want = expected_state(platform, n)
            if want is None:
                continue
            mine = [s for s in states if s.startswith(f"{name} = ")]
            if eid:
                # A light publishes only a change, so a report of the state it already has publishes nothing
                same = platform == "light" and not mine and prev.get(name) == want
                c.check(
                    f"{label} {name} follows {eid}",
                    f"{name} = {want}" in mine or same,
                    str(mine),
                )
            else:
                c.check(f"{label} {name} unset publishes nothing", not mine, str(mine))
        c.check(
            f"{label} stale IDs ignored",
            not any("999" in s or "= 'heat'" in s for s in states),
            "",
        )
        for name, eid, platform in entries:
            if platform in ("sensor", "sensor_attr", "binary_sensor", "text_sensor"):
                continue
            template = command(c, name, platform, n)
            if eid is None:
                got = c.collect(f"{label}-cmd {name}", None, 2)
            else:
                got = c.collect(f"{label}-cmd {name}", lambda o: bool(o["ACTION"]), 6)
            actions = got["ACTION"]
            if eid is None:
                c.check(
                    f"{label} {name} unset sends nothing", not actions, str(actions)
                )
                continue
            want = action_for(template, eid)
            c.check(
                f"{label} {name} action", actions == [want], f"{actions} want {want}"
            )
            if platform == "light":
                c.ha_state(
                    eid, "on" if n % 2 == 0 else "off"
                )  # Home Assistant echoes the change
                c.collect(f"{label}-echo {name}", None, 1.5)

    fixed = [(r.fix, r.fix_id, r.platform) for r in rows if r.fix]
    fixed += [
        ("HA button", "button.doorbell", "button"),
        ("HA input", "input_button.chime", "button"),
    ]
    wiz = [(r.wiz, None, r.platform) for r in wizard]
    print("== A: fixed entities, wizard inputs not set")
    drive("A", fixed + wiz, 1, ["sensor.wiz_one", "climate.wiz_one"])

    def set_all(label: str, idx: int) -> dict[str, str]:
        ids = {}
        for row in wizard:
            name = row.wiz
            eid = row.one if idx == 1 else row.two
            ids[name] = eid
            c.wizard(row.input_id, eid)
            want = sorted(s.format(eid) for s in SUBS[row.platform])
            n_want = len(want)
            if want:
                subs = c.collect(
                    f"{label}-set {name}",
                    lambda o, n=n_want: len(o["SUB"]) >= n,
                    6,
                    0.5,
                )["SUB"]
            else:
                subs = c.collect(f"{label}-set {name}", None, 2)["SUB"]
            c.check(
                f"{label} {name} subscribes to {eid}",
                sorted(subs) == want,
                f"{sorted(subs)}",
            )
        return ids

    print("== B: wizard inputs set to the first IDs")
    ids = set_all("B", 1)
    drive("B", [(name, ids[name], p) for name, _, p in wiz], 2, [])
    print("== C: wizard inputs changed to the second IDs")
    old = [row.one for row in wizard]
    ids = set_all("C", 2)
    drive("C", [(name, ids[name], p) for name, _, p in wiz], 3, old)
    print("== D: invalid ID rejected")
    for input_id in ("n_wiz", "hab_wiz", "s_wiz"):
        c.wizard(input_id, "not_an_entity")
        subs = c.collect(f"D-set {input_id}", None, 2)["SUB"]
        c.check(f"D {input_id} invalid set sends no subscription", not subs, str(subs))
    drive(
        "D",
        [
            (name, ids[name], p)
            for name, _, p in wiz
            if name in ("N wiz", "HA wizard", "S wiz")
        ],
        4,
        [],
    )
    print("== E: a new connection gets every subscription with the current IDs")
    c.sock.close()
    c2 = Client(parser.parse_args().host)
    c2.login("hadrive2", list_entities=False)
    c2.send(38)
    expected = sorted(
        [s.format(r.fix_id) for r in rows if r.fix for s in SUBS[r.platform]]
        + [s.format(ids[r.wiz]) for r in wizard for s in SUBS[r.platform]]
    )
    subs = c2.collect("E", lambda o: len(o["SUB"]) >= len(expected), 10, 2)["SUB"]
    c.check(
        "E subscriptions on reconnect",
        sorted(subs) == expected,
        f"{len(subs)} subs, want {len(expected)}",
    )
    if c.failures:
        print("FAIL: " + "; ".join(c.failures))
        sys.exit(1)
    print("PASS")


if __name__ == "__main__":
    main()
