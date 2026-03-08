import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import CONF_SY6970_ID, SY6970Component, sy6970_ns

DEPENDENCIES = ["sy6970"]

CONF_BUS_STATUS = "bus_status"
CONF_CHARGE_STATUS = "charge_status"
CONF_NTC_STATUS = "ntc_status"

SY6970BusStatusTextSensor = sy6970_ns.class_(
    "SY6970BusStatusTextSensor", text_sensor.TextSensor
)
SY6970ChargeStatusTextSensor = sy6970_ns.class_(
    "SY6970ChargeStatusTextSensor", text_sensor.TextSensor
)
SY6970NtcStatusTextSensor = sy6970_ns.class_(
    "SY6970NtcStatusTextSensor", text_sensor.TextSensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SY6970_ID): cv.use_id(SY6970Component),
        cv.Optional(CONF_BUS_STATUS): text_sensor.text_sensor_schema(
            SY6970BusStatusTextSensor
        ),
        cv.Optional(CONF_CHARGE_STATUS): text_sensor.text_sensor_schema(
            SY6970ChargeStatusTextSensor
        ),
        cv.Optional(CONF_NTC_STATUS): text_sensor.text_sensor_schema(
            SY6970NtcStatusTextSensor
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SY6970_ID])

    if bus_status_config := config.get(CONF_BUS_STATUS):
        sens = await text_sensor.new_text_sensor(bus_status_config)
        cg.add(parent.add_listener(sens))

    if charge_status_config := config.get(CONF_CHARGE_STATUS):
        sens = await text_sensor.new_text_sensor(charge_status_config)
        cg.add(parent.add_listener(sens))

    if ntc_status_config := config.get(CONF_NTC_STATUS):
        sens = await text_sensor.new_text_sensor(ntc_status_config)
        cg.add(parent.add_listener(sens))
