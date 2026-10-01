from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_EC,
    CONF_ID,
    CONF_TEMPERATURE,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
)
from esphome.types import ConfigType

CODEOWNERS = ["@SeByDocKy"]
DEPENDENCIES = ["i2c"]

CONF_E25 = "e25"
CONF_VWC = "vwc"

ICON_EPSILON = "mdi:epsilon"
ICON_SIGMA = "mdi:sigma-lower"
ICON_ALPHA = "mdi:alpha-h-circle-outline"

pmwcs3_ns = cg.esphome_ns.namespace("pmwcs3")
PMWCS3Component = pmwcs3_ns.class_(
    "PMWCS3Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PMWCS3Component),
            cv.Optional(CONF_E25): sensor.sensor_schema(
                icon=ICON_EPSILON,
                accuracy_decimals=3,
                unit_of_measurement="dS/m",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_EC): sensor.sensor_schema(
                icon=ICON_SIGMA,
                accuracy_decimals=2,
                unit_of_measurement="mS/m",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                icon=ICON_THERMOMETER,
                accuracy_decimals=3,
                unit_of_measurement="°C",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VWC): sensor.sensor_schema(
                icon=ICON_ALPHA,
                accuracy_decimals=3,
                unit_of_measurement="cm3cm−3",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x63))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_E25, var.set_e25_sensor)
    await sensors(CONF_EC, var.set_ec_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_VWC, var.set_vwc_sensor)


# Actions
PMWCS3_CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(PMWCS3Component),
    }
)

automation.register_apply_action(
    "pmwcs3.air_calibration",
    PMWCS3_CALIBRATION_SCHEMA,
    automation.ApplyCall("air_calibration()"),
)

automation.register_apply_action(
    "pmwcs3.water_calibration",
    PMWCS3_CALIBRATION_SCHEMA,
    automation.ApplyCall("water_calibration()"),
)

PMWCS3_NEW_I2C_ADDRESS_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(PMWCS3Component),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.i2c_address),
    },
    key=CONF_ADDRESS,
)

automation.register_apply_action(
    "pmwcs3.new_i2c_address",
    PMWCS3_NEW_I2C_ADDRESS_SCHEMA,
    automation.ApplyField(CONF_ADDRESS, "new_i2c_address", cg.uint8),
)
