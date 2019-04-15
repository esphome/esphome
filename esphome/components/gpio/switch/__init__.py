import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import pins
from esphome.components import switch
from esphome.const import CONF_ID, CONF_INTERLOCK, CONF_NAME, CONF_PIN, CONF_RESTORE_MODE
from .. import gpio_ns

GPIOSwitch = gpio_ns.class_('GPIOSwitch', switch.Switch, cg.Component)
GPIOSwitchRestoreMode = gpio_ns.enum('GPIOSwitchRestoreMode')

RESTORE_MODES = {
    'RESTORE_DEFAULT_OFF': GPIOSwitchRestoreMode.GPIO_SWITCH_RESTORE_DEFAULT_OFF,
    'RESTORE_DEFAULT_ON': GPIOSwitchRestoreMode.GPIO_SWITCH_RESTORE_DEFAULT_ON,
    'ALWAYS_OFF': GPIOSwitchRestoreMode.GPIO_SWITCH_ALWAYS_OFF,
    'ALWAYS_ON': GPIOSwitchRestoreMode.GPIO_SWITCH_ALWAYS_ON,
}

CONFIG_SCHEMA = cv.nameable(switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(GPIOSwitch),
    cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_RESTORE_MODE, default='RESTORE_DEFAULT_OFF'):
        cv.one_of(*RESTORE_MODES, upper=True, space='_'),
    cv.Optional(CONF_INTERLOCK): cv.ensure_list(cv.use_variable_id(switch.Switch)),
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    rhs = GPIOSwitch.new(config[CONF_NAME], pin)
    gpio = cg.Pvariable(config[CONF_ID], rhs)
    yield cg.register_component(gpio, config)
    yield switch.register_switch(gpio, config)

    cg.add(gpio.set_restore_mode(RESTORE_MODES[config[CONF_RESTORE_MODE]]))

    if CONF_INTERLOCK in config:
        interlock = []
        for it in config[CONF_INTERLOCK]:
            lock = yield cg.get_variable(it)
            interlock.append(lock)
        cg.add(gpio.set_interlock(interlock))
