"""Tests for esphome.components.nrf52.framework helpers."""

from unittest.mock import patch

import pytest

from esphome.components.nrf52.framework import _get_toolchain_platform_info


@pytest.mark.parametrize(
    ("system", "machine", "expected"),
    [
        # default — no branch hit
        ("Linux", "x86_64", ("linux", "x86_64", "tar.xz")),
        # arm64 → aarch64 rename
        ("Linux", "arm64", ("linux", "aarch64", "tar.xz")),
        # darwin → macos rename only
        ("Darwin", "x86_64", ("macos", "x86_64", "tar.xz")),
        # both renames apply
        ("Darwin", "arm64", ("macos", "aarch64", "tar.xz")),
        # windows forces x86_64 + 7z; arm64 rename is overwritten
        ("Windows", "arm64", ("windows", "x86_64", "7z")),
    ],
)
def test_get_toolchain_platform_info(
    system: str, machine: str, expected: tuple[str, str, str]
) -> None:
    with (
        patch("platform.system", return_value=system),
        patch("platform.machine", return_value=machine),
    ):
        assert _get_toolchain_platform_info() == expected
