import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, DEVICE_CLASS_EMPTY, ICON_RADIATOR, UNIT_EMPTY

DEPENDENCIES = ["i2c"]

CODEOWNERS = ["@SenexCrenshaw"]

sgp40_ns = cg.esphome_ns.namespace("sgp40")
SGP40Component = sgp40_ns.class_(
    "SGP40Component", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CONF_COMPENSATION = "compensation"
CONF_HUMIDITY_SOURCE = "humidity_source"
CONF_TEMPERATURE_SOURCE = "temperature_source"
CONF_STORE_BASELINE = "store_baseline"
CONF_VOC_BASELINE = "voc_baseline"

CONFIG_SCHEMA = (
    sensor.sensor_schema(UNIT_EMPTY, ICON_RADIATOR, 0, DEVICE_CLASS_EMPTY)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(SGP40Component),
            cv.Optional(CONF_STORE_BASELINE, default=True): cv.boolean,
            cv.Optional(CONF_VOC_BASELINE): cv.hex_uint16_t,
            cv.Optional(CONF_COMPENSATION): cv.Schema(
                {
                    cv.Required(CONF_HUMIDITY_SOURCE): cv.use_id(sensor.Sensor),
                    cv.Required(CONF_TEMPERATURE_SOURCE): cv.use_id(sensor.Sensor),
                },
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x59))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
    yield sensor.register_sensor(var, config)

    if CONF_COMPENSATION in config:
        compensation_config = config[CONF_COMPENSATION]
        sens = yield cg.get_variable(compensation_config[CONF_HUMIDITY_SOURCE])
        cg.add(var.set_humidity_sensor(sens))
        sens = yield cg.get_variable(compensation_config[CONF_TEMPERATURE_SOURCE])
        cg.add(var.set_temperature_sensor(sens))

    cg.add(var.set_store_baseline(config[CONF_STORE_BASELINE]))

    if CONF_VOC_BASELINE in config:
        cg.add(var.set_voc_baseline(CONF_VOC_BASELINE))
