import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_OVERSAMPLING,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    CONF_VARIANT,
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
CONF_PRESSURE_MAX = "pressure_max"
CONF_PRESSURE_MIN = "pressure_min"

VARIANT_XGZP68XX = "xgzp68xx"
VARIANT_XGZP6899DC = "xgzp6899dc"

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
# The XGZP6899DC CFG_OSR register codes run in order from 256X (0) to 32768X (7)
XGZP6899DC_OVERSAMPLING_OPTIONS = {
    "256X": 0,
    "512X": 1,
    "1024X": 2,
    "2048X": 3,
    "4096X": 4,
    "8192X": 5,
    "16384X": 6,
    "32768X": 7,
}
XGZP68XXComponent = xgzp68xx_ns.class_(
    "XGZP68XXComponent", cg.PollingComponent, i2c.I2CDevice
)
XGZP6899DCComponent = xgzp68xx_ns.class_(
    "XGZP6899DCComponent", cg.PollingComponent, i2c.I2CDevice
)

pressure_pa = cv.float_with_unit("pressure", "(Pa)", optional_unit=True)


def _temperature_schema() -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _pressure_schema(oversampling: cv.Optional, options: dict) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_PASCAL,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_PRESSURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend({oversampling: cv.enum(options, upper=True)})


def _xgzp68xx_schema() -> cv.Schema:
    return (
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(XGZP68XXComponent),
                cv.Optional(CONF_PRESSURE): _pressure_schema(
                    cv.Optional(CONF_OVERSAMPLING, default="4096X"),
                    OVERSAMPLING_OPTIONS,
                ),
                cv.Optional(CONF_TEMPERATURE): _temperature_schema(),
                cv.Optional(CONF_K_VALUE, default=4096): cv.int_range(min=1, max=65535),
                # The likely mistake is a 6899DC config without its variant line
                **{
                    cv.Optional(key): cv.invalid(
                        f"'{key}' is only used with 'variant: {VARIANT_XGZP6899DC}'"
                    )
                    for key in (CONF_PRESSURE_MIN, CONF_PRESSURE_MAX)
                },
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(0x6D))
    )


def _validate_pressure_range(config: ConfigType) -> ConfigType:
    # The range only scales the pressure reading, so a temperature-only config can leave it out
    if CONF_PRESSURE not in config:
        return config
    for key in (CONF_PRESSURE_MIN, CONF_PRESSURE_MAX):
        if key not in config:
            raise cv.Invalid(f"'{key}' is required with a pressure sensor", path=[key])
    if config[CONF_PRESSURE_MAX] <= config[CONF_PRESSURE_MIN]:
        raise cv.Invalid(
            f"'{CONF_PRESSURE_MAX}' must be greater than '{CONF_PRESSURE_MIN}'"
        )
    return config


def _xgzp6899dc_schema() -> cv.All:
    return cv.All(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(XGZP6899DCComponent),
                # No default: the factory oversampling setting is kept unless one is given
                cv.Optional(CONF_PRESSURE): _pressure_schema(
                    cv.Optional(CONF_OVERSAMPLING), XGZP6899DC_OVERSAMPLING_OPTIONS
                ),
                cv.Optional(CONF_TEMPERATURE): _temperature_schema(),
                cv.Optional(CONF_PRESSURE_MIN): pressure_pa,
                cv.Optional(CONF_PRESSURE_MAX): pressure_pa,
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(0x58)),
        _validate_pressure_range,
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        VARIANT_XGZP68XX: _xgzp68xx_schema(),
        VARIANT_XGZP6899DC: _xgzp6899dc_schema(),
    },
    key=CONF_VARIANT,
    default_type=VARIANT_XGZP68XX,
    lower=True,
)


async def to_code(config: ConfigType) -> None:
    # The XGZP6899DC takes the rated span of the part in Pa, unused without a pressure sensor
    args = []
    if config[CONF_VARIANT] == VARIANT_XGZP6899DC:
        args.append(
            config[CONF_PRESSURE_MAX] - config[CONF_PRESSURE_MIN]
            if CONF_PRESSURE in config
            else 0.0
        )
    var = await i2c.new_i2c_component(config, *args)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)

    if await sensors(CONF_PRESSURE, var.set_pressure_sensor):
        cg.add_optional(
            var.set_pressure_oversampling, config[CONF_PRESSURE].get(CONF_OVERSAMPLING)
        )

    cg.add_optional(var.set_k_value, config.get(CONF_K_VALUE))
