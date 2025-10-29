"""
Test schema.extend functionality in esphome.config_validation.
"""

from typing import Any

import esphome.config_validation as cv


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
