"""gen.py: write led.yaml, one config with every driver on its own pins and a scripted capture per driver."""

import pathlib

DRIVERS = [
    # name, hub yaml, clk, dat, lat, outputs [(id, extra yaml)], steps [(name, {id: level})]
]


def hub(platform, clk, dat, extra=""):
    return f"{platform}:\n  id: {platform}_hub\n  clock_pin: {clk}\n  data_pin: {dat}\n{extra}"


def outs(platform, chans, extra=None):
    extra = extra or {}
    y = ""
    for ch in chans:
        y += f"  - platform: {platform}\n    id: {platform}_c{ch}\n    channel: {ch}\n"
        for k, v in extra.get(ch, {}).items():
            y += f"    {k}: {v}\n"
    return y


bp_steps = [
    ("all_off", {}),
    ("c1_full", {1: 1.0}),
    ("rgb_mix", {1: 0.5, 2: 0.25, 3: 0.001}),
    ("same_again", {1: 0.5, 2: 0.25, 3: 0.001}),
    ("white_only", {1: 0, 2: 0, 3: 0, 4: 0.3, 5: 1.0}),
    ("all_five", {1: 0.1, 2: 0.2, 3: 0.3, 4: 0.4, 5: 0.5}),
    ("lsb", {1: 0.0005, 2: 0.002, 3: 0.999, 4: 0.0, 5: 0.0}),
    ("back_off", {1: 0, 2: 0, 3: 0, 4: 0, 5: 0}),
]
five = [1, 2, 3, 4, 5]
DRIVERS = [
    (
        "bp5758d",
        hub("bp5758d", 25, 26),
        25,
        26,
        -1,
        outs(
            "bp5758d",
            five,
            {
                c: {"current": cur}
                for c, cur in zip(five, [10, 20, 63, 64, 90], strict=True)
            },
        ),
        bp_steps,
    ),
    (
        "bp1658cj",
        hub(
            "bp1658cj",
            23,
            4,
            "  max_power_color_channels: 4\n  max_power_white_channels: 6\n",
        ),
        23,
        4,
        -1,
        outs("bp1658cj", [0, 1, 2, 3, 4]),
        [(n, {k - 1: v for k, v in d.items()}) for n, d in bp_steps],
    ),
    (
        "sm2235",
        hub(
            "sm2235",
            18,
            19,
            "  max_power_color_channels: 9\n  max_power_white_channels: 5\n",
        ),
        18,
        19,
        -1,
        outs("sm2235", [0, 1, 2, 3, 4]),
        [(n, {k - 1: v for k, v in d.items()}) for n, d in bp_steps],
    ),
    (
        "sm2335",
        hub(
            "sm2335",
            21,
            22,
            "  max_power_color_channels: 3\n  max_power_white_channels: 12\n",
        ),
        21,
        22,
        -1,
        outs("sm2335", [0, 1, 2, 3, 4]),
        [(n, {k - 1: v for k, v in d.items()}) for n, d in bp_steps],
    ),
    (
        "tlc5971",
        hub("tlc5971", 27, 32, "  num_chips: 2\n"),
        27,
        32,
        -1,
        outs("tlc5971", [0, 1, 11, 12, 23], {1: {"max_power": 0.8}}),
        [
            ("c0_full", {0: 1.0}),
            ("mix", {1: 0.5, 12: 0.25, 23: 1 / 65535}),
            ("same_again", {1: 0.5, 12: 0.25, 23: 1 / 65535}),
            ("c11_mid", {11: 0.333}),
            ("all_off", {0: 0, 1: 0, 11: 0, 12: 0, 23: 0}),
        ],
    ),
    (
        "tlc5947",
        hub("tlc5947", 33, 13, "  lat_pin: 14\n  num_chips: 2\n"),
        33,
        13,
        14,
        outs("tlc5947", [0, 1, 23, 24, 47], {1: {"max_power": 0.8}}),
        [
            ("c0_full", {0: 1.0}),
            ("mix", {1: 0.5, 24: 0.25, 47: 1 / 4095}),
            ("same_again", {1: 0.5, 24: 0.25, 47: 1 / 4095}),
            ("c23_mid", {23: 0.333}),
            ("all_off", {0: 0, 1: 0, 23: 0, 24: 0, 47: 0}),
        ],
    ),
    (
        "my9231",
        hub("my9231", 16, 17, "  num_channels: 6\n  num_chips: 2\n  bit_depth: 16\n"),
        16,
        17,
        -1,
        outs("my9231", [0, 1, 2, 3, 4, 5]),
        [
            ("c0_full", {0: 1.0}),
            ("mix", {1: 0.5, 4: 0.25, 5: 1 / 65535}),
            ("same_again", {1: 0.5, 4: 0.25, 5: 1 / 65535}),
            ("c3_mid", {3: 0.333}),
            ("all_off", {0: 0, 1: 0, 2: 0, 3: 0, 4: 0, 5: 0}),
        ],
    ),
    (
        "sm16716",
        hub("sm16716", 5, 12, "  num_channels: 6\n  num_chips: 2\n"),
        5,
        12,
        -1,
        outs("sm16716", [0, 1, 2, 3, 4, 5]),
        [
            ("c0_full", {0: 1.0}),
            ("mix", {1: 0.5, 4: 0.25, 5: 1 / 255}),
            ("same_again", {1: 0.5, 4: 0.25, 5: 1 / 255}),
            ("c3_mid", {3: 0.333}),
            ("all_off", {0: 0, 1: 0, 2: 0, 3: 0, 4: 0, 5: 0}),
        ],
    ),
]

y = """# substitutions: name
esphome:
  name: ${name}
  includes:
    - sniff.hpp
  on_boot:
    priority: -100
    then:
      - delay: 3s
      - lambda: sniff::selftest(15, 0, 0);
      - lambda: sniff::selftest(15, 0, 1);
"""
for d in DRIVERS:
    y += f"      - script.execute: run_{d[0]}\n      - script.wait: run_{d[0]}\n"
y += """      - lambda: ESP_LOGI("sniff", "DONE");

esp32:
  board: esp32dev
  framework:
    type: esp-idf
    advanced:
      include_builtin_idf_components:
        - esp_driver_spi
        - esp_driver_pcnt

external_components:
  - source:
      type: local
      path: ext
    components: [sniff]

sniff:

logger:
  level: DEBUG

"""
for d in DRIVERS:
    y += d[1] + "\n"
y += "output:\n"
for d in DRIVERS:
    y += d[5]
y += "\nscript:\n"
for name, _, clk, dat, lat, _, steps in DRIVERS:
    y += f"  - id: run_{name}\n    then:\n"
    y += f'      - lambda: sniff::use("{name}", {clk}, {dat}, {lat});\n'
    for sname, levels in steps:
        sets = " ".join(
            f"id({name}_c{c})->set_level({float(v)!r}f);" for c, v in levels.items()
        )
        y += f'      - lambda: sniff::open(); {sets}\n      - delay: 200ms\n      - lambda: sniff::close("{sname}");\n'
    # timing: one change written by calling the hub loop directly
    first = steps[1][1]
    c, v = next(iter(first.items()))
    for i, lv in enumerate([0.75, 0.0]):
        y += (
            f"      - lambda: |-\n          sniff::open(); id({name}_c{c})->set_level({lv}f);\n"
            f"          int64_t t0 = esp_timer_get_time(); id({name}_hub)->loop(); int64_t t1 = esp_timer_get_time();\n"
            f'          ESP_LOGI("sniff", "TIME {name} loop %d us", (int) (t1 - t0));\n'
            f'      - delay: 200ms\n      - lambda: sniff::close("timed{i}");\n'
        )
    # a pass with no change at all
    y += '      - lambda: sniff::open();\n      - delay: 500ms\n      - lambda: sniff::close("idle");\n'
pathlib.Path("led.yaml").write_text(y, encoding="utf-8")
