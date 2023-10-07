import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

CONF_THERMOCOUPLE_TYPE = "thermocouple_type"
CONF_HOT_JUNCTION = "hot_junction"
CONF_COLD_JUNCTION = "cold_junction"

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@mreditor97"]

mcp9600_ns = cg.esphome_ns.namespace("mcp9600")
MCP9600Component = mcp9600_ns.class_(
    "MCP9600Component", cg.PollingComponent, i2c.I2CDevice
)

MCP9600ThermocoupleType = mcp9600_ns.enum("MCP9600ThermocoupleType")
THERMOCOUPLE_TYPE = {
    "K": MCP9600ThermocoupleType.MCP9600_THERMOCOUPLE_TYPE_K,
    "J": MCP9600ThermocoupleType.MCP9600_THERMOCOUPLE_TYPE_J,
    "T": MCP9600ThermocoupleType.MCP9600_THERMOCOUPLE_TYPE_T,
    "N": MCP9600ThermocoupleType.MCP9600_THERMOCOUPLE_TYPE_N,
    "S": MCP9600ThermocoupleType.MCP9600_THERMOCOUPLE_TYPE_S,
    "E": MCP9600ThermocoupleType.MCP9600_THERMOCOUPLE_TYPE_E,
    "B": MCP9600ThermocoupleType.MCP9600_THERMOCOUPLE_TYPE_B,
    "R": MCP9600ThermocoupleType.MCP9600_THERMOCOUPLE_TYPE_R,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MCP9600Component),
            cv.Optional(CONF_THERMOCOUPLE_TYPE, default="K"): cv.enum(
                THERMOCOUPLE_TYPE, upper=True
            ),
            cv.Optional(CONF_HOT_JUNCTION): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COLD_JUNCTION): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x67))
)

FINAL_VALIDATE_SCHEMA = i2c.final_validate_device_schema(
    "mcp9600", min_frequency="10khz"
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_thermocouple_type(config[CONF_THERMOCOUPLE_TYPE]))

    if CONF_HOT_JUNCTION in config:
        conf = config[CONF_HOT_JUNCTION]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_hot_junction(sens))

    if CONF_COLD_JUNCTION in config:
        conf = config[CONF_COLD_JUNCTION]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_cold_junction(sens))
