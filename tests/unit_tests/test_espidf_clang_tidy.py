"""Tests for esphome.espidf.clang_tidy tidy-project setup."""

import os
from pathlib import Path

import pytest

from esphome.espidf.clang_tidy import _Settings, _setup_core


def _settings(target_framework: str) -> _Settings:
    return _Settings(
        idf_target="esp32",
        variant="ESP32",
        idf_version="5.5.4",
        target_framework=target_framework,
        platform_defines=("USE_ESP32",),
        framework_deps={},
    )


@pytest.mark.parametrize(
    ("target_framework", "expected"),
    [("arduino", "1"), ("espidf", "0")],
)
def test_setup_core_sets_arduino_env(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    target_framework: str,
    expected: str,
) -> None:
    """_setup_core sets ESPHOME_ARDUINO, which gates arduino-only manifest deps."""
    # monkeypatch snapshots os.environ, so the env var _setup_core writes is
    # restored after the test instead of leaking into later tests.
    monkeypatch.delenv("ESPHOME_ARDUINO", raising=False)

    _setup_core(tmp_path / "proj", _settings(target_framework))

    assert os.environ["ESPHOME_ARDUINO"] == expected
