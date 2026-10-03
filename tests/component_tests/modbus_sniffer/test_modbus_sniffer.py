"""Tests for modbus_sniffer configuration validation.

The handlers' PDU spans point into hub buffers reused once the handler returns, so the
deferring-actions guard is a safety property: these tests pin it to both handler slots.
"""

import pytest

from esphome import config_validation as cv
from esphome.components.modbus_sniffer import CONF_ON_REQUEST, CONFIG_SCHEMA
from esphome.const import CONF_ON_RESPONSE
from esphome.core import Lambda
from esphome.types import ConfigType

HANDLER_KEYS = [CONF_ON_REQUEST, CONF_ON_RESPONSE]

# A deferring action (registered synchronous=False) and a synchronous one, for contrast.
DEFERRING_ACTION = {"delay": "10ms"}
SYNCHRONOUS_ACTION = {"lambda": Lambda('ESP_LOGD("test", "ran");')}
TRUE_CONDITION = {"lambda": Lambda("return true;")}

DEFER_MESSAGE = "Deferring actions"


def _config(handler_key: str, actions: list) -> ConfigType:
    """A minimal modbus_sniffer config with one handler populated."""
    return {"modbus_id": "hub", handler_key: {"then": actions}}


@pytest.mark.parametrize("handler_key", HANDLER_KEYS)
def test_synchronous_handler_accepted(handler_key: str) -> None:
    CONFIG_SCHEMA(_config(handler_key, [SYNCHRONOUS_ACTION]))


@pytest.mark.parametrize("handler_key", HANDLER_KEYS)
def test_deferring_action_rejected(handler_key: str) -> None:
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        CONFIG_SCHEMA(_config(handler_key, [DEFERRING_ACTION]))


@pytest.mark.parametrize("handler_key", HANDLER_KEYS)
def test_nested_deferring_action_rejected(handler_key: str) -> None:
    # has_non_synchronous_actions recurses, so a delay buried in if: is still caught.
    actions = [{"if": {"condition": TRUE_CONDITION, "then": [DEFERRING_ACTION]}}]
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        CONFIG_SCHEMA(_config(handler_key, actions))
