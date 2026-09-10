"""Native (PlatformIO-free) build support for the ESP8266 Arduino core.

This package downloads the Arduino ESP8266 core and the xtensa-lx106
toolchain, generates a ninja build for them plus the ESPHome sources, and
drives the build directly — the ESP8266 equivalent of ``esphome.espidf``.

Deliberately importable without the esp8266 component to avoid circular
imports; the component wires these modules in via lazy imports.
"""
