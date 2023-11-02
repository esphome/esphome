import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_PRESENCE,
    ICON_MOTION_SENSOR,
)

from . import CONF_LD2450_ID, CONF_ZONE_ID, CONF_ZONES, LD2450Component, PresenceZone

DEPENDENCIES = ["ld2450"]
CONF_ANY_PRESENCE = "any_presence"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    cv.Optional(CONF_ANY_PRESENCE): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PRESENCE, icon=ICON_MOTION_SENSOR
    ),
    cv.Optional(CONF_ZONES): cv.ensure_list(
        binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PRESENCE, icon=ICON_MOTION_SENSOR
        ).extend(
            {
                cv.Required(CONF_ZONE_ID): cv.use_id(PresenceZone),
            },
            extra=cv.ALLOW_EXTRA,
        )
    ),
}


async def to_code(config):
    ld2450_component = await cg.get_variable(config[CONF_LD2450_ID])
    if any_presence := config.get(CONF_ANY_PRESENCE):
        sens = await binary_sensor.new_binary_sensor(any_presence)
        cg.add(ld2450_component.set_any_presence_binary_sensor(sens))

    if zone_configs := config.get(CONF_ZONES):
        for zone_config in zone_configs:
            zone_obj = await cg.get_variable(zone_config[CONF_ZONE_ID])
            sens = await binary_sensor.new_binary_sensor(zone_config)
            cg.add(zone_obj.set_presence_binary_sensor(sens))
