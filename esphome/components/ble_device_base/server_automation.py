"""Actions shared by the BLE server platforms (esp32_ble_server, zephyr_ble_server).

An action name lives in one registry, so a platform cannot register it on its own without
replacing another platform's entry. Each server instead adds its schema and builder here,
and the action validates and builds with the one for the target platform.
"""

from collections.abc import Awaitable, Callable
from dataclasses import dataclass
from typing import Any

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import ID
from esphome.cpp_generator import TemplateArgsType
from esphome.types import ConfigType

ActionBuilder = Callable[
    [ConfigType, ID, cg.TemplateArguments, TemplateArgsType], Awaitable[cg.MockObj]
]


@dataclass(frozen=True)
class _PlatformAction:
    is_target: Callable[[], bool]
    schema: cv.Schema
    builder: ActionBuilder


_ACTIONS: dict[str, dict[str, _PlatformAction]] = {}


def _for_target(name: str) -> _PlatformAction:
    for platform in _ACTIONS[name].values():
        if platform.is_target():
            return platform
    raise cv.Invalid(f"{name} is not supported on this platform")


def register_server_apply_action(
    name: str,
    domain: str,
    is_target: Callable[[], bool],
    schema: cv.Schema,
    *fields: automation.ApplyField | automation.ApplyCall,
) -> None:
    """Add a server platform's version of the apply action ``name``.

    ``is_target`` tells whether the build is for this platform, e.g. ``lambda: CORE.is_esp32``.
    The other arguments are those of ``automation.register_apply_action``.
    """
    if name not in _ACTIONS:
        _ACTIONS[name] = {}

        def validate(value: Any) -> ConfigType:
            return _for_target(name).schema(value)

        async def builder(
            config: ConfigType,
            action_id: ID,
            template_arg: cg.TemplateArguments,
            args: TemplateArgsType,
        ) -> cg.MockObj:
            return await _for_target(name).builder(
                config, action_id, template_arg, args
            )

        automation.register_action(
            name, automation.ApplyAction, validate, synchronous=True
        )(builder)
    _ACTIONS[name][domain] = _PlatformAction(
        is_target, schema, automation.apply_action_builder(name, schema, *fields)
    )
