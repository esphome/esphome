"""Tests that mqtt on_json_message automations reject deferring actions.

The trigger passes a view into the parsed JSON document, which is freed once the automation
returns, so the deferring-actions guard is a safety property.
"""

from collections.abc import Callable

import pytest

from esphome import config_validation as cv
from esphome.components.mqtt import CONFIG_SCHEMA
from esphome.const import CONF_ON_JSON_MESSAGE
from esphome.core import Lambda
from esphome.types import ConfigType

DEFERRING_ACTION = {"delay": "1s"}
SYNCHRONOUS_ACTION = {"lambda": Lambda('ESP_LOGD("test", "ran");')}
TRUE_CONDITION = {"lambda": Lambda("return true;")}

DEFER_MESSAGE = "Deferring actions"


def _automation(*actions: ConfigType) -> ConfigType:
    return {"topic": "test/topic", "then": list(actions)}


def _trigger_schema() -> Callable[[list[ConfigType]], list[ConfigType]]:
    """The validator CONFIG_SCHEMA applies to on_json_message."""
    return CONFIG_SCHEMA.validators[0].schema[cv.Optional(CONF_ON_JSON_MESSAGE)]


def test_synchronous_automation_accepted() -> None:
    _trigger_schema()([_automation(SYNCHRONOUS_ACTION)])


def test_deferring_action_rejected() -> None:
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        _trigger_schema()([_automation(SYNCHRONOUS_ACTION, DEFERRING_ACTION)])


def test_nested_deferring_action_rejected() -> None:
    action = {"if": {"condition": TRUE_CONDITION, "then": [DEFERRING_ACTION]}}
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        _trigger_schema()([_automation(action)])


def test_deferring_action_in_second_automation_rejected() -> None:
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        _trigger_schema()(
            [_automation(SYNCHRONOUS_ACTION), _automation(DEFERRING_ACTION)]
        )
