import logging

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

version_ns = cg.esphome_ns.namespace("version")
VersionTextSensor = version_ns.class_(
    "VersionTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(
        icon=ICON_NEW_BOX,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(VersionTextSensor),
            # Hide the config hash suffix and restore the pre-2026.1
            # version text format when set to true.
            cv.Optional(CONF_HIDE_HASH, default=False): cv.boolean,
            cv.Optional(CONF_HIDE_TIMESTAMP, default=False): cv.boolean,
            # Report this component's version instead of ESPHome's. Mainly for
            # external components, which otherwise have no version anywhere in
            # the config. hide_hash / hide_timestamp do not apply.
            cv.Optional(CONF_COMPONENT): cv.valid_name,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)

    if (name := config.get(CONF_COMPONENT)) is not None:
        cg.add(var.set_component_name(name))
        # A component reporting no version is not an error: COMPONENT_VERSION is
        # optional and most components will never declare one. Warn so the user
        # knows why the sensor reads unknown, and leave the state unpublished
        # rather than inventing a value - "unknown" is what Home Assistant
        # already uses for "we do not know", and an empty string would be
        # indistinguishable from a component that reports an empty version.
        manifest = get_component(name)
        if manifest is None or (version := manifest.component_version) is None:
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
