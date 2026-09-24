"""Adafruit Matrix Portal board definitions."""

from . import BoardConfig

# Adafruit Matrix Portal S3
BoardConfig(
    "adafruit-matrix-portal-s3",
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
    ignore_strapping_pins=("a_pin",),  # GPIO45 is a strapping pin
)
