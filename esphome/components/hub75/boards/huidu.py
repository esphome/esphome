"""Huidu board definitions."""

from . import BoardConfig

# Huidu HD-WF2
BoardConfig(
    "huidu-hd-wf2",
    r1_pin=2,
    g1_pin=6,
    b1_pin=10,
    r2_pin=3,
    g2_pin=7,
    b2_pin=11,
    a_pin=39,
    b_pin=38,
    c_pin=37,
    d_pin=36,
    e_pin=21,
    lat_pin=33,
    oe_pin=35,
    clk_pin=34,
)
