import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ICON,
    CONF_INITIAL_VALUE,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    CONF_RESTORE_VALUE,
    CONF_STEP,
    CONF_UNIT_OF_MEASUREMENT,
    UNIT_CELSIUS,
)
from ...opentherm import (
    OpenThermComponent,
    CONF_OPENTHERM_ID,
)
from .. import opentherm

DEPENDENCIES = ["opentherm"]

OpenThermNumber = opentherm.class_("OpenThermNumber", number.Number, cg.Component)

CONF_CH_SETPOINT_TEMPERATURE = "ch_setpoint_temperature"
CONF_CH_2_SETPOINT_TEMPERATURE = "ch_2_setpoint_temperature"
CONF_DHW_SETPOINT_TEMPERATURE = "dhw_setpoint_temperature"

ICON_HOME_THERMOMETER = "mdi:home-thermometer"
ICON_WATER_THERMOMETER = "mdi:water-thermometer"

TYPES = [
    CONF_CH_SETPOINT_TEMPERATURE,
    CONF_CH_2_SETPOINT_TEMPERATURE,
    CONF_DHW_SETPOINT_TEMPERATURE,
]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(OpenThermComponent),
            cv.Optional(CONF_CH_SETPOINT_TEMPERATURE): number.number_schema(
                OpenThermNumber,
                icon=ICON_HOME_THERMOMETER,
                unit_of_measurement=UNIT_CELSIUS,
            ).extend(
                {
                    cv.Required(CONF_MAX_VALUE): cv.float_,
                    cv.Required(CONF_MIN_VALUE): cv.float_,
                    cv.Required(CONF_STEP): cv.positive_float,
                    cv.Optional(CONF_MODE, default="BOX"): cv.enum(
                        number.NUMBER_MODES, upper=True
                    ),
                    cv.Optional(CONF_INITIAL_VALUE): cv.float_,
                    cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
                }
            ).extend(cv.COMPONENT_SCHEMA),
            cv.Optional(CONF_CH_2_SETPOINT_TEMPERATURE): number.number_schema(
                OpenThermNumber,
                icon=ICON_HOME_THERMOMETER,
                unit_of_measurement=UNIT_CELSIUS,
            ).extend(
                {
                    cv.Required(CONF_MAX_VALUE): cv.float_,
                    cv.Required(CONF_MIN_VALUE): cv.float_,
                    cv.Required(CONF_STEP): cv.positive_float,
                    cv.Optional(CONF_MODE, default="BOX"): cv.enum(
                        number.NUMBER_MODES, upper=True
                    ),
                    cv.Optional(CONF_INITIAL_VALUE): cv.float_,
                    cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
                }
            ).extend(cv.COMPONENT_SCHEMA),
            cv.Optional(CONF_DHW_SETPOINT_TEMPERATURE):  number.number_schema(
                OpenThermNumber,
                icon=ICON_WATER_THERMOMETER,
                unit_of_measurement=UNIT_CELSIUS,
            ).extend(
                {
                    cv.Required(CONF_MAX_VALUE): cv.float_,
                    cv.Required(CONF_MIN_VALUE): cv.float_,
                    cv.Required(CONF_STEP): cv.positive_float,
                    cv.Optional(CONF_MODE, default="BOX"): cv.enum(
                        number.NUMBER_MODES, upper=True
                    ),
                    cv.Optional(CONF_INITIAL_VALUE): cv.float_,
                    cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
                }
            ).extend(cv.COMPONENT_SCHEMA),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if conf := config.get(key):
        var = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        if CONF_INITIAL_VALUE in conf:
            cg.add(var.set_initial_value(conf[CONF_INITIAL_VALUE]))
        if CONF_RESTORE_VALUE in conf:
            cg.add(var.set_restore_value(conf[CONF_RESTORE_VALUE]))
        cg.add(getattr(hub, f"set_{key}_number")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OPENTHERM_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
