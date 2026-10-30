"""Tests for the argument types of the nextion custom-protocol triggers."""

from collections.abc import Callable
from pathlib import Path

from esphome.automation import owning_args
import esphome.codegen as cg
from esphome.core import Lambda

ARGS = [(cg.StringRef, "key"), (cg.bool_, "value")]
SYNCHRONOUS_ACTION = {"lambda": Lambda('ESP_LOGD("test", "ran");')}
DEFERRING_ACTION = {"delay": "1s"}
TRUE_CONDITION = {"lambda": Lambda("return true;")}


def test_synchronous_automation_keeps_string_ref() -> None:
    assert owning_args(ARGS, {"then": [SYNCHRONOUS_ACTION]}) == ARGS


def test_deferring_automation_gets_std_string() -> None:
    assert owning_args(ARGS, {"then": [SYNCHRONOUS_ACTION, DEFERRING_ACTION]}) == [
        (cg.std_string, "key"),
        (cg.bool_, "value"),
    ]


def test_nested_deferring_action_gets_std_string() -> None:
    actions = [{"if": {"condition": TRUE_CONDITION, "then": [DEFERRING_ACTION]}}]
    assert owning_args(ARGS, {"then": actions})[0] == (cg.std_string, "key")


def test_all_string_refs_swapped() -> None:
    args = [(cg.StringRef, "key"), (cg.StringRef, "value")]
    assert owning_args(args, {"then": [DEFERRING_ACTION]}) == [
        (cg.std_string, "key"),
        (cg.std_string, "value"),
    ]


def test_generated_automation_types(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("custom_triggers.yaml"))

    assert "Automation<StringRef, StringRef>" in main_cpp
    assert "Automation<std::string, std::string>" in main_cpp
