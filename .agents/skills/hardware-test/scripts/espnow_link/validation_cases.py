"""Writes one config per validation case; expected outcome in the file name prefix (ok_ / bad_)."""

import pathlib
import textwrap

E32 = "esp32:\n  board: esp32dev\n  framework:\n    type: esp-idf\n"
E66 = "esp8266:\n  board: d1_mini\n"
EN = "espnow:\n  channel: 1\n{extra}"
WIFI = "wifi:\n  ssid: x\n  password: yyyyyyyy\n"


def en(name, link, plat=E32, extra=""):
    pathlib.Path(f"{name}.yaml").write_text(
        f"esphome:\n  name: v{abs(hash(name)) % 10**6}\n{plat}{EN.format(extra=extra)}packet_interface:\n  - platform: espnow\n    id: l\n"
        + textwrap.indent(textwrap.dedent(link), "    "),
        encoding="utf-8",
    )


def udp(name, addrs, link, plat=E32):
    pathlib.Path(f"{name}.yaml").write_text(
        f"esphome:\n  name: v{abs(hash(name)) % 10**6}\n{plat}{WIFI}udp:\n  - id: u\n    addresses: {addrs}\npacket_interface:\n  - platform: udp\n    id: l\n    udp_id: u\n"
        + textwrap.indent(textwrap.dedent(link), "    "),
        encoding="utf-8",
    )


ADDR = "addressing:\n  address: 1\n  destination: 2\n"
ADDR_BC = "addressing:\n  address: 1\n  destination: broadcast\n"
# ESP-NOW
en("bad_en_peer_addressing", "peer_address: 12:22:33:44:55:66\n" + ADDR)
en(
    "ok_en_peer_rel_sess",
    "peer_address: 12:22:33:44:55:66\nreliability:\nsession:\nencryption:\n  key: k\n  rolling_code_enable: true\n",
)
en("ok_en_bc_addressing", ADDR)
en("bad_en_bc_reliability", "reliability:\n")
en("bad_en_bc_session", "session:\n")
en("bad_en_bc_rolling", "encryption:\n  key: k\n  rolling_code_enable: true\n")
en("ok_en_bc_plain_encryption", "encryption: k\n")
en(
    "ok_en_bc_addr_rel_sess_rolling",
    ADDR
    + "reliability:\nsession:\nencryption:\n  key: k\n  rolling_code_enable: true\n",
)
en("bad_en_bc_addr_bcdest_reliability", ADDR_BC + "reliability:\n")
en("ok_en_size_250", "max_packet_size: 250\n")
en("bad_en_size_251_payload_250", "max_packet_size: 251\n")
en(
    "ok_en_size_1470_payload_1470",
    "max_packet_size: 1470\n",
    extra="  max_payload_size: 1470\n",
)
en(
    "bad_en_size_1471_payload_1470",
    "max_packet_size: 1471\n",
    extra="  max_payload_size: 1470\n",
)
en(
    "bad_en_size_1000_payload_800",
    "max_packet_size: 1000\n",
    extra="  max_payload_size: 800\n",
)
en(
    "bad_en8266_payload_300",
    "max_packet_size: 300\n",
    plat=E66,
    extra="  max_payload_size: 300\n",
)
en("ok_en8266_peer_rel", "peer_address: 12:22:33:44:55:66\nreliability:\n", plat=E66)
# UDP
udp("bad_udp_p2p_addressing", '["192.0.2.60"]', ADDR)
udp(
    "ok_udp_p2p_rel_sess_rolling",
    '["192.0.2.60"]',
    "reliability:\nsession:\nencryption:\n  key: k\n  rolling_code_enable: true\n",
)
udp("ok_udp_255_addressing", '["192.0.2.255"]', ADDR)
udp("bad_udp_255_reliability", '["192.0.2.255"]', "reliability:\n")
udp("bad_udp_255_session", '["192.0.2.255"]', "session:\n")
udp(
    "bad_udp_255_rolling",
    '["192.0.2.255"]',
    "encryption:\n  key: k\n  rolling_code_enable: true\n",
)
udp("ok_udp_255_addr_rel", '["192.0.2.255"]', ADDR + "reliability:\nsession:\n")
udp("bad_udp_mcast_reliability", '["239.0.60.53"]', "reliability:\n")
udp("ok_udp_mcast_addressing", '["239.0.60.53"]', ADDR_BC)
udp("bad_udp_two_session", '["192.0.2.60", "192.0.2.61"]', "session:\n")
udp("ok_udp_two_addressing", '["192.0.2.60", "192.0.2.61"]', ADDR)
udp("ok_udp_default_bcast_addressing", '["255.255.255.255"]', ADDR_BC)
udp("bad_udp_default_bcast_reliability", '["255.255.255.255"]', "reliability:\n")
udp(
    "bad_udp_p2p_true_255_addressing",
    '["192.0.2.255"]',
    "point_to_point: true\n" + ADDR,
)
udp(
    "ok_udp_p2p_true_255_reliability",
    '["192.0.2.255"]',
    "point_to_point: true\nreliability:\n",
)
udp(
    "ok_udp_p2p_false_unicast_addressing",
    '["192.0.2.60"]',
    "point_to_point: false\n" + ADDR,
)
udp(
    "bad_udp_p2p_false_unicast_reliability",
    '["192.0.2.60"]',
    "point_to_point: false\nreliability:\n",
)
udp("bad_udp_p2p_true_mcast", '["239.0.60.53"]', "point_to_point: true\n")
udp("bad_udp_p2p_true_two", '["192.0.2.60", "192.0.2.61"]', "point_to_point: true\n")
udp("ok_udp_p2p_8266", '["192.0.2.60"]', "reliability:\nsession:\n", plat=E66)
# added for head f12e235cdd
en("bad_en_peer_group_mac", "peer_address: 01:22:33:44:55:66\n")
en("bad_en_peer_zero_mac", "peer_address: 00:00:00:00:00:00\n")
en(
    "ok_en_peer_explicit_broadcast_addressing",
    "peer_address: FF:FF:FF:FF:FF:FF\n" + ADDR,
)
udp("bad_udp_allones_p2p_true", '["255.255.255.255"]', "point_to_point: true\n")
udp("ok_udp_allones_addressing", '["255.255.255.255"]', ADDR)
