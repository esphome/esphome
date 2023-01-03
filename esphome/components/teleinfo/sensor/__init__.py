import esphome.codegen as cg
from esphome.components import sensor
from esphome.const import CONF_ID, ICON_FLASH, UNIT_WATT_HOURS

from .. import (
    CONF_TAG_NAME,
    TELEINFO_LISTENER_SCHEMA,
    teleinfo_ns,
    CONF_TELEINFO_ID,
)


TeleInfoSensor = teleinfo_ns.class_("TeleInfoSensor", sensor.Sensor, cg.Component)

CONFIG_SCHEMA = sensor.sensor_schema(
    TeleInfoSensor,
    unit_of_measurement=UNIT_WATT_HOURS,
    icon=ICON_FLASH,
    accuracy_decimals=0,
).extend(TELEINFO_LISTENER_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_TAG_NAME])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    teleinfo = await cg.get_variable(config[CONF_TELEINFO_ID])
    cg.add(teleinfo.register_teleinfo_listener(var))
