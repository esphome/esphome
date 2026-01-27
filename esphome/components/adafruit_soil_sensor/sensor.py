import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MOISTURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_PERCENT,
)

CODEOWNERS = ["@thegreatco"]

DEPENDENCIES = ["i2c"]

adafruit_soil_sensor_ns = cg.esphome_ns.namespace("adafruit_soil_sensor")
AdafruitSoilSensor = adafruit_soil_sensor_ns.class_(
    "AdafruitSoilSensor", cg.PollingComponent, i2c.I2CDevice
)
CONF_MOISTURE_RAW = CONF_MOISTURE + "_raw"
CONF_DRY_VALUE = "dry_value"
CONF_WET_VALUE = "wet_value"

CONF_MOISTURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.Required(CONF_DRY_VALUE): cv.int_range(min=0, max=65535),
        cv.Required(CONF_WET_VALUE): cv.int_range(min=0, max=65535),
    }
)


def _validate_percent_calibration(config):
    # If the calibrated moisture sensor is configured, require dry/wet values.
    if CONF_MOISTURE in config:
        if CONF_MOISTURE_RAW not in config:
            raise cv.Invalid(
                f"When '{CONF_MOISTURE}:' is configured, you must also set '{CONF_MOISTURE_RAW}:' in the config."
            )
        if (
            CONF_DRY_VALUE not in config[CONF_MOISTURE]
            or CONF_WET_VALUE not in config[CONF_MOISTURE]
        ):
            raise cv.Invalid(
                f"When '{CONF_MOISTURE}:' is configured, you must also set '{CONF_DRY_VALUE}:' and '{CONF_WET_VALUE}:' "
                "to calibrate the sensor."
            )
        if (
            config[CONF_MOISTURE][CONF_WET_VALUE]
            <= config[CONF_MOISTURE][CONF_DRY_VALUE]
        ):
            raise cv.Invalid(
                f"'{CONF_WET_VALUE}' must be greater than '{CONF_DRY_VALUE}'."
            )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AdafruitSoilSensor),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MOISTURE_RAW): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_MOISTURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_MOISTURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Required(CONF_DRY_VALUE): cv.int_range(min=0, max=65535),
                    cv.Required(CONF_WET_VALUE): cv.int_range(min=0, max=65535),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x36)),
    _validate_percent_calibration,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if (conf_temperature := config.get(CONF_TEMPERATURE)) is not None:
        ct = await sensor.new_sensor(conf_temperature)
        cg.add(var.set_temperature(ct))

    if (conf_moisture_raw := config.get(CONF_MOISTURE_RAW)) is not None:
        cmr = await sensor.new_sensor(conf_moisture_raw)
        cg.add(var.set_moisture_raw(cmr))

    if (conf_moisture_calibrated := config.get(CONF_MOISTURE)) is not None:
        cmc = await sensor.new_sensor(conf_moisture_calibrated)
        cg.add(var.set_moisture_calibrated(cmc))
        cg.add(
            var.set_calibration(
                conf_moisture_calibrated[CONF_DRY_VALUE],
                conf_moisture_calibrated[CONF_WET_VALUE],
            )
        )
