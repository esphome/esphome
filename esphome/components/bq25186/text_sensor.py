import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import BQ25186_LISTENER_SCHEMA, CONF_BQ25186_ID, bq25186_ns, register_listener

DEPENDENCIES = ["bq25186"]

CONF_CHARGE_STATUS = "charge_status"
CONF_TS_STATUS = "ts_status"

BQ25186TextSensor = bq25186_ns.class_("BQ25186TextSensor", text_sensor.TextSensor)
BQ25186TextSensorKind = bq25186_ns.enum("BQ25186TextSensorKind", is_class=True)
_KINDS = {
    CONF_CHARGE_STATUS: BQ25186TextSensorKind.CHARGE_STATUS,
    CONF_TS_STATUS: BQ25186TextSensorKind.TS_STATUS,
}

CONFIG_SCHEMA = cv.All(
    BQ25186_LISTENER_SCHEMA.extend(
        {
            cv.Optional(key): text_sensor.text_sensor_schema(BQ25186TextSensor)
            for key in _KINDS
        }
    ),
    cv.has_at_least_one_key(*_KINDS),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25186_ID])
    for key, kind in _KINDS.items():
        if (conf := config.get(key)) is not None:
            var = await text_sensor.new_text_sensor(conf, kind)
            await register_listener(hub, var)
