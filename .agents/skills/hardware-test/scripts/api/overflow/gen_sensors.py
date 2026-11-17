"""gen_sensors.py: write putbe-sensors.yaml for putbe-common.yaml.

It holds the BS_COUNT (60) template sensors bs0..bs59 and the BT_COUNT (4) 800 char text sensors bt0..bt3 that the
flood lambda publishes through the BS and BT pointer arrays of txt.hpp (filled by a boot lambda), and the remote
entity_transport sensors and binary sensors: count, flt and bin from both the base (${peer}b) and the PR (${peer}p)
name of the peer, over both links (et_plain, et_full).
"""

import pathlib

BS_COUNT = 60  # txt.hpp
BT_COUNT = 4  # txt.hpp
LINKS = ("plain", "full")
PEERS = ("b", "p")


def ptr_fill(kind: str, arr: str, prefix: str, n: int) -> list[str]:
    ids = ", ".join(f"id({prefix}{i})" for i in range(n))
    return [
        "      - lambda: |-",
        f"          {kind} *all[] = {{{ids}}};",
        f"          for (size_t i = 0; i < {arr}_COUNT; i++) {arr}[i] = all[i];",
    ]


def remote(link: str, peer: str, rid: str, name: str) -> list[str]:
    return [
        "  - platform: entity_transport",
        f"    entity_transport_id: et_{link}",
        f"    id: r_{link}_{peer}_{rid}",
        f"    name: R {link} {peer} {name}",
        f"    provider: ${{peer}}{peer}",
        f"    remote_id: {rid}",
    ]


out = ["esphome:", "  on_boot:", "    priority: 600", "    then:"]
out += ptr_fill("sensor::Sensor", "BS", "bs", BS_COUNT)
out += ptr_fill("text_sensor::TextSensor", "BT", "bt", BT_COUNT)
out.append("sensor:")
for i in range(BS_COUNT):
    out += [
        "  - platform: template",
        f"    id: bs{i}",
        f"    name: S{i}",
        "    update_interval: never",
        "    accuracy_decimals: 0",
    ]
for link in LINKS:
    for peer in PEERS:
        out += remote(link, peer, "count", "count")
        out += remote(link, peer, "flt", "flt") + ["    accuracy_decimals: 3"]
out.append("binary_sensor:")
for link in LINKS:
    for peer in PEERS:
        out += remote(link, peer, "bin", "bin")
out.append("text_sensor:")
for i in range(BT_COUNT):
    out += [
        "  - platform: template",
        f"    id: bt{i}",
        f"    name: T{i}",
        "    update_interval: never",
    ]
pathlib.Path(__file__).with_name("putbe-sensors.yaml").write_text(
    "\n".join(out) + "\n", encoding="utf-8"
)
