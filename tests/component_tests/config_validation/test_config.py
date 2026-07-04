"""
Test config_validation functionality in esphome.config_validation.
"""

from typing import Any

import pytest
from voluptuous import Invalid

import esphome.config_validation as cv
from esphome.core import CORE


def test_config_extend() -> None:
    """Test that schema.extend correctly merges schemas with extras."""

    def func1(data: dict[str, Any]) -> dict[str, Any]:
        data["extra_1"] = "value1"
        return data

    def func2(data: dict[str, Any]) -> dict[str, Any]:
        data["extra_2"] = "value2"
        return data

    schema1 = cv.Schema(
        {
            cv.Required("key1"): cv.string,
        }
    )
    schema1.add_extra(func1)
    schema2 = cv.Schema(
        {
            cv.Required("key2"): cv.string,
        }
    )
    schema2.add_extra(func2)
    extended_schema = schema1.extend(schema2)
    config = {
        "key1": "initial_value1",
        "key2": "initial_value2",
    }
    validated = extended_schema(config)
    assert validated["key1"] == "initial_value1"
    assert validated["key2"] == "initial_value2"
    assert validated["extra_1"] == "value1"
    assert validated["extra_2"] == "value2"

    # Check the opposite order of extension
    extended_schema = schema2.extend(schema1)

    validated = extended_schema(config)
    assert validated["key1"] == "initial_value1"
    assert validated["key2"] == "initial_value2"
    assert validated["extra_1"] == "value1"
    assert validated["extra_2"] == "value2"


def test_requires_component_passes_when_loaded() -> None:
    """Test requires_component passes when the required component is loaded."""
    CORE.loaded_integrations.update({"wifi", "logger"})
    validator = cv.requires_component("wifi")
    result = validator("test_value")
    assert result == "test_value"


def test_requires_component_fails_when_not_loaded() -> None:
    """Test requires_component raises Invalid when the required component is not loaded."""
    CORE.loaded_integrations.add("logger")
    validator = cv.requires_component("wifi")
    with pytest.raises(Invalid) as exc_info:
        validator("test_value")
    assert "requires component wifi" in str(exc_info.value)


def test_conflicts_with_component_passes_when_not_loaded() -> None:
    """Test conflicts_with_component passes when the conflicting component is not loaded."""
    CORE.loaded_integrations.update({"wifi", "logger"})
    validator = cv.conflicts_with_component("esp32_hosted")
    result = validator("test_value")
    assert result == "test_value"


def test_conflicts_with_component_fails_when_loaded() -> None:
    """Test conflicts_with_component raises Invalid when the conflicting component is loaded."""
    CORE.loaded_integrations.update({"wifi", "esp32_hosted"})
    validator = cv.conflicts_with_component("esp32_hosted")
    with pytest.raises(Invalid) as exc_info:
        validator("test_value")
    assert "not compatible with component esp32_hosted" in str(exc_info.value)
