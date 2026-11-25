"""Tests for the script component's config validation."""

from types import SimpleNamespace
from typing import Any
from unittest.mock import AsyncMock, patch

import pytest

from esphome import automation
import esphome.codegen as cg
from esphome.components.const import CONF_ON_STOP
from esphome.components.script import (
    CONF_SCRIPT_STOP,
    CONFIG_SCHEMA,
    SCRIPT_MODES,
    Script,
    ScriptWaitAction,
    default_script_stop_ids,
    script_wait_action_to_code,
    to_code,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_PARAMETERS, CONF_THEN
from esphome.core import CORE, ID


def _plan(*direct: bool) -> Any:
    """A plan_automation stand-in whose plans report ``direct`` in turn."""
    plans = iter(direct)

    async def plan(args: Any, config: Any) -> Any:
        return SimpleNamespace(direct=next(plans, direct[-1]))

    return plan


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


def test_script_stop_with_id_is_valid() -> None:
    result = automation.validate_action({CONF_SCRIPT_STOP: "my_script"})

    assert result[CONF_SCRIPT_STOP][CONF_ID].id == "my_script"


@pytest.mark.parametrize(
    "value",
    [
        pytest.param("my_script", id="not_a_list_or_mapping"),
        pytest.param({CONF_THEN: [CONF_SCRIPT_STOP]}, id="mapping_without_id"),
        pytest.param(["my_script", {CONF_THEN: [CONF_SCRIPT_STOP]}], id="no_id"),
    ],
)
def test_script_stop_ids_left_alone_without_script_id(value: Any) -> None:
    assert default_script_stop_ids(value) == value


def test_bare_script_stop_in_single_script_mapping() -> None:
    config = {CONF_ID: "my_script", CONF_THEN: [CONF_SCRIPT_STOP]}

    result = default_script_stop_ids(config)

    assert result[CONF_THEN] == [{CONF_SCRIPT_STOP: {CONF_ID: "my_script"}}]


@pytest.mark.asyncio
async def test_on_stop_builds_stop_trigger_automation() -> None:
    stop_conf = {CONF_THEN: []}
    config = [
        {
            CONF_ID: ID("my_script", is_declaration=True, type=SCRIPT_MODES["single"]),
            CONF_MODE: "single",
            CONF_PARAMETERS: {},
            CONF_THEN: [],
            CONF_ON_STOP: stop_conf,
        },
        {
            CONF_ID: ID(
                "other_script", is_declaration=True, type=SCRIPT_MODES["single"]
            ),
            CONF_MODE: "single",
            CONF_PARAMETERS: {},
            CONF_THEN: [],
        },
    ]
    with (
        patch("esphome.codegen.add"),
        patch("esphome.codegen.add_define") as add_define,
        patch(
            "esphome.automation.build_automation", new_callable=AsyncMock
        ) as build_automation,
        patch(
            "esphome.automation.plan_automation", side_effect=_plan(False)
        ) as plan_automation,
        patch(
            "esphome.automation.automation_or_run_function", return_value=None
        ) as build_or_run,
    ):
        await to_code(config)

    add_define.assert_called_once_with("USE_SCRIPT_ON_STOP")

    assert [c.args for c in plan_automation.await_args_list] == [
        ([], config[0]),
        ([], config[1]),
    ]
    assert [str(c.args[1]) for c in build_or_run.call_args_list] == [
        "my_script",
        "other_script",
    ]
    assert [
        (str(c.args[0]), *c.args[1:]) for c in build_automation.await_args_list
    ] == [("my_script->get_stop_trigger()", [], stop_conf)]


@pytest.mark.asyncio
async def test_no_on_stop_define_without_on_stop() -> None:
    config = [
        {
            CONF_ID: ID("my_script", is_declaration=True, type=SCRIPT_MODES["single"]),
            CONF_MODE: "single",
            CONF_PARAMETERS: {},
            CONF_THEN: [],
        }
    ]
    with (
        patch("esphome.codegen.add"),
        patch("esphome.codegen.add_define") as add_define,
        patch("esphome.automation.plan_automation", side_effect=_plan(False)),
        patch("esphome.automation.automation_or_run_function", return_value=None),
    ):
        await to_code(config)

    add_define.assert_not_called()


@pytest.mark.asyncio
async def test_script_needing_an_automation_keeps_its_group_on_it() -> None:
    """Inline scripts of the same mode and parameters as one that needs an Automation use it
    too, so the mode code is compiled once; other groups still become functions."""

    def script(name: str, mode: str, parameters: dict[str, str]) -> dict[str, Any]:
        return {
            CONF_ID: ID(name, is_declaration=True, type=SCRIPT_MODES[mode]),
            CONF_MODE: mode,
            CONF_PARAMETERS: parameters,
            CONF_THEN: [],
        }

    config = [
        script("inline_queued", "queued", {"x": "int"}),
        script("delay_queued", "queued", {"x": "int"}),
        script("inline_queued_float", "queued", {"x": "float"}),
        script("inline_single", "single", {"x": "int"}),
    ]
    with (
        patch("esphome.codegen.add"),
        patch("esphome.codegen.register_component", new_callable=AsyncMock),
        patch(
            "esphome.automation.plan_automation",
            side_effect=_plan(True, False, True, True),
        ),
        patch(
            "esphome.automation.automation_or_run_function", return_value=None
        ) as build_or_run,
    ):
        await to_code(config)

    assert [
        (str(c.args[1]), c.kwargs["direct"]) for c in build_or_run.call_args_list
    ] == [
        # Built at once, then the scripts that waited for their group
        ("delay_queued", False),
        ("inline_queued", False),
        ("inline_queued_float", True),
        ("inline_single", True),
    ]


@pytest.mark.parametrize("name", ["esphome__running", "esphome__arg_0"])
def test_reserved_parameter_names_rejected(name: str) -> None:
    """The reason reaches the user, at the parameter's path."""
    config = [{CONF_ID: "my_script", CONF_PARAMETERS: {name: "int"}, CONF_THEN: []}]
    with pytest.raises(cv.MultipleInvalid) as err:
        CONFIG_SCHEMA(config)
    (error,) = err.value.errors
    assert (
        error.msg == "Names starting with 'esphome__' are reserved for generated code"
    )
    assert error.path == [0, CONF_PARAMETERS, name]


@pytest.mark.asyncio
async def test_script_wait_on_script_declared_elsewhere() -> None:
    """A Script that the script: list did not declare is waited on with its own class."""
    other = cg.esphome_ns.class_("OtherScript", Script)
    other_id = ID("other_script", is_declaration=True, type=other)
    cg.new_Pvariable(other_id)
    action_id = ID("wait_action", is_declaration=True, type=ScriptWaitAction)

    with patch("esphome.codegen.register_component", new_callable=AsyncMock):
        var = await script_wait_action_to_code(
            {CONF_ID: ID("other_script", type=Script)},
            action_id,
            cg.TemplateArguments(),
            [],
        )

    assert str(var) == "wait_action"
    assert "script::ScriptWaitAction<OtherScript>" in CORE.cpp_global_section
