"""Minimal stand-in for a component that installs updates on a secondary MCU."""

import esphome.codegen as cg
from esphome.components.http_request import http_request_ns
from esphome.components.http_request.update import UpdateTarget
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

McuFlasher = http_request_ns.class_("McuFlasher", cg.Component, UpdateTarget)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(McuFlasher)}).extend(
    cv.COMPONENT_SCHEMA
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
