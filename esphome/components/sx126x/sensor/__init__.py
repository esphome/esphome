import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_NOISE_FLOOR, CONF_SAMPLE_INTERVAL
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)
from esphome.types import ConfigType

from .. import CONF_SX126X_ID, SX126x, sx126x_ns

DEPENDENCIES = ["sx126x"]

# Samples kept for one update; each is two bytes
MAX_SAMPLES = 1000

SX126xSensor = sx126x_ns.class_(
    "SX126xSensor", cg.PollingComponent, cg.Parented.template(SX126x)
)


def _fits_buffer(config: ConfigType) -> ConfigType:
    update = config[CONF_UPDATE_INTERVAL]
    if update.total_milliseconds is None or update.total_milliseconds == 0:
        return config
    if (
        update.total_milliseconds / config[CONF_SAMPLE_INTERVAL].total_milliseconds
        > MAX_SAMPLES
    ):
        raise cv.Invalid(
            f"At most {MAX_SAMPLES} samples are taken in each update interval; "
            f"lengthen '{CONF_SAMPLE_INTERVAL}' or shorten '{CONF_UPDATE_INTERVAL}'"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SX126xSensor),
            cv.GenerateID(CONF_SX126X_ID): cv.use_id(SX126x),
            cv.Optional(CONF_SAMPLE_INTERVAL, default="100ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(milliseconds=10)),
            ),
            cv.Optional(CONF_NOISE_FLOOR): sensor.sensor_schema(
                unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    ).extend(cv.polling_component_schema("10s")),
    _fits_buffer,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SX126X_ID])
    sample_ms = config[CONF_SAMPLE_INTERVAL].total_milliseconds
    cg.add(var.set_sample_interval(sample_ms))
    update_ms = config[CONF_UPDATE_INTERVAL].total_milliseconds or sample_ms
    cg.add(var.set_max_samples(min(MAX_SAMPLES, update_ms // sample_ms + 1)))
    if noise_floor_config := config.get(CONF_NOISE_FLOOR):
        cg.add(var.set_noise_floor_sensor(await sensor.new_sensor(noise_floor_config)))
