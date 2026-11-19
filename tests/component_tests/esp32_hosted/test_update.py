"""Tests for the esp32_hosted update platform validation."""

import logging

import pytest

from esphome.components.esp32_hosted.update import _warn_esp32c5_partition_table
from esphome.const import CONF_VARIANT, PlatformFramework

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize(
    ("variant", "warns"),
    [("ESP32C5", True), ("ESP32C6", False)],
)
def test_esp32c5_partition_table_warning(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
    variant: str,
    warns: bool,
) -> None:
    """Only an ESP32-C5 coprocessor gets the partition table offset warning."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={"esp32_hosted": {CONF_VARIANT: variant}},
    )

    with caplog.at_level(logging.WARNING):
        _warn_esp32c5_partition_table()

    assert ("partition table offset" in caplog.text) is warns
