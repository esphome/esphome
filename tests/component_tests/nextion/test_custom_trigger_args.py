"""Tests for the argument types of the nextion custom-protocol triggers."""

import esphome.codegen as cg
from esphome.components.nextion.display import _owning_args
from esphome.core import Lambda

ARGS = [(cg.StringRef, "key"), (cg.bool_, "value")]
SYNCHRONOUS_ACTION = {"lambda": Lambda('ESP_LOGD("test", "ran");')}
DEFERRING_ACTION = {"delay": "1s"}
TRUE_CONDITION = {"lambda": Lambda("return true;")}


def test_synchronous_automation_keeps_string_ref() -> None:
    assert _owning_args(ARGS, {"then": [SYNCHRONOUS_ACTION]}) == ARGS


def test_deferring_automation_gets_std_string() -> None:
    assert _owning_args(ARGS, {"then": [SYNCHRONOUS_ACTION, DEFERRING_ACTION]}) == [
        (cg.std_string, "key"),
        (cg.bool_, "value"),
    ]


def test_nested_deferring_action_gets_std_string() -> None:
    actions = [{"if": {"condition": TRUE_CONDITION, "then": [DEFERRING_ACTION]}}]
    assert _owning_args(ARGS, {"then": actions})[0] == (cg.std_string, "key")
