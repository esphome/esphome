"""Exercise the I2S speaker keep_alive validation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE


def test_keep_alive_generates_setter(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("keep_alive.yaml"))
    assert "set_keep_alive(true)" in source


@pytest.mark.parametrize(
    ("config_file", "message"),
    [
        ("keep_alive_shared_bus.yaml", "needs its own I2S bus"),
        ("keep_alive_shared_mic.yaml", "needs its own I2S bus"),
        ("keep_alive_spdif.yaml", "SPDIF mode does not support 'keep_alive'"),
    ],
)
def test_keep_alive_rejected(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
    config_file: str,
    message: str,
) -> None:
    CORE.config_path = component_config_path(config_file)
    assert read_config({}) is None
    assert message in capsys.readouterr().out


def test_keep_tx_active_requires_full_duplex(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("keep_tx_active_without_duplex.yaml")
    assert read_config({}) is None
    assert "'keep_tx_active' requires 'full_duplex'" in capsys.readouterr().out
