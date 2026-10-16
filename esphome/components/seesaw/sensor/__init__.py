import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_ID,
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
                cv.Required(CONF_PIN): cv.int_,
            }
        )
        .extend(cv.polling_component_schema("60s")),
        CONF_ENCODER: sensor.sensor_schema(
            SeesawEncoder,
            unit_of_measurement=UNIT_STEPS,
            icon=ICON_ROTATE_RIGHT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_NONE,
        )
        .extend(
            {
                cv.GenerateID(CONF_SEESAW_ID): cv.use_id(Seesaw),
                cv.Optional(CONF_NUMBER, default=0): cv.int_,
                cv.Optional(CONF_MIN_VALUE): cv.int_,
                cv.Optional(CONF_MAX_VALUE): cv.int_,
            }
        )
        .extend(cv.COMPONENT_SCHEMA),
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
                cv.Required(CONF_CHANNEL): cv.int_,
            }
        )
        .extend(cv.polling_component_schema("60s")),
    },
    lower=True,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    seesaw = await cg.get_variable(config[CONF_SEESAW_ID])
    cg.add(var.set_parent(seesaw))
    if config[CONF_TYPE] == CONF_ADC:
        cg.add(var.set_pin(config[CONF_PIN]))
    elif config[CONF_TYPE] == CONF_ENCODER:
        cg.add(var.set_number(config[CONF_NUMBER]))
        if CONF_MIN_VALUE in config:
            cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
        if CONF_MAX_VALUE in config:
            cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
    elif config[CONF_TYPE] == CONF_TOUCH:
        cg.add(var.set_channel(config[CONF_CHANNEL]))
