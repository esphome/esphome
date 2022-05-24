import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_COLD,
)

from . import hydreon_rgxx_ns, HydreonRGxxComponent

CONF_HYDREON_RGXX_ID = "hydreon_rgxx_id"
CONF_TOO_COLD = "too_cold"

HydreonRGxxBinarySensor = hydreon_rgxx_ns.class_(
    "HydreonRGxxBinaryComponent", cg.Component
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HydreonRGxxBinarySensor),
        cv.GenerateID(CONF_HYDREON_RGXX_ID): cv.use_id(HydreonRGxxComponent),
        cv.Optional(CONF_TOO_COLD): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_COLD
        ),
    }
)


async def to_code(config):
    main_sensor = await cg.get_variable(config[CONF_HYDREON_RGXX_ID])
    bin_component = cg.new_Pvariable(config[CONF_ID], main_sensor)
    await cg.register_component(bin_component, config)
    if CONF_TOO_COLD in config:
        tc = await binary_sensor.new_binary_sensor(config[CONF_TOO_COLD])
        cg.add(main_sensor.set_too_cold_sensor(tc))
