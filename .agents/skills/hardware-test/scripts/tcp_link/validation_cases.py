import pathlib

cases = {
    "reliability": "    reliability:\n      timeout: 200ms\n",
    "session": "    session:\n      keep_alive: 2s\n",
    "addressing": "    addressing:\n      address: 1\n      destination: 2\n",
    "maxpkt1023": "    max_packet_size: 1023\n",
    "maxpkt1022": "    max_packet_size: 1022\n",
    "ka500ms": "    keep_alive: 500ms\n",
    "ka1s": "    keep_alive: 1s\n",
    "ka2h": "    keep_alive: 2h\n",
    "ka1h": "    keep_alive: 1h\n",
    "to_eq_ka": "    keep_alive: 5s\n    timeout: 5s\n",
    "to_gt_ka": "    keep_alive: 5s\n    timeout: 5001ms\n",
    "to4h": "    keep_alive: 1h\n    timeout: 4h\n",
    "to3h": "    keep_alive: 1h\n    timeout: 3h\n",
    "rolling": "    encryption:\n      key: abc\n      rolling_code_enable: true\n",
    "enc_frag": "    encryption: abc\n    fragmentation:\n      max_packet_size: 4096\n",
    "client_allowed": "    allowed_ips: [10.0.0.0/8]\n",
    "client_nohost": None,
    "ipv6host": "IPV6",
    "et_timeout": "ET",
}
base = "esphome:\n  name: v{n}\nhost:\nlogger:\npacket_interface:\n  - platform: tcp\n    id: l\n    host: 10.1.2.3\n    port: 6060\n"
for name, extra in cases.items():
    if extra is None:
        y = base.replace("    host: 10.1.2.3\n", "")
    elif extra == "IPV6":
        y = base.replace("10.1.2.3", "fe80::1")
    elif extra == "ET":
        y = (
            base
            + "sensor:\n  - platform: template\n    id: s\n    lambda: return 1;\nentity_transport:\n  - packet_interface: l\n    timeout: 1min\n    sensors: [s]\n"
        )
    else:
        y = base + extra
    pathlib.Path(f"{name}.yaml").write_text(
        y.replace("{n}", name.replace("_", "")), encoding="utf-8"
    )
