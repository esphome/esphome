"""bk72xx_ble keeps WiFi power save off: the Beken SDK's MCU sleep does not
wake up once the station is stopped while the BLE controller runs."""

from collections.abc import Callable
from pathlib import Path

import pytest


def test_power_save_mode_is_not_applied_with_ble(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    main_cpp = generate_main(component_config_path("test_power_save.yaml"))

    assert "bk72xx_ble::BK72xxBLE" in main_cpp
    assert "set_power_save_mode(" not in main_cpp
    assert "power_save_mode HIGH is not applied" in caplog.text
    assert "issues/18592" in caplog.text
