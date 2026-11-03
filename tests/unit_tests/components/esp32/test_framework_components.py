"""Tests for the esp32 framework ``components:`` schema."""

import pytest

from esphome.components import esp32
import esphome.config_validation as cv


@pytest.mark.parametrize(
    "source",
    [
        "https://github.com/owner/component.git",
        "git@github.com:owner/component.git",
    ],
)
def test_component_source_accepts_repository_url(source: str) -> None:
    config = esp32.FRAMEWORK_SCHEMA(
        {"components": [{"name": "owner/component", "source": source}]}
    )
    assert config["components"][0]["source"] == source


@pytest.mark.parametrize("source", [1.0, ""])
def test_component_source_rejects_invalid(source: object) -> None:
    with pytest.raises(cv.Invalid):
        esp32.FRAMEWORK_SCHEMA(
            {"components": [{"name": "owner/component", "source": source}]}
        )
