from esphome.const import CONF_INVERTED, CONF_MODE, CONF_NUMBER, CONF_PCF8574, \
    CONF_SETUP_PRIORITY, CONF_MCP23017, CONF_ID
from esphome.core import CORE, EsphomeError, coroutine
from esphome.cpp_generator import IntLiteral, RawExpression, add, Pvariable
from esphome.cpp_types import App, GPIOPin


@coroutine
def gpio_pin_expression(conf):
    if conf is None:
        return
    number = conf[CONF_NUMBER]
    mode = conf[CONF_MODE]
    inverted = conf.get(CONF_INVERTED)
    if CONF_PCF8574 in conf:
        from esphome.components import pcf8574

        hub = yield CORE.get_variable(conf[CONF_PCF8574])

        mode = pcf8574.PCF8675_GPIO_MODES[mode]
        yield hub.make_pin(number, mode, inverted)
        return
    if CONF_MCP23017 in conf:
        from esphome.components import mcp23017

        hub = yield CORE.get_variable(conf[CONF_MCP23017])
        mode = mcp23017.MCP23017_GPIO_MODES[mode]
        yield hub.make_pin(number, mode, inverted)
        return
    yield GPIOPin.new(number, RawExpression(mode), inverted)


@coroutine
def register_component(obj, config):
    if CONF_SETUP_PRIORITY in config:
        add(obj.set_setup_priority(config[CONF_SETUP_PRIORITY]))
    add(App.register_component(obj))


@coroutine
def _build_registry_entry(registry, full_config):
    key, config = next((k, v) for k, v in full_config.items() if k in registry)
    builder = coroutine(registry[key][1])
    yield builder(config)


@coroutine
def build_registry_list(registry, config):
    actions = []
    for conf in config:
        action = yield _build_registry_entry(registry, conf)
        actions.append(action)
    yield actions
