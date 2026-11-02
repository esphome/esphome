import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

from .. import CONF_MT6701_SPI_ID, MT6701SPIComponent, mt6701_spi_ns

DEPENDENCIES = ["mt6701_spi"]

CONF_PUSH_BUTTON = "push_button"
CONF_TRACK_LOSS = "track_loss"

MT6701SPIBinarySensors = mt6701_spi_ns.class_(
    "MT6701SPIBinarySensors", cg.Component, cg.Parented.template(MT6701SPIComponent)
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(MT6701SPIBinarySensors),
        cv.GenerateID(CONF_MT6701_SPI_ID): cv.use_id(MT6701SPIComponent),
        # A press has to be seen between two reads, so the default is short
        cv.Optional(
            CONF_UPDATE_INTERVAL, default="50ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PUSH_BUTTON): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_TRACK_LOSS): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_MT6701_SPI_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_PUSH_BUTTON, var.set_push_button_binary_sensor)
    await binary_sensors(CONF_TRACK_LOSS, var.set_track_loss_binary_sensor)
