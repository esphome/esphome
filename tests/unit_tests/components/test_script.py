"""Tests for the script component's config validation."""

from typing import Any

import pytest

from esphome import automation
from esphome.components.script import CONF_SCRIPT_STOP, default_script_stop_ids
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_THEN


def test_bare_script_stop_defaults_to_enclosing_script() -> None:
    config = [
        {
            CONF_ID: "my_script",
            CONF_THEN: [
                CONF_SCRIPT_STOP,
                {CONF_SCRIPT_STOP: None},
                {CONF_SCRIPT_STOP: {}},
                {CONF_SCRIPT_STOP: "other_script"},
                {CONF_SCRIPT_STOP: {CONF_ID: "other_script"}},
                {"if": {"condition": "x", CONF_THEN: [CONF_SCRIPT_STOP]}},
            ],
            "on_stop": [{CONF_SCRIPT_STOP: None}],
        },
        {CONF_ID: "second", CONF_THEN: [CONF_SCRIPT_STOP]},
    ]

    result = default_script_stop_ids(config)

    assert result[0][CONF_THEN] == [
        {CONF_SCRIPT_STOP: {CONF_ID: "my_script"}},
        {CONF_SCRIPT_STOP: {CONF_ID: "my_script"}},
        {CONF_SCRIPT_STOP: {CONF_ID: "my_script"}},
        {CONF_SCRIPT_STOP: "other_script"},
        {CONF_SCRIPT_STOP: {CONF_ID: "other_script"}},
        {
            "if": {
                "condition": "x",
                CONF_THEN: [{CONF_SCRIPT_STOP: {CONF_ID: "my_script"}}],
            }
        },
    ]
    assert result[0]["on_stop"] == [{CONF_SCRIPT_STOP: {CONF_ID: "my_script"}}]
    assert result[1][CONF_THEN] == [{CONF_SCRIPT_STOP: {CONF_ID: "second"}}]
    # The input is not modified
    assert config[1][CONF_THEN] == [CONF_SCRIPT_STOP]


def test_unchanged_script_config_is_not_copied() -> None:
    then = [{"logger.log": "hello"}]
    config = [{CONF_ID: "my_script", CONF_THEN: then}]

    result = default_script_stop_ids(config)

    assert result[0] is config[0]
    assert result[0][CONF_THEN] is then


def test_script_stop_without_id_outside_script_is_invalid() -> None:
    with pytest.raises(cv.Invalid, match="can only be used inside a script"):
        automation.validate_action(CONF_SCRIPT_STOP)


@pytest.mark.parametrize(
    "value",
    [
        pytest.param([], id="empty"),
        pytest.param([{CONF_THEN: [{"delay": "1s"}]}], id="missing_if"),
        pytest.param([{"if": {"and": []}}], id="missing_then"),
        pytest.param(
            [
                {CONF_THEN: [{"delay": "1s"}]},
                {"if": {"and": []}, CONF_THEN: [{"delay": "2s"}]},
            ],
            id="default_not_last",
        ),
    ],
)
def test_choose_invalid(value: Any) -> None:
    with pytest.raises(cv.Invalid):
        automation.validate_action({"choose": value})


def test_choose_valid() -> None:
    result = automation.validate_action(
        {
            "choose": [
                {"if": {"or": []}, CONF_THEN: [{"delay": "1s"}]},
                {"if": [{"or": []}, {"and": []}], CONF_THEN: [{"delay": "2s"}]},
            ]
        }
    )

    choices = result["choose"]
    assert len(choices) == 2
    assert "and" in choices[1]["if"]
    assert all(automation.CONF_CHOICE_ID in choice for choice in choices)


def test_choose_default_choice() -> None:
    result = automation.validate_action(
        {
            "choose": [
                {"if": {"or": []}, CONF_THEN: [{"delay": "1s"}]},
                {CONF_THEN: [{"delay": "2s"}]},
            ]
        }
    )

    assert "if" not in result["choose"][1]
