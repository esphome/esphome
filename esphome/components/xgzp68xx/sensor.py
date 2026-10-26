import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PASCAL,
)
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@gcormier"]

CONF_K_VALUE = "k_value"

xgzp68xx_ns = cg.esphome_ns.namespace("xgzp68xx")
XGZP68XXOversampling = xgzp68xx_ns.enum("XGZP68XXOversampling")
OVERSAMPLING_OPTIONS = XGZP68XXOversampling.options(
    "256X",
    "512X",
    "1024X",
    "2048X",
    "4096X",
    "8192X",
    "16384X",
    "32768X",
    prefix="XGZP68XX_OVERSAMPLING_",
)
XGZP68XXComponent = xgzp68xx_ns.class_(
    "XGZP68XXComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XGZP68XXComponent),
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PASCAL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_OVERSAMPLING, default="4096X"): cv.enum(
                        OVERSAMPLING_OPTIONS, upper=True
                    ),
                }
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_K_VALUE, default=4096): cv.int_range(min=1, max=65535),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6D))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)

    if await sensors(CONF_PRESSURE, var.set_pressure_sensor):
        oversampling = config[CONF_PRESSURE][CONF_OVERSAMPLING]
        cg.add(var.set_pressure_oversampling(oversampling))

    cg.add(var.set_k_value(config[CONF_K_VALUE]))
