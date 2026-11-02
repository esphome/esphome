"""Tests that haier on_status_message automations reject deferring actions.

The trigger passes a pointer into the protocol's receive buffer, which is reused for the next
frame, so the deferring-actions guard is a safety property.
"""

from collections.abc import Callable

import pytest

from esphome import config_validation as cv
from esphome.components.haier.climate import (
    CONF_ON_STATUS_MESSAGE,
    HonClimate,
    Smartair2Climate,
    _base_config_schema,
)
from esphome.core import Lambda
from esphome.cpp_generator import MockObjClass
from esphome.types import ConfigType

CLIMATE_CLASSES = [HonClimate, Smartair2Climate]

DEFERRING_ACTION = {"delay": "1s"}
SYNCHRONOUS_ACTION = {"lambda": Lambda('ESP_LOGD("test", "ran");')}
TRUE_CONDITION = {"lambda": Lambda("return true;")}

DEFER_MESSAGE = "Deferring actions"


def _trigger_schema(
    class_: MockObjClass,
) -> Callable[[list[ConfigType]], list[ConfigType]]:
    """The validator the protocol's schema applies to on_status_message."""
    return _base_config_schema(class_).schema[cv.Optional(CONF_ON_STATUS_MESSAGE)]


@pytest.mark.parametrize("class_", CLIMATE_CLASSES)
def test_synchronous_automation_accepted(class_: MockObjClass) -> None:
    _trigger_schema(class_)([{"then": [SYNCHRONOUS_ACTION]}])


@pytest.mark.parametrize("class_", CLIMATE_CLASSES)
def test_deferring_action_rejected(class_: MockObjClass) -> None:
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        _trigger_schema(class_)([{"then": [SYNCHRONOUS_ACTION, DEFERRING_ACTION]}])


@pytest.mark.parametrize("class_", CLIMATE_CLASSES)
def test_nested_deferring_action_rejected(class_: MockObjClass) -> None:
    actions = [{"if": {"condition": TRUE_CONDITION, "then": [DEFERRING_ACTION]}}]
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        _trigger_schema(class_)([{"then": actions}])


@pytest.mark.parametrize("class_", CLIMATE_CLASSES)
def test_deferring_action_in_second_automation_rejected(class_: MockObjClass) -> None:
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        _trigger_schema(class_)(
            [{"then": [SYNCHRONOUS_ACTION]}, {"then": [DEFERRING_ACTION]}]
        )
