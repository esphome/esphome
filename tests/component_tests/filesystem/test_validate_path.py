"""Tests for the filesystem path validator."""

import pytest

from esphome.components import filesystem
import esphome.config_validation as cv


def test_validate_path_accepts_absolute() -> None:
    assert filesystem.validate_path("/logs/a.txt") == "/logs/a.txt"


@pytest.mark.parametrize("value", ["logs/a.txt", "/a\0b", 5])
def test_validate_path_rejects(value: object) -> None:
    with pytest.raises(cv.Invalid):
        filesystem.validate_path(value)
