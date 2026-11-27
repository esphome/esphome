"""After hadrive.py: the wizard light must follow its current entity ID (light.wiz_two) and ignore the old one.

usage: lightcheck.py HOST
Sends on and off states for both IDs and prints what the device publishes after each; only light.wiz_two may move
"L wiz". A light publishes only changes, so a repeated state shows nothing.
"""

import sys

from hadrive import Client


def main() -> None:
    c = Client(sys.argv[1])
    c.login("lightcheck")
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
