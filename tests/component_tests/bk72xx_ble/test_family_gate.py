"""The non-5.x family rejection lives in to_code (config validation must stay
family-agnostic for the validate-only CI fixtures), so codegen is the only
place it can be pinned."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import EsphomeError


@pytest.mark.parametrize(
    ("config_file", "match"),
    [
        ("test_bk7231t.yaml", "BK7231T.*BLE 4.2"),
        ("test_bk7252.yaml", "BK7251.*BLE 4.2"),
        ("test_bk7231q.yaml", "BK7231Q.*no BLE"),
        ("test_bk7238.yaml", "BK7238.*bootloader"),
    ],
)
def test_unsupported_family_rejected(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    match: str,
    caplog: pytest.LogCaptureFixture,
) -> None:
    with pytest.raises(EsphomeError, match=match):
        generate_main(component_config_path(config_file))
    # Validation itself must not fail (CI validate fixtures run on a BLE 4.2
    # board), but it warns before codegen raises.
    assert "cannot compile" in caplog.text


def test_ble5_family_generates(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("test_bk7231n.yaml"))
    assert "bk72xx_ble::BK72xxBLE" in main_cpp
