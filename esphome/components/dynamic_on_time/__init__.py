'''
tbd
'''
from esphome.const import (
    CONF_ID,
    CONF_ON_TIME,
    CONF_THEN,
    CONF_HOUR,
    CONF_MINUTE,
)
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.components.number import Number
from esphome.components.switch import Switch
from esphome.components.time import RealTimeClock

MULTI_CONF = True
DEPENDENCIES = ["time", "number", "switch"]
CODEOWNERS = ["@hostcc"]

CONF_RTC = 'rtc'
CONF_MON = 'mon'
CONF_TUE = 'tue'
CONF_WED = 'wed'
CONF_THU = 'thu'
CONF_FRI = 'fri'
CONF_SAT = 'sat'
CONF_SUN = 'sun'
CONF_DISABLED = 'disabled'

dynamic_on_time_ns = cg.esphome_ns.namespace("dynamic_on_time")
DynamicOnTimeComponent = dynamic_on_time_ns.class_(
    "DynamicOnTime", cg.Component
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DynamicOnTimeComponent),
    cv.Required(CONF_RTC): cv.use_id(RealTimeClock),
    cv.Required(CONF_HOUR): cv.use_id(Number),
    cv.Required(CONF_MINUTE): cv.use_id(Number),
    cv.Required(CONF_MON): cv.use_id(Switch),
    cv.Required(CONF_TUE): cv.use_id(Switch),
    cv.Required(CONF_WED): cv.use_id(Switch),
    cv.Required(CONF_THU): cv.use_id(Switch),
    cv.Required(CONF_FRI): cv.use_id(Switch),
    cv.Required(CONF_SAT): cv.use_id(Switch),
    cv.Required(CONF_SUN): cv.use_id(Switch),
    cv.Required(CONF_DISABLED): cv.use_id(Switch),
    cv.Required(CONF_ON_TIME): automation.validate_automation({}),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    '''
    tbd
    '''
    actions = []
    for conf in config[CONF_ON_TIME]:
        actions.extend(await automation.build_action_list(
            conf[CONF_THEN], cg.TemplateArguments(), [])
        )

    var = cg.new_Pvariable(
        config[CONF_ID],
        await cg.get_variable(config[CONF_RTC]),
        await cg.get_variable(config[CONF_HOUR]),
        await cg.get_variable(config[CONF_MINUTE]),
        await cg.get_variable(config[CONF_MON]),
        await cg.get_variable(config[CONF_TUE]),
        await cg.get_variable(config[CONF_WED]),
        await cg.get_variable(config[CONF_THU]),
        await cg.get_variable(config[CONF_FRI]),
        await cg.get_variable(config[CONF_SAT]),
        await cg.get_variable(config[CONF_SUN]),
        await cg.get_variable(config[CONF_DISABLED]),
        actions,
    )
    await cg.register_component(var, config)
