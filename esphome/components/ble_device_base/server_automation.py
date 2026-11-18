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
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

ActionBuilder = Callable[
    [ConfigType, ID, cg.TemplateArguments, TemplateArgsType], Awaitable[cg.MockObj]
]


@dataclass(frozen=True)
class _PlatformAction:
    is_target: Callable[[], bool]
    schema: cv.Schema | cv.All
    builder: ActionBuilder


_ACTIONS: dict[str, list[_PlatformAction]] = {}


def _for_target(name: str) -> _PlatformAction:
    for platform in _ACTIONS[name]:
        if platform.is_target():
            return platform
    raise cv.Invalid(f"{name} is not supported on this platform")


def _keys_schema(schema: cv.Schema | cv.All) -> cv.Schema:
    """The dict schema of ``schema``, or of a ``cv.All`` that adds validators after it."""
    if isinstance(schema, cv.All):
        return next(v for v in schema.validators if isinstance(v, cv.Schema))
    return schema


def _merged_schema(platforms: list[_PlatformAction]) -> cv.Schema:
    """All platforms' keys, with a key that not every platform takes shown as optional."""
    schemas = [_keys_schema(p.schema).schema for p in platforms]
    shared = set.intersection(*({str(key) for key in s} for s in schemas))
    merged: dict[Any, Any] = {}
    seen: set[str] = set()
    for schema in schemas:
        for key, validator in schema.items():
            if (text := str(key)) in seen:
                continue
            seen.add(text)
            merged[key if text in shared else cv.Optional(text)] = validator
    return cv.Schema(merged)


def register_server_action(
    name: str,
    is_target: Callable[[], bool],
    schema: cv.Schema | cv.All,
    builder: ActionBuilder,
) -> None:
    """Add a server platform's version of the action ``name``.

    ``is_target`` tells whether the build is for this platform, e.g. ``lambda: CORE.is_esp32``.
    """
    if (platforms := _ACTIONS.get(name)) is None:
        platforms = _ACTIONS[name] = []

        # The schema depends on the target, so the language schema shows every platform's keys
        @schema_extractor("schema")
        def validate(value: Any) -> ConfigType:
            if value is SCHEMA_EXTRACT:
                return _merged_schema(platforms)
            return _for_target(name).schema(value)

        async def dispatch(
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
        )(dispatch)
    platforms.append(_PlatformAction(is_target, schema, builder))


def register_server_apply_action(
    name: str,
    is_target: Callable[[], bool],
    schema: cv.Schema | cv.All,
    *fields: automation.ApplyField | automation.ApplyCall,
) -> None:
    """Add a server platform's version of the apply action ``name``.

    The other arguments are those of ``register_server_action`` and
    ``automation.register_apply_action``.
    """
    register_server_action(
        name,
        is_target,
        schema,
        automation.apply_action_builder(name, schema, *fields),
    )
