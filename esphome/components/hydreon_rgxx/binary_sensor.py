import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_COLD, DEVICE_CLASS_PROBLEM

from . import HydreonRGxxComponent, hydreon_rgxx_ns

CONF_HYDREON_RGXX_ID = "hydreon_rgxx_id"
CONF_TOO_COLD = "too_cold"
CONF_LENS_BAD = "lens_bad"
CONF_EM_SAT = "em_sat"

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
        cv.Optional(CONF_LENS_BAD): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_EM_SAT): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
        ),
    }
)


async def to_code(config):
    main_sensor = await cg.get_variable(config[CONF_HYDREON_RGXX_ID])
    bin_component = cg.new_Pvariable(config[CONF_ID], main_sensor)
    await cg.register_component(bin_component, config)

    mapping = {
        CONF_TOO_COLD: main_sensor.set_too_cold_sensor,
        CONF_LENS_BAD: main_sensor.set_lens_bad_sensor,
        CONF_EM_SAT: main_sensor.set_em_sat_sensor,
    }

    for key, value in mapping.items():
        if key in config:
            sensor = await binary_sensor.new_binary_sensor(config[key])
            cg.add(value(sensor))
