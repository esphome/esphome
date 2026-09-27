"""Resolution of the native (non-PlatformIO) build backend for a config.

Kept deliberately light: the serial upload and logs fast path resolves a
backend for its artifact paths alone, so importing this must not pull in a
platform component package or the backend itself.
"""

from __future__ import annotations

import importlib
from types import ModuleType

from esphome.const import Toolchain
from esphome.core import CORE, EsphomeError

# Native build backend per (target platform, toolchain)
NATIVE_TOOLCHAIN_MODULES = {
    ("esp32", Toolchain.ESP_IDF): "esphome.espidf.toolchain",
    ("esp8266", Toolchain.ARDUINO): "esphome.arduino8266.toolchain",
}


def native_backend() -> ModuleType | None:
    """The native build backend module for the resolved toolchain."""
    if not CORE.using_native_toolchain:
        return None
    key = (CORE.target_platform, CORE.toolchain)
    if (module_path := NATIVE_TOOLCHAIN_MODULES.get(key)) is None:
        # Degrading to the PlatformIO path would build with the wrong backend
        raise EsphomeError(
            f"Toolchain '{CORE.toolchain.value}' has no native build backend "
            f"module for platform {CORE.target_platform}"
        )
    return importlib.import_module(module_path)


# Binutils and the linked image for memory analysis, for toolchains that build
# without PlatformIO but have no native build backend (which supplies them)
ANALYSIS_TOOLCHAIN_MODULES = {
    ("nrf52", Toolchain.SDK_NRF): "esphome.components.nrf52.toolchain",
}


def analysis_backend() -> ModuleType | None:
    """The module giving objdump, readelf and the ELF of a non-PlatformIO build.

    None means PlatformIO's idedata supplies them (or nothing can).
    """
    if (native := native_backend()) is not None:
        return native
    module_path = ANALYSIS_TOOLCHAIN_MODULES.get((CORE.target_platform, CORE.toolchain))
    return importlib.import_module(module_path) if module_path else None
