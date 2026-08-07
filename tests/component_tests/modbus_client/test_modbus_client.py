"""Tests for modbus_client configuration validation.

Handler PDU spans point into hub buffers reused once the handler returns, so the deferring-actions
guard is a safety property: these tests pin it to every handler slot.
"""

import pytest

from esphome import config_validation as cv
from esphome.components.modbus_client import (
    CONF_ON_NO_RESPONSE,
    CONF_ON_NOT_SENT,
    CONF_ON_SENT,
    CONF_PDU,
    MODBUS_CLIENT_SEND_SCHEMA,
)
from esphome.const import CONF_ADDRESS, CONF_ON_ERROR, CONF_ON_RESPONSE
from esphome.core import Lambda
from esphome.types import ConfigType

# Every handler slot on modbus_client.send. All five must reject deferring actions.
HANDLER_KEYS = [
    CONF_ON_SENT,
    CONF_ON_RESPONSE,
    CONF_ON_ERROR,
    CONF_ON_NO_RESPONSE,
    CONF_ON_NOT_SENT,
]

# A deferring action (registered synchronous=False) and a synchronous one, for contrast.
DEFERRING_ACTION = {"delay": "1s"}
SYNCHRONOUS_ACTION = {"lambda": Lambda('ESP_LOGD("test", "ran");')}
TRUE_CONDITION = {"lambda": Lambda("return true;")}

# The same deferring action buried inside nested control flow, which the guard must still find.
NESTED_ACTIONS = [
    pytest.param(
        [{"if": {"condition": TRUE_CONDITION, "then": [DEFERRING_ACTION]}}],
        id="if",
    ),
    pytest.param([{"repeat": {"count": 2, "then": [DEFERRING_ACTION]}}], id="repeat"),
    pytest.param(
        [
            {
                "repeat": {
                    "count": 2,
                    "then": [
                        {
                            "if": {
                                "condition": TRUE_CONDITION,
                                "then": [DEFERRING_ACTION],
                            }
                        }
                    ],
                }
            }
        ],
        id="repeat_if",
    ),
]

DEFER_MESSAGE = "Deferring actions"


def _config(handler_key: str, actions: list) -> ConfigType:
    """A minimal valid modbus_client.send config with one handler populated."""
    return {
        CONF_ADDRESS: 0x01,
        CONF_PDU: [0x03, 0x00, 0x10, 0x00, 0x01],
        handler_key: {"then": actions},
    }


@pytest.mark.parametrize("handler_key", HANDLER_KEYS)
def test_synchronous_handler_accepted(handler_key: str) -> None:
    # The guard must not get in the way of an ordinary inline handler.
    MODBUS_CLIENT_SEND_SCHEMA(_config(handler_key, [SYNCHRONOUS_ACTION]))


@pytest.mark.parametrize("handler_key", HANDLER_KEYS)
def test_deferring_action_rejected(handler_key: str) -> None:
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        MODBUS_CLIENT_SEND_SCHEMA(_config(handler_key, [DEFERRING_ACTION]))


@pytest.mark.parametrize("handler_key", HANDLER_KEYS)
@pytest.mark.parametrize("actions", NESTED_ACTIONS)
def test_nested_deferring_action_rejected(handler_key: str, actions: list) -> None:
    # has_non_synchronous_actions recurses, so a delay buried in if:/repeat: is still caught.
    with pytest.raises(cv.Invalid, match=DEFER_MESSAGE):
        MODBUS_CLIENT_SEND_SCHEMA(_config(handler_key, actions))


def test_on_no_response_lambda_form_accepted() -> None:
    # The returning-lambda form has no action list; the guard is a no-op on it.
    MODBUS_CLIENT_SEND_SCHEMA(
        {
            CONF_ADDRESS: 0x01,
            CONF_PDU: [0x03, 0x00, 0x10, 0x00, 0x01],
            CONF_ON_NO_RESPONSE: Lambda("return false;"),
        }
    )


def test_on_no_response_retry_lambda_accepted() -> None:
    # The automation form may also carry a nested retry: lambda.
    MODBUS_CLIENT_SEND_SCHEMA(
        {
            CONF_ADDRESS: 0x01,
            CONF_PDU: [0x03, 0x00, 0x10, 0x00, 0x01],
            CONF_ON_NO_RESPONSE: {
                "then": [SYNCHRONOUS_ACTION],
                "retry": Lambda("return true;"),
            },
        }
    )
