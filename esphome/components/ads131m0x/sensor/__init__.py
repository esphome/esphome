import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_MODEL, CONF_TYPE
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_ADS131M0X_ID, MODELS, ADS131M0x, ads131m0x_ns

DEPENDENCIES = ["ads131m0x"]

ADS131M0xSensor = ads131m0x_ns.class_(
    "ADS131M0xSensor",
    cg.PollingComponent,
    sensor.Sensor,
    cg.Parented.template(ADS131M0x),
)

ADS131M0xSensorType = ads131m0x_ns.enum("ADS131M0xSensorType")
SENSOR_TYPES = {
    "rms": ADS131M0xSensorType.ADS131M0X_SENSOR_TYPE_RMS,
    "mean": ADS131M0xSensorType.ADS131M0X_SENSOR_TYPE_MEAN,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        ADS131M0xSensor,
        kind=sensor.VOLTAGE,
        accuracy_decimals=4,
    )
    .extend(
        {
            cv.GenerateID(CONF_ADS131M0X_ID): cv.use_id(ADS131M0x),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
            cv.Optional(CONF_TYPE, default="rms"): cv.enum(SENSOR_TYPES, lower=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


def _final_validate(config: ConfigType) -> ConfigType:
    hub_config = fv.get_declaring_config(config[CONF_ADS131M0X_ID])
    channels = MODELS[hub_config[CONF_MODEL]]
    if config[CONF_CHANNEL] >= channels:
        raise cv.Invalid(
            f"{hub_config[CONF_MODEL]} has {channels} channels (0..{channels - 1})",
            [CONF_CHANNEL],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_ADS131M0X_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_type(config[CONF_TYPE]))
