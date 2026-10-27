"""Tests that emontx on_json/on_data automations reject deferring actions.

The triggers pass views into the receive buffer and the parsed JSON document, which are only valid
while the automation runs, so the deferring-actions guard is a safety property.
"""

from collections.abc import Callable

import pytest

from esphome import config_validation as cv
from esphome.components.emontx import CONF_ON_JSON, CONFIG_SCHEMA
from esphome.const import CONF_ON_DATA
from esphome.core import Lambda
from esphome.types import ConfigType

TRIGGER_KEYS = [CONF_ON_JSON, CONF_ON_DATA]

DEFERRING_ACTION = {"delay": "1s"}
SYNCHRONOUS_ACTION = {"lambda": Lambda('ESP_LOGD("test", "ran");')}
TRUE_CONDITION = {"lambda": Lambda("return true;")}

DEFER_MESSAGE = "Deferring actions"


def _trigger_schema(trigger_key: str) -> Callable[[list[ConfigType]], list[ConfigType]]:
    """The validator CONFIG_SCHEMA applies to this trigger key."""
    return CONFIG_SCHEMA.schema[cv.Optional(trigger_key)]


@pytest.mark.parametrize("trigger_key", TRIGGER_KEYS)
def test_synchronous_automation_accepted(trigger_key: str) -> None:
    _trigger_schema(trigger_key)([{"then": [SYNCHRONOUS_ACTION]}])


@pytest.mark.parametrize("trigger_key", TRIGGER_KEYS)
def test_deferring_action_rejected(trigger_key: str) -> None:
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        _trigger_schema(trigger_key)([{"then": [SYNCHRONOUS_ACTION, DEFERRING_ACTION]}])


@pytest.mark.parametrize("trigger_key", TRIGGER_KEYS)
def test_nested_deferring_action_rejected(trigger_key: str) -> None:
    actions = [{"if": {"condition": TRUE_CONDITION, "then": [DEFERRING_ACTION]}}]
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        _trigger_schema(trigger_key)([{"then": actions}])


@pytest.mark.parametrize("trigger_key", TRIGGER_KEYS)
def test_deferring_action_in_second_automation_rejected(trigger_key: str) -> None:
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        _trigger_schema(trigger_key)(
            [{"then": [SYNCHRONOUS_ACTION]}, {"then": [DEFERRING_ACTION]}]
        )
