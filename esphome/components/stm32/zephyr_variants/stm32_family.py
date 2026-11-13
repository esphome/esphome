"""Code shared by the STM32 variants."""

from esphome.types import ConfigType


def to_code(config: ConfigType) -> None:
    """REQUIRES_FULL_LIBCPP selects the GNU C++ library; Zephyr's default minimal
    library has no STL, which ESPHome's headers need."""
    # deferred: zephyr's __init__ may load the variants while it imports
    from esphome.components.zephyr import zephyr_add_prj_conf

    zephyr_add_prj_conf("CPP", True)
    zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
    # Off by default, which leaves floating point in software
    zephyr_add_prj_conf("FPU", True)
