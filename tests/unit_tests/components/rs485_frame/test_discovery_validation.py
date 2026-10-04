"""Tests for rs485_frame discovery: baud_sweep: platform validation.

Runtime UART reconfiguration (load_settings) is only implemented on ESP-IDF and ESP8266;
on other platforms discovery.baud_sweep: cannot change the line settings, so it was only
ever caught at setup() with an ESP_LOGW warning that silently disables the sweep. A code
review of the upstream PR found this should be rejected at config-validate time instead,
so the user finds out before flashing rather than watching a sweep that never happens.
"""

from __future__ import annotations

from pathlib import Path

import voluptuous as vol

from esphome import config as esphome_config, yaml_util
from esphome.core import CORE


def test_baud_sweep_on_rp2040_raises_clean_invalid(fixture_path: Path) -> None:
    """discovery.baud_sweep: on rp2040 (no runtime UART reconfiguration) must be
    rejected at config-validate time."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_discovery_baud_sweep_rp2040.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert result.errors, "expected a validation error for baud_sweep: on rp2040"
    assert all(isinstance(err, vol.Invalid) for err in result.errors)
