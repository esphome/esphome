"""Native (PlatformIO-free) build support for the host platform.

Builds the generated sources with the machine's own C/C++ compiler through a
ninja file and drives the build directly -- the host equivalent of
``esphome.espidf``. Nothing is downloaded: the compiler comes from PATH and
registry libraries go through the shared library converter.

Deliberately importable without the host component to avoid circular
imports; the component wires these modules in via lazy imports.
"""
