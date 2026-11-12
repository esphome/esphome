"""Write spi<board>.yaml for each board: a hardware and a software SPI bus, several spi_device instances and a max7219.

usage: gen.py   then run.sh BOARD LABEL SECONDS builds one of them with the name spi<board><LABEL>.
Pins are (clk, mosi, miso, ...); a device entry is (id, bus, spi_mode, bit_order, data_rate, cs_pin). A software bus
with mosi == miso is the loopback. Classic ESP32 also uses GPIO14 as the capture peripheral's spare CS (spitest.hpp).
"""

from pathlib import Path

HERE = Path(__file__).parent

BOARDS = {
    "e32": {
        "platform": "esp32:\n  board: pico32\n  framework:\n    type: esp-idf\n",
        "logger": "logger:\n  level: DEBUG\n",
        "hw": {"clk": 25, "mosi": 26, "miso": 27, "interface": "spi2"},
        "sw": {"clk": 18, "mosi": 19, "miso": 21},
        "devs": [
            ("hA", "hw", 0, "msb_first", "1MHz", 32),
            ("hB", "hw", 3, "lsb_first", "4MHz", 33),
            ("hE", "hw", 1, "lsb_first", "2MHz", 4),
            ("hF", "hw", 2, "msb_first", "8MHz", 13),
            ("sC", "sw", 1, "msb_first", "500kHz", 5),
            ("sD", "sw", 2, "lsb_first", "200kHz", 23),
        ],
        "mx": (22, "1MHz"),
    },
    "c3": {
        "platform": "esp32:\n  variant: esp32c3\n  board: esp32-c3-devkitm-1\n  framework:\n    type: esp-idf\n",
        "logger": "logger:\n  level: DEBUG\n  hardware_uart: USB_SERIAL_JTAG\n",
        "hw": {"clk": 4, "mosi": 5, "miso": 6, "interface": "spi2"},
        "sw": {"clk": 0, "mosi": 1, "miso": 1},
        "devs": [
            ("hA", "hw", 0, "msb_first", "80kHz", 7),
            ("hB", "hw", 3, "lsb_first", "100kHz", 10),
            ("hE", "hw", 1, "lsb_first", "90kHz", 3),
            ("hF", "hw", 2, "msb_first", "80kHz", 9),
            ("sC", "sw", 1, "msb_first", "10kHz", 20),
            ("sD", "sw", 2, "lsb_first", "5kHz", 21),
        ],
        "mx": (8, "80kHz"),
    },
    "e66": {
        "platform": "esp8266:\n  board: huzzah\n",
        "logger": "logger:\n  level: DEBUG\n",
        "hw": {"clk": 14, "mosi": 13, "miso": 12, "interface": "hardware"},
        "sw": {"clk": 4, "mosi": 5, "miso": 5},
        "devs": [
            ("hA", "hw", 0, "msb_first", "10kHz", 15),
            ("hB", "hw", 3, "lsb_first", "20kHz", 0),
            ("sC", "sw", 1, "msb_first", "10kHz", 2),
            ("sD", "sw", 2, "lsb_first", "5kHz", 16),
        ],
        "mx": None,
    },
    "rp": {
        "platform": "rp2040:\n  board: rpipico2w\n",
        "logger": "logger:\n  level: DEBUG\n",
        "hw": {"clk": 18, "mosi": 19, "miso": 16, "interface": "hardware"},
        "sw": {"clk": 10, "mosi": 11, "miso": 11},
        "devs": [
            ("hA", "hw", 0, "msb_first", "10kHz", 17),
            ("hB", "hw", 3, "lsb_first", "20kHz", 26),
            ("hE", "hw", 1, "lsb_first", "15kHz", 21),
            ("hF", "hw", 2, "msb_first", "10kHz", 22),
            ("sC", "sw", 1, "msb_first", "10kHz", 12),
            ("sD", "sw", 2, "lsb_first", "5kHz", 13),
        ],
        "mx": (20, "10kHz"),
    },
}


def pin(n: int, other: bool = False) -> str:
    if other:
        return f"\n      number: {n}\n      allow_other_uses: true"
    return f" {n}"


def gen(board: str) -> str:
    b = BOARDS[board]
    hw, sw = b["hw"], b["sw"]
    shared = sw["mosi"] == sw["miso"]
    out = [
        (
            "esphome:\n  name: ${name}\n  includes:\n    - spitest.hpp\n"
            "  on_boot:\n    - priority: -100\n      then:\n"
            "        - lambda: |-\n"
            '            spitest::log_heap("boot");\n'
        ),
    ]
    lines = []
    for nm, bus, mode, order, _rate, cs in b["devs"]:
        p = hw if bus == "hw" else sw
        lines.append(
            f'            spitest::add("{nm}", id({nm}), {mode}, {"true" if order == "lsb_first" else "false"}, '
            f"{cs}, {p['clk']}, {p['mosi']}, {p['miso']}, {'true' if bus == 'hw' else 'false'});"
        )
    if b["mx"]:
        lines.append(
            f"            spitest::add_mx(id(mx), {b['mx'][0]}, {hw['clk']}, {hw['mosi']});"
        )
    out.append("\n".join(lines) + "\n")
    out.append("        - delay: 2s\n        - lambda: spitest::start();\n\n")
    out.append(b["platform"] + "\n" + b["logger"] + "\n")
    out.append(
        "interval:\n  - interval: 20ms\n    then:\n      - lambda: spitest::tick();\n\n"
    )
    out.append(
        "spi:\n"
        f"  - id: hwbus\n    interface: {hw['interface']}\n    clk_pin: {hw['clk']}\n"
        f"    mosi_pin: {hw['mosi']}\n    miso_pin: {hw['miso']}\n"
        f"  - id: swbus\n    interface: software\n    clk_pin: {sw['clk']}\n"
        f"    mosi_pin:{pin(sw['mosi'], shared)}\n    miso_pin:{pin(sw['miso'], shared)}\n\n"
    )
    out.append("spi_device:\n")
    for nm, bus, mode, order, rate, cs in b["devs"]:
        out.append(
            f"  - id: {nm}\n    spi_id: {'hwbus' if bus == 'hw' else 'swbus'}\n    cs_pin: {cs}\n"
            f"    spi_mode: {mode}\n    bit_order: {order}\n    data_rate: {rate}\n"
        )
    if b["mx"]:
        out.append(
            "\ndisplay:\n  - platform: max7219\n    id: mx\n    spi_id: hwbus\n"
            f"    cs_pin: {b['mx'][0]}\n    data_rate: {b['mx'][1]}\n    num_chips: 1\n"
            '    update_interval: never\n    lambda: it.print("01234567");\n'
        )
    return "".join(out)


if __name__ == "__main__":
    for board in BOARDS:
        (HERE / f"spi{board}.yaml").write_text(gen(board), encoding="utf-8")
        print(HERE / f"spi{board}.yaml")
