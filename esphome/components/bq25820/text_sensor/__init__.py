import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import CONF_BQ25820_ID, BQ25820BaseSchema, bq25820_ns

DEPENDENCIES = ["bq25820"]

CONF_CHARGE_STATUS = "charge_status"
CONF_TEMP_STATUS = "temp_status"

ICON_BATTERY_CHARGING = "mdi:battery-charging"
ICON_THERMOMETER = "mdi:thermometer"

BQ25820ChargeStatusTextSensor = bq25820_ns.class_(
    "BQ25820ChargeStatusTextSensor", text_sensor.TextSensor
)
BQ25820TempStatusTextSensor = bq25820_ns.class_(
    "BQ25820TempStatusTextSensor", text_sensor.TextSensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CHARGE_STATUS): text_sensor.text_sensor_schema(
            BQ25820ChargeStatusTextSensor,
            icon=ICON_BATTERY_CHARGING,
        ),
        cv.Optional(CONF_TEMP_STATUS): text_sensor.text_sensor_schema(
            BQ25820TempStatusTextSensor,
            icon=ICON_THERMOMETER,
        ),
    }
).extend(BQ25820BaseSchema)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25820_ID])
    if (conf := config.get(CONF_CHARGE_STATUS)) is not None:
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.add_listener(sens))
    if (conf := config.get(CONF_TEMP_STATUS)) is not None:
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.add_listener(sens))
