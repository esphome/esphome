"""After hadrive.py: the wizard light must follow its current entity ID (light.wiz_two) and ignore the old one.

usage: lightcheck.py HOST
Sends on and off states for both IDs and prints what the device publishes after each; only light.wiz_two may move
"L wiz". A light publishes only changes, so a repeated state shows nothing.
"""

import struct
import sys

from hadrive import LIST_TYPES, Client, f_bytes, f_varint, fields


def main() -> None:
    c = Client(sys.argv[1])
    c.send(
        1, f_bytes(1, "lightcheck") + f_varint(2, 1) + f_varint(3, 14)
    )  # HelloRequest, API 1.14
    c.pump(10, until=2)
    c.send(3)  # ConnectRequest
    c.pump(1)
    c.send(11)  # ListEntitiesRequest
    for msg_type, payload in c.pump(10, until=19):
        if msg_type in LIST_TYPES:
            d = dict(fields(payload))
            c.names[struct.unpack("<I", d[2])[0]] = d[3].decode()
    c.send(20)  # SubscribeStatesRequest
    c.collect("init", None, 3)
    c.send(38)  # SubscribeHomeAssistantStatesRequest
    c.collect("subs", None, 4)
    for eid, st in (
        ("light.wiz_one", "on"),
        ("light.wiz_two", "on"),
        ("light.wiz_one", "off"),
        ("light.wiz_two", "off"),
    ):
        print(f"send {eid} = {st}")
        c.ha_state(eid, st)
        c.collect("  ->", None, 2.5)


if __name__ == "__main__":
    main()
