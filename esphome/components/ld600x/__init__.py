"""Shared base for the Hi-Link 60 GHz radars that speak the TinyFrame serial protocol.

Covers the LD6002B and the LD6004. The LD6001A is a different family (AT commands and
another report format) and gets its own component, so it does not build on this one.
"""

CODEOWNERS = ["@hepter", "@wolph"]
DOMAIN = "ld600x"
