"""Tests that mqtt on_json_message automations reject deferring actions.

The trigger passes a view into the parsed JSON document, which is freed once the automation
returns, so the deferring-actions guard is a safety property.
"""

import pytest

from esphome import config_validation as cv
from esphome.components.mqtt import CONFIG_SCHEMA, JSON_MESSAGE_AUTOMATION_SCHEMA
from esphome.const import CONF_ON_JSON_MESSAGE
from esphome.core import Lambda
from esphome.types import ConfigType

DEFERRING_ACTION = {"delay": "1s"}
SYNCHRONOUS_ACTION = {"lambda": Lambda('ESP_LOGD("test", "ran");')}
TRUE_CONDITION = {"lambda": Lambda("return true;")}

DEFER_MESSAGE = "Deferring actions"


def _automation(*actions: ConfigType) -> ConfigType:
    return {"topic": "test/topic", "then": list(actions)}


def test_schema_wired_to_on_json_message() -> None:
    schema = CONFIG_SCHEMA.validators[0].schema
    assert schema[cv.Optional(CONF_ON_JSON_MESSAGE)] is JSON_MESSAGE_AUTOMATION_SCHEMA


def test_synchronous_automation_accepted() -> None:
    JSON_MESSAGE_AUTOMATION_SCHEMA([_automation(SYNCHRONOUS_ACTION)])


def test_deferring_action_rejected() -> None:
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        JSON_MESSAGE_AUTOMATION_SCHEMA(
            [_automation(SYNCHRONOUS_ACTION, DEFERRING_ACTION)]
        )


def test_nested_deferring_action_rejected() -> None:
    action = {"if": {"condition": TRUE_CONDITION, "then": [DEFERRING_ACTION]}}
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        JSON_MESSAGE_AUTOMATION_SCHEMA([_automation(action)])


def test_deferring_action_in_second_automation_rejected() -> None:
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        JSON_MESSAGE_AUTOMATION_SCHEMA(
            [_automation(SYNCHRONOUS_ACTION), _automation(DEFERRING_ACTION)]
        )
