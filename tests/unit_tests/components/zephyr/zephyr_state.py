"""Shared builder for CORE.data[KEY_ZEPHYR] in unit tests."""

from __future__ import annotations

from typing import Any

from esphome.components.zephyr import zephyr_set_core_data
from esphome.components.zephyr.const import KEY_BOOTLOADER, KEY_ZEPHYR
from esphome.components.zephyr.variants import VARIANTS
from esphome.const import CONF_BOARD
from esphome.core import CORE


def empty_zephyr_data(
    variant: str | None = None,
    framework_type: str = "zephyr",
    **overrides: Any,
) -> dict[str, Any]:
    """Return a full ZephyrData dict, built by production code so no key can drift.

    Does not touch CORE.data; assign the result to CORE.data[KEY_ZEPHYR].
    """
    previous = CORE.data.get(KEY_ZEPHYR)
    zephyr_set_core_data({CONF_BOARD: "some_board", KEY_BOOTLOADER: "none"})
    data: dict[str, Any] = dict(CORE.data.pop(KEY_ZEPHYR))
    if previous is not None:
        CORE.data[KEY_ZEPHYR] = previous
    variant_info = VARIANTS.get(variant) if variant is not None else None
    data["variant"] = variant
    data["family"] = variant_info.family if variant_info is not None else None
    data["framework_type"] = framework_type
    data.update(overrides)
    return data
