import logging
import re

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HIDE_HASH,
    CONF_HIDE_TIMESTAMP,
    CONF_NAME,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_NEW_BOX,
)
from esphome.loader import get_component
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CONF_COMPONENT = "component"

# Component directory names, which are also Python module names.
_COMPONENT_NAME_RE = re.compile(r"^[a-z_][a-z0-9_]*$")

version_ns = cg.esphome_ns.namespace("version")
VersionTextSensor = version_ns.class_(
    "VersionTextSensor", text_sensor.TextSensor, cg.Component
)
ComponentVersionTextSensor = version_ns.class_(
    "ComponentVersionTextSensor", text_sensor.TextSensor, cg.Component
)


def _component_name(value: str) -> str:
    value = cv.string_strict(value)
    if not _COMPONENT_NAME_RE.match(value):
        raise cv.Invalid(
            f"'{value}' is not a valid component name; expected lowercase letters, "
            "digits and underscores"
        )
    return value


_BASE_SCHEMA = text_sensor.text_sensor_schema(
    icon=ICON_NEW_BOX,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(cv.COMPONENT_SCHEMA)

ESPHOME_VERSION_SCHEMA = _BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(VersionTextSensor),
        # Hide the config hash suffix and restore the pre-2026.1
        # version text format when set to true.
        cv.Optional(CONF_HIDE_HASH, default=False): cv.boolean,
        cv.Optional(CONF_HIDE_TIMESTAMP, default=False): cv.boolean,
    }
)

COMPONENT_VERSION_SCHEMA = _BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ComponentVersionTextSensor),
        cv.Required(CONF_COMPONENT): _component_name,
    }
)


def CONFIG_SCHEMA(config: ConfigType) -> ConfigType:
    # Separate classes so the ESPHome-version sensor carries neither the fields nor
    # the branch that only component mode needs, and so hide_hash / hide_timestamp
    # are rejected in component mode rather than silently ignored.
    if isinstance(config, dict) and CONF_COMPONENT in config:
        return COMPONENT_VERSION_SCHEMA(config)
    return ESPHOME_VERSION_SCHEMA(config)


def _final_validate(config: ConfigType) -> ConfigType:
    # Deferred to final validation so external components are loaded by now. A typo
    # is an error here rather than a sensor that silently reads unknown forever.
    name = config.get(CONF_COMPONENT)
    if name is not None and get_component(name) is None:
        raise cv.Invalid(
            f"Component '{name}' not found. It must be a core component or one "
            "listed under external_components.",
            path=[CONF_COMPONENT],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)

    if (name := config.get(CONF_COMPONENT)) is not None:
        cg.add(var.set_component_name(name))
        # Declaring COMPONENT_VERSION is optional and most components never will,
        # so this is a warning rather than an error. The state is left unpublished
        # rather than set to "" so it cannot be mistaken for a reported value.
        if (version := get_component(name).component_version) is None:
            _LOGGER.warning(
                "Component '%s' does not report a version, so text sensor '%s' "
                "will read as unknown. Components opt in by defining "
                "COMPONENT_VERSION; this one has not.",
                name,
                config.get(CONF_NAME, name),
            )
        else:
            cg.add(var.set_version(version))
        return

    cg.add(var.set_hide_hash(config[CONF_HIDE_HASH]))
    cg.add(var.set_hide_timestamp(config[CONF_HIDE_TIMESTAMP]))
