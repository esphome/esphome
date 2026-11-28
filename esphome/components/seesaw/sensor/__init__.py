import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_NUMBER,
    CONF_PIN,
    CONF_TEMPERATURE,
    CONF_TYPE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_ROTATE_RIGHT,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_NONE,
    UNIT_CELSIUS,
    UNIT_STEPS,
)
from esphome.types import ConfigType

from .. import CONF_SEESAW_ID, Seesaw, seesaw_ns

DEPENDENCIES = ["seesaw"]

SeesawSensor = seesaw_ns.class_("SeesawSensor", sensor.Sensor, cg.PollingComponent)
SeesawEncoder = seesaw_ns.class_("SeesawEncoder", sensor.Sensor, cg.Component)
SeesawSensorType = seesaw_ns.enum("SeesawSensorType", is_class=True)

CONF_ADC = "adc"
CONF_ENCODER = "encoder"
CONF_TOUCH = "touch"

# The polled types: their enum value and the key holding their pin or channel
_POLLED = {
    CONF_ADC: (SeesawSensorType.SEESAW_SENSOR_TYPE_ADC, CONF_PIN),
    CONF_TEMPERATURE: (SeesawSensorType.SEESAW_SENSOR_TYPE_TEMPERATURE, None),
    CONF_TOUCH: (SeesawSensorType.SEESAW_SENSOR_TYPE_TOUCH, CONF_CHANNEL),
}
_HUB = cv.Schema({cv.GenerateID(CONF_SEESAW_ID): cv.use_id(Seesaw)})

_INT32 = cv.int_range(min=-(2**31), max=2**31 - 1)


def _validate_encoder_limits(config: ConfigType) -> ConfigType:
    low, high = config.get(CONF_MIN_VALUE), config.get(CONF_MAX_VALUE)
    if low is not None and high is not None and low > high:
        raise cv.Invalid(f"'{CONF_MIN_VALUE}' must not be above '{CONF_MAX_VALUE}'")
    return config


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_ADC: sensor.sensor_schema(
            SeesawSensor,
            accuracy_decimals=0,
            state_class=STATE_CLASS_NONE,
        )
        .extend(_HUB)
        .extend({cv.Required(CONF_PIN): cv.int_range(min=0, max=31)})
        .extend(cv.polling_component_schema("60s")),
        CONF_ENCODER: cv.All(
            sensor.sensor_schema(
                SeesawEncoder,
                unit_of_measurement=UNIT_STEPS,
                icon=ICON_ROTATE_RIGHT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_NONE,
            )
            .extend(_HUB)
            .extend(
                {
                    cv.Optional(CONF_NUMBER, default=0): cv.int_range(min=0, max=15),
                    cv.Optional(CONF_MIN_VALUE): _INT32,
                    cv.Optional(CONF_MAX_VALUE): _INT32,
                }
            )
            .extend(cv.COMPONENT_SCHEMA),
            _validate_encoder_limits,
        ),
        CONF_TEMPERATURE: sensor.sensor_schema(
            SeesawSensor,
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_THERMOMETER,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend(_HUB)
        .extend(cv.polling_component_schema("60s")),
        CONF_TOUCH: sensor.sensor_schema(
            SeesawSensor,
            accuracy_decimals=0,
            state_class=STATE_CLASS_NONE,
        )
        .extend(_HUB)
        .extend({cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15)})
        .extend(cv.polling_component_schema("60s")),
    },
    lower=True,
)


async def to_code(config: ConfigType) -> None:
    if (kind := config[CONF_TYPE]) == CONF_ENCODER:
        var = await sensor.new_sensor(config, config[CONF_NUMBER])
        cg.add_optional(var.set_min_value, config.get(CONF_MIN_VALUE))
        cg.add_optional(var.set_max_value, config.get(CONF_MAX_VALUE))
    else:
        sensor_type, channel_key = _POLLED[kind]
        channel = config[channel_key] if channel_key else 0
        var = await sensor.new_sensor(config, sensor_type, channel)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SEESAW_ID])
