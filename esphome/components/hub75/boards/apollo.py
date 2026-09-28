"""Apollo Automation M1 board definitions."""

from . import BoardConfig

# Apollo Automation M1 Rev4
BoardConfig(
    "apollo-automation-m1-rev4",
    r1_pin=42,
    g1_pin=41,
    b1_pin=40,
    r2_pin=38,
    g2_pin=39,
    b2_pin=37,
    a_pin=45,
    b_pin=36,
    c_pin=48,
    d_pin=35,
    e_pin=21,
    lat_pin=47,
    oe_pin=14,
    clk_pin=2,
)

# Apollo Automation M1 Rev6
BoardConfig(
    "apollo-automation-m1-rev6",
    r1_pin=1,
    g1_pin=5,
    b1_pin=6,
    r2_pin=7,
    g2_pin=13,
    b2_pin=9,
    a_pin=16,
    b_pin=48,
    c_pin=47,
    d_pin=21,
    e_pin=38,
    lat_pin=8,
    oe_pin=4,
    clk_pin=18,
)
