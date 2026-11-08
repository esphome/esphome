"""gen_case.py CASE [cases.txt]: print the config for one RMT budget case.

A cases.txt line is `<case> <s3|c6> <kind>:<symbols>...`: kind s is an esp32_rmt_led_strip, t a remote_transmitter,
r a remote_receiver; symbols is rmt_symbols, or def to leave it at the default. Item i gets the i-th pin of the board
and the id so<i>, tx<i> or rx<i>; an interval logs `STATUS so0=<is_failed> ...` every 3 s.
"""

import pathlib
import sys

BOARDS = {
    "s3": (
        "esp32-s3-devkitc-1",
        "  hardware_uart: USB_SERIAL_JTAG\n",
        [4, 5, 6, 7, 15, 16],
    ),
    "c6": (
        "esp32-c6-devkitc-1",
        "  hardware_uart: UART0\n",
        [4, 5, 6, 7, 0, 1],
    ),
}


def main() -> int:
    case = sys.argv[1]
    path = pathlib.Path(
        sys.argv[2]
        if len(sys.argv) > 2
        else pathlib.Path(__file__).parent / "cases.txt"
    )
    rows = {
        f[0]: f[1:]
        for line in path.read_text(encoding="utf-8").splitlines()
        if (f := line.split())
    }
    if case not in rows:
        print(f"unknown case {case}", file=sys.stderr)
        return 1
    board, items = rows[case][0], rows[case][1:]
    name, uart, pins = BOARDS[board]
    blocks: dict[str, list[str]] = {"s": [], "t": [], "r": []}
    ids = []
    for i, item in enumerate(items):
        kind, sym = item.split(":")
        extra = "" if sym == "def" else f"    rmt_symbols: {sym}\n"
        if kind == "s":
            ids.append(f"so{i}")
            blocks["s"].append(
                f"  - platform: esp32_rmt_led_strip\n    id: strip{i}\n    output_id: so{i}\n    name: Strip {i}\n"
                f"    pin: GPIO{pins[i]}\n    num_leds: 8\n    chipset: WS2812\n    rgb_order: GRB\n{extra}"
            )
        elif kind == "t":
            ids.append(f"tx{i}")
            blocks["t"].append(
                f"  - id: tx{i}\n    pin: GPIO{pins[i]}\n    carrier_duty_percent: 50%\n{extra}"
            )
        else:
            ids.append(f"rx{i}")
            blocks["r"].append(
                f"  - id: rx{i}\n    pin: GPIO{pins[i]}\n    dump: []\n{extra}"
            )
    out = f"esphome:\n  name: rmt{board}\nesp32:\n  board: {name}\n  framework:\n    type: esp-idf\nlogger:\n{uart}"
    for key, title in (
        ("s", "light"),
        ("t", "remote_transmitter"),
        ("r", "remote_receiver"),
    ):
        if blocks[key]:
            out += f"{title}:\n" + "".join(blocks[key])
    fmt = " ".join(f"{i}=%d" for i in ids)
    args = ", ".join(f"(int) id({i})->is_failed()" for i in ids)
    out += (
        "interval:\n  - interval: 3s\n    then:\n"
        f'      - lambda: \'ESP_LOGI("chk", "STATUS {fmt}", {args});\'\n'
    )
    sys.stdout.write(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
