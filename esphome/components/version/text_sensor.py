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
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
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


@schema_extractor("schema")
def CONFIG_SCHEMA(config: ConfigType) -> ConfigType:
    # Separate classes so the ESPHome-version sensor carries neither the fields nor
    # the branch that only component mode needs, and so hide_hash / hide_timestamp
    # are rejected in component mode rather than silently ignored.
    if config is SCHEMA_EXTRACT:
        return ESPHOME_VERSION_SCHEMA.extend(
            {cv.Optional(CONF_COMPONENT): _component_name}
        )
    if CONF_COMPONENT in config:
        return COMPONENT_VERSION_SCHEMA(config)
    return ESPHOME_VERSION_SCHEMA(config)


def _final_validate(config: ConfigType) -> ConfigType:
    # Deferred to final validation so external components are loaded by now, and so
    # both the error and the warning show up in `esphome config`.
    name = config.get(CONF_COMPONENT)
    if name is None:
        return config

    manifest = get_component(name)
    if manifest is None:
        # A typo is an error rather than a sensor that reads unknown forever.
        raise cv.Invalid(
            f"Component '{name}' not found. It must be a core component or one "
            "listed under external_components.",
            path=[CONF_COMPONENT],
        )
    # Declaring COMPONENT_VERSION is optional and most components never will, so
    # this is a warning. The state is left unpublished rather than set to "" so it
    # cannot be mistaken for a reported value.
    if manifest.component_version is None:
        _LOGGER.warning(
            "Component '%s' does not report a version, so text sensor '%s' will "
            "read as unknown. Components opt in by defining COMPONENT_VERSION; "
            "this one has not.",
            name,
            config.get(CONF_NAME, name),
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    if (name := config.get(CONF_COMPONENT)) is not None:
        # Both literals go to flash on esp8266, like the ESPHome-version path.
        var = await text_sensor.new_text_sensor(config, cg.LogStringLiteral(name))
        await cg.register_component(var, config)
        # _final_validate has already warned when there is no version to report.
        if (version := get_component(name).component_version) is not None:
            cg.add(var.set_version(cg.FlashStringLiteral(version)))
        return

    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_hide_hash(config[CONF_HIDE_HASH]))
    cg.add(var.set_hide_timestamp(config[CONF_HIDE_TIMESTAMP]))
