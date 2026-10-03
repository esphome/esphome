"""The non-BLE family rejection lives in to_code (config validation must stay
family-agnostic for the validate-only CI fixtures), so codegen is the only
place it can be pinned."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE, EsphomeError


def test_unsupported_family_rejected(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    with pytest.raises(EsphomeError, match="RTL8710B.*no BLE radio"):
        generate_main(component_config_path("test_rtl8710b.yaml"))
    # Validation itself must not fail (CI validate fixtures run on an RTL8710B
    # board), but it warns before codegen raises.
    assert "cannot compile" in caplog.text


def test_amebad_family_generates(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("test_rtl8720d.yaml"))
    assert "rtl87xx_ble::RTL87xxBLE" in main_cpp
    # AmebaD gates btgap.a behind this option; losing it breaks the build far
    # from the cause, and CI compiles no AmebaD board to catch it.
    assert "CONFIG_BT=1" in CORE.platformio_options["custom_options.platform_opts_bt#h"]


def test_rtl8720c_generates_with_untested_warning(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    # AmebaZ2 is compile-supported (LibreTiny links its BT stack in releases)
    # but has never run on hardware; codegen must succeed and say so.
    main_cpp = generate_main(component_config_path("test_rtl8720c.yaml"))
    assert "rtl87xx_ble::RTL87xxBLE" in main_cpp
    assert "not yet verified on hardware" in caplog.text
    # AmebaZ2 builds its BT stack unconditionally; requesting the AmebaD-only
    # option there would be wrong.
    assert "custom_options.platform_opts_bt#h" not in CORE.platformio_options
