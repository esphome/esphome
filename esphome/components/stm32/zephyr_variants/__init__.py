"""Zephyr variants of the stm32 platform, which hands them to the zephyr component."""

from types import ModuleType

from . import stm32c5

# Variant name -> its module, which exports a ZephyrVariant named VARIANT
VARIANT_MODULES: dict[str, ModuleType] = {mod.VARIANT.name: mod for mod in (stm32c5,)}
