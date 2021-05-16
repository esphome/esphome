import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, DEVICE_CLASS_TEMPERATURE, ICON_EMPTY, UNIT_CELSIUS

CODEOWNERS = ["@k7hpn"]
DEPENDENCIES = ["i2c"]

mcp9808_ns = cg.esphome_ns.namespace("mcp9808")
MCP9808Sensor = mcp9808_ns.class_(
    "MCP9808Sensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(UNIT_CELSIUS, ICON_EMPTY, 1, DEVICE_CLASS_TEMPERATURE)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(MCP9808Sensor),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x18))
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)
    yield sensor.register_sensor(var, config)
