import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import switch
from esphome.const import CONF_ID, CONF_INTERLOCK, CONF_PIN, CONF_RESTORE_MODE
from .. import gpio_ns

GPIOSwitch = gpio_ns.class_('GPIOSwitch', switch.Switch, cg.Component)
GPIOSwitchRestoreMode = gpio_ns.enum('GPIOSwitchRestoreMode')

RESTORE_MODES = {
    'RESTORE_DEFAULT_OFF': GPIOSwitchRestoreMode.GPIO_SWITCH_RESTORE_DEFAULT_OFF,
    'RESTORE_DEFAULT_ON': GPIOSwitchRestoreMode.GPIO_SWITCH_RESTORE_DEFAULT_ON,
    'ALWAYS_OFF': GPIOSwitchRestoreMode.GPIO_SWITCH_ALWAYS_OFF,
    'ALWAYS_ON': GPIOSwitchRestoreMode.GPIO_SWITCH_ALWAYS_ON,
}

CONF_INTERLOCK_WAIT_TIME = 'interlock_wait_time'
CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(GPIOSwitch),
    cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_RESTORE_MODE, default='RESTORE_DEFAULT_OFF'):
        cv.enum(RESTORE_MODES, upper=True, space='_'),
    cv.Optional(CONF_INTERLOCK): cv.ensure_list(cv.use_id(switch.Switch)),
    cv.Optional(CONF_INTERLOCK_WAIT_TIME, default='0ms'): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)

    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    if CONF_INTERLOCK in config:
        interlock = []
        for it in config[CONF_INTERLOCK]:
            lock = yield cg.get_variable(it)
            interlock.append(lock)
        cg.add(var.set_interlock(interlock))
        cg.add(var.set_interlock_wait_time(config[CONF_INTERLOCK_WAIT_TIME]))
