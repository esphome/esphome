"""Writes one host config per tcp link validation rule into out-valid/; expected outcome in the name prefix (ok_/bad_).

Run `esphome config` on each from the tree under test.
"""

import pathlib

OUT = pathlib.Path(__file__).parent / "out-valid"
LINK = "  - platform: tcp\n    id: l\n"
HOST = "    host: 10.1.2.3\n    port: 6060\n"
ET = (
    "sensor:\n  - platform: template\n    id: s\n    lambda: return 1;\n"
    "entity_transport:\n  - packet_interface: l\n    timeout: 1min\n    sensors: [s]\n"
)


def write(name: str, link: str, host: str = HOST, tail: str = "") -> None:
    node = name.replace("_", "-")[:31]
    text = f"esphome:\n  name: {node}\nhost:\nlogger:\npacket_interface:\n{LINK}{host}{link}{tail}"
    (OUT / f"{name}.yaml").write_text(text, encoding="utf-8")


OUT.mkdir(exist_ok=True)
write("bad_reliability", "    reliability:\n      timeout: 200ms\n")
write("bad_session", "    session:\n      keep_alive: 2s\n")
write("bad_addressing", "    addressing:\n      address: 1\n      destination: 2\n")
write("bad_maxpkt1023", "    max_packet_size: 1023\n")
write("ok_maxpkt1022", "    max_packet_size: 1022\n")
write("bad_ka500ms", "    keep_alive: 500ms\n")
write("ok_ka1s", "    keep_alive: 1s\n")
write("bad_ka2h", "    keep_alive: 2h\n")
write("ok_ka1h", "    keep_alive: 1h\n")
write("bad_to_eq_ka", "    keep_alive: 5s\n    timeout: 5s\n")
write("ok_to_gt_ka", "    keep_alive: 5s\n    timeout: 5001ms\n")
write("bad_to4h", "    keep_alive: 1h\n    timeout: 4h\n")
write("ok_to3h", "    keep_alive: 1h\n    timeout: 3h\n")
write(
    "bad_rolling", "    encryption:\n      key: abc\n      rolling_code_enable: true\n"
)
write(
    "ok_enc_frag",
    "    encryption: abc\n    fragmentation:\n      max_packet_size: 4096\n",
)
write("bad_client_allowed", "    allowed_ips: [10.0.0.0/8]\n")
write("bad_client_nohost", "", host="    port: 6060\n")
write("bad_ipv6host", "", host="    host: fe80::1\n    port: 6060\n")
write("bad_et_timeout", "", tail=ET)
