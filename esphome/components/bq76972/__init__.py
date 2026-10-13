from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
)
from esphome.types import ConfigType

AUTO_LOAD = ["sensor"]
CODEOWNERS = ["@limpkin"]
DEPENDENCIES = ["i2c"]
DOMAIN = "bq76972"
MULTI_CONF = True

bq76972_ns = cg.esphome_ns.namespace("bq76972")
BQ76972Component = bq76972_ns.class_(
    "BQ76972Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_REG_DISABLED = "reg_disabled"
CONF_CRC_ENABLED = "crc_enabled"
CONF_STACK_VOLTAGE = "stack_voltage"

# Ordered as the chip reports them, starting at command 0x68
CONF_TEMPERATURES = [
    "internal_temperature",
    "cfetoff_temperature",
    "dfetoff_temperature",
    "alert_temperature",
    "ts1_temperature",
    "ts2_temperature",
    "ts3_temperature",
    "hdq_temperature",
    "dchg_temperature",
    "ddsg_temperature",
]

CONF_CELL_VOLTAGES = [f"cell_{i}_voltage" for i in range(1, 17)]

voltage_sensor_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)

temp_sensor_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BQ76972Component),
            cv.Optional(CONF_CRC_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_REG_DISABLED, default=False): cv.boolean,
            cv.Optional(CONF_STACK_VOLTAGE): voltage_sensor_schema,
            **{cv.Optional(key): voltage_sensor_schema for key in CONF_CELL_VOLTAGES},
            **{cv.Optional(key): temp_sensor_schema for key in CONF_TEMPERATURES},
        }
    )
    .extend(i2c.i2c_device_schema(0x08))
    .extend(cv.polling_component_schema("10s"))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_crc_mode(config[CONF_CRC_ENABLED]))
    cg.add(var.set_reg_disable(config[CONF_REG_DISABLED]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_STACK_VOLTAGE, var.set_stack_voltage_sensor)

    for idx, key in enumerate(CONF_TEMPERATURES):
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(var.set_temperature_sensor(sens, idx))

    for idx, key in enumerate(CONF_CELL_VOLTAGES):
        if (conf := config.get(key)) is not None:
            sens = await sensor.new_sensor(conf)
            cg.add(var.set_cell_sensor(sens, idx))


BQ76972_PROGRAM_I2C_ADDRESS_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(BQ76972Component),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.i2c_address),
    },
    key=CONF_ADDRESS,
)

automation.register_apply_action(
    "bq76972.program_i2c_address",
    BQ76972_PROGRAM_I2C_ADDRESS_SCHEMA,
    automation.ApplyField(CONF_ADDRESS, "program_i2c_address", cg.uint8),
)
