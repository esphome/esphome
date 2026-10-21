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

SeesawADC = seesaw_ns.class_("SeesawADC", sensor.Sensor, cg.PollingComponent)
SeesawEncoder = seesaw_ns.class_("SeesawEncoder", sensor.Sensor, cg.Component)
SeesawTemperature = seesaw_ns.class_(
    "SeesawTemperature", sensor.Sensor, cg.PollingComponent
)
SeesawTouch = seesaw_ns.class_("SeesawTouch", sensor.Sensor, cg.PollingComponent)

CONF_ADC = "adc"
CONF_ENCODER = "encoder"
CONF_TOUCH = "touch"

# The setting each type takes in its constructor
_CONSTRUCTOR_KEYS = {
    CONF_ADC: CONF_PIN,
    CONF_ENCODER: CONF_NUMBER,
    CONF_TOUCH: CONF_CHANNEL,
}

_INT32 = cv.int_range(min=-(2**31), max=2**31 - 1)


def _validate_encoder_limits(config: ConfigType) -> ConfigType:
    low, high = config.get(CONF_MIN_VALUE), config.get(CONF_MAX_VALUE)
    if low is not None and high is not None and low > high:
        raise cv.Invalid(f"'{CONF_MIN_VALUE}' must not be above '{CONF_MAX_VALUE}'")
    return config


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_ADC: sensor.sensor_schema(
            SeesawADC,
            accuracy_decimals=0,
            state_class=STATE_CLASS_NONE,
        )
        .extend(
            {
                cv.GenerateID(CONF_SEESAW_ID): cv.use_id(Seesaw),
                cv.Required(CONF_PIN): cv.int_range(min=0, max=31),
            }
        )
        .extend(cv.polling_component_schema("60s")),
        CONF_ENCODER: cv.All(
            sensor.sensor_schema(
                SeesawEncoder,
                unit_of_measurement=UNIT_STEPS,
                icon=ICON_ROTATE_RIGHT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_NONE,
            )
            .extend(
                {
                    cv.GenerateID(CONF_SEESAW_ID): cv.use_id(Seesaw),
                    cv.Optional(CONF_NUMBER, default=0): cv.int_range(min=0, max=15),
                    cv.Optional(CONF_MIN_VALUE): _INT32,
                    cv.Optional(CONF_MAX_VALUE): _INT32,
                }
            )
            .extend(cv.COMPONENT_SCHEMA),
            _validate_encoder_limits,
        ),
        CONF_TEMPERATURE: sensor.sensor_schema(
            SeesawTemperature,
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_THERMOMETER,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        )
        .extend(
            {
                cv.GenerateID(CONF_SEESAW_ID): cv.use_id(Seesaw),
            }
        )
        .extend(cv.polling_component_schema("60s")),
        CONF_TOUCH: sensor.sensor_schema(
            SeesawTouch,
            accuracy_decimals=0,
            state_class=STATE_CLASS_NONE,
        )
        .extend(
            {
                cv.GenerateID(CONF_SEESAW_ID): cv.use_id(Seesaw),
                cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
            }
        )
        .extend(cv.polling_component_schema("60s")),
    },
    lower=True,
)


async def to_code(config: ConfigType) -> None:
    kind = config[CONF_TYPE]
    args = [config[_CONSTRUCTOR_KEYS[kind]]] if kind in _CONSTRUCTOR_KEYS else []
    var = await sensor.new_sensor(config, *args)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SEESAW_ID])
    if kind == CONF_ENCODER:
        cg.add_optional(var.set_min_value, config.get(CONF_MIN_VALUE))
        cg.add_optional(var.set_max_value, config.get(CONF_MAX_VALUE))
