"""Shared base for the Hi-Link 60 GHz radars that speak the TinyFrame serial protocol.

Covers the LD6002B and the LD6004. The LD6001A is a different family (AT commands and
another report format) and gets its own component, so it does not build on this one.
"""

import esphome.codegen as cg

CODEOWNERS = ["@hepter", "@wolph"]

ld600x_ns = cg.esphome_ns.namespace("ld600x")
