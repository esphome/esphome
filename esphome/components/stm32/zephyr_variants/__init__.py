"""Zephyr variants of the stm32 platform, which hands them to the zephyr component."""

from types import ModuleType

from . import stm32c5

# Variant name -> its module, which exports VARIANT_NAME and a ZephyrVariant named VARIANT
VARIANT_MODULES: dict[str, ModuleType] = {mod.VARIANT_NAME: mod for mod in (stm32c5,)}
