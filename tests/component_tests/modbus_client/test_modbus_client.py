"""Tests for modbus_client configuration validation.

Handler PDU spans point into hub buffers reused once the handler returns, so the deferring-actions
guard is a safety property: these tests pin it to every handler slot.
"""

import pytest

from esphome import config_validation as cv
from esphome.components import modbus_client
from esphome.components.modbus_client import (
    CONF_ON_NO_RESPONSE,
    CONF_ON_NOT_SENT,
    CONF_ON_SENT,
    CONF_PDU,
    CONFIG_SCHEMA,
    MODBUS_CLIENT_SEND_SCHEMA,
)
from esphome.const import (
    CONF_ADDRESS,
    CONF_CONTINUOUS,
    CONF_ID,
    CONF_ON_ERROR,
    CONF_ON_RESPONSE,
)
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


def test_continuous_on_write_pdu_rejected() -> None:
    """A literal write-code PDU with continuous: true is rejected at config time (reads only)."""
    with pytest.raises(cv.Invalid, match="does not apply to a write PDU"):
        MODBUS_CLIENT_SEND_SCHEMA(
            {
                CONF_ADDRESS: 0x01,
                CONF_PDU: [0x06, 0x00, 0x01, 0x00, 0x0A],
                CONF_CONTINUOUS: True,
            }
        )


def test_continuous_on_read_pdu_accepted() -> None:
    """A literal read-code PDU with continuous: true is fine - continuous polling applies to reads."""
    MODBUS_CLIENT_SEND_SCHEMA(
        {
            CONF_ADDRESS: 0x01,
            CONF_PDU: [0x03, 0x00, 0x10, 0x00, 0x01],
            CONF_CONTINUOUS: True,
        }
    )


# The standalone component block. The compile fixtures cover the accepted shapes end to end; these pin
# the parts a fixture cannot express - a rejection, and a module flag whose absence breaks other
# components rather than this one.


def test_component_requires_an_address() -> None:
    """The address identifies the device on the bus, so there is no sensible default."""
    with pytest.raises(cv.Invalid, match=CONF_ADDRESS):
        CONFIG_SCHEMA({CONF_ID: "bare_client"})


def test_component_requires_an_id() -> None:
    """The device is reachable only through id() in a lambda, so a generated id would be dead config."""
    with pytest.raises(cv.Invalid, match=CONF_ID):
        CONFIG_SCHEMA({CONF_ADDRESS: 0x01})


def test_component_accepts_an_id_and_address() -> None:
    """modbus_id stays optional: it resolves to the single hub when only one is declared."""
    config = CONFIG_SCHEMA({CONF_ID: "bare_client", CONF_ADDRESS: 0x01})
    assert config[CONF_ADDRESS] == 0x01


def test_component_rejects_an_out_of_range_address() -> None:
    """A Modbus device address is one byte."""
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({CONF_ID: "bare_client", CONF_ADDRESS: 0x100})


def test_multi_conf_no_default_is_set() -> None:
    """Load-bearing: the modbus hub auto-loads this component to register its actions.

    Without MULTI_CONF_NO_DEFAULT that auto-load builds a default entry, which then fails the required
    address above - breaking every configuration that uses modbus but never declares a modbus_client
    block. validate-autoload.esp32-idf.yaml covers the same path end to end; this names the reason.
    """
    assert modbus_client.MULTI_CONF is True
    assert modbus_client.MULTI_CONF_NO_DEFAULT is True
