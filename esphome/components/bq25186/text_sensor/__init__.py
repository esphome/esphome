import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from .. import CONF_BQ25186_ID, BQ25186Component, bq25186_ns

DEPENDENCIES = ["bq25186"]

CONF_CHARGE_STATUS = "charge_status"
CONF_TS_STATUS = "ts_status"

BQ25186ChargeStatusTextSensor = bq25186_ns.class_(
    "BQ25186ChargeStatusTextSensor", text_sensor.TextSensor
)
BQ25186TsStatusTextSensor = bq25186_ns.class_(
    "BQ25186TsStatusTextSensor", text_sensor.TextSensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BQ25186_ID): cv.use_id(BQ25186Component),
        cv.Optional(CONF_CHARGE_STATUS): text_sensor.text_sensor_schema(
            BQ25186ChargeStatusTextSensor
        ),
        cv.Optional(CONF_TS_STATUS): text_sensor.text_sensor_schema(
            BQ25186TsStatusTextSensor
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BQ25186_ID])

    if conf := config.get(CONF_CHARGE_STATUS):
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.add_listener(sens))

    if conf := config.get(CONF_TS_STATUS):
        sens = await text_sensor.new_text_sensor(conf)
        cg.add(parent.add_listener(sens))
