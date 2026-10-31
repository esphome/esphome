import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_THERMOCOUPLE_TYPE
from esphome.types import ConfigType

CONF_HOT_JUNCTION = "hot_junction"
CONF_COLD_JUNCTION = "cold_junction"

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@mreditor97"]

mcp9600_ns = cg.esphome_ns.namespace("mcp9600")
MCP9600Component = mcp9600_ns.class_(
    "MCP9600Component", cg.PollingComponent, i2c.I2CDevice
)

MCP9600ThermocoupleType = mcp9600_ns.enum("MCP9600ThermocoupleType")
THERMOCOUPLE_TYPE = MCP9600ThermocoupleType.options(
    "K", "J", "T", "N", "S", "E", "B", "R", prefix="MCP9600_THERMOCOUPLE_TYPE_"
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MCP9600Component),
            cv.Optional(CONF_THERMOCOUPLE_TYPE, default="K"): cv.enum(
                THERMOCOUPLE_TYPE, upper=True
            ),
            cv.Optional(CONF_HOT_JUNCTION): sensor.sensor_schema(
                kind=sensor.TEMPERATURE, accuracy_decimals=1
            ),
            cv.Optional(CONF_COLD_JUNCTION): sensor.sensor_schema(
                kind=sensor.TEMPERATURE, accuracy_decimals=1
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x67))
)

FINAL_VALIDATE_SCHEMA = i2c.final_validate_device_schema(
    "mcp9600", min_frequency="10khz"
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    cg.add(var.set_thermocouple_type(config[CONF_THERMOCOUPLE_TYPE]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_HOT_JUNCTION, var.set_hot_junction_sensor)
    await sensors(CONF_COLD_JUNCTION, var.set_cold_junction_sensor)
