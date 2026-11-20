"""Write the RP2 UART pin choice configs: u<set><board>.yaml for sets a, b, c on the Pico W and Pico 2 W.

Each bus is (id, tx, rx, loop, inverted); -1 leaves the pin out. The configs are written next to lb.hpp.
"""

import pathlib

SETS = {
    "a": [
        ("ua0", 0, 1, 1, 0),
        ("ua1", 8, 9, 1, 0),
        ("up23", 2, 3, 1, 0),
        ("um1621", 16, 21, 1, 0),
    ],
    "b": [
        ("ub0", 12, 13, 1, 0),
        ("ub1", 20, 21, 1, 0),
        ("ubinv", 16, 17, 1, 1),
        ("ubt6", 6, -1, 0, 0),
        ("ubr27", -1, 27, 0, 0),
    ],
    "c": [
        ("uc0", 16, 17, 1, 0),
        ("ucr9", -1, 9, 0, 0),
        ("uct26", 26, -1, 0, 0),
        ("ucr7", -1, 7, 0, 0),
    ],
}
BOARDS = {"w": "rpipicow", "2": "rpipico2w"}


def pin(n: int, inv: int) -> str:
    return f"{{number: {n}, inverted: true}}" if inv else str(n)


def flag(v: int) -> str:
    return "true" if v else "false"


def config(
    set_name: str, board: str, buses: list[tuple[str, int, int, int, int]]
) -> str:
    uarts, checks, chk_only, dumps = [], [], [], []
    for bid, tx, rx, loop, inv in buses:
        u = f"  - id: {bid}\n    baud_rate: 115200\n"
        if tx >= 0:
            u += f"    tx_pin: {pin(tx, inv)}\n"
        if rx >= 0:
            u += f"    rx_pin: {pin(rx, inv)}\n"
        uarts.append(u)
        checks.append(
            f'          lb::check("{bid}", id({bid}), {tx}, {rx}, {flag(loop)}, {flag(inv)});'
        )
        chk_only.append(
            f'          lb::check("{bid}", id({bid}), {tx}, {rx}, false, false);'
        )
        dumps.append(f"          id({bid}).dump_config();")
    nl = "\n"
    return f"""esphome:
  name: ${{name}}
  includes: [lb.hpp]
  on_boot:
    priority: -100
    then:
      - delay: 4s
      - lambda: |-
{nl.join(checks)}
{nl.join(dumps)}
          ESP_LOGI("lb", "SETDONE {set_name}");
rp2:
  board: {board}
logger:
  level: VERBOSE
uart:
{"".join(uarts)}interval:
  - interval: 15s
    then:
      - lambda: |-
{nl.join(chk_only)}
"""


for s, buses in SETS.items():
    for b, board in BOARDS.items():
        (pathlib.Path(__file__).parent / f"u{s}{b}.yaml").write_text(
            config(s, board, buses), encoding="utf-8"
        )
