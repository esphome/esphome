"""ESP32 Trinity board definitions."""

from . import BoardConfig

# ESP32 Trinity
# https://esp32trinity.com/
# Pin assignments from: https://github.com/witnessmenow/ESP32-Trinity/blob/master/FAQ.md
BoardConfig(
    "esp32-trinity",
    r1_pin=25,
    g1_pin=26,
    b1_pin=27,
    r2_pin=14,
    g2_pin=12,
    b2_pin=13,
    a_pin=23,
    b_pin=19,
    c_pin=5,
    d_pin=17,
    e_pin=18,
    lat_pin=4,
    oe_pin=15,
    clk_pin=16,
)
